#ifndef UART_INTERFACE_H
#define UART_INTERFACE_H

#include "STD_TYPES.h"
#include "STD_BUFFER.h"  /* changed: Buffer_t used in polling transfer signatures */

/* ============================
 *   UART Error Codes
 * ============================ */
typedef enum {
    UART_OK                  = 0x00U,  /* U-04/U-05: explicit value — MISRA Rule 8.12 */
    UART_ERROR_INVALID_PARAM = 0x01U,  /* U-04/U-05: explicit value */
    UART_ERROR_INVALID_UART  = 0x02U,  /* U-04/U-05: explicit value */
    UART_ERROR_BUSY          = 0x03U,  /* U-04/U-05: explicit value */
    UART_ERROR_UNSUPPORTED   = 0x04U,  /* U-04/U-05: explicit value */
    UART_ERROR_TIMEOUT       = 0x05U,  /* U-05: added — polling operation timed out */
    UART_ERROR_OVERRUN       = 0x06U,  /* U-05: added — RX overrun error */
    UART_ERROR_FRAMING       = 0x07U,  /* U-05: added — framing error on RX */
    UART_ERROR_NOISE         = 0x08U,  /* U-05: added — noise flag on RX */
    UART_ERROR_PARITY        = 0x09U   /* U-05: added — parity error on RX */
} UART_Error_t;

/* ============================
 *   UART ID
 * ============================ */
typedef enum {
    UART1_ID = 0U,  /* U-04: explicit value */
    UART2_ID = 1U,  /* U-04: explicit value */
    UART6_ID = 2U   /* U-04: explicit value */
} UART_Id_t;

/* ============================
 *   Protocol Configuration
 * ============================ */
typedef enum {
    UART_PARITY_NONE = 0U,  /* U-04: explicit value */
    UART_PARITY_EVEN = 1U,  /* U-04: explicit value */
    UART_PARITY_ODD  = 2U   /* U-04: explicit value */
} UART_Parity_t;

typedef enum {
    UART_STOP_1 = 0U,  /* U-04: explicit value */
    UART_STOP_2 = 1U   /* U-04: explicit value */
} UART_StopBits_t;

typedef enum {
    UART_DATA_8BIT = 0U,  /* U-04: explicit value */
    UART_DATA_9BIT = 1U   /* U-04: explicit value */
} UART_DataBits_t;

typedef enum {
    UART_OVER8_DISABLE = 0U,  /* U-04: explicit value */
    UART_OVER8_ENABLE  = 1U   /* U-04: explicit value */
} UART_OverSampling_t;

typedef enum {
    UART_MODE_POLLING = 0U,  /* U-04: added U suffix to existing literal */
    UART_MODE_IRQ     = 1U,  /* U-04: explicit value */
    UART_MODE_DMA     = 2U   /* U-04: explicit value */
} UART_TransferMode_t;

/* ============================
 *   UART State
 * ============================ */
typedef enum {
    UART_STATE_IDLE      = 0U,  /* U-04: explicit value */
    UART_STATE_BUSY_TX   = 1U,  /* U-04: explicit value */
    UART_STATE_BUSY_RX   = 2U,  /* U-04: explicit value */
    UART_STATE_BUSY_TXRX = 3U   /* U-04: explicit value */
} UART_State_t;

/* ============================
 *   Baudrate
 * ============================ */
typedef uint32_t UART_Baudrate_t;

/* ============================
 *   UART Events (IRQ -> Upper Layer)
 * ============================ */
typedef enum {
    UART_EVENT_TXE   = 0U,  /* U-04: explicit value — TX data register empty */
    UART_EVENT_TC    = 1U,  /* U-04: explicit value — Transmission complete */
    UART_EVENT_RXNE  = 2U,  /* U-04: explicit value — RX data register not empty */
    UART_EVENT_IDLE  = 3U,  /* U-04: explicit value — Idle line detected */
    UART_EVENT_ERROR = 4U,  /* U-04: explicit value — FE / NE / ORE / PE */
    UART_EVENT_BREAK = 5U   /* U-04: explicit value — Break detection */
} UART_Event_t;

