/*****************************************************************************
 * collect_data_app.c — Data Collection Application Layer
 *                       STM32F401CC — Road Surface Classification Project
 *
 * HARDWARE MAP:
 *   I2C1  SCL  → PB6  (AF4, Open-Drain, Pull-Up)
 *   I2C1  SDA  → PB7  (AF4, Open-Drain, Pull-Up)
 *   USART1 TX  → PA9  (AF7, Push-Pull)
 *   USART1 RX  → PA10 (AF7, Pull-Up — configured, unused in collection)
 *   DMA1  Stream0, Channel1 → I2C1 RX  (RM0368 Table 27)
 *
 * CLOCKS:
 *   SYSCLK  = 84 MHz (HSI PLL)
 *   APB1    = 42 MHz → TIM2 clock = 84 MHz (x2 multiplier)
 *   APB2    = 84 MHz (USART1)
 *
 * DEPENDENCIES:
 *   RCC, GPIO, NVIC, I2C, I2C_SERVICE, UART, UART_SERVICE,
 *   DMA, TIM, SYSTICK, MPU6050, RING_BUFFER, FRAME
 *****************************************************************************/

#include "collect_data_app.h"

#include "RCC_INTERFACE.h"
#include "GPIO_INTERFACE.h"
#include "NVIC_INTERFACE.h"
#include "I2C_INTERFACE.h"
#include "I2C_SERVICE.h"
#include "UART_INTERFACE.h"
#include "UART_SERVICE.h"
#include "DMA_INTERFACE.h"
#include "TIM_INTERFACE.h"
#include "SYSTICK_INTERFACE.h"
#include "MPU6050.h"
#include "RING_BUFFER.h"
#include "FRAME.h"

/* ================================================================
 *  Hardware Configuration Constants
 * ================================================================ */

/* NVIC priorities — lower number = higher priority on Cortex-M4  */
#define APP_PRIO_TIM2      1U   /* 100 Hz tick — must preempt all  */
#define APP_PRIO_I2C1_EV   2U   /* I2C event ISR                   */
#define APP_PRIO_I2C1_ER   2U   /* I2C error ISR                   */
#define APP_PRIO_DMA1_S0   2U   /* DMA1 Stream0 — I2C1 RX complete */
#define APP_PRIO_USART1    3U   /* UART TX IRQ                     */

/* DMA timeout — 5 ms. I2C 14-byte burst at 400 kHz ≈ 350 µs.
 * 5 ms gives 14× margin. Timeout means I2C hardware problem.      */
#define APP_DMA_TIMEOUT_MS  5U

/* Sensor stabilisation after MPU6050_Init completes.
 * DLPF settling: 44 Hz filter takes ~22 ms to settle.
 * Using 100 ms for full margin before first real sample.          */
#define APP_SENSOR_SETTLE_MS  100U

/* MPU6050_Init timeout — decrement counter, NOT milliseconds.
 * At 84 MHz, ~4 cycles per loop → 5,000,000 ≈ 238 ms.
 * Covers: 100ms reset delay + 50ms gyro settle + DMA overhead.   */
#define APP_MPU_INIT_TIMEOUT  5000000UL

/* ================================================================
 *  Module-Private State
 * ================================================================ */

/* Debug stage breadcrumb — readable in Watch panel / debugger.
 *   0    = power-on
 *   1    = RCC done
 *   2    = SysTick done
 *   3    = I2C done
 *   4    = I2C_SVC done
 *   5    = UART done
 *   6    = UART_SVC done
 *   7    = MPU6050 done
 *   8    = RingBuffer done
 *   9    = FRAME done
 *  10    = TIM2 done
 *  11    = NVIC done
 *  12    = TIM2 started — running
 *  0xEE  = fatal init error — halted                              */
static volatile uint8_t  dbg_app_stage    = 0U;

/* Lifetime runtime counters — never cleared, saturate at max.
 * volatile: written in ISR (missed_ticks in TIM2 callback),
 *           read in main context by CollectApp_GetDiag().         */
