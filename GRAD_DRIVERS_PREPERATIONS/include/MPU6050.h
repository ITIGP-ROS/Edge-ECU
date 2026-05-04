#ifndef MPU6050_H
#define MPU6050_H

#include "STD_TYPES.h"
#include "I2C_SERVICE.h"

/* ============================================================
 *  I2C addresses
 * ============================================================ */
#define MPU6050_ADDR_LOW    0x68U   /* AD0 = GND (default) */
#define MPU6050_ADDR_HIGH   0x69U   /* AD0 = VCC           */

/* ============================================================
 *  Register map (from MPU6050 datasheet + engineering ref)
 * ============================================================ */
#define MPU6050_REG_SMPLRT_DIV    0x19U  /* Sample rate divider         */
#define MPU6050_REG_CONFIG        0x1AU  /* DLPF configuration          */
#define MPU6050_REG_GYRO_CONFIG   0x1BU  /* Gyro full-scale range       */
#define MPU6050_REG_ACCEL_CONFIG  0x1CU  /* Accel full-scale range      */
#define MPU6050_REG_ACCEL_XOUT_H  0x3BU  /* Burst read start — 14 bytes */
#define MPU6050_REG_PWR_MGMT_1   0x6BU  /* Power management 1          */
#define MPU6050_REG_WHO_AM_I      0x75U  /* Device identity register    */

/* ============================================================
 *  Expected WHO_AM_I response
 * ============================================================ */
#define MPU6050_WHO_AM_I_VAL      0x68U  /* Fixed device ID byte        */

/* ============================================================
 *  Scaling constants (document §3.5)
 * ============================================================ */
#define MPU6050_ACCEL_SCALE       8192.0f  /* LSB per g  at ±4g range   */
#define MPU6050_GYRO_SCALE        65.5f    /* LSB per °/s at ±500°/s    */
#define MPU6050_BURST_LEN         14U      /* bytes in one sensor burst  */

/* ============================================================
 *  Error codes
 * ============================================================ */
typedef enum {
    MPU6050_OK             = 0x00U,
    MPU6050_ERROR_PARAM    = 0x01U,  /* NULL pointer or bad argument   */
    MPU6050_ERROR_I2C      = 0x02U,  /* I2C service layer returned err */
    MPU6050_ERROR_WHOAMI   = 0x03U,  /* WHO_AM_I mismatch              */
    MPU6050_ERROR_BUSY     = 0x04U,  /* previous read not yet complete */
    MPU6050_ERROR_INIT     = 0x05U   /* driver not initialised         */
} MPU6050_Error_t;

/* ============================================================
 *  Raw sensor data — direct from 14-byte burst
 *  All values big-endian on wire; parsed to host int16_t here
 * ============================================================ */
typedef struct {
    sint16_t accel_x;   /* Accel X — divide by 8192.0f for g  */
    sint16_t accel_y;   /* Accel Y                             */
    sint16_t accel_z;   /* Accel Z — should read ~8192 at rest */
    sint16_t temp_raw;  /* Temperature — not used for CNN      */
    sint16_t gyro_x;    /* Gyro X — divide by 65.5f for °/s   */
    sint16_t gyro_y;    /* Gyro Y                              */
    sint16_t gyro_z;    /* Gyro Z                              */
} MPU6050_RawData_t;

/* ============================================================
 *  Callback type — fired from DMA ISR when read is complete
 *  MUST be ISR-safe: short, no blocking, no FreeRTOS API
 *  except xSemaphoreGiveFromISR / portYIELD_FROM_ISR
 * ============================================================ */
typedef void (*MPU6050_ReadDone_t)(const MPU6050_RawData_t *data, void *ctx);

/* ============================================================
 *  Public API
 * ============================================================ */

/**
 * @brief  Initialise the MPU6050 driver.
 *         1. Validates WHO_AM_I register (0x75) == 0x68.
 *         2. Writes 5 configuration registers per the engineering
 *            reference document §3.2 init sequence.
 *         3. Marks the driver as ready for TriggerRead calls.
 *         Blocking — must only be called at startup, not in a task loop.
 *
 * @param  i2c_id   I2C instance (I2C_ID_1 for PB6/PB7)
 * @param  addr     Device address (MPU6050_ADDR_LOW or HIGH)
 * @param  timeout  Decrement counter for each blocking I2C write
 * @return MPU6050_OK on success
 *         MPU6050_ERROR_WHOAMI if identity check fails
 *         MPU6050_ERROR_I2C    if any register write fails
 */
MPU6050_Error_t MPU6050_Init(
    I2C_Id_t        i2c_id,
    I2C_DevAddr7_t  addr,
    uint32_t        timeout
);

/**
 * @brief  Trigger a non-blocking 14-byte DMA burst read.
 *         Sends register address 0x3B to MPU6050, then DMA
 *         pulls 14 bytes into an internal static buffer.
 *         callback fires from DMA ISR when all 14 bytes are ready.
 *         Parsed MPU6050_RawData_t is passed to callback — caller
 *         must NOT free or modify the pointed-to data inside the ISR.
 *
 *         Call pattern (Thread 1, 100 Hz):
 *           MPU6050_TriggerRead(cb, ctx);
 *           xSemaphoreTake(dma_done_sem, portMAX_DELAY);
 *           // cb already gave the semaphore
 *
 * @param  callback  ISR-safe function to call when data is ready
 * @param  ctx       Opaque pointer passed back to callback unchanged
 * @return MPU6050_OK if transfer started
 *         MPU6050_ERROR_BUSY if previous read not yet complete
 *         MPU6050_ERROR_INIT if MPU6050_Init not called first
 *         MPU6050_ERROR_I2C  if service layer rejects the request
 */
MPU6050_Error_t MPU6050_TriggerRead(
    MPU6050_ReadDone_t callback,
    void              *ctx
);

#endif /* MPU6050_H */