/* ============================
 *   Callback Type
 * ============================ */
typedef void (*UART_Callback_t)(UART_Event_t event, void *context);

/* ============================
 *   UART Configuration
 * ============================ */
typedef struct {
    UART_Id_t            uart_id;
    UART_Baudrate_t      baudrate;
    UART_Parity_t        parity;
    UART_StopBits_t      stop_bits;
    UART_DataBits_t      data_bits;
    UART_OverSampling_t  oversampling;
    UART_TransferMode_t  tx_mode;
    UART_TransferMode_t  rx_mode;
} UART_Config_t;

/* ============================
 *   Initialization & Control
 * ============================ */
UART_Error_t UART_Init(const UART_Config_t *cfg);

/* ============================
 *   Data Register Access
 * ============================ */
UART_Error_t UART_WriteDR(UART_Id_t uart, uint16_t data);
UART_Error_t UART_ReadDR(UART_Id_t uart, uint16_t *data);
uint32_t     UART_GetDRAddress(UART_Id_t uart);

/* ============================
 *   Status Flags (for polling)
 * ============================ */
UART_Error_t UART_IsTxEmpty(UART_Id_t uart, uint8_t *state);     /* changed: out-param avoids ambiguity with error codes */
UART_Error_t UART_IsRxNotEmpty(UART_Id_t uart, uint8_t *state);  /* changed: out-param avoids ambiguity with error codes */
UART_Error_t UART_IsTxComplete(UART_Id_t uart, uint8_t *state);  /* changed: out-param avoids ambiguity with error codes */

/* ============================
 *   Polling Transfer
 * ============================ */

/**
 * @brief Transmit buffer by polling TXE and TC flags
 * @param uart_id  UART instance
 * @param buf      Buffer descriptor — must pass BUFFER_IS_VALID; buf->length bytes sent
 * @param timeout  Simple decrement counter per TXE poll iteration (not ms)
 * @return UART_OK on success; UART_ERROR_INVALID_PARAM / UART_ERROR_INVALID_UART /
 *         UART_ERROR_TIMEOUT on failure
 */
UART_Error_t UART_Transmit_Polling(UART_Id_t uart_id,       /* changed: Buffer_t replaces (buf, len) */
                                    const Buffer_t *buf,
                                    uint32_t timeout);

/**
 * @brief Receive buffer by polling RXNE flag with error detection
 * @param uart_id  UART instance
 * @param buf      Buffer descriptor — data/size must be valid; buf->size bytes received
 * @param timeout  Simple decrement counter per RXNE poll iteration (not ms)
 * @return UART_OK on success; UART_ERROR_INVALID_PARAM / UART_ERROR_INVALID_UART /
 *         UART_ERROR_TIMEOUT / UART_ERROR_OVERRUN / UART_ERROR_FRAMING /
 *         UART_ERROR_NOISE / UART_ERROR_PARITY on failure
 */
UART_Error_t UART_Receive_Polling(UART_Id_t uart_id,        /* changed: Buffer_t replaces (buf, len) */
                                   Buffer_t *buf,
                                   uint32_t timeout);

/* ============================
 *   Interrupt Control
 * ============================ */
UART_Error_t UART_EnableTxIRQ(UART_Id_t uart);
UART_Error_t UART_DisableTxIRQ(UART_Id_t uart);

UART_Error_t UART_EnableTxCompleteIRQ(UART_Id_t uart);
UART_Error_t UART_DisableTxCompleteIRQ(UART_Id_t uart);

UART_Error_t UART_EnableRxIRQ(UART_Id_t uart);
UART_Error_t UART_DisableRxIRQ(UART_Id_t uart);

