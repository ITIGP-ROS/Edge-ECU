/*****************************************************************************
 * main.c — Road Surface Classifier (TinyML on STM32F401CC + FreeRTOS)
 *
 * Pipeline:
 *   TIM2 (100 Hz) → Thread 1 (Sensor) → RingBuffer
 *                                          ↓
 *                                    Thread 2 (TinyML)
 *                                          ↓
 *                              Frame Request Queue → Thread 3 (UART TX)
 *
 *   Thread 4 (Heartbeat) — sends runtime stats every 1 s via same queue
 *
 * Build modes:
 *   #define REPLAY_MODE 1   → Thread 1 reads from replay_data.h instead
 *                              of the live IMU. Used for offline pipeline
 *                              validation against known-good CSV recordings.
 *   #define REPLAY_MODE 0   → Live mode (default). Thread 1 reads MPU6050.
 *****************************************************************************/

/* ===== Build Configuration ============================================== */
#define REPLAY_MODE   1   /* 1 = inject CSV, 0 = live IMU */

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

#include "FRAME.h"
#include "UART_INTERFACE.h"
#include "UART_SERVICE.h"
#include "frame_request.h"
#include "heartbeat.h"
#include "IWDG_INTERFACE.h"
#include "logger.h"

#if REPLAY_MODE
  #include "replay_data.h"
#endif

/* ===== Configuration Constants ========================================== */
#define WINDOW_STRIDE         25U   /* 50% overlap → 4 Hz inference rate */
#define THREAD1_STACK_WORDS   96U
#define THREAD2_STACK_WORDS   1280U
#define THREAD3_STACK_WORDS   128U
#define THREAD4_STACK_WORDS   256U
#define THREAD1_PRIORITY      3U
#define THREAD2_PRIORITY      2U
#define FRAME_QUEUE_DEPTH     6U   /* (4 classifications) + (1 heartbeat) + 1 headroom */
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

/* Frame request queue — holds unified FrameRequest_t items */
static StaticQueue_t     s_frame_queue_storage;
static uint8_t           s_frame_queue_buffer[FRAME_QUEUE_DEPTH *
                                              sizeof(FrameRequest_t)];
QueueHandle_t            g_frame_queue;

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

/* Thread 3 storage */
static StackType_t  s_thread3_stack[256];
static StaticTask_t s_thread3_tcb;
static TaskHandle_t s_thread3_handle;

/* Thread 4 (Heartbeat) storage */
static StackType_t  s_thread4_stack[256];
static StaticTask_t s_thread4_tcb;
static TaskHandle_t s_thread4_handle;

/* Frame TX buffer — sized for max possible frame */
static uint8_t  s_tx_frame[FRAME_OVERHEAD_BYTES + FRAME_MAX_PAYLOAD];

/* ===== Shared Heartbeat Stats =========================================== */
static volatile Heartbeat_Payload_t g_stats;   /* accessed by Thread 2 & 4 */


/* ===== ISR Callbacks ==================================================== */

/* TIM2 100 Hz tick — wakes Thread 1 */
static void on_tim2_update(void *ctx){
    (void)ctx;
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_tim_sem, &woken);
    portYIELD_FROM_ISR(woken);
}

/* MPU6050 DMA-read complete — copies sample, signals Thread 1 */
static void on_mpu_read_done(const MPU6050_RawData_t *data, void *ctx){
    (void)ctx;
    s_latest_sample = *data;

    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_dma_sem, &woken);
    portYIELD_FROM_ISR(woken);
}

static void config_gpio_pa8_sync(void){
    GPIO_CONFIG_t sync = {
        .Pin = GPIO_PIN8, .Port = GPIO_PORTA,
        .Mode = GPIO_MODE_OUTPUT, .Type = GPIO_OUTPUT_PUSH_PULL,
        .Speed = GPIO_SPEED_VERY_HIGH, .Pull = GPIO_NO_PULL,
        .Alternate = AF_SYSTEM
    };
    GPIO_INIT(&sync);
    GPIO_WritePin(GPIO_PORTA, GPIO_PIN8, GPIO_PIN_RESET);   /* default LOW */
}

static void config_gpio_uart1(void){
    /* PA9 TX, PA10 RX (PA10 not used yet but configure for future BL) */
    GPIO_CONFIG_t tx = {
        .Pin = GPIO_PIN9, .Port = GPIO_PORTA,
        .Mode = GPIO_MODE_ALTERNATE, .Type = GPIO_OUTPUT_PUSH_PULL,
        .Speed = GPIO_SPEED_VERY_HIGH, .Pull = GPIO_NO_PULL,
        .Alternate = AF_USART_1_2
    };
    GPIO_INIT(&tx);
}