static volatile uint32_t app_missed_ticks = 0U;  /* TIM2 overruns */
static volatile uint32_t app_dma_timeouts = 0U;  /* I2C DMA hangs */
static volatile uint32_t app_frames_sent  = 0U;  /* successful TX */
static volatile uint32_t app_frames_drop  = 0U;  /* UART full     */



/* ADD THESE 4 LINES immediately after: */
static volatile uint32_t dbg_trigger_ok   = 0U;  /* TriggerRead accepted     */
static volatile uint32_t dbg_trigger_busy = 0U;  /* TriggerRead hit BUSY     */
static volatile uint32_t dbg_trigger_err  = 0U;  /* TriggerRead returned err */
static volatile uint32_t dbg_dma_cb_count = 0U;  /* DMA callback fired count */





/* ISR → main signalling flags.
 * uint8_t: single-byte read/write is atomic on Cortex-M4.
 * volatile: prevents compiler from caching in register.          */
static volatile uint8_t flag_sample_due = 0U;  /* set by TIM2 ISR */
static volatile uint8_t flag_dma_done   = 0U;  /* set by DMA cb   */

/* Parsed sensor data written in DMA callback, read in main loop.
 * Protected by flag_dma_done handshake:
 *   ISR  writes app_latest_sample THEN sets flag_dma_done = 1U
 *   Main reads flag_dma_done THEN reads app_latest_sample
 * No race condition under SPSC invariant.                         */
static MPU6050_RawData_t app_latest_sample;

/* ================================================================
 *  Static Helper — blocking millisecond delay (uses SysTick)
 *  Only used during init — never in the main loop.
 * ================================================================ */
static void app_delay_ms(uint32_t ms)
{
    uint32_t start = Systick_GetTick();
    while ((Systick_GetTick() - start) < ms)
    {
        /* busy wait — startup only */
    }
}

/* ================================================================
 *  Static Helper — fatal error handler
 *
 *  Halts execution with a breadcrumb visible in the debugger.
 *  In data collection phase: no IWDG — we want to see the error,
 *  not silently reset.
 * ================================================================ */
static void app_fatal_error(uint8_t stage)
{
    dbg_app_stage = 0xEEU;
    (void)stage;            /* visible in call stack                */
    while (1U == 1U)        /* halt — attach debugger to inspect    */
    {
        /* intentional infinite loop */
    }
}

/* ================================================================
 *  ISR Callback — TIM2 100 Hz tick
 *
 *  Called from TIM2_IRQHandler (inside TIM.c) after SR.UIF cleared.
 *  MUST be ISR-safe: sets flag only, no blocking, no UART.
 *
 *  If flag_sample_due is still 1 when we arrive, the previous tick
 *  was not serviced — main loop is behind budget. Count it.
 * ================================================================ */
static void on_tim2_tick(void *ctx)
{
    (void)ctx;

    if (flag_sample_due != 0U)
    {
        if (app_missed_ticks < 0xFFFFFFFFUL)
        {
            app_missed_ticks++;
        }
    }

    flag_sample_due = 1U;
}

/* ================================================================
 *  ISR Callback — MPU6050 DMA read complete
 *
 *  Called from DMA1_Stream0_IRQHandler (inside I2C_SERVICE.c) when
 *  the 14-byte burst from register 0x3B is fully received.
 *  MUST be ISR-safe: field copy + ring buffer push + flag set only.
 *
 *  data == NULL signals a DMA error path — skip push, wake main.
 * ================================================================ */