UART_Error_t UART_EnableIdleIRQ(UART_Id_t uart);
UART_Error_t UART_DisableIdleIRQ(UART_Id_t uart);

/* ============================
 *   DMA Request Control
 * ============================ */
UART_Error_t UART_EnableTxDMA(UART_Id_t uart);
UART_Error_t UART_DisableTxDMA(UART_Id_t uart);

UART_Error_t UART_EnableRxDMA(UART_Id_t uart);
UART_Error_t UART_DisableRxDMA(UART_Id_t uart);

/* ============================
 *   Callback Registration
 * ============================ */
UART_Error_t UART_RegisterCallback(
    UART_Id_t uart,
    UART_Callback_t cb,
    void *context
);

/* ============================
 *   State Query
 * ============================ */
UART_State_t UART_GetState(UART_Id_t uart);

/* ============================
 *   Error Utilities
 * ============================ */

/**
 * @brief Clear all UART error flags via SR+DR read sequence (RM0368 §19.3.4)
 * @param uart_id  UART instance
 * @return UART_OK on success; UART_ERROR_INVALID_UART if ID is invalid
 */
UART_Error_t UART_ClearErrors(UART_Id_t uart_id);  /* U-17: added */

/**
 * @brief Drain the RX shift register to prevent overrun (max 32 reads)
 * @param uart_id  UART instance
 * @return UART_OK on success; UART_ERROR_INVALID_UART if ID is invalid
 */
UART_Error_t UART_FlushRx(UART_Id_t uart_id);      /* U-18: added */

/**
 * @brief Check if any RX error flag (FE, NF, ORE, PE) is set in SR
 * @param uart_id  UART instance
 * @param state    Set to 1U if any error flag active, 0U if clean
 * @return UART_OK on success; UART_ERROR_INVALID_PARAM / UART_ERROR_INVALID_UART
 */
UART_Error_t UART_IsError(UART_Id_t uart_id, uint8_t *state);  /* changed: out-param avoids ambiguity with error codes */

/* ============================
 *   LIN Mode Control
 * ============================ */

/**
 * @brief Enable LIN mode on UART
 * @param uart  UART ID
 * @return      UART_OK on success
 * @note        Enables CR2.LINEN bit
 *              Sets 11-bit break detection (CR2.LBDL=1)
 */
UART_Error_t UART_EnableLINMode(UART_Id_t uart);

/**
 * @brief Disable LIN mode on UART
 * @param uart  UART ID
 * @return      UART_OK on success
 */
UART_Error_t UART_DisableLINMode(UART_Id_t uart);

/**
 * @brief Send break field (Master mode)
 * @param uart  UART ID
 * @return      UART_OK on success
 * @note        Sets CR1.SBK bit, hardware clears after break sent
 *              Break is 13-bit LOW for LIN
 */
UART_Error_t UART_SendBreak(UART_Id_t uart);

/**
 * @brief Enable break detection interrupt (Slave mode)
 * @param uart  UART ID
 * @return      UART_OK on success
 * @note        Enables CR2.LBDIE bit
 */
UART_Error_t UART_EnableBreakIRQ(UART_Id_t uart);

/**
 * @brief Disable break detection interrupt
 * @param uart  UART ID
 * @return      UART_OK on success
 */
UART_Error_t UART_DisableBreakIRQ(UART_Id_t uart);

/**
 * @brief Check if break was detected
 * @param uart   UART ID
 * @param state  Set to 1U if break detected, 0U otherwise
 * @return UART_OK on success; UART_ERROR_INVALID_PARAM / UART_ERROR_INVALID_UART
 */
UART_Error_t UART_IsBreakDetected(UART_Id_t uart, uint8_t *state);  /* changed: out-param avoids ambiguity with error codes */

/**
 * @brief Clear break detection flag
 * @param uart  UART ID
 * @note        Writes 0 to SR.LBD bit
 */
void UART_ClearBreakFlag(UART_Id_t uart);

#endif /* UART_INTERFACE_H */