static void app_uart_init(void){
    UART_Config_t uart_cfg = {
        .uart_id      = UART1_ID,
        .baudrate     = 115200U,
        .parity       = UART_PARITY_NONE,
        .stop_bits    = UART_STOP_1,
        .data_bits    = UART_DATA_8BIT,
        .oversampling = UART_OVER8_DISABLE,
        .tx_mode      = UART_MODE_DMA,
        .rx_mode      = UART_MODE_IRQ
    };
    UART_Init(&uart_cfg);

    UART_SVC_Init(UART1_ID, 6U, UART_SVC_TX_MODE_DMA, DMA_2, DMA_STREAM_7, DMA_CHANNEL_4);
}

static void on_uart_tx_done(void *ctx)
{
    (void)ctx;
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(s_thread3_handle, &woken);
    portYIELD_FROM_ISR(woken);
}


/* ===== Thread 1 — Sensor Producer ======================================= */
static void Thread1_Sensor(void *arg)
{
    (void)arg;

#if REPLAY_MODE
    uint32_t idx = 0U;

    for (;;)
    {
        xSemaphoreTake(s_tim_sem, portMAX_DELAY);

        if (idx >= REPLAY_NUM_SAMPLES)
        {
            idx = 0U;
            g_replay_done = 1U;
        }

        MPU6050_RawData_t sample = replay_samples[idx];

        if (RingBuffer_Push(&sample) == RING_BUFFER_OK)
        {
            xTaskNotifyGive(s_thread2_handle);
            g_replay_samples_pushed++;
        }
        idx++;

        GPIO_TogglePin(GPIO_PORTC, GPIO_PIN13);
        IWDG_Thread_SetAlive(&IWDG_Thread1_Alive);
    }
#else
    MPU6050_RawData_t snapshot;

    for (;;){
        xSemaphoreTake(s_tim_sem, portMAX_DELAY);

        if (MPU6050_TriggerRead(on_mpu_read_done, NULL) == MPU6050_OK){
            if (xSemaphoreTake(s_dma_sem, pdMS_TO_TICKS(DMA_TIMEOUT_MS)) == pdTRUE){
                taskENTER_CRITICAL();
                snapshot = (MPU6050_RawData_t)s_latest_sample;
                taskEXIT_CRITICAL();

                if (RingBuffer_Push(&snapshot) == RING_BUFFER_OK){
                    xTaskNotifyGive(s_thread2_handle);
                }
                else{
                    LOG_ERROR(LOG_CODE_RING_BUFFER_DROP, 1U);
                }

                IWDG_Thread_SetAlive(&IWDG_Thread1_Alive);
                GPIO_TogglePin(GPIO_PORTC, GPIO_PIN13);
            }
            else
            {
                LOG_WARN(LOG_CODE_MPU6050_TIMEOUT, 0U);
            }
        }
        else
        {
            LOG_ERROR(LOG_CODE_MPU6050_TIMEOUT, 1U);
        }
    }
#endif
}

