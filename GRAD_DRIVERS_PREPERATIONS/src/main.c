/*****************************************************************************
 * main.c — Road Surface Classifier (TinyML on STM32F401CC + FreeRTOS)
 *
 * Pipeline:
 *   TIM2 (100 Hz) → Thread 1 (Sensor) → RingBuffer
 *                                          ↓
 *                                    Thread 2 (TinyML)
 *                                          ↓
 *                                    Inference Result Queue → Thread 3 (TBD)
 *
 * Build modes:
 *   #define REPLAY_MODE 1   → Thread 1 reads from replay_data.h instead
 *                              of the live IMU. Used for offline pipeline
 *                              validation against known-good CSV recordings.
 *   #define REPLAY_MODE 0   → Live mode (default). Thread 1 reads MPU6050.
 *
 * Validated:
 *   ✅ Smooth CSV → predicts smooth (vote 7/0, conf 64%)
 *   ✅ Rough CSV  → predicts rough  (vote 0/7, conf 95%)
 *****************************************************************************/

/* ===== Build Configuration ============================================== */
#define REPLAY_MODE   0   /* 1 = inject CSV, 0 = live IMU */

/* ===== Includes ========================================================= */
#include "RCC_INTERFACE.h"
#include "GPIO_INTERFACE.h"
#include "TIM_INTERFACE.h"
#include "NVIC_INTERFACE.h"
#include "DWT_INTERFACE.h"
#include "I2C_SERVICE.h"
#include "MPU6050.h"
#include "RING_BUFFER.h"
#include "DMA_INTERFACE.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

#include "scale.h"
#include "features.h"
#include "quantize.h"
#include "inference.h"
#include "vote.h"

#if REPLAY_MODE
  #include "replay_data.h"
#endif

/* ===== Configuration Constants ========================================== */
#define WINDOW_STRIDE         25U   /* 50% overlap → 4 Hz inference rate */
#define THREAD1_STACK_WORDS   256U
#define THREAD2_STACK_WORDS   1024U
#define THREAD1_PRIORITY      3U
#define THREAD2_PRIORITY      2U
#define RESULT_QUEUE_DEPTH    4U
#define DMA_TIMEOUT_MS        5U

/* ===== FreeRTOS Object Storage ========================================== */
static StaticSemaphore_t s_tim_sem_storage;
static SemaphoreHandle_t s_tim_sem;

static StaticSemaphore_t s_dma_sem_storage;
static SemaphoreHandle_t s_dma_sem;

static StackType_t       s_thread1_stack[THREAD1_STACK_WORDS];
static StaticTask_t      s_thread1_tcb;
static TaskHandle_t      s_thread1_handle;

static StackType_t       s_thread2_stack[THREAD2_STACK_WORDS];
static StaticTask_t      s_thread2_tcb;
static TaskHandle_t      s_thread2_handle;

static StaticQueue_t     s_result_queue_storage;
static uint8_t           s_result_queue_buffer[RESULT_QUEUE_DEPTH *
                                               sizeof(Inference_Result_t)];
QueueHandle_t            g_result_queue;

/* ===== Sensor State ===================================================== */
static volatile MPU6050_RawData_t s_latest_sample;

/* ===== Diagnostics ====================================================== */
/* Thread 2 activity counters */
volatile uint32_t g_t2_wake_count       = 0U;
volatile uint32_t g_t2_inference_count  = 0U;
volatile uint32_t g_t2_queue_drops      = 0U;

/* Thread 1 runtime stats (computed in idle hook) */
volatile uint32_t g_sensor_cpu_cycles      = 0U;
volatile uint32_t g_sensor_stack_remaining = 0U;
volatile uint32_t g_total_runtime          = 0U;
volatile uint32_t g_sensor_cpu_percent_x100 = 0U;
volatile uint32_t g_sensor_stack_used       = 0U;
volatile uint32_t g_sensor_stack_percent    = 0U;
volatile uint32_t g_uptime_ms               = 0U;

#if REPLAY_MODE
/* Replay-mode validation counters */
volatile uint32_t g_replay_samples_pushed = 0U;
volatile uint32_t g_replay_done           = 0U;
volatile uint32_t g_replay_last_label     = 0xFFU;
volatile uint32_t g_replay_last_conf      = 0U;
volatile uint32_t g_replay_vote_smooth    = 0U;
volatile uint32_t g_replay_vote_rough     = 0U;
#endif

/* ===== ISR Callbacks ==================================================== */

/* TIM2 100 Hz tick — wakes Thread 1 */
static void on_tim2_update(void *ctx)
{
    (void)ctx;
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_tim_sem, &woken);
    portYIELD_FROM_ISR(woken);
}

