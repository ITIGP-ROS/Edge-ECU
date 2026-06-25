/* ================================================================
 *  MPU6050.c
 *  Dedicated driver for MPU6050 6-axis IMU on I2C1 @ 400 kHz FM.
 *  Uses I2C_SERVICE.h exclusively — never calls I2C.c directly.
 *
 *  Init sequence per datasheet §6.1, §6.3, §7.4:
 *    1. Wake device (PWR_MGMT_1 = 0x00)
 *    2. Wait ≥ 30ms for gyro ZRO settling
 *    3. Read WHO_AM_I to verify identity
 *    4. Write config registers
 * ================================================================ */

#include "MPU6050.h"
#include "I2C_SERVICE.h"
#include "STD_TYPES.h"

/* ================================================================
 *  Internal state — all file-scope static, no dynamic allocation.
 * ================================================================ */

/* I2C instance and device address latched during Init */
static I2C_Id_t        mpu_i2c_id   = I2C_ID_1;
static I2C_DevAddr7_t  mpu_dev_addr = MPU6050_ADDR_LOW;

/* Raw DMA receive buffer — 14 bytes from MPU6050 burst read.
 * MUST be static (file-scope SRAM) so it remains valid from
 * DMA_Start until DMA_TC fires. NEVER use a stack buffer —
 * the stack frame will be gone before DMA completes.          */
static uint8_t mpu_rx_raw[MPU6050_BURST_LEN];

/* Parsed output — filled by MPU6050_ParseRaw inside DMA callback */
static MPU6050_RawData_t mpu_parsed;

/* User callback and context stored at TriggerRead time */
static MPU6050_ReadDone_t mpu_user_cb  = NULL;
static void              *mpu_user_ctx = NULL;

/* Driver state flags */
static uint8_t mpu_initialized = 0U;
static volatile uint8_t mpu_busy = 0U;

/* ================================================================
 *  Static TX buffer for config register writes.
 *  FIX (Bug 4): Moved from stack to file-scope static.
 *  Even though I2C_SVC_WriteReg is blocking, this eliminates
 *  any latent corruption risk if the service layer has an
 *  internal async path or yields during transmission.
 * ================================================================ */
static uint8_t cfg_buf[2U];

/* ================================================================
 *  WHO_AM_I blocking read support — used only during Init.
 *  Static so the DMA destination and flag persist across ISR.
 *
 *  FIX (Bug 5 — single-byte DMA):
 *  Buffer is 2 bytes, NOT 1. STM32F4 I2C cannot reliably use
 *  DMA for single-byte receives — the ADDR handler generates
 *  STOP before DMA can latch the transfer (RM0368 §27.3.3).
 *  Reading 2 bytes takes the multi-byte ADDR path, letting
 *  DMA and CR2.LAST handle NACK+STOP correctly. The second
 *  byte (register 0x76) is harmless and discarded.
 * ================================================================ */
static volatile uint8_t         whoami_done   = 0U;
static uint8_t                  whoami_buf[2U] = {0U, 0U};  /* FIX: was single uint8_t */
static volatile I2C_SVC_Error_t whoami_result = I2C_SVC_OK;

/* ================================================================
 *  Debug breadcrumbs — read these in the debugger Watch panel.
 *
 *  dbg_init_stage:
 *    10 = PWR_MGMT_1 wake write rejected by SVC
 *    20 = WHO_AM_I DMA read rejected by SVC
 *    21 = WHO_AM_I DMA callback never fired (timeout)
 *    22 = WHO_AM_I DMA completed with I2C error
 *    23 = WHO_AM_I value mismatch
 *    30 = ACCEL_CONFIG write failed
 *    40 = GYRO_CONFIG write failed
 *    50 = CONFIG (DLPF) write failed
 *    60 = SMPLRT_DIV write failed
 *    99 = SUCCESS — all stages passed
 * ================================================================ */
static volatile uint8_t  dbg_init_stage = 0U;
static volatile uint8_t  dbg_whoami_val = 0U;
static volatile uint8_t  dbg_svc_err    = 0U;

/* ================================================================
 *  Spin delay — portable software delay for startup sequences.
 *
 *  Calibration at 84 MHz, ~4 cycles per iteration:
 *    1,050,000  ≈  50 ms
 *    2,100,000  ≈ 100 ms
 *
 *  NOTE: This is only used before the scheduler starts.
 *  Once FreeRTOS is running, use vTaskDelay() instead.
 * ================================================================ */
static void MPU6050_SpinDelay(volatile uint32_t count)
{
    while (count > 0U)
    {
        count--;
    }
}

/* ================================================================
 *  whoami_cb — local callback for blocking WHO_AM_I read.
 *  Called from DMA ISR context. Saves result and sets flag so
 *  Init can stop polling.
 * ================================================================ */
static void whoami_cb(I2C_SVC_Error_t result, void *ctx)
{
    (void)ctx;

    whoami_result = result;
    whoami_done   = 1U;
}

// /* ================================================================
//  *  MPU6050_ParseRaw
//  *  Converts 14 raw big-endian bytes into signed 16-bit host values.
//  *  Byte layout from MPU6050 datasheet:
//  *    [0..1] ACCEL_X   [2..3] ACCEL_Y   [4..5] ACCEL_Z
//  *    [6..7] TEMP      [8..9] GYRO_X    [10..11] GYRO_Y
//  *    [12..13] GYRO_Z
//  * ================================================================ */
// static void MPU6050_ParseRaw(const uint8_t *raw, MPU6050_RawData_t *out)
// {
//     out->accel_x  = (sint16_t)(((uint16_t)raw[0U]  << 8U) | (uint16_t)raw[1U]);
//     out->accel_y  = (sint16_t)(((uint16_t)raw[2U]  << 8U) | (uint16_t)raw[3U]);
//     out->accel_z  = (sint16_t)(((uint16_t)raw[4U]  << 8U) | (uint16_t)raw[5U]);
//     out->temp_raw = (sint16_t)(((uint16_t)raw[6U]  << 8U) | (uint16_t)raw[7U]);
//     out->gyro_x   = (sint16_t)(((uint16_t)raw[8U]  << 8U) | (uint16_t)raw[9U]);
//     out->gyro_y   = (sint16_t)(((uint16_t)raw[10U] << 8U) | (uint16_t)raw[11U]);
//     out->gyro_z   = (sint16_t)(((uint16_t)raw[12U] << 8U) | (uint16_t)raw[13U]);
// }

/* ================================================================
 *  MPU6050_ParseRaw — with axis remap for VERTICAL mounting
 *
 *  Physical mounting (board vertical, pins down in breadboard):
 *    Board +Y axis → points DOWN (toward gravity)
 *    Board +X axis → points horizontal
 *    Board +Z axis → points horizontal
 *
 *  Remap so logical +Z = UP (away from road):
 *    logical_X =  raw_X
 *    logical_Y =  raw_Z
 *    logical_Z = -raw_Y   (negate so +Z = up = +1g at rest)
 *
 *  Same remap applied to gyroscope axes for consistency.
 * ================================================================ */
static void MPU6050_ParseRaw(const uint8_t *raw, MPU6050_RawData_t *out)
{
    /* ---- Extract all 6 raw values (big-endian from MPU6050) ---- */
    sint16_t raw_ax = (sint16_t)(((uint16_t)raw[0U]  << 8U) | (uint16_t)raw[1U]);
    sint16_t raw_ay = (sint16_t)(((uint16_t)raw[2U]  << 8U) | (uint16_t)raw[3U]);
    sint16_t raw_az = (sint16_t)(((uint16_t)raw[4U]  << 8U) | (uint16_t)raw[5U]);
    sint16_t raw_gx = (sint16_t)(((uint16_t)raw[8U]  << 8U) | (uint16_t)raw[9U]);
    sint16_t raw_gy = (sint16_t)(((uint16_t)raw[10U] << 8U) | (uint16_t)raw[11U]);
    sint16_t raw_gz = (sint16_t)(((uint16_t)raw[12U] << 8U) | (uint16_t)raw[13U]);

    /* ---- Axis remap for vertical mount ----
     *  Board +X points UP → becomes logical +Z
     *  Board +Y stays    → becomes logical +Y
     *  Board +Z          → becomes logical +X
     *
     *  logical_X =  raw_Z
     *  logical_Y =  raw_Y
     *  logical_Z =  raw_X   (+X points up = +Z up = +1g)
     * ---------------------------------------- */
    out->accel_x =  raw_az;
    out->accel_y =  raw_ay;
    out->accel_z =  raw_ax;

    out->gyro_x  =  raw_gz;
    out->gyro_y  =  raw_gy;
    out->gyro_z  =  raw_gx;

    /* ---- Temp unchanged ---- */
    out->temp_raw = (sint16_t)(((uint16_t)raw[6U] << 8U) | (uint16_t)raw[7U]);
}



/* ================================================================
 *  mpu_dma_done
 *  Internal DMA complete callback — registered with
 *  I2C_SVC_ReadBurst_DMA inside TriggerRead.
 *  Called from DMA TC ISR — must be short, ISR-safe.
 * ================================================================ */
static void mpu_dma_done(I2C_SVC_Error_t result, void *ctx)
{
    (void)ctx;

    /* Clear busy BEFORE calling user callback — mandatory ordering.
     * This allows the user callback to immediately re-trigger
     * without hitting the busy guard.                              */
    mpu_busy = 0U;

    if (result == I2C_SVC_OK)
    {
        MPU6050_ParseRaw(mpu_rx_raw, &mpu_parsed);

        if (mpu_user_cb != NULL)
        {
            mpu_user_cb(&mpu_parsed, mpu_user_ctx);
        }
    }
    else
    {
        if (mpu_user_cb != NULL)
        {
            mpu_user_cb(NULL, mpu_user_ctx);
        }
    }
}

/* ================================================================
 *  MPU6050_Init
 *  Blocking initialisation — call at startup only, before scheduler.
 *
 *  CORRECT sequence per datasheet §6.1, §6.3, §7.4:
 *    1. Power-on delay (VDD ramp, §7.4)
 *    2. Wake from sleep (PWR_MGMT_1 = 0x00)
 *    3. Gyro ZRO settling delay ≥ 30ms (§6.1)
 *    4. WHO_AM_I identity check
 *    5. Write config registers
 *
 *  IMPORTANT: Caller MUST have called I2C_SVC_Init() before this
 *  function. The DMA stream, channel, and NVIC IRQs must be
 *  configured or the WHO_AM_I DMA read will silently hang.
 * ================================================================ */
MPU6050_Error_t MPU6050_Init(
    I2C_Id_t        i2c_id,
    I2C_DevAddr7_t  addr,
    uint32_t        timeout
)
{
    I2C_SVC_Error_t svc_err;

    /* ---- Reset debug state ---- */
    dbg_init_stage = 0U;
    dbg_whoami_val = 0U;
    dbg_svc_err    = 0U;

    /* ---- 1. Validate device address ---- */
    if ((addr != (I2C_DevAddr7_t)MPU6050_ADDR_LOW) &&
        (addr != (I2C_DevAddr7_t)MPU6050_ADDR_HIGH))
    {
        return MPU6050_ERROR_PARAM;
    }

    /* ---- 2. Latch I2C instance and device address ---- */
    mpu_i2c_id   = i2c_id;
    mpu_dev_addr = addr;

    /* ────────────────────────────────────────────────────────────
     *  3. Power-on delay — §7.4 VDD ramp time ≤ 100ms.
     *     Wait for internal oscillator to stabilize before any
     *     I2C communication. At 84 MHz, ~4 cycles/loop:
     *       2,100,000 ≈ 100 ms
     * ──────────────────────────────────────────────────────────── */
    MPU6050_SpinDelay(2100000U);

    /* ────────────────────────────────────────────────────────────
     *  4. Device reset — MUST be step 1 of any robust init.
     *
     *     PWR_MGMT_1 bit 7 = DEVICE_RESET.
     *     Writing 1 resets ALL internal registers to defaults
     *     and clears any corrupted state from a previous session.
     *     The bit auto-clears when reset is complete.
     *
     *     After power-on, MPU6050 starts with SLEEP=1 (bit 6).
     *     After a failed I2C session, the sensor may be stuck in
     *     an unknown internal state — a device reset is the only
     *     guaranteed recovery path without cycling power.
     *
     *     Datasheet §7.4: wait ≥ 100ms after reset before any
     *     further register access. Using 100ms spin delay.
     *
     *     NOTE: This write may fail if the I2C bus was left stuck
     *     from a previous session (SDA held LOW by sensor).
     *     If it fails, the bus is physically stuck — a bus
     *     recovery sequence (9 SCL clocks) is needed at the
     *     hardware level before calling MPU6050_Init().
     * ──────────────────────────────────────────────────────────── */
    dbg_init_stage = 1U;

    cfg_buf[0U] = MPU6050_REG_PWR_MGMT_1;   /* register 0x6B              */
    cfg_buf[1U] = 0x80U;                      /* DEVICE_RESET = 1           */
    svc_err = I2C_SVC_WriteReg(i2c_id, addr, cfg_buf, 2U, timeout);
    if (svc_err != I2C_SVC_OK)
    {
        dbg_init_stage = 10U;
        dbg_svc_err    = (uint8_t)svc_err;
        return MPU6050_ERROR_I2C;
    }

    /* Wait for device reset to complete — datasheet §7.4 requires
     * ≥ 100ms. DEVICE_RESET bit auto-clears internally.
     * At 84 MHz, 2,100,000 loop iterations ≈ 100ms.              */
    MPU6050_SpinDelay(2100000U);

    /* ────────────────────────────────────────────────────────────
     *  5. Wake from sleep — clear SLEEP bit after reset.
     *
     *     After device reset, SLEEP bit is SET again (0x40 default).
     *     Must be cleared before sensors can operate.
     *     CLKSEL = 0 → internal 8 MHz RC oscillator.
     *
     *     This write is separate from the reset write — the reset
     *     restores PWR_MGMT_1 to its power-on default (0x40),
     *     so we must write 0x00 again to clear SLEEP.
     * ──────────────────────────────────────────────────────────── */
    dbg_init_stage = 2U;

    cfg_buf[0U] = MPU6050_REG_PWR_MGMT_1;   /* register 0x6B              */
    cfg_buf[1U] = 0x01U;                      /* SLEEP=0, CLKSEL=0          */
    svc_err = I2C_SVC_WriteReg(i2c_id, addr, cfg_buf, 2U, timeout);
    if (svc_err != I2C_SVC_OK)
    {
        dbg_init_stage = 11U;
        dbg_svc_err    = (uint8_t)svc_err;
        return MPU6050_ERROR_I2C;
    }

    /* ────────────────────────────────────────────────────────────
     *  6. Gyro ZRO settling delay — §6.1 specifies minimum 30ms
     *     from sleep-exit for gyroscope zero-rate output to
     *     stabilize. Using 50ms for margin.
     *     At 84 MHz, 1,050,000 ≈ 50ms.
     * ──────────────────────────────────────────────────────────── */
    MPU6050_SpinDelay(1050000U);

    /* ────────────────────────────────────────────────────────────
     *  7. WHO_AM_I identity check — NOW safe to read.
     *     Device is awake, oscillator is stable, I2C bus is idle.
     *
     *     Uses I2C_SVC_ReadBurst_DMA with rx_len=2.
     *     STM32F4 I2C cannot reliably use DMA for single-byte
     *     receives (RM0368 §27.3.3) — STOP is generated before
     *     DMA can latch the transfer. Fix: read 2 bytes, use [0].
     *     The second byte (register 0x76) is discarded.
     * ──────────────────────────────────────────────────────────── */
    dbg_init_stage = 3U;

    whoami_done    = 0U;
    whoami_buf[0U] = 0U;
    whoami_buf[1U] = 0U;
    whoami_result  = I2C_SVC_OK;

    svc_err = I2C_SVC_ReadBurst_DMA(
        i2c_id,
        addr,
        MPU6050_REG_WHO_AM_I,    /* register 0x75                   */
        &whoami_buf[0U],          /* 2-byte static DMA destination   */
        2U,                       /* rx_len=2, not 1 — DMA minimum   */
        whoami_cb,
        NULL
    );

    if (svc_err != I2C_SVC_OK)
    {
        dbg_init_stage = 20U;
        dbg_svc_err    = (uint8_t)svc_err;
        return MPU6050_ERROR_I2C;
    }

    /* Poll whoami_done — blocking spin, startup only (no RTOS) */
    uint32_t cnt = timeout;
    while ((whoami_done == 0U) && (cnt > 0U))
    {
        cnt--;
    }

    if (cnt == 0U)
    {
        /* DMA callback never fired. Most common causes:
         *   - I2C_SVC_Init() not called before MPU6050_Init()
         *   - Wrong DMA stream/channel for I2C1 RX
         *   - NVIC IRQ for DMA stream not enabled
         *   - ITBUFEN left enabled — RXNE ISR stealing DMA bytes
         *   - timeout value too small for CPU clock speed          */
        dbg_init_stage = 21U;
        return MPU6050_ERROR_I2C;
    }

    if (whoami_result != I2C_SVC_OK)
    {
        dbg_init_stage = 22U;
        dbg_svc_err    = (uint8_t)whoami_result;
        return MPU6050_ERROR_I2C;
    }

    dbg_whoami_val = whoami_buf[0U];

    if (whoami_buf[0U] != MPU6050_WHO_AM_I_VAL)
    {
        /* Common dbg_whoami_val values:
         *   0x00 = sensor not responding (SDA stuck low)
         *   0xFF = SDA stuck high (no pull-down / no device)
         *   0x68 = correct MPU6050
         *   0x69 = MPU6050 with AD0=VCC (address mismatch)
         *   0x71 = MPU9250 (not MPU6050)
         *   0x73 = MPU6500 variant                                 */
        dbg_init_stage = 23U;
        return MPU6050_ERROR_WHOAMI;
    }

    /* ────────────────────────────────────────────────────────────
     *  8. Configuration registers — device awake and verified.
     *     All writes use file-scope static cfg_buf.
     * ──────────────────────────────────────────────────────────── */

    /* Step A: Accel full-scale ±4g — ACCEL_CONFIG = 0x08 (AFS_SEL=1) */
    dbg_init_stage = 4U;

    cfg_buf[0U] = MPU6050_REG_ACCEL_CONFIG;   /* 0x1C                      */
    cfg_buf[1U] = 0x08U;                       /* ±4g → 8192 LSB/g          */
    svc_err = I2C_SVC_WriteReg(i2c_id, addr, cfg_buf, 2U, timeout);
    if (svc_err != I2C_SVC_OK)
    {
        dbg_init_stage = 30U;
        dbg_svc_err    = (uint8_t)svc_err;
        return MPU6050_ERROR_I2C;
    }

    /* Step B: Gyro full-scale ±500°/s — GYRO_CONFIG = 0x08 (FS_SEL=1) */
    dbg_init_stage = 5U;

    cfg_buf[0U] = MPU6050_REG_GYRO_CONFIG;   /* 0x1B                       */
    cfg_buf[1U] = 0x08U;                      /* ±500°/s → 65.5 LSB/°/s    */
    svc_err = I2C_SVC_WriteReg(i2c_id, addr, cfg_buf, 2U, timeout);
    if (svc_err != I2C_SVC_OK)
    {
        dbg_init_stage = 40U;
        dbg_svc_err    = (uint8_t)svc_err;
        return MPU6050_ERROR_I2C;
    }

    /* Step C: DLPF bandwidth 44 Hz — CONFIG = 0x03 (DLPF_CFG=3) */
    dbg_init_stage = 6U;

    cfg_buf[0U] = MPU6050_REG_CONFIG;   /* 0x1A                            */
    cfg_buf[1U] = 0x03U;                /* 44 Hz BW, 4.9 ms delay          */
    svc_err = I2C_SVC_WriteReg(i2c_id, addr, cfg_buf, 2U, timeout);
    if (svc_err != I2C_SVC_OK)
    {
        dbg_init_stage = 50U;
        dbg_svc_err    = (uint8_t)svc_err;
        return MPU6050_ERROR_I2C;
    }

    /* Step D: Sample rate 100 Hz — SMPLRT_DIV = 0x09 */
    dbg_init_stage = 7U;

    cfg_buf[0U] = MPU6050_REG_SMPLRT_DIV;   /* 0x19                        */
    cfg_buf[1U] = 0x09U;                     /* 1000 / (1+9) = 100 Hz       */
    svc_err = I2C_SVC_WriteReg(i2c_id, addr, cfg_buf, 2U, timeout);
    if (svc_err != I2C_SVC_OK)
    {
        dbg_init_stage = 60U;
        dbg_svc_err    = (uint8_t)svc_err;
        return MPU6050_ERROR_I2C;
    }

    /* ---- 9. Mark driver as ready ---- */
    mpu_initialized = 1U;
    mpu_busy        = 0U;

    dbg_init_stage = 99U;   /* SUCCESS */

    return MPU6050_OK;
}

/* ================================================================
 *  MPU6050_TriggerRead
 *  Non-blocking 14-byte DMA burst read starting at register 0x3B.
 *  Returns immediately — callback fires from DMA TC ISR.
 * ================================================================ */
MPU6050_Error_t MPU6050_TriggerRead(
    MPU6050_ReadDone_t callback,
    void              *ctx
)
{
    if (callback == NULL)
    {
        return MPU6050_ERROR_PARAM;
    }

    if (mpu_initialized != 1U)
    {
        return MPU6050_ERROR_INIT;
    }

    if (mpu_busy != 0U)
    {
        return MPU6050_ERROR_BUSY;
    }

    mpu_user_cb  = callback;
    mpu_user_ctx = ctx;

    mpu_busy = 1U;

    I2C_SVC_Error_t svc_err = I2C_SVC_ReadBurst_DMA(
        mpu_i2c_id,
        mpu_dev_addr,
        MPU6050_REG_ACCEL_XOUT_H,   /* 0x3B */
        mpu_rx_raw,                   /* static SRAM buffer */
        MPU6050_BURST_LEN,            /* 14 bytes */
        mpu_dma_done,
        NULL
    );

    if (svc_err != I2C_SVC_OK)
    {
        mpu_busy = 0U;
        return MPU6050_ERROR_I2C;
    }

    return MPU6050_OK;
}