static void on_mpu6050_read_done(const MPU6050_RawData_t *data, void *ctx)
{
    (void)ctx;
    
    dbg_dma_cb_count++;
    if (data == NULL)
    {
        /* DMA error — signal main loop to skip this sample        */
        flag_dma_done = 1U;
        return;
    }

    /* Field-by-field copy — no memcpy (MISRA Rule 17.1).
     * Written in ISR, read by main loop after flag_dma_done = 1.  */
    app_latest_sample.accel_x  = data->accel_x;
    app_latest_sample.accel_y  = data->accel_y;
    app_latest_sample.accel_z  = data->accel_z;
    app_latest_sample.temp_raw = data->temp_raw;
    app_latest_sample.gyro_x   = data->gyro_x;
    app_latest_sample.gyro_y   = data->gyro_y;
    app_latest_sample.gyro_z   = data->gyro_z;

    /* Push to ring buffer — ISR is the SPSC producer.
     * RING_BUFFER_FULL return means main loop is too slow.
     * Drop counter is maintained inside RingBuffer driver.        */
    (void)RingBuffer_Push(data);

    /* Signal main loop — must be last write in callback           */
    flag_dma_done = 1U;
}

/* ================================================================
 *  I2C Bus Recovery
 *
 *  Sends 9 SCL clock pulses then a STOP condition on PB6/PB7.
 *  Releases any MPU6050 that is holding SDA LOW mid-byte from a
 *  previous failed I2C session.
 *
 *  Called BEFORE app_gpio_init() (which switches PB6/PB7 to AF4).
 *  Uses GPIO output mode temporarily — pins reconfigured by
 *  app_gpio_init() afterwards.
 *
 *  Delay: 2000 nop iterations ≈ 24 µs at 84 MHz (286 ns/nop × 84).
 *  SCL period ≈ 48 µs → ~20 kHz — well within I2C spec for recovery.
 * ================================================================ */
static void app_i2c_bus_recovery(void)
{
    GPIO_CONFIG_t pin_cfg;
    uint32_t      i;
    uint32_t      d;

    /* Configure PB6 (SCL) as GPIO output, open-drain, pull-up    */
    pin_cfg.Port      = GPIO_PORTB;
    pin_cfg.Pin       = GPIO_PIN6;
    pin_cfg.Mode      = GPIO_MODE_OUTPUT;
    pin_cfg.Type      = GPIO_OUTPUT_OPEN_DRAIN;
    pin_cfg.Speed     = GPIO_SPEED_HIGH;
    pin_cfg.Pull      = GPIO_PULL_UP;
    pin_cfg.Alternate = AF_SYSTEM;
    (void)GPIO_INIT(&pin_cfg);

    /* Configure PB7 (SDA) identically                             */
    pin_cfg.Pin = GPIO_PIN7;
    (void)GPIO_INIT(&pin_cfg);

    /* Drive both lines HIGH — release bus                         */
    (void)GPIO_WritePin(GPIO_PORTB, GPIO_PIN6, GPIO_PIN_SET);
    (void)GPIO_WritePin(GPIO_PORTB, GPIO_PIN7, GPIO_PIN_SET);
    for (d = 0U; d < 2000U; d++) { __asm volatile("nop"); }

    /* 9 SCL clock pulses — each stuck slave advances one bit.
     * After 9 clocks, any slave must have completed its byte.    */
    for (i = 0U; i < 9U; i++)
    {
        (void)GPIO_WritePin(GPIO_PORTB, GPIO_PIN6, GPIO_PIN_RESET);
        for (d = 0U; d < 2000U; d++) { __asm volatile("nop"); }
        (void)GPIO_WritePin(GPIO_PORTB, GPIO_PIN6, GPIO_PIN_SET);
        for (d = 0U; d < 2000U; d++) { __asm volatile("nop"); }
    }

    /* Generate STOP condition: SDA LOW → SCL HIGH → SDA HIGH.
     * This terminates any pending transaction on the slave side.  */
    (void)GPIO_WritePin(GPIO_PORTB, GPIO_PIN7, GPIO_PIN_RESET); /* SDA LOW  */
    for (d = 0U; d < 2000U; d++) { __asm volatile("nop"); }
    (void)GPIO_WritePin(GPIO_PORTB, GPIO_PIN6, GPIO_PIN_SET);   /* SCL HIGH */
    for (d = 0U; d < 2000U; d++) { __asm volatile("nop"); }
    (void)GPIO_WritePin(GPIO_PORTB, GPIO_PIN7, GPIO_PIN_SET);   /* SDA HIGH */
    for (d = 0U; d < 2000U; d++) { __asm volatile("nop"); }
}