/* MPU6050 DMA-read complete — copies sample, signals Thread 1 */
static void on_mpu_read_done(const MPU6050_RawData_t *data, void *ctx)
{
    (void)ctx;
    s_latest_sample = *data;

    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_dma_sem, &woken);
    portYIELD_FROM_ISR(woken);
}

/* ===== Thread 1 — Sensor Producer ======================================= */
/*
 * Live mode: triggers MPU6050 DMA read on every TIM2 tick, pushes
 *            parsed sample into ring buffer, notifies Thread 2.
 *
 * Replay mode: pushes pre-recorded samples from replay_data.h at the
 *              same 10 ms cadence. Used for pipeline validation against
 *              known-good CSV recordings.
 */
static void Thread1_Sensor(void *arg)
{
    (void)arg;

#if REPLAY_MODE
    uint32_t idx = 0U;

    for (;;)
    {
        xSemaphoreTake(s_tim_sem, portMAX_DELAY);

        if (idx < REPLAY_NUM_SAMPLES)
        {
            MPU6050_RawData_t sample = replay_samples[idx];

            if (RingBuffer_Push(&sample) == RING_BUFFER_OK)
            {
                xTaskNotifyGive(s_thread2_handle);
                g_replay_samples_pushed++;
            }
            idx++;
        }
        else
        {
            g_replay_done = 1U;
        }

        GPIO_TogglePin(GPIO_PORTC, GPIO_PIN13);
    }
#else
    MPU6050_RawData_t snapshot;

    for (;;)
    {
        /* 1. Wait for 10 ms tick from TIM2 */
        xSemaphoreTake(s_tim_sem, portMAX_DELAY);

        /* 2. Trigger DMA read from MPU6050 */
        if (MPU6050_TriggerRead(on_mpu_read_done, NULL) == MPU6050_OK)
        {
            /* 3. Wait for DMA completion (5 ms timeout) */
            if (xSemaphoreTake(s_dma_sem, pdMS_TO_TICKS(DMA_TIMEOUT_MS)) == pdTRUE)
            {
                /* 4. Snapshot the latest parsed sample */
                taskENTER_CRITICAL();
                snapshot = (MPU6050_RawData_t)s_latest_sample;
                taskEXIT_CRITICAL();

                /* 5. Push to ring buffer; notify Thread 2 if successful */
                if (RingBuffer_Push(&snapshot) == RING_BUFFER_OK)
                {
                    xTaskNotifyGive(s_thread2_handle);
                }

                GPIO_TogglePin(GPIO_PORTC, GPIO_PIN13);
            }
        }
    }
#endif
}

/* ===== Thread 2 — TinyML Inference ====================================== */
/*
 * Drains all available 50-sample windows from the ring buffer per wake.
 * Pipeline (per window):
 *   PeekWindow → Repack(struct→flat) → Scale → Features →
 *   Quantize_TS + Quantize_NormalizeStat → Quantize_Stat → Inference →
 *   Vote → (if ready) Send to Thread 3 result queue → Advance(stride)
 *
 * NOTE on repacking: MPU6050_RawData_t is 14 bytes (includes temp_raw).
 * Scale_RawWindow expects a flat int16[][6] layout. We MUST repack —
 * casting the struct array directly causes byte-misaligned reads that
 * corrupt the gyro channels. See lessons-learned in handoff doc.
 */
static void Thread2_TinyML(void *arg)
{
    (void)arg;

    /* Stack-local window buffer (50 × 14B = 700 B) */
    MPU6050_RawData_t window[WINDOW_SIZE];

    /* Static working buffers — kept off the stack to save space */
    static int16_t   flat_window[WINDOW_SIZE][N_FEATURES];
    static float32_t scaled     [WINDOW_SIZE][N_FEATURES];
    static float32_t features   [N_STAT_FEATURES];
    static int8_t    ts_q       [WINDOW_SIZE * N_FEATURES];
    static int8_t    stat_q     [N_STAT_FEATURES];

    /* One-time initialisation */
    Vote_t vote;
    Vote_Init(&vote);
    Features_Init();
    Inference_Init();

    for (;;)
    {
        /* 1. Block until Thread 1 notifies us a new sample arrived */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        g_t2_wake_count++;

        /* 2. Drain all complete windows (≥ 50 samples available) */
        while (RingBuffer_Count() >= WINDOW_SIZE)
        {
            /* 2a. Snapshot the oldest 50 samples (without consuming) */
            if (RingBuffer_PeekWindow(window, WINDOW_SIZE) != RING_BUFFER_OK)
            {
                break;
            }

            /* 2b. Repack struct → flat int16 (skips temp_raw) */
            for (uint32_t t = 0U; t < (uint32_t)WINDOW_SIZE; ++t)
            {
                flat_window[t][0] = window[t].accel_x;
                flat_window[t][1] = window[t].accel_y;
                flat_window[t][2] = window[t].accel_z;
                flat_window[t][3] = window[t].gyro_x;
                flat_window[t][4] = window[t].gyro_y;
                flat_window[t][5] = window[t].gyro_z;
            }

            /* 2c. ML pipeline */
            Scale_RawWindow(flat_window, scaled);
            Features_Extract(scaled, features);
            Quantize_TS(scaled, ts_q);
            Quantize_NormalizeStat(features);
            Quantize_Stat(features, stat_q);

            Inference_Result_t result;
            Inference_Run(ts_q, stat_q, &result);
            g_t2_inference_count++;

#if REPLAY_MODE
            /* Track per-window predictions for offline validation */
            g_replay_last_label = (uint32_t)result.label;
            g_replay_last_conf  = (uint32_t)result.confidence;
            if (result.label == INFERENCE_LABEL_SMOOTH) {
                g_replay_vote_smooth++;
            } else {
                g_replay_vote_rough++;
            }
#endif

            /* 2d. Vote, and if we have enough samples, publish result */
            Vote_Push(&vote, result.label);
            if (Vote_Ready(&vote))
            {
                result.label = Vote_Decide(&vote);

                /* Non-blocking send — drop if Thread 3 is behind */
                if (xQueueSend(g_result_queue, &result, 0) != pdTRUE)
                {
                    g_t2_queue_drops++;
                }
            }

            /* 2e. Slide window forward by stride */
            RingBuffer_Advance(WINDOW_STRIDE);
        }
    }
}