/* ===== Thread 2 — TinyML Inference ====================================== */
static void Thread2_TinyML(void *arg)
{
    (void)arg;

    MPU6050_RawData_t window[WINDOW_SIZE];

    static int16_t   flat_window[WINDOW_SIZE][N_FEATURES];
    static float32_t scaled     [WINDOW_SIZE][N_FEATURES];
    static float32_t features   [N_STAT_FEATURES];
    static int8_t    ts_q       [WINDOW_SIZE * N_FEATURES];
    static int8_t    stat_q     [N_STAT_FEATURES];

    Vote_t vote;
    Vote_Init(&vote);
    Features_Init();
    Inference_Init();

    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        g_t2_wake_count++;

        while (RingBuffer_Count() >= WINDOW_SIZE)
        {
            if (RingBuffer_PeekWindow(window, WINDOW_SIZE) != RING_BUFFER_OK)
            {
                LOG_ERROR(LOG_CODE_RING_BUFFER_DROP, 2U);
                break;
            }

            for (uint32_t t = 0U; t < (uint32_t)WINDOW_SIZE; ++t)
            {
                flat_window[t][0] = window[t].accel_x;
                flat_window[t][1] = window[t].accel_y;
                flat_window[t][2] = window[t].accel_z;
                flat_window[t][3] = window[t].gyro_x;
                flat_window[t][4] = window[t].gyro_y;
                flat_window[t][5] = window[t].gyro_z;
            }

            Scale_RawWindow(flat_window, scaled);
            Features_Extract(scaled, features);
            Quantize_TS(scaled, ts_q);
            Quantize_NormalizeStat(features);
            Quantize_Stat(features, stat_q);

            Inference_Result_t result;

            /* WCET measurement — update global max */
            uint32_t t0 = DWT_GetCycles();
            Inference_Run(ts_q, stat_q, &result);
            uint32_t us = (DWT_GetCycles() - t0) / 84U;   /* 84 MHz clock */
            if (us > g_stats.inf_wcet_us) {
                g_stats.inf_wcet_us = (uint16_t)us;
            }

            g_t2_inference_count++;

#if REPLAY_MODE
            g_replay_last_label = (uint32_t)result.label;
            g_replay_last_conf  = (uint32_t)result.confidence;
            if (result.label == INFERENCE_LABEL_SMOOTH) {
                g_replay_vote_smooth++;
            } else {
                g_replay_vote_rough++;
            }
#endif

            Vote_Push(&vote, result.label);
            if (Vote_Ready(&vote))
            {
                result.label = Vote_Decide(&vote);

                FrameRequest_t req;
                req.type     = FRAME_TYPE_CLASSIFICATION;
                req.ecu_id   = ECU_ID_STM32_NODE1;
                req.length   = 6U;
                uint32_t ts = (uint32_t)xTaskGetTickCount();
                req.payload[0] = result.label;
                req.payload[1] = result.confidence;
                req.payload[2] = (uint8_t)(ts >> 24);
                req.payload[3] = (uint8_t)(ts >> 16);
                req.payload[4] = (uint8_t)(ts >>  8);
                req.payload[5] = (uint8_t)(ts);

                if (xQueueSend(g_frame_queue, &req, 0) != pdTRUE)
                {
                    g_t2_queue_drops++;
                    LOG_ERROR(LOG_CODE_QUEUE_FULL, g_t2_queue_drops);
                }
            }

            RingBuffer_Advance(WINDOW_STRIDE);
        }
        IWDG_Thread_SetAlive(&IWDG_Thread2_Alive);
    }
}