/* ================================================================
 *  GPIO Configuration
 *
 *  PB6 — I2C1 SCL (AF4, Open-Drain, Pull-Up)
 *  PB7 — I2C1 SDA (AF4, Open-Drain, Pull-Up)
 *  PA9 — USART1 TX (AF7, Push-Pull)
 *  PA10— USART1 RX (AF7, Pull-Up — configured, not used)
 *
 *  NOTE: GPIOA and GPIOB clocks must be enabled before calling.
 *  NOTE: app_i2c_bus_recovery() must be called before this —
 *        it uses PB6/PB7 as GPIO output; this switches them to AF4.
 * ================================================================ */
static void app_gpio_init(void)
{
    GPIO_CONFIG_t pin_cfg;

    /* ---- PB6 — I2C1 SCL ---- */
    pin_cfg.Port      = GPIO_PORTB;
    pin_cfg.Pin       = GPIO_PIN6;
    pin_cfg.Mode      = GPIO_MODE_ALTERNATE;
    pin_cfg.Type      = GPIO_OUTPUT_OPEN_DRAIN;   /* I2C: open-drain mandatory */
    pin_cfg.Speed     = GPIO_SPEED_HIGH;
    pin_cfg.Pull      = GPIO_PULL_UP;
    pin_cfg.Alternate = AF_I2C_1_3;               /* AF4 */
    if (GPIO_INIT(&pin_cfg) != GPIO_OK) { app_fatal_error(2U); }

    /* ---- PB7 — I2C1 SDA ---- */
    pin_cfg.Pin = GPIO_PIN7;
    if (GPIO_INIT(&pin_cfg) != GPIO_OK) { app_fatal_error(2U); }

    /* ---- PA9 — USART1 TX ---- */
    pin_cfg.Port      = GPIO_PORTA;
    pin_cfg.Pin       = GPIO_PIN9;
    pin_cfg.Mode      = GPIO_MODE_ALTERNATE;
    pin_cfg.Type      = GPIO_OUTPUT_PUSH_PULL;
    pin_cfg.Speed     = GPIO_SPEED_HIGH;
    pin_cfg.Pull      = GPIO_NO_PULL;
    pin_cfg.Alternate = AF_USART_1_2;             /* AF7 */
    if (GPIO_INIT(&pin_cfg) != GPIO_OK) { app_fatal_error(2U); }

    /* ---- PA10 — USART1 RX ---- */
    pin_cfg.Pin  = GPIO_PIN10;
    pin_cfg.Pull = GPIO_PULL_UP;
    if (GPIO_INIT(&pin_cfg) != GPIO_OK) { app_fatal_error(2U); }
}

/* ================================================================
 *  I2C Initialisation
 *
 *  I2C1 at 400 kHz Fast Mode, DMA RX via DMA1 Stream0 Channel1.
 *  RM0368 Table 27: DMA1 Stream0 Channel1 = I2C1_RX.
 * ================================================================ */