/* ===== FreeRTOS Hooks (mandatory for static allocation) ================= */

void vApplicationGetIdleTaskMemory(StaticTask_t **tcb,
                                    StackType_t **stack,
                                    uint32_t     *size)
{
    static StaticTask_t idle_tcb;
    static StackType_t  idle_stack[configMINIMAL_STACK_SIZE];
    *tcb   = &idle_tcb;
    *stack = idle_stack;
    *size  = configMINIMAL_STACK_SIZE;
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    for (;;) { /* halt — debugger inspection */ }
}

/* Idle hook — periodically refreshes runtime/stack diagnostics.
 * (Future: also feeds IWDG once Thread 3 is added — Milestone 6.) */
void vApplicationIdleHook(void)
{
    static uint32_t count = 0U;
    count++;

    if ((count % 500000U) == 0U)
    {
        TaskStatus_t xStatus;
        vTaskGetInfo(s_thread1_handle, &xStatus, pdTRUE, eInvalid);

        g_sensor_cpu_cycles      = xStatus.ulRunTimeCounter;
        g_sensor_stack_remaining = xStatus.usStackHighWaterMark;
        g_total_runtime          = ulGetRunTimeCounterValue();

        if (g_total_runtime > 0U)
        {
            g_sensor_cpu_percent_x100 =
                (g_sensor_cpu_cycles * 10000U) / g_total_runtime;
        }

        g_sensor_stack_used    = THREAD1_STACK_WORDS - g_sensor_stack_remaining;
        g_sensor_stack_percent =
            (g_sensor_stack_remaining * 100U) / THREAD1_STACK_WORDS;
        g_uptime_ms = g_total_runtime / (84000000U / 1000U);
    }
}

/* Runtime stats — backed by DWT cycle counter */
void vConfigureTimerForRunTimeStats(void)
{
    DWT_Init();
}

uint32_t ulGetRunTimeCounterValue(void)
{
    return DWT_GetCycles();
}

/* ===== Hardware Initialisation ========================================== */

static void config_gpio_i2c1(void)
{
    /* PB6 = SCL, PB7 = SDA — AF4, open-drain, pull-up */
    GPIO_CONFIG_t scl = {
        .Pin = GPIO_PIN6, .Port = GPIO_PORTB,
        .Mode = GPIO_MODE_ALTERNATE, .Type = GPIO_OUTPUT_OPEN_DRAIN,
        .Speed = GPIO_SPEED_HIGH, .Pull = GPIO_PULL_UP,
        .Alternate = AF_I2C_1_3
    };
    GPIO_INIT(&scl);

    GPIO_CONFIG_t sda = {
        .Pin = GPIO_PIN7, .Port = GPIO_PORTB,
        .Mode = GPIO_MODE_ALTERNATE, .Type = GPIO_OUTPUT_OPEN_DRAIN,
        .Speed = GPIO_SPEED_HIGH, .Pull = GPIO_PULL_UP,
        .Alternate = AF_I2C_1_3
    };
    GPIO_INIT(&sda);
}

static void config_led_pc13(void)
{
    GPIO_CONFIG_t led = {
        .Pin = GPIO_PIN13, .Port = GPIO_PORTC,
        .Mode = GPIO_MODE_OUTPUT, .Type = GPIO_OUTPUT_PUSH_PULL,
        .Speed = GPIO_SPEED_LOW, .Pull = GPIO_NO_PULL,
        .Alternate = AF_SYSTEM
    };
    GPIO_INIT(&led);
}

