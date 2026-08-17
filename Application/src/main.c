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

#include "FRAME.h"
#include "UART_INTERFACE.h"
#include "UART_SERVICE.h"
#include "frame_request.h"
#include "heartbeat.h"
#include "IWDG_INTERFACE.h"
#include "logger.h"
#include "FLASH_INTERFACE.h"
#include "ADC_INTERFACE.h"
#include "HCSR04.h"
#include "TIM_REGS.h"
#undef TIM2
#undef TIM3
#undef TIM4
#undef TIM5

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

/* MPU6050 bring-up result — MPU6050_OK (0) once the IMU answered
 * WHO_AM_I and took its configuration. Non-zero means Thread 1 will
 * produce no samples; readable in the debugger and logged at boot. */
volatile uint8_t g_mpu_init_status = (uint8_t)MPU6050_ERROR_INIT;

/* Thread 3 storage */
static StackType_t  s_thread3_stack[256];
static StaticTask_t s_thread3_tcb;
static TaskHandle_t s_thread3_handle;

/* Thread 4 (Heartbeat) storage */
static StackType_t  s_thread4_stack[256];
static StaticTask_t s_thread4_tcb;
static TaskHandle_t s_thread4_handle;

/* Thread 5 (Bootloader RX) storage */
#define THREAD5_STACK_WORDS 256U
static StackType_t  s_thread5_stack[THREAD5_STACK_WORDS];
static StaticTask_t s_thread5_tcb;
static TaskHandle_t s_thread5_handle;

/* Thread 6 (Temperature sensor) storage */
#define THREAD6_STACK_WORDS 192U
static StackType_t  s_thread6_stack[THREAD6_STACK_WORDS];
static StaticTask_t s_thread6_tcb;
static TaskHandle_t s_thread6_handle;

#define THREAD7_STACK_WORDS 128U
static StackType_t  s_thread7_stack[THREAD7_STACK_WORDS];
static StaticTask_t s_thread7_tcb;
static TaskHandle_t s_thread7_handle;

/* Ultrasonic queue for passing distances from ISR to Thread7 */
static StaticQueue_t s_ultrasonic_queue_storage;
static uint32_t      s_ultrasonic_queue_buffer[4U]; /* Queue of 4 uint32_t msgs */
static QueueHandle_t g_ultrasonic_queue;

/* Frame TX buffer — sized for max possible frame */
static uint8_t  s_tx_frame[FRAME_OVERHEAD_BYTES + FRAME_MAX_PAYLOAD];

/* ===== Shared Heartbeat Stats =========================================== */
static volatile Heartbeat_Payload_t g_stats;   /* accessed by Thread 2 & 4 */


/* ===== ISR Callbacks ==================================================== */

static volatile uint32_t capture_val1[2] = {0U, 0U};
static volatile uint32_t capture_val2[2] = {0U, 0U};
static volatile uint8_t  capture_state[2] = {0U, 0U}; /* 0 = wait rising, 1 = wait falling */

static void on_hcsr04_capture(void *ctx)
{
    uint32_t id = (uint32_t)ctx; /* 0 for CH1, 1 for CH2 */
    TIM_Channel_t ch = (id == 0U) ? TIM_CH_1 : TIM_CH_2;
    uint32_t val = 0U;
    TIM_IC_GetCapture(TIM_ID_3, ch, &val);

    volatile TIM_REGS_t *tim3_regs = (volatile TIM_REGS_t *)0x40000400UL;
    uint32_t cc_shift = (id == 0U) ? 0U : 4U;

    /* Check if we were waiting for rising edge (CCxP bit [1] is 0) */
    if ((tim3_regs->CCER.ALL & (1U << (cc_shift + 1U))) == 0U)
    {
        capture_val1[id] = val;
        /* Switch polarity to Falling Edge (set CCxP = 1) */
        tim3_regs->CCER.ALL |= (1U << (cc_shift + 1U));
    }
    else
    {
        capture_val2[id] = val;
        /* Switch polarity back to Rising Edge (clear CCxP = 0) */
        tim3_regs->CCER.ALL &= ~(1U << (cc_shift + 1U));

        uint32_t diff;
        if (capture_val2[id] >= capture_val1[id])
        {
            diff = capture_val2[id] - capture_val1[id];
        }
        else
        {
            diff = (0xFFFFU - capture_val1[id]) + capture_val2[id] + 1U;
        }

        uint32_t dist_cm = diff / 58U;
        uint32_t msg = (id << 16U) | (dist_cm & 0xFFFFU);

        BaseType_t woken = pdFALSE;
        xQueueSendFromISR(g_ultrasonic_queue, &msg, &woken);
        portYIELD_FROM_ISR(woken);
    }
}