static void app_i2c_init(void)
{
    I2C_Config_t    i2c_cfg;
    I2C_Error_t     i2c_err;
    I2C_SVC_Error_t svc_err;

    i2c_cfg.id                = I2C_ID_1;
    i2c_cfg.speed             = I2C_SPEED_FAST;           /* 400 kHz        */
    i2c_cfg.duty_cycle        = I2C_DUTY_2;               /* Tlow/Thigh=2   */
    i2c_cfg.addressing_mode   = I2C_ADDR_7BIT;
    i2c_cfg.own_address1      = 0x00U;
    i2c_cfg.own_address2      = 0x00U;
    i2c_cfg.dual_address_mode = I2C_DUAL_ADDR_DISABLE;
    i2c_cfg.general_call_mode = I2C_GENERAL_CALL_DISABLE;
    i2c_cfg.clock_stretching  = I2C_CLOCK_STRETCH_ENABLE;
    i2c_cfg.analog_filter     = I2C_ANALOG_FILTER_ENABLE;
    i2c_cfg.digital_filter    = I2C_DIGITALFILTER_0;
    i2c_cfg.transfer_mode     = I2C_MODE_DMA;

    i2c_err = I2C_Init(&i2c_cfg);
    if (i2c_err != I2C_OK) { app_fatal_error(3U); }
    dbg_app_stage = 3U;

    svc_err = I2C_SVC_Init(
        I2C_ID_1,
        APP_PRIO_I2C1_EV,
        APP_PRIO_I2C1_ER,
        DMA_1,
        DMA_STREAM_0,
        DMA_CHANNEL_1,
        APP_PRIO_DMA1_S0
    );
    if (svc_err != I2C_SVC_OK) { app_fatal_error(4U); }
    dbg_app_stage = 4U;
}

/* ================================================================
 *  UART Initialisation
 *
 *  USART1 at 115200 baud, 8N1, IRQ TX.
 *  IRQ TX chosen over DMA TX: copies bytes into ring buffer
 *  immediately — no DMA lifetime concern (FRAME.c N-F04).
 *  20 bytes at 115200 ≈ 1.7 ms — well within 10 ms budget.
 * ================================================================ */
static void app_uart_init(void)
{
    UART_Config_t    uart_cfg;
    UART_Error_t     uart_err;
    UART_SVC_Error_t svc_err;

    uart_cfg.uart_id      = UART1_ID;
    uart_cfg.baudrate     = 115200UL;
    uart_cfg.parity       = UART_PARITY_NONE;
    uart_cfg.stop_bits    = UART_STOP_1;
    uart_cfg.data_bits    = UART_DATA_8BIT;
    uart_cfg.oversampling = UART_OVER8_DISABLE;
    uart_cfg.tx_mode      = UART_MODE_IRQ;
    uart_cfg.rx_mode      = UART_MODE_IRQ;

    uart_err = UART_Init(&uart_cfg);
    if (uart_err != UART_OK) { app_fatal_error(5U); }
    dbg_app_stage = 5U;

    svc_err = UART_SVC_Init(
        UART1_ID,
        APP_PRIO_USART1,
        UART_SVC_TX_MODE_IRQ,
        DMA_1,            /* ignored in IRQ mode */
        DMA_STREAM_7,     /* ignored in IRQ mode */
        DMA_CHANNEL_4     /* ignored in IRQ mode */
    );
    if (svc_err != UART_SVC_OK) { app_fatal_error(6U); }
    dbg_app_stage = 6U;
}

/* ================================================================
 *  Public — CollectApp_Init
 * ================================================================ */
CollectApp_Error_t CollectApp_Init(void)
{
    MPU6050_Error_t mpu_err;
    TIM_Config_t    tim_cfg;
    TIM_Error_t     tim_err;

    /* ---- 1. RCC — 84 MHz HSI PLL ---- */
    dbg_app_stage = 1U;
    if (RCC_INIT_84MHz_HSI() != RCC_OK) { app_fatal_error(1U); }

    /* ---- 2. SysTick — 1 ms tick (must start before app_delay_ms) ---- */
    if (Systick_Init(SYSTICK_CLK_PROCESSOR) != SYSTICK_OK) { app_fatal_error(2U); }
    if (Systick_SetReload(1U)               != SYSTICK_OK) { app_fatal_error(2U); }
    if (Systick_Start()                     != SYSTICK_OK) { app_fatal_error(2U); }
    dbg_app_stage = 2U;

    /* ---- 3. Peripheral clocks ---- */
    (void)RCC_EN_CLK_PERIPHERAL(PERIPH_GPIOA);
    (void)RCC_EN_CLK_PERIPHERAL(PERIPH_GPIOB);
    (void)RCC_EN_CLK_PERIPHERAL(PERIPH_I2C1);
    (void)RCC_EN_CLK_PERIPHERAL(PERIPH_DMA1);
    (void)RCC_EN_CLK_PERIPHERAL(PERIPH_USART1);

    /* ---- 4. I2C bus recovery (before GPIO AF switch) ---- */
    app_i2c_bus_recovery();

    /* ---- 5. GPIO — switches PB6/PB7 from output to AF4 ---- */
    app_gpio_init();

    /* ---- 6. I2C1 + DMA1 Stream0 ---- */
    app_i2c_init();

    /* ---- 7. USART1 IRQ TX ---- */
    app_uart_init();

    /* ---- 8. MPU6050 ---- */
    /* MPU6050_Init performs:
     *   a) 100ms power-on delay
     *   b) DEVICE_RESET (0x80 → PWR_MGMT_1) — clears any stuck state
     *   c) 100ms reset completion delay
     *   d) Wake from sleep (0x00 → PWR_MGMT_1)
     *   e) 50ms gyro ZRO settling delay
     *   f) WHO_AM_I verify via 2-byte DMA read
     *   g) Config: ±4g, ±500°/s, DLPF 44 Hz, 100 Hz ODR          */
    mpu_err = MPU6050_Init(
        I2C_ID_1,
        (I2C_DevAddr7_t)MPU6050_ADDR_LOW,
        APP_MPU_INIT_TIMEOUT
    );
    if (mpu_err != MPU6050_OK) { app_fatal_error(7U); }
    dbg_app_stage = 7U;

    /* ---- 9. Ring Buffer + Frame module ---- */
    (void)RingBuffer_Init();
    dbg_app_stage = 8U;

    (void)FRAME_Init();
    dbg_app_stage = 9U;

    /* ---- 10. Sensor settle after MPU6050_Init ---- */
    app_delay_ms(APP_SENSOR_SETTLE_MS);

    /* ---- 11. TIM2 — 100 Hz hardware timer ----
     * TIM2 clock = 84 MHz (APB1 x2 multiplier)
     * PSC = 8399 → tick = 84 MHz / 8400 = 10 kHz
     * ARR = 99   → update = 10 kHz / 100 = 100 Hz
     *
     * CRITICAL ORDER:
     *   TIM_Init  → configure registers, fire EGR.UG, clear UIF
     *   NVIC      → arm interrupt (BEFORE TIM_Start to avoid
     *               missing the first tick)
     *   TIM_Start → set CEN=1, counting begins               */
    tim_cfg.id        = TIM_ID_2;
    tim_cfg.prescaler = TIM_PSC_100HZ;       /* 8399U                */
    tim_cfg.period    = TIM_ARR_100HZ;       /* 99U                  */
    tim_cfg.mode      = TIM_MODE_UP;
    tim_cfg.arpe      = TIM_ARPE_ENABLE;
    tim_cfg.callback  = on_tim2_tick;
    tim_cfg.ctx       = NULL;

    tim_err = TIM_Init(&tim_cfg);
    if (tim_err != TIM_OK) { app_fatal_error(10U); }
    dbg_app_stage = 10U;

    /* ---- 12. NVIC — arm all interrupt lines ---- */
    (void)NVIC_SetPriority(TIM2, (uint32_t)APP_PRIO_TIM2);
    (void)NVIC_EnableIRQ(TIM2);
    dbg_app_stage = 11U;

    /* ---- 13. Start TIM2 — system begins sampling ---- */
    tim_err = TIM_Start(TIM_ID_2);
    if (tim_err != TIM_OK) { app_fatal_error(11U); }
    dbg_app_stage = 12U;

    return COLLECT_APP_OK;
}

/* ================================================================
 *  Public — CollectApp_Run
 *
 *  100 Hz bare-metal sample loop. Never returns.
 *
 *  Timing budget per 10 ms window (at 115200 baud):
 *    I2C DMA transfer   ≈ 350 µs
 *    Ring buffer ops    ≈   2 µs
 *    Frame build + CRC  ≈  10 µs
 *    UART TX 20 bytes   ≈ 1.7 ms
 *    TOTAL              ≈ 2.1 ms  →  CPU idle ~79% of each tick
 * ================================================================ */
void CollectApp_Run(void)
{
    MPU6050_RawData_t pop_sample;
    FRAME_Error_t     frame_err;
    MPU6050_Error_t   mpu_err;
    uint32_t          wait_start;
    uint32_t          tick_ms;

    while (1U == 1U)
    {
        /* ---- 1. Wait for 100 Hz tick ---- */
        if (flag_sample_due == 0U)
        {
            /* CPU idle — busy-wait is fine for data collection.
             * __WFI() could save power but adds latency on wake. */
            continue;
        }

        /* ---- 2. Consume tick, start DMA read ---- */
        flag_sample_due = 0U;
        flag_dma_done   = 0U;

        mpu_err = MPU6050_TriggerRead(on_mpu6050_read_done, NULL);

        if (mpu_err == MPU6050_ERROR_BUSY)
        {
            if (dbg_trigger_busy < 0xFFFFFFFFUL) { dbg_trigger_busy++; }
            if (app_missed_ticks < 0xFFFFFFFFUL) { app_missed_ticks++; }
            continue;
        }
       if (mpu_err != MPU6050_OK)
        {
            if (dbg_trigger_err < 0xFFFFFFFFUL) { dbg_trigger_err++; }
            continue;
        }
         
        if (dbg_trigger_ok < 0xFFFFFFFFUL) { dbg_trigger_ok++; }
        
        /* ---- 3. Wait for DMA complete (5 ms timeout) ---- */
        wait_start = Systick_GetTick();
        while (flag_dma_done == 0U)
        {
            if ((Systick_GetTick() - wait_start) >= APP_DMA_TIMEOUT_MS)
            {
                if (app_dma_timeouts < 0xFFFFFFFFUL) { app_dma_timeouts++; }
                break;
            }
        }

        if (flag_dma_done == 0U)
        {
            /* DMA timed out — I2C hardware problem.
             * Watch app_dma_timeouts; non-zero = hardware issue.  */
            continue;
        }

        /* ---- 4. Pop sample from ring buffer ---- */
        if (RingBuffer_Pop(&pop_sample) != RING_BUFFER_OK)
        {
            /* Buffer empty — DMA callback fired with data=NULL    */
            continue;
        }

        /* ---- 5. Build + send 20-byte UART frame ---- */
        tick_ms   = Systick_GetTick();
        frame_err = FRAME_BuildAndSend(UART1_ID, &pop_sample, tick_ms);

        if (frame_err == FRAME_OK)
        {
            if (app_frames_sent < 0xFFFFFFFFUL) { app_frames_sent++; }
        }
        else
        {
            /* FRAME_ERROR_UART_FULL most likely — UART ring buffer
             * overrun means laptop is not reading fast enough.    */
            if (app_frames_drop < 0xFFFFFFFFUL) { app_frames_drop++; }
        }
    }
}

/* ================================================================
 *  Public — CollectApp_GetDiag
 * ================================================================ */
void CollectApp_GetDiag(CollectApp_Diag_t *diag)
{
    if (diag == NULL) { return; }

    /* 32-bit aligned reads are atomic on Cortex-M4.
     * No critical section needed for individual reads.            */
    diag->app_stage      = dbg_app_stage;
    diag->missed_ticks   = app_missed_ticks;
    diag->dma_timeouts   = app_dma_timeouts;
    diag->frames_sent    = app_frames_sent;
    diag->frames_dropped = app_frames_drop;
}