static void app_i2c_init(void)
{
    I2C_Config_t i2c_cfg = {
        .id                = I2C_ID_1,
        .speed             = I2C_SPEED_FAST,
        .duty_cycle        = I2C_DUTY_2,
        .addressing_mode   = I2C_ADDR_7BIT,
        .own_address1      = 0x00U,
        .own_address2      = 0x00U,
        .dual_address_mode = I2C_DUAL_ADDR_DISABLE,
        .general_call_mode = I2C_GENERAL_CALL_DISABLE,
        .clock_stretching  = I2C_CLOCK_STRETCH_ENABLE,
        .analog_filter     = I2C_ANALOG_FILTER_ENABLE,
        .digital_filter    = I2C_DIGITALFILTER_0,
        .transfer_mode     = I2C_MODE_DMA
    };

    if (I2C_Init(&i2c_cfg) != I2C_OK) { while (1); }

    (void)I2C_SVC_Init(
        I2C_ID_1,
        6U,            /* I2C event IRQ priority */
        6U,            /* I2C error IRQ priority */
        DMA_1, DMA_STREAM_0, DMA_CHANNEL_1,
        6U             /* DMA IRQ priority */
    );
}

/* ===== main ============================================================== */
/*
 * Initialisation order (CRITICAL — see handoff doc Bug 1):
 *   PHASE 1: All hardware init (no FreeRTOS API allowed)
 *   PHASE 2: Create FreeRTOS objects (semaphores, queues, tasks)
 *   PHASE 3: vTaskStartScheduler()
 *
 * Calling any FreeRTOS API before scheduler start can leak BASEPRI to
 * a non-zero value (uxCriticalNesting sentinel issue), causing all
 * subsequent IRQs at priority ≥ configMAX_SYSCALL_INTERRUPT_PRIORITY
 * to be silently masked.
 */
int main(void)
{
    /* ----- PHASE 1: Hardware ----- */
    RCC_INIT_84MHz_HSI();

    RCC_EN_CLK_PERIPHERAL(PERIPH_GPIOB);
    RCC_EN_CLK_PERIPHERAL(PERIPH_GPIOC);
    RCC_EN_CLK_PERIPHERAL(PERIPH_I2C1);
    RCC_EN_CLK_PERIPHERAL(PERIPH_DMA1);

    config_gpio_i2c1();
    app_i2c_init();
    config_led_pc13();

    MPU6050_Init(I2C_ID_1, (I2C_DevAddr7_t)MPU6050_ADDR_LOW, 5000000UL);
    RingBuffer_Init();

    TIM_Config_t tim_cfg = {
        .id        = TIM_ID_2,
        .prescaler = TIM_PSC_100HZ,
        .period    = TIM_ARR_100HZ,
        .mode      = TIM_MODE_UP,
        .arpe      = TIM_ARPE_ENABLE,
        .callback  = on_tim2_update,
        .ctx       = NULL
    };
    TIM_Init(&tim_cfg);

    NVIC_SetPriority(TIM2, 5U);
    NVIC_EnableIRQ(TIM2);

    /* ----- PHASE 2: FreeRTOS Objects ----- */
    s_tim_sem = xSemaphoreCreateBinaryStatic(&s_tim_sem_storage);
    s_dma_sem = xSemaphoreCreateBinaryStatic(&s_dma_sem_storage);

    g_result_queue = xQueueCreateStatic(
        RESULT_QUEUE_DEPTH,
        sizeof(Inference_Result_t),
        s_result_queue_buffer,
        &s_result_queue_storage
    );
    configASSERT(g_result_queue != NULL);

    /* Create Thread 2 BEFORE Thread 1 — the latter notifies the former,
     * so the handle must exist before the producer can run. */
    s_thread2_handle = xTaskCreateStatic(
        Thread2_TinyML, "ML",
        THREAD2_STACK_WORDS, NULL, THREAD2_PRIORITY,
        s_thread2_stack, &s_thread2_tcb
    );
    configASSERT(s_thread2_handle != NULL);

    s_thread1_handle = xTaskCreateStatic(
        Thread1_Sensor, "Sensor",
        THREAD1_STACK_WORDS, NULL, THREAD1_PRIORITY,
        s_thread1_stack, &s_thread1_tcb
    );
    configASSERT(s_thread1_handle != NULL);

    /* Start TIM2 — must happen AFTER tasks exist (TIM ISR signals s_tim_sem,
     * Thread 1 must be ready to consume notifications). */
    TIM_Start(TIM_ID_2);

    /* ----- PHASE 3: Scheduler ----- */
    vTaskStartScheduler();

    /* Should never reach here */
    for (;;) {}
}