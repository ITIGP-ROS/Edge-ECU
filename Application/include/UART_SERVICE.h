#ifndef UART_SERVICE_H
#define UART_SERVICE_H

#include "STD_TYPES.h"
#include "UART_INTERFACE.h"
#include "STD_BUFFER.h"  /* changed: Buffer_t used in service-layer API signatures */
#include "DMA_INTERFACE.h"

/* ============================================================
 *  Ring buffer sizes — tune per instance if needed
 *  Must be power of 2 for efficient masking
 * ============================================================ */
#define UART_SVC_TX_BUF_SIZE  256U   /* TX ring buffer size per instance */
#define UART_SVC_RX_BUF_SIZE  256U   /* RX ring buffer size per instance */

#define UART_SVC_TX_MASK  (UART_SVC_TX_BUF_SIZE - 1U)
#define UART_SVC_RX_MASK  (UART_SVC_RX_BUF_SIZE - 1U)

/* ============================================================
 *  Error codes
 * ============================================================ */
typedef enum {
    UART_SVC_OK            = 0x00U,
    UART_SVC_ERROR_PARAM   = 0x01U,  /* NULL pointer or bad ID */
    UART_SVC_ERROR_FULL    = 0x02U,  /* TX ring buffer full */
    UART_SVC_ERROR_EMPTY   = 0x03U,  /* RX ring buffer empty */
    UART_SVC_ERROR_BUSY    = 0x04U,  /* TX already in progress */
    UART_SVC_ERROR_INIT    = 0x05U ,  /* Instance not initialized */
    UART_SVC_ERROR_DMA     = 0x06U,  /* DMA init or start failed */
    UART_SVC_ERROR_NO_DMA  = 0x07U   /* DMA TX requested but not configured */
} UART_SVC_Error_t;


/* TX transfer mode for the service layer */
typedef enum {
    UART_SVC_TX_MODE_IRQ = 0U,  /* IRQ-driven ring buffer TX (default) */
    UART_SVC_TX_MODE_DMA = 1U   /* DMA TX — bypasses ring buffer for large frames */
} UART_SVC_TxMode_t;

/* ============================================================
 *  Callback types
 * ============================================================ */

/**
 * @brief Callback invoked when a DMA TX transfer completes.
 *        Called from DMA interrupt context (ISR).
 * @param ctx  User-defined context pointer
 */
typedef void (*UART_SVC_TxDoneCb_t)(void *ctx);

/* ============================================================
 *  Public API
 * ============================================================ */

/**
 * @brief Initialize the UART service layer for one instance.
 * @param id        UART instance
 * @param priority  NVIC priority for this UART IRQ
 * @param tx_mode   IRQ or DMA TX mode
 * @param dma_id    DMA controller (ignored if tx_mode = IRQ)
 * @param dma_stream DMA stream (ignored if tx_mode = IRQ)
 * @param dma_ch    DMA channel (ignored if tx_mode = IRQ)
 * @return UART_SVC_OK on success
 */
UART_SVC_Error_t UART_SVC_Init(UART_Id_t        id,
                                uint8_t          priority,
                                UART_SVC_TxMode_t tx_mode,
                                DMA_Id_t          dma_id,
                                DMA_StreamId_t    dma_stream,
                                DMA_Channel_t     dma_ch);

/**
 * @brief Queue bytes into the TX ring buffer and start TX IRQ.
 *        Non-blocking — returns immediately after loading buffer.
 *        If TX is already running, new bytes are appended and
 *        will be sent automatically.
 * @param id    UART instance
 * @param buf   Data to transmit
 * @param len   Number of bytes
 * @return UART_SVC_OK, UART_SVC_ERROR_FULL if buffer overflows
 */
UART_SVC_Error_t UART_SVC_Transmit(UART_Id_t id,
                                    const Buffer_t *buf);  /* changed: Buffer_t replaces (buf, len) */

/**
 * @brief Transmit a buffer via DMA — non-blocking, fire and forget.
 *        Only available when UART_SVC_Init called with UART_SVC_TX_MODE_DMA.
 *        Caller must ensure buf->data remains valid until UART_SVC_IsTxIdle
 *        returns 1U — DMA reads directly from the caller's buffer.
 *        Do NOT call again until previous transfer is complete.
 * @param id   UART instance
 * @param buf  Buffer to transmit — buf->data and buf->length must be valid
 * @return UART_SVC_OK, UART_SVC_ERROR_BUSY if previous DMA not done,
 *         UART_SVC_ERROR_NO_DMA if instance not in DMA mode,
 *         UART_SVC_ERROR_PARAM if buf invalid
 */
UART_SVC_Error_t UART_SVC_TransmitDMA(UART_Id_t id, const Buffer_t *buf);                                    
/**
 * @brief Check how many bytes are waiting in the TX ring buffer.
 * @param id  UART instance
 * @return byte count pending in TX buffer
 */
UART_SVC_Error_t UART_SVC_TxPending(UART_Id_t id, uint16_t *pending);  /* changed: out-param avoids ambiguity with error codes */

/**
 * @brief Check if TX is fully complete (buffer empty + TC fired).
 * @param id  UART instance
 * @return 1U if idle, 0U if still transmitting
 */
UART_SVC_Error_t UART_SVC_IsTxIdle(UART_Id_t id, uint8_t *idle);  /* changed: out-param avoids ambiguity with error codes */

/**
 * @brief Read bytes from the RX ring buffer.
 *        Non-blocking — copies however many bytes are available
 *        up to len. Returns actual count copied.
 * @param id    UART instance
 * @param buf   Destination buffer
 * @param len   Maximum bytes to read
 * @return number of bytes actually copied (0 if buffer empty)
 */
UART_SVC_Error_t UART_SVC_Receive(UART_Id_t id, Buffer_t *buf);  /* changed: Buffer_t replaces (buf, len); returns error code */

/**
 * @brief Check how many bytes are available in the RX ring buffer.
 * @param id  UART instance
 * @return byte count available
 */
UART_SVC_Error_t UART_SVC_RxAvailable(UART_Id_t id, uint16_t *available);  /* changed: out-param avoids ambiguity with error codes */

/**
 * @brief Flush (discard) all bytes in the RX ring buffer.
 * @param id  UART instance
 * @return UART_SVC_OK
 */
UART_SVC_Error_t UART_SVC_FlushRx(UART_Id_t id);

/**
 * @brief Flush (discard) all bytes in the TX ring buffer.
 *        Also disables TXE IRQ. Use to abort a TX sequence.
 * @param id  UART instance
 * @return UART_SVC_OK
 */
UART_SVC_Error_t UART_SVC_FlushTx(UART_Id_t id);

/**
 * @brief Check if a UART error occurred since last clear.
 *        Errors are set by the IRQ handler on FE/NF/ORE/PE.
 * @param id  UART instance
 * @return 1U if error pending, 0U if clean
 */
UART_SVC_Error_t UART_SVC_IsError(UART_Id_t id, uint8_t *error);  /* changed: out-param avoids ambiguity with error codes */

/**
 * @brief Clear the error flag for this instance.
 * @param id  UART instance
 */
void UART_SVC_ClearError(UART_Id_t id);

/**
 * @brief Register a callback to be invoked when a DMA TX transfer completes.
 *        Only relevant for DMA-enabled instances.
 *        The callback is called from the DMA interrupt context (ISR).
 * @param id   UART instance
 * @param cb   Callback function pointer (NULL to unregister)
 * @param ctx  User-defined context pointer passed to cb
 * @return UART_SVC_OK on success, UART_SVC_ERROR_PARAM if id invalid
 */
UART_SVC_Error_t UART_SVC_RegisterTxDoneCb(UART_Id_t           id,
                                            UART_SVC_TxDoneCb_t cb,
                                            void               *ctx);

#endif /* UART_SERVICE_H */