/* TIM2 100 Hz tick — wakes Thread 1 */
static void on_tim2_update(void *ctx){
    (void)ctx;
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_tim_sem, &woken);
    portYIELD_FROM_ISR(woken);
}

/* Set by on_mpu_read_done when the burst failed; cleared by Thread 1. */
static volatile uint8_t s_mpu_read_failed = 0U;

/* MPU6050 DMA-read complete — copies sample, signals Thread 1.
 *
 * NOTE: mpu_dma_done() invokes this with data == NULL when the I2C
 * transfer errored. Dereferencing it would read address 0x00000000 —
 * which on this part aliases to the bootloader's vector table — and
 * push those bytes into the ring buffer as a perfectly valid-looking
 * IMU sample. Flag the failure instead and let Thread 1 drop it. */
static void on_mpu_read_done(const MPU6050_RawData_t *data, void *ctx){
    (void)ctx;

    if (data != NULL){
        s_latest_sample   = *data;
        s_mpu_read_failed = 0U;
    }
    else{
        s_mpu_read_failed = 1U;
    }

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
    /* PA9  = UART1 TX */
    GPIO_CONFIG_t tx = {
        .Pin = GPIO_PIN9, .Port = GPIO_PORTA,
        .Mode = GPIO_MODE_ALTERNATE, .Type = GPIO_OUTPUT_PUSH_PULL,
        .Speed = GPIO_SPEED_VERY_HIGH, .Pull = GPIO_NO_PULL,
        .Alternate = AF_USART_1_2
    };
    GPIO_INIT(&tx);

    /* PA10 = UART1 RX — must be in AF mode so USART1 peripheral drives it */
    GPIO_CONFIG_t rx = {
        .Pin = GPIO_PIN10, .Port = GPIO_PORTA,
        .Mode = GPIO_MODE_ALTERNATE, .Type = GPIO_OUTPUT_PUSH_PULL,
        .Speed = GPIO_SPEED_VERY_HIGH, .Pull = GPIO_PULL_UP,
        .Alternate = AF_USART_1_2
    };
    GPIO_INIT(&rx);
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

        IWDG_Thread_SetAlive(&IWDG_Thread1_Alive);
    }
#else
    MPU6050_RawData_t snapshot;

    /* Sensor faults repeat at the 100 Hz tick rate. Logging every one
     * would swamp the logger queue and the UART link to the gateway,
     * so emit at most one per LOG_THROTTLE_TICKS (~1 s) and carry the
     * suppressed count in the aux field.                             */
    #define LOG_THROTTLE_TICKS  100U
    uint32_t fault_ticks = 0U;
    uint32_t fault_count = 0U;

    for (;;){
        xSemaphoreTake(s_tim_sem, portMAX_DELAY);

        uint8_t  faulted  = 0U;
        uint8_t  fault_id = 0U;

        if (MPU6050_TriggerRead(on_mpu_read_done, NULL) == MPU6050_OK){
            if (xSemaphoreTake(s_dma_sem, pdMS_TO_TICKS(DMA_TIMEOUT_MS)) == pdTRUE){
                taskENTER_CRITICAL();
                uint8_t read_failed = s_mpu_read_failed;
                snapshot = (MPU6050_RawData_t)s_latest_sample;
                taskEXIT_CRITICAL();

                if (read_failed != 0U){
                    /* I2C errored mid-burst — s_latest_sample still holds the
                     * previous tick's reading. Drop it rather than feed the
                     * model a duplicate sample.                              */
                    faulted  = 1U;
                    fault_id = 2U;
                }
                else if (RingBuffer_Push(&snapshot) == RING_BUFFER_OK){
                    xTaskNotifyGive(s_thread2_handle);
                }
                else{
                    LOG_ERROR(LOG_CODE_RING_BUFFER_DROP, 1U);
                }

            }
            else{
                /* DMA completion never arrived within DMA_TIMEOUT_MS. */
                faulted  = 1U;
                fault_id = 0U;
            }
        }
        else{
            /* Driver not initialised, or still busy from the last tick. */
            faulted  = 1U;
            fault_id = 1U;
        }

        if (faulted != 0U){
            fault_count++;
            if (fault_ticks == 0U){
                LOG_WARN(LOG_CODE_MPU6050_TIMEOUT,
                         ((uint32_t)fault_id << 24) | (fault_count & 0xFFFFFFU));
                fault_ticks = LOG_THROTTLE_TICKS;
                fault_count = 0U;
            }
        }
        if (fault_ticks > 0U){ fault_ticks--; }

        /* Fed unconditionally: the thread is alive and servicing its tick
         * even when the IMU is not answering. Letting the IWDG reset here
         * would boot-loop a board with a wiring fault, leaving no window
         * for Thread 5 to accept a recovery image over UART.            */
        IWDG_Thread_SetAlive(&IWDG_Thread1_Alive);
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
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000U));
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
                
                if (result.label == INFERENCE_LABEL_SMOOTH) {
                    GPIO_WritePin(GPIO_PORTC, GPIO_PIN13, GPIO_PIN_SET); // Turn off LED (Active Low)
                } else {
                    GPIO_WritePin(GPIO_PORTC, GPIO_PIN13, GPIO_PIN_RESET); // Turn on LED (Active Low)
                }

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
        if (xQueueReceive(g_frame_queue, &req, pdMS_TO_TICKS(1000U)) != pdTRUE) {
            IWDG_Thread_SetAlive(&IWDG_Thread3_Alive);
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
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000U));
        GPIO_WritePin(GPIO_PORTA, GPIO_PIN8, GPIO_PIN_RESET);
        IWDG_Thread_SetAlive(&IWDG_Thread3_Alive);
    }
}

/* ===== Thread 6 — LM35 Temperature Sensor ================================
 * Wakes every 2 seconds, reads ADC1 ch1 (PA1), converts to tenths of Celsius,
 * and pushes a FRAME_TYPE_TEMPERATURE request to g_frame_queue.
 *
 * Payload layout (6 bytes, all big-endian):
 *   [0..1]  temperature_x10  uint16 BE  (e.g. 0x00EB = 235 = 23.5 C)
 *   [2..5]  timestamp_ms     uint32 BE
 * ========================================================================= */
static void Thread6_Temperature(void *arg);
static void Thread7_Ultrasonic(void *arg);

static void config_gpio_i2c1(void);

static void Thread6_Temperature(void *arg)
{
    (void)arg;

    TickType_t prev_wake = xTaskGetTickCount();

    for (;;)
    {
        vTaskDelayUntil(&prev_wake, pdMS_TO_TICKS(2000U));

        uint16_t adc_raw = 0U;
        if (ADC_Read(&adc_raw) != ADC_OK)
        {
            LOG_WARN(LOG_CODE_ADC_TIMEOUT, 0U);
            continue;   /* skip this tick on error — next tick will retry */
        }
        else
        {
            LOG_INFO(LOG_CODE_ADC_TIMEOUT, 0U);
        }

        /* Convert to tenths of Celsius (e.g. 235 = 23.5°C) */
        uint16_t temp_x10 = ADC_LM35_ToTenthsCelsius(adc_raw, 3300U);

        /* Timestamp in milliseconds */
        uint32_t ts = (uint32_t)(xTaskGetTickCount() * (uint32_t)portTICK_PERIOD_MS);

        /* Pack and enqueue — same pattern as Thread2 (classification) */
        FrameRequest_t req;
        req.type       = FRAME_TYPE_TEMPERATURE;
        req.ecu_id     = ECU_ID_STM32_NODE1;
        req.length     = 6U;
        req.payload[0] = (uint8_t)(temp_x10 >> 8);     /* temperature BE high byte */
        req.payload[1] = (uint8_t)(temp_x10);          /* temperature BE low byte  */
        req.payload[2] = (uint8_t)(ts >> 24);
        req.payload[3] = (uint8_t)(ts >> 16);
        req.payload[4] = (uint8_t)(ts >>  8);
        req.payload[5] = (uint8_t)(ts);

        if (xQueueSend(g_frame_queue, &req, 0) != pdTRUE)
        {
            LOG_WARN(LOG_CODE_QUEUE_FULL, 2U);
        }
    }
}