/* ===== Thread 3 — UART TX =============================================== */
static void Thread3_UartTx(void *arg)
{
    (void)arg;
    FrameRequest_t req;
    uint16_t frame_len;
    Buffer_t buf;

    for (;;)
    {
        if (xQueueReceive(g_frame_queue, &req, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (Frame_Build(req.type, req.ecu_id, req.payload, req.length,
                        s_tx_frame, sizeof(s_tx_frame), &frame_len) != FRAME_OK) {
            LOG_ERROR(LOG_CODE_FRAME_BUILD_FAIL, req.type);
            continue;
        }

        buf.data   = s_tx_frame;
        buf.size   = sizeof(s_tx_frame);
        buf.length = frame_len;
        buf.index  = 0U;
        UART_SVC_TransmitDMA(UART1_ID, &buf);

        GPIO_WritePin(GPIO_PORTA, GPIO_PIN8, GPIO_PIN_SET);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        GPIO_WritePin(GPIO_PORTA, GPIO_PIN8, GPIO_PIN_RESET);
        IWDG_Thread_SetAlive(&IWDG_Thread3_Alive);
    }
}

/* ===== Thread 4 — Heartbeat ============================================= */
static void Thread4_Heartbeat(void *arg)
{
    (void)arg;
    FrameRequest_t req;

    static uint32_t prev_t1_cycles    = 0U;
    static uint32_t prev_t2_cycles    = 0U;
    static uint32_t prev_t3_cycles    = 0U;
    static uint32_t prev_idle_cycles  = 0U;
    static uint32_t prev_total        = 0U;
    static uint8_t  first_run         = 1U;
    static TaskHandle_t s_idle_handle = NULL;

    /* 5-second averaging window */
#define HB_SEND_INTERVAL  5U
    static uint8_t  tick_count    = 0U;
    static uint32_t start_tick = 0U; // record start tick for uptime
    /* Initialize start_tick once at task start for uptime calculation */
    start_tick = xTaskGetTickCount();

    TickType_t prev_wake = xTaskGetTickCount();

    // Obtain idle task handle if not already set (does not affect start_tick)
    if (s_idle_handle == NULL) {
        s_idle_handle = xTaskGetIdleTaskHandle();
    }

    static uint32_t acc_t1_x100   = 0U;
    static uint32_t acc_t2_x100   = 0U;
    static uint32_t acc_t3_x100   = 0U;
    static uint32_t acc_idle_x100 = 0U;
    static uint16_t peak_wcet_us  = 0U;   /* max over window */
    static uint16_t peak_rb_fill  = 0U;   /* max over window */

    for (;;)
    {
        vTaskDelayUntil(&prev_wake, pdMS_TO_TICKS(1000));

        IWDG_Thread_SetAlive(&IWDG_Thread4_Alive);

        TaskStatus_t info;
        vTaskGetInfo(s_thread1_handle, &info, pdFALSE, eInvalid);
        uint32_t t1_now = info.ulRunTimeCounter;

        vTaskGetInfo(s_thread2_handle, &info, pdFALSE, eInvalid);
        uint32_t t2_now = info.ulRunTimeCounter;

        vTaskGetInfo(s_thread3_handle, &info, pdFALSE, eInvalid);
        uint32_t t3_now = info.ulRunTimeCounter;

        vTaskGetInfo(s_idle_handle, &info, pdFALSE, eInvalid);
        uint32_t idle_now = info.ulRunTimeCounter;

        uint32_t total_now = ulGetRunTimeCounterValue();

        if (first_run) {
            prev_t1_cycles   = t1_now;
            prev_t2_cycles   = t2_now;
            prev_t3_cycles   = t3_now;
            prev_idle_cycles = idle_now;
            prev_total       = total_now;
            first_run        = 0U;

            g_stats.cpu_t1_x100   = 0U;
            g_stats.cpu_t2_x100   = 0U;
            g_stats.cpu_t3_x100   = 0U;
            g_stats.cpu_idle_x100 = 0U;
        }
        else {
            /* delta math — 1-second slice */
            uint32_t dt_t1    = t1_now    - prev_t1_cycles;
            uint32_t dt_t2    = t2_now    - prev_t2_cycles;
            uint32_t dt_t3    = t3_now    - prev_t3_cycles;
            uint32_t dt_idle  = idle_now  - prev_idle_cycles;
            uint32_t dt_total = total_now - prev_total;

            if (dt_total > 0U) {
                /* x100 → 12.34% = 1234. Use uint64 to avoid overflow */
                g_stats.cpu_t1_x100   = (uint16_t)(((uint64_t)dt_t1   * 10000ULL) / dt_total);
                g_stats.cpu_t2_x100   = (uint16_t)(((uint64_t)dt_t2   * 10000ULL) / dt_total);
                g_stats.cpu_t3_x100   = (uint16_t)(((uint64_t)dt_t3   * 10000ULL) / dt_total);
                g_stats.cpu_idle_x100 = (uint16_t)(((uint64_t)dt_idle * 10000ULL) / dt_total);
                // Clamp values to 100.00% (10000) to avoid overflow artifacts
                if (g_stats.cpu_t1_x100 > 10000U) g_stats.cpu_t1_x100 = 10000U;
                if (g_stats.cpu_t2_x100 > 10000U) g_stats.cpu_t2_x100 = 10000U;
                if (g_stats.cpu_t3_x100 > 10000U) g_stats.cpu_t3_x100 = 10000U;
                if (g_stats.cpu_idle_x100 > 10000U) g_stats.cpu_idle_x100 = 10000U;
            }

            prev_t1_cycles   = t1_now;
            prev_t2_cycles   = t2_now;
            prev_t3_cycles   = t3_now;
            prev_idle_cycles = idle_now;
            prev_total       = total_now;

            /* Accumulate for 5-second average */
            acc_t1_x100   += g_stats.cpu_t1_x100;
            acc_t2_x100   += g_stats.cpu_t2_x100;
            acc_t3_x100   += g_stats.cpu_t3_x100;
            acc_idle_x100 += g_stats.cpu_idle_x100;
            if (g_stats.inf_wcet_us > peak_wcet_us) { peak_wcet_us = g_stats.inf_wcet_us; }
            if (g_stats.rb_max_fill > peak_rb_fill)  { peak_rb_fill  = g_stats.rb_max_fill;  }
            tick_count++;
        }

        /* gather other stats */
        g_stats.uptime_ms = (uint32_t)((xTaskGetTickCount() - start_tick) * portTICK_PERIOD_MS);
        g_stats.stack_t1_free = (uint16_t)uxTaskGetStackHighWaterMark(s_thread1_handle);
        g_stats.stack_t2_free = (uint16_t)uxTaskGetStackHighWaterMark(s_thread2_handle);
        g_stats.stack_t3_free = (uint16_t)uxTaskGetStackHighWaterMark(s_thread3_handle);
        g_stats.rb_max_fill   = (uint16_t)RingBuffer_GetMaxFill();
        g_stats.reserved      = 0U;

        /* ---- Send heartbeat frame only every HB_SEND_INTERVAL seconds --- */
        if (tick_count >= HB_SEND_INTERVAL)
        {
            /* Compute averages over the window */
            uint16_t avg_t1_x100   = (uint16_t)(acc_t1_x100   / HB_SEND_INTERVAL);
            uint16_t avg_t2_x100   = (uint16_t)(acc_t2_x100   / HB_SEND_INTERVAL);
            uint16_t avg_t3_x100   = (uint16_t)(acc_t3_x100   / HB_SEND_INTERVAL);
            uint16_t avg_idle_x100 = (uint16_t)(acc_idle_x100 / HB_SEND_INTERVAL);

            /* pack — uptime BE (snapshot at send time) */
            req.payload[0]  = (uint8_t)(g_stats.uptime_ms >> 24);
            req.payload[1]  = (uint8_t)(g_stats.uptime_ms >> 16);
            req.payload[2]  = (uint8_t)(g_stats.uptime_ms >>  8);
            req.payload[3]  = (uint8_t)(g_stats.uptime_ms);

            /* CPU 5-second averages BE */
            req.payload[4]  = (uint8_t)(avg_t1_x100   >> 8);
            req.payload[5]  = (uint8_t)(avg_t1_x100);
            req.payload[6]  = (uint8_t)(avg_t2_x100   >> 8);
            req.payload[7]  = (uint8_t)(avg_t2_x100);
            req.payload[8]  = (uint8_t)(avg_t3_x100   >> 8);
            req.payload[9]  = (uint8_t)(avg_t3_x100);
            req.payload[10] = (uint8_t)(avg_idle_x100 >> 8);
            req.payload[11] = (uint8_t)(avg_idle_x100);

            /* stacks BE */
            req.payload[12] = (uint8_t)(g_stats.stack_t1_free >> 8);
            req.payload[13] = (uint8_t)(g_stats.stack_t1_free);
            req.payload[14] = (uint8_t)(g_stats.stack_t2_free >> 8);
            req.payload[15] = (uint8_t)(g_stats.stack_t2_free);
            req.payload[16] = (uint8_t)(g_stats.stack_t3_free >> 8);
            req.payload[17] = (uint8_t)(g_stats.stack_t3_free);

            /* Peak WCET + RB over the 5-second window, BE */
            req.payload[18] = (uint8_t)(peak_wcet_us >> 8);
            req.payload[19] = (uint8_t)(peak_wcet_us);
            req.payload[20] = (uint8_t)(peak_rb_fill  >> 8);
            req.payload[21] = (uint8_t)(peak_rb_fill);
            req.payload[22] = 0U;
            req.payload[23] = 0U;

            req.type   = FRAME_TYPE_HEARTBEAT;
            req.ecu_id = ECU_ID_STM32_NODE1;
            req.length = HEARTBEAT_PAYLOAD_SIZE;

            if (xQueueSend(g_frame_queue, &req, 0) != pdTRUE)
            {
                LOG_WARN(LOG_CODE_QUEUE_FULL, 1U);
            }

            /* Reset accumulators for next window */
            acc_t1_x100   = 0U;
            acc_t2_x100   = 0U;
            acc_t3_x100   = 0U;
            acc_idle_x100 = 0U;
            peak_wcet_us  = 0U;
            peak_rb_fill  = 0U;
            tick_count    = 0U;
        }

        IWDG_Thread_SetAlive(&IWDG_Thread4_Alive);

        GPIO_TogglePin(GPIO_PORTC, GPIO_PIN13);
    }
}


/* ===== FreeRTOS Hooks =================================================== */
void vApplicationGetIdleTaskMemory(StaticTask_t **tcb, StackType_t **stack, uint32_t *size){
    static StaticTask_t idle_tcb;
    static StackType_t  idle_stack[configMINIMAL_STACK_SIZE];
    *tcb   = &idle_tcb;
    *stack = idle_stack;
    *size  = configMINIMAL_STACK_SIZE;
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name){
    (void)task;
    (void)name;
    for (;;) {}
}

void vApplicationIdleHook(void){
    IWDG_SupervisorFeed();
}

void vConfigureTimerForRunTimeStats(void){
    DWT_Init();
}

uint32_t ulGetRunTimeCounterValue(void){
    return DWT_GetCycles();
}

/* ===== Hardware Initialisation ========================================== */
static void config_gpio_i2c1(void){
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

static void config_led_pc13(void){
    GPIO_CONFIG_t led = {
        .Pin = GPIO_PIN13, .Port = GPIO_PORTC,
        .Mode = GPIO_MODE_OUTPUT, .Type = GPIO_OUTPUT_PUSH_PULL,
        .Speed = GPIO_SPEED_LOW, .Pull = GPIO_NO_PULL,
        .Alternate = AF_SYSTEM
    };
    GPIO_INIT(&led);
}

static void app_i2c_init(void){
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
int main(void){
    /* --- Relocate vector table to Sector 2 (required when app runs from 0x08008000) --- */
    /* SCB->VTOR is at 0xE000ED08. Write the app base address so all interrupts   */
    /* are dispatched through our vector table, not the bootloader's at 0x08000000 */
    *((volatile uint32_t *)0xE000ED08U) = 0x08008000U;

    RCC_INIT_84MHz_HSI();

    RCC_EN_CLK_PERIPHERAL(PERIPH_GPIOB);
    RCC_EN_CLK_PERIPHERAL(PERIPH_GPIOC);
    RCC_EN_CLK_PERIPHERAL(PERIPH_I2C1);
    RCC_EN_CLK_PERIPHERAL(PERIPH_DMA1);
    RCC_EN_CLK_PERIPHERAL(PERIPH_GPIOA);
    RCC_EN_CLK_PERIPHERAL(PERIPH_USART1);
    RCC_EN_CLK_PERIPHERAL(PERIPH_DMA2);
    RCC_LSI_Enable();

    config_gpio_i2c1();
    config_gpio_pa8_sync();
    config_gpio_uart1();

    app_i2c_init();
    app_uart_init();
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

    /* FreeRTOS objects */
    s_tim_sem = xSemaphoreCreateBinaryStatic(&s_tim_sem_storage);
    s_dma_sem = xSemaphoreCreateBinaryStatic(&s_dma_sem_storage);

    g_frame_queue = xQueueCreateStatic(
        FRAME_QUEUE_DEPTH,
        sizeof(FrameRequest_t),
        s_frame_queue_buffer,
        &s_frame_queue_storage
    );
    configASSERT(g_frame_queue != NULL);

    /* Logger needs g_frame_queue — init here */
    if(Logger_Init() != 0U){
        for (;;) {}   /* halt — logger creation failed */
    }
    LOG_INFO(LOG_CODE_BOOT, 0U);

    s_thread2_handle = xTaskCreateStatic(
        Thread2_TinyML, "ML",
        THREAD2_STACK_WORDS, NULL, THREAD2_PRIORITY,
        s_thread2_stack, &s_thread2_tcb
    );
    configASSERT(s_thread2_handle != NULL);

    s_thread3_handle = xTaskCreateStatic(
        Thread3_UartTx, "TX",
        THREAD3_STACK_WORDS, NULL, 1,
        s_thread3_stack, &s_thread3_tcb);
    configASSERT(s_thread3_handle != NULL);

    UART_SVC_RegisterTxDoneCb(UART1_ID, on_uart_tx_done, NULL);

    /* Heartbeat task */
    s_thread4_handle = xTaskCreateStatic(
        Thread4_Heartbeat, "HB",
        THREAD4_STACK_WORDS, NULL, 1,
        s_thread4_stack, &s_thread4_tcb);
    configASSERT(s_thread4_handle != NULL);

    s_thread1_handle = xTaskCreateStatic(
        Thread1_Sensor, "Sensor",
        THREAD1_STACK_WORDS, NULL, THREAD1_PRIORITY,
        s_thread1_stack, &s_thread1_tcb
    );
    configASSERT(s_thread1_handle != NULL);

    // Initialize the Independent Watchdog (IWDG) with a timeout of 3000 ms
    if(IWDG_Init(3000U) != IWDG_OK){
        for (;;) {}
    }

    IWDG_Thread1_Alive = 1U;
    IWDG_Thread2_Alive = 1U;
    IWDG_Thread3_Alive = 1U;
    IWDG_Thread4_Alive = 1U;

    IWDG_Start();

    TIM_Start(TIM_ID_2);

    vTaskStartScheduler();

    for (;;) {}
}