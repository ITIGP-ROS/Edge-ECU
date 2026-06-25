#ifndef I2C_SERVICE_H
#define I2C_SERVICE_H

#include "STD_TYPES.h"
#include "I2C_INTERFACE.h"
#include "DMA_INTERFACE.h"




/* ============================================================
 *  Error codes
 * ============================================================ */
typedef enum {
    I2C_SVC_OK            = 0x00U,
    I2C_SVC_ERROR_PARAM   = 0x01U,  /* NULL pointer or bad ID */
    I2C_SVC_ERROR_BUSY    = 0x02U,  /* transfer already in progress */
    I2C_SVC_ERROR_NACK    = 0x03U,  /* device did not ACK */
    I2C_SVC_ERROR_DMA     = 0x04U,  /* DMA init or start failed */
    I2C_SVC_ERROR_TIMEOUT = 0x05U,  /* blocking wait timed out */
    I2C_SVC_ERROR_INIT    = 0x06U   /* instance not initialized */
} I2C_SVC_Error_t;

/* ============================================================
 *  Internal state
 * ============================================================ */
typedef enum {
    I2C_SVC_STATE_IDLE   = 0x00U,
    I2C_SVC_STATE_TX     = 0x01U,   /* IRQ TX phase active */
    I2C_SVC_STATE_RX_DMA = 0x02U,   /* DMA RX phase active */
    I2C_SVC_STATE_ERROR  = 0x03U
} I2C_SVC_State_t;

/* ============================================================
 *  User callback — fired from DMA ISR on RX complete or error
 *  MUST be ISR-safe (short, no blocking calls)
 * ============================================================ */
typedef void (*I2C_SVC_Callback_t)(I2C_SVC_Error_t result, void *ctx);

/* ============================================================
 *  Public API
 * ============================================================ */

/**
 * @brief  Initialize the I2C service layer for one instance.
 *         Configures the DMA stream for RX, enables NVIC for
 *         both I2C EV/ER and the DMA stream. Call AFTER
 *         I2C_Init() and GPIO config for SCL/SDA.
 * @param  id               I2C instance (I2C_ID_1 / I2C_ID_2 / I2C_ID_3)
 * @param  i2c_ev_priority  NVIC priority for I2C event IRQ
 * @param  i2c_er_priority  NVIC priority for I2C error IRQ
 * @param  dma_id           DMA controller for RX (DMA_1 for I2C1)
 * @param  dma_stream       DMA stream for RX (DMA_STREAM_0 for I2C1)
 * @param  dma_channel      DMA channel for RX (DMA_CHANNEL_1 for I2C1)
 * @param  dma_priority     NVIC priority for DMA stream IRQ
 * @return I2C_SVC_OK on success
 */
I2C_SVC_Error_t I2C_SVC_Init(
    I2C_Id_t        id,
    uint8_t         i2c_ev_priority,
    uint8_t         i2c_er_priority,
    DMA_Id_t        dma_id,
    DMA_StreamId_t  dma_stream,
    DMA_Channel_t   dma_channel,
    uint8_t         dma_priority
);

/**
 * @brief  Blocking register write — IRQ TX, waits for completion.
 *         Used only at startup for sensor config writes.
 *         tx_buf layout: [reg_addr, value, ...] — caller fills it.
 *         Internally calls I2C_MasterTransmit_IT and polls
 *         I2C_GetXferStatus() with a decrement timeout.
 * @param  id        I2C instance
 * @param  dev_addr  7-bit device address (e.g. 0x68 for MPU6050)
 * @param  tx_buf    Bytes to transmit (register address + data)
 * @param  tx_len    Number of bytes (must be >= 1)
 * @param  timeout   Decrement counter — not milliseconds
 * @return I2C_SVC_OK on success; I2C_SVC_ERROR_NACK / TIMEOUT on failure
 */
I2C_SVC_Error_t I2C_SVC_WriteReg(
    I2C_Id_t        id,
    I2C_DevAddr7_t  dev_addr,
    const uint8_t  *tx_buf,
    uint16_t        tx_len,
    uint32_t        timeout
);

/**
 * @brief  Non-blocking DMA burst read — the 100 Hz MPU6050 path.
 *         Phase 1 (IRQ TX): sends reg_addr byte to slave with no STOP.
 *         Phase 2 (DMA RX): repeated START then DMA pulls rx_len bytes.
 *         Returns immediately after starting Phase 1.
 *         callback fires from DMA TC ISR when rx_buf is fully filled.
 *
 *         CALLER MUST keep rx_buf valid until callback fires.
 *         Do NOT call again until callback fires (busy guard enforced).
 *
 * @param  id        I2C instance
 * @param  dev_addr  7-bit device address
 * @param  reg_addr  Register to read from (e.g. 0x3B for MPU6050)
 * @param  rx_buf    Destination buffer — must be rx_len bytes long
 * @param  rx_len    Number of bytes to read (14 for MPU6050 burst)
 * @param  callback  Called from ISR when transfer is complete or errors
 * @param  ctx       Passed back to callback unchanged
 * @return I2C_SVC_OK if transfer started; I2C_SVC_ERROR_BUSY if previous
 *         transfer not yet complete; I2C_SVC_ERROR_PARAM if invalid args
 */
I2C_SVC_Error_t I2C_SVC_ReadBurst_DMA(
    I2C_Id_t           id,
    I2C_DevAddr7_t     dev_addr,
    uint8_t            reg_addr,
    uint8_t           *rx_buf,
    uint16_t           rx_len,
    I2C_SVC_Callback_t callback,
    void              *ctx
);

/**
 * @brief  Query current state of the service layer instance.
 * @param  id     I2C instance
 * @param  state  Output — current I2C_SVC_State_t
 * @return I2C_SVC_OK on success; I2C_SVC_ERROR_PARAM if state is NULL
 */
I2C_SVC_Error_t I2C_SVC_GetState(I2C_Id_t id, I2C_SVC_State_t *state);



#endif /* I2C_SERVICE_H */