/* ===== Thread 7 — Ultrasonic Sensors ====================================
 * Wakes every 250 ms, triggers 2 HC-SR04 sensors, waits for ISR to send
 * the calculated distance via g_ultrasonic_queue, and packs into FRAME.
 * ========================================================================= */
static void Thread7_Ultrasonic(void *arg)
{
    (void)arg;
    FrameRequest_t req;

    HCSR04_Config_t u1 = { .trigger_port = GPIO_PORTA, .trigger_pin = GPIO_PIN4 };
    HCSR04_Config_t u2 = { .trigger_port = GPIO_PORTA, .trigger_pin = GPIO_PIN5 };

    TickType_t prev_wake = xTaskGetTickCount();

    for (;;)
    {
        vTaskDelayUntil(&prev_wake, pdMS_TO_TICKS(250U));

        /* Trigger Sensor 1 */
        xQueueReset(g_ultrasonic_queue);
        HCSR04_Trigger(&u1);
        uint32_t msg1 = 0U;
        uint16_t dist1 = 0xFFFFU;
        if (xQueueReceive(g_ultrasonic_queue, &msg1, pdMS_TO_TICKS(50U)) == pdTRUE)
        {
            if ((msg1 >> 16U) == 0U) {
                dist1 = (uint16_t)(msg1 & 0xFFFFU);
            }
            if (dist1 > 400U) {
                dist1 = 400U; /* Cap at 4 meters (max range) */
            }
            LOG_INFO(LOG_CODE_ULTRASONIC1_TIMEOUT, 1U); /* Hardware is healthy */
        }
        else
        {
            LOG_WARN(LOG_CODE_ULTRASONIC1_TIMEOUT, 1U); /* Hardware freeze/disconnect */
            dist1 = 0xFFFFU;
        }

        /* Trigger Sensor 2 */
        xQueueReset(g_ultrasonic_queue);
        HCSR04_Trigger(&u2);
        uint32_t msg2 = 0U;
        uint16_t dist2 = 0xFFFFU;
        if (xQueueReceive(g_ultrasonic_queue, &msg2, pdMS_TO_TICKS(50U)) == pdTRUE)
        {
            if ((msg2 >> 16U) == 1U) {
                dist2 = (uint16_t)(msg2 & 0xFFFFU);
            }
            if (dist2 > 400U) {
                dist2 = 400U; /* Cap at 4 meters (max range) */
            }
            LOG_INFO(LOG_CODE_ULTRASONIC2_TIMEOUT, 2U); /* Hardware is healthy */
        }
        else
        {
            LOG_WARN(LOG_CODE_ULTRASONIC2_TIMEOUT, 2U); /* Hardware freeze/disconnect */
            dist2 = 0xFFFFU;
        }

        /* Pack and enqueue */
        uint32_t ts = (uint32_t)(xTaskGetTickCount() * (uint32_t)portTICK_PERIOD_MS);
        
        req.type       = FRAME_TYPE_ULTRASONIC;
        req.ecu_id     = ECU_ID_STM32_NODE1;
        req.length     = 8U;
        req.payload[0] = (uint8_t)(dist1 >> 8);
        req.payload[1] = (uint8_t)(dist1);
        req.payload[2] = (uint8_t)(dist2 >> 8);
        req.payload[3] = (uint8_t)(dist2);
        req.payload[4] = (uint8_t)(ts >> 24);
        req.payload[5] = (uint8_t)(ts >> 16);
        req.payload[6] = (uint8_t)(ts >>  8);
        req.payload[7] = (uint8_t)(ts);

        if (xQueueSend(g_frame_queue, &req, 0) != pdTRUE)
        {
            LOG_WARN(LOG_CODE_QUEUE_FULL, 3U);
        }
    }
}


/* ===== Thread 5 — Bootloader Entry Command ============================== */
/* Woken by the UART RX ISR on every received byte (zero polling).          */
/* State machine: waits for 0xAA then 0xEB, then ACKs + erases + resets.   */
static void Thread5_BootloaderRx(void *arg)
{
    (void)arg;

    /* Register this task so the UART ISR wakes us on every received byte */
    UART_SVC_SetRxNotifyTask(UART1_ID, xTaskGetCurrentTaskHandle());

    uint8_t  rx_byte = 0U;
    uint8_t  state   = 0U;   /* 0 = waiting for 0xAA, 1 = waiting for 0xEB */
    Buffer_t rx_buf;
    uint16_t avail = 0U;

    for (;;)
    {
        /* Sleep until ISR gives us a notification */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        /* Drain all bytes currently in the ring buffer */
        while (UART_SVC_RxAvailable(UART1_ID, &avail) == UART_SVC_OK && avail > 0U)
        {
            rx_buf.data   = &rx_byte;
            rx_buf.size   = 1U;
            rx_buf.length = 1U;
            rx_buf.index  = 0U;

            if (UART_SVC_Receive(UART1_ID, &rx_buf) != UART_SVC_OK)
                break;

            if (state == 0U)
            {
                if (rx_byte == 0xAAU)
                    state = 1U;   /* first byte matched — wait for 0xEB */
            }
            else  /* state == 1 */
            {
                if(rx_byte == 0xEBU){
                    /* ---- Full command received — enter bootloader ---- */

                    /* 1. ACK back to ESP32 using UART Polling to bypass DMA conflicts */
                    static uint8_t ack_packet[2] = {0xEEU, 0xAAU};
                    Buffer_t tx_buf;
                    tx_buf.data   = ack_packet;
                    tx_buf.size   = 2U;
                    tx_buf.length = 2U;
                    tx_buf.index  = 0U;
                    (void)UART_Transmit_Polling(UART1_ID, &tx_buf, 10000UL);

                    /* 4. Set the bootloader request flag in reserved RAM */
                    *((volatile uint32_t *)0x2000FFF8UL) = 0xDEADBEEF;

                    /* 5. Software reset — bootloader takes over */
                    *((volatile uint32_t *)0xE000ED0CU) = (0x05FAUL << 16U) | (1UL << 2U);
                    for (;;) {}  /* never reached */
                }
                else{
                    /* Wrong second byte — reset and re-check this byte */
                    state = (rx_byte == 0xAAU) ? 1U : 0U;
                }
            }
        }
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
    static uint32_t prev_t6_cycles    = 0U;
    static uint32_t prev_t7_cycles    = 0U;
    static uint32_t prev_idle_cycles  = 0U;
    static uint32_t prev_total        = 0U;
    static uint8_t  first_run         = 1U;
    static TaskHandle_t s_idle_handle = NULL;

    /* 10-second averaging window */
#define HB_SEND_INTERVAL  10U
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
    static uint32_t acc_t6_x100   = 0U;
    static uint32_t acc_t7_x100   = 0U;
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

        vTaskGetInfo(s_thread6_handle, &info, pdFALSE, eInvalid);
        uint32_t t6_now = info.ulRunTimeCounter;

        vTaskGetInfo(s_thread7_handle, &info, pdFALSE, eInvalid);
        uint32_t t7_now = info.ulRunTimeCounter;

        vTaskGetInfo(s_idle_handle, &info, pdFALSE, eInvalid);
        uint32_t idle_now = info.ulRunTimeCounter;

        uint32_t total_now = ulGetRunTimeCounterValue();

        if (first_run) {
            prev_t1_cycles   = t1_now;
            prev_t2_cycles   = t2_now;
            prev_t3_cycles   = t3_now;
            prev_t6_cycles   = t6_now;
            prev_t7_cycles   = t7_now;
            prev_idle_cycles = idle_now;
            prev_total       = total_now;
            first_run        = 0U;

            g_stats.cpu_t1_x100   = 0U;
            g_stats.cpu_t2_x100   = 0U;
            g_stats.cpu_t3_x100   = 0U;
            g_stats.cpu_t6_x100   = 0U;
            g_stats.cpu_t7_x100   = 0U;
            g_stats.cpu_idle_x100 = 0U;
        }
        else {
            /* delta math — 1-second slice */
            uint32_t dt_t1    = t1_now    - prev_t1_cycles;
            uint32_t dt_t2    = t2_now    - prev_t2_cycles;
            uint32_t dt_t3    = t3_now    - prev_t3_cycles;
            uint32_t dt_t6    = t6_now    - prev_t6_cycles;
            uint32_t dt_t7    = t7_now    - prev_t7_cycles;
            uint32_t dt_idle  = idle_now  - prev_idle_cycles;
            uint32_t dt_total = total_now - prev_total;

            if (dt_total > 0U) {
                /* x100 → 12.34% = 1234. Use uint64 to avoid overflow */
                g_stats.cpu_t1_x100   = (uint16_t)(((uint64_t)dt_t1   * 10000ULL) / dt_total);
                g_stats.cpu_t2_x100   = (uint16_t)(((uint64_t)dt_t2   * 10000ULL) / dt_total);
                g_stats.cpu_t3_x100   = (uint16_t)(((uint64_t)dt_t3   * 10000ULL) / dt_total);
                g_stats.cpu_t6_x100   = (uint16_t)(((uint64_t)dt_t6   * 10000ULL) / dt_total);
                g_stats.cpu_t7_x100   = (uint16_t)(((uint64_t)dt_t7   * 10000ULL) / dt_total);
                g_stats.cpu_idle_x100 = (uint16_t)(((uint64_t)dt_idle * 10000ULL) / dt_total);
                // Clamp values to 100.00% (10000) to avoid overflow artifacts
                if (g_stats.cpu_t1_x100 > 10000U) g_stats.cpu_t1_x100 = 10000U;
                if (g_stats.cpu_t2_x100 > 10000U) g_stats.cpu_t2_x100 = 10000U;
                if (g_stats.cpu_t3_x100 > 10000U) g_stats.cpu_t3_x100 = 10000U;
                if (g_stats.cpu_t6_x100 > 10000U) g_stats.cpu_t6_x100 = 10000U;
                if (g_stats.cpu_t7_x100 > 10000U) g_stats.cpu_t7_x100 = 10000U;
                if (g_stats.cpu_idle_x100 > 10000U) g_stats.cpu_idle_x100 = 10000U;
            }

            prev_t1_cycles   = t1_now;
            prev_t2_cycles   = t2_now;
            prev_t3_cycles   = t3_now;
            prev_t6_cycles   = t6_now;
            prev_t7_cycles   = t7_now;
            prev_idle_cycles = idle_now;
            prev_total       = total_now;

            /* Accumulate for 5-second average */
            acc_t1_x100   += g_stats.cpu_t1_x100;
            acc_t2_x100   += g_stats.cpu_t2_x100;
            acc_t3_x100   += g_stats.cpu_t3_x100;
            acc_t6_x100   += g_stats.cpu_t6_x100;
            acc_t7_x100   += g_stats.cpu_t7_x100;
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
        g_stats.stack_t6_free = (uint16_t)uxTaskGetStackHighWaterMark(s_thread6_handle);
        g_stats.stack_t7_free = (uint16_t)uxTaskGetStackHighWaterMark(s_thread7_handle);
        g_stats.rb_max_fill   = (uint16_t)RingBuffer_GetMaxFill();

        /* ---- Send heartbeat frame only every HB_SEND_INTERVAL seconds --- */
        if (tick_count >= HB_SEND_INTERVAL)
        {
            /* Compute averages over the window */
            uint16_t avg_t1_x100   = (uint16_t)(acc_t1_x100   / HB_SEND_INTERVAL);
            uint16_t avg_t2_x100   = (uint16_t)(acc_t2_x100   / HB_SEND_INTERVAL);
            uint16_t avg_t3_x100   = (uint16_t)(acc_t3_x100   / HB_SEND_INTERVAL);
            uint16_t avg_t6_x100   = (uint16_t)(acc_t6_x100   / HB_SEND_INTERVAL);
            uint16_t avg_t7_x100   = (uint16_t)(acc_t7_x100   / HB_SEND_INTERVAL);
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
            req.payload[10] = (uint8_t)(avg_t6_x100   >> 8);
            req.payload[11] = (uint8_t)(avg_t6_x100);
            req.payload[12] = (uint8_t)(avg_t7_x100   >> 8);
            req.payload[13] = (uint8_t)(avg_t7_x100);
            req.payload[14] = (uint8_t)(avg_idle_x100 >> 8);
            req.payload[15] = (uint8_t)(avg_idle_x100);

            /* stacks BE */
            req.payload[16] = (uint8_t)(g_stats.stack_t1_free >> 8);
            req.payload[17] = (uint8_t)(g_stats.stack_t1_free);
            req.payload[18] = (uint8_t)(g_stats.stack_t2_free >> 8);
            req.payload[19] = (uint8_t)(g_stats.stack_t2_free);
            req.payload[20] = (uint8_t)(g_stats.stack_t3_free >> 8);
            req.payload[21] = (uint8_t)(g_stats.stack_t3_free);
            req.payload[22] = (uint8_t)(g_stats.stack_t6_free >> 8);
            req.payload[23] = (uint8_t)(g_stats.stack_t6_free);
            req.payload[24] = (uint8_t)(g_stats.stack_t7_free >> 8);
            req.payload[25] = (uint8_t)(g_stats.stack_t7_free);

            req.payload[26] = (uint8_t)(peak_wcet_us >> 8);
            req.payload[27] = (uint8_t)(peak_wcet_us);
            req.payload[28] = (uint8_t)(peak_rb_fill  >> 8);
            req.payload[29] = (uint8_t)(peak_rb_fill);

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
            acc_t6_x100   = 0U;
            acc_t7_x100   = 0U;
            acc_idle_x100 = 0U;
            peak_wcet_us  = 0U;
            peak_rb_fill  = 0U;
            tick_count    = 0U;
        }
        IWDG_Thread_SetAlive(&IWDG_Thread4_Alive);
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
    RCC_EN_CLK_PERIPHERAL(PERIPH_ADC1);
    RCC_LSI_Enable();

    config_gpio_i2c1();
    config_gpio_pa8_sync();
    config_gpio_uart1();

    /* PA1 = ADC1 channel 1 (LM35) — must be analog, no pull */
    GPIO_CONFIG_t adc_pin = {
        .Pin = GPIO_PIN1, .Port = GPIO_PORTA,
        .Mode = GPIO_MODE_ANALOG, .Type = GPIO_OUTPUT_PUSH_PULL,
        .Speed = GPIO_SPEED_LOW, .Pull = GPIO_NO_PULL,
        .Alternate = AF_SYSTEM
    };
    GPIO_INIT(&adc_pin);

    ADC_Config_t adc_cfg = {
        .channel     = 1U,                    /* PA1 = ADC1_IN1 */
        .resolution  = ADC_RES_12BIT,
        .sample_time = ADC_SAMPLETIME_480,    /* slowest — suits LM35 high-Z output */
    };
    (void)ADC_Init(&adc_cfg);

    app_i2c_init();
    app_uart_init();
    config_led_pc13();

    /* Retry the IMU bring-up — a bus left stuck by a warm reset usually
     * clears once the device-reset write in MPU6050_Init lands. Status is
     * logged after Logger_Init() below; a dead IMU must not halt boot,
     * because Thread 5 still needs to accept an OTA to recover.        */
    for (uint8_t attempt = 0U; attempt < 3U; ++attempt){
        g_mpu_init_status = (uint8_t)MPU6050_Init(
            I2C_ID_1, (I2C_DevAddr7_t)MPU6050_ADDR_LOW, 5000000UL);
        if (g_mpu_init_status == (uint8_t)MPU6050_OK){ break; }
    }

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

    /* Bootloader RX — highest priority (4) so it preempts everything */
    s_thread5_handle = xTaskCreateStatic(
        Thread5_BootloaderRx, "BL_RX",
        THREAD5_STACK_WORDS, NULL, 4U,
        s_thread5_stack, &s_thread5_tcb);
    configASSERT(s_thread5_handle != NULL);

    /* Temperature sensor task — priority 1 */
    s_thread6_handle = xTaskCreateStatic(
        Thread6_Temperature, "TEMP",
        THREAD6_STACK_WORDS, NULL, 1U,
        s_thread6_stack, &s_thread6_tcb);
    configASSERT(s_thread6_handle != NULL);

    /* Ultrasonic Init */
    g_ultrasonic_queue = xQueueCreateStatic(4U, sizeof(uint32_t), (uint8_t *)s_ultrasonic_queue_buffer, &s_ultrasonic_queue_storage);
    configASSERT(g_ultrasonic_queue != NULL);

    HCSR04_Config_t u1 = { .trigger_port = GPIO_PORTA, .trigger_pin = GPIO_PIN4 };
    HCSR04_Init(&u1);
    HCSR04_Config_t u2 = { .trigger_port = GPIO_PORTA, .trigger_pin = GPIO_PIN5 };
    HCSR04_Init(&u2);

    GPIO_CONFIG_t echo1 = {
        .Pin = GPIO_PIN6, .Port = GPIO_PORTA,
        .Mode = GPIO_MODE_ALTERNATE, .Type = GPIO_OUTPUT_PUSH_PULL,
        .Speed = GPIO_SPEED_VERY_HIGH, .Pull = GPIO_PULL_DOWN,
        .Alternate = AF_TIM_3_5
    };
    GPIO_INIT(&echo1);

    GPIO_CONFIG_t echo2 = {
        .Pin = GPIO_PIN7, .Port = GPIO_PORTA,
        .Mode = GPIO_MODE_ALTERNATE, .Type = GPIO_OUTPUT_PUSH_PULL,
        .Speed = GPIO_SPEED_VERY_HIGH, .Pull = GPIO_PULL_DOWN,
        .Alternate = AF_TIM_3_5
    };
    GPIO_INIT(&echo2);

    TIM_Config_t tim3_cfg = {
        .id        = TIM_ID_3,
        .prescaler = 83U,
        .period    = 65535U,
        .mode      = TIM_MODE_UP,
        .arpe      = TIM_ARPE_ENABLE,
        .callback  = NULL,
        .ctx       = NULL
    };
    TIM_Init(&tim3_cfg);

    TIM_IC_Config_t ic1 = {
        .id       = TIM_ID_3,
        .channel  = TIM_CH_1,
        .polarity = TIM_IC_POLARITY_RISING,
        .callback = on_hcsr04_capture,
        .ctx      = (void *)0U
    };
    TIM_IC_Init(&ic1);

    TIM_IC_Config_t ic2 = {
        .id       = TIM_ID_3,
        .channel  = TIM_CH_2,
        .polarity = TIM_IC_POLARITY_RISING,
        .callback = on_hcsr04_capture,
        .ctx      = (void *)1U
    };
    TIM_IC_Init(&ic2);

    NVIC_SetPriority(TIM3, 5U);
    NVIC_EnableIRQ(TIM3);
    TIM_Start(TIM_ID_3);

    /* Ultrasonic sensor task — priority 1 */
    s_thread7_handle = xTaskCreateStatic(
        Thread7_Ultrasonic, "ULTRA",
        THREAD7_STACK_WORDS, NULL, 1U,
        s_thread7_stack, &s_thread7_tcb);
    configASSERT(s_thread7_handle != NULL);

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