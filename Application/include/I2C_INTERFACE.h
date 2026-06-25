#ifndef I2C_INTERFACE_H
#define I2C_INTERFACE_H

#include "STD_TYPES.h"

/**
 * @file    I2C_INTERFACE.h
 * @brief   I2C Driver Interface for STM32F401xx
 *
 * Fixes applied:
 *   I2C-I01  Explicit U-suffixed values on every enumerator (MISRA Rule 8.12).
 *   I2C-I02  Status flag functions return I2C_Error_t with uint8_t *state out-param.
 *   I2C-I03  Added I2C_GetStatus() prototype (implemented in I2C.c but was missing).
 *   I2C-I04  No change required (structs already well-typed).
 *   I2C-I05  Removed unused I2C_Direction_t enum.
 *   I2C-I06  Added all six IRQ handler prototypes.
 */

/* ========================================================= */
/* ===================== I2C Instances ===================== */
/* ========================================================= */

typedef enum
{
    I2C_ID_1 = 0U,   /* I2C-I01: explicit U-suffixed value */
    I2C_ID_2 = 1U,   /* I2C-I01: explicit U-suffixed value */
    I2C_ID_3 = 2U    /* I2C-I01: explicit U-suffixed value */
} I2C_Id_t;

/* ========================================================= */
/* ================= Speed Mode ============================ */
/* ========================================================= */

typedef enum
{
    I2C_SPEED_STANDARD = 0U,  /* I2C-I01: 100 kHz */
    I2C_SPEED_FAST     = 1U   /* I2C-I01: 400 kHz */
} I2C_Speed_t;

/* ========================================================= */
/* ================= Duty Cycle (Fast Mode) ================ */
/* ========================================================= */

typedef enum
{
    I2C_DUTY_2    = 0U,   /* I2C-I01: Tlow/Thigh = 2   */
    I2C_DUTY_16_9 = 1U    /* I2C-I01: Tlow/Thigh = 16/9 */
} I2C_DutyCycle_t;

/* ========================================================= */
/* ================= Addressing Mode ======================= */
/* ========================================================= */

typedef enum
{
    I2C_ADDR_7BIT  = 0U,  /* I2C-I01: explicit U-suffixed value */
    I2C_ADDR_10BIT = 1U   /* I2C-I01: explicit U-suffixed value */
} I2C_AddressingMode_t;

/* ========================================================= */
/* ================= Dual Address Mode ===================== */
/* ========================================================= */

typedef enum
{
    I2C_DUAL_ADDR_DISABLE = 0U,  /* I2C-I01: explicit U-suffixed value */
    I2C_DUAL_ADDR_ENABLE  = 1U   /* I2C-I01: explicit U-suffixed value */
} I2C_DualAddr_t;

/* ========================================================= */
/* ================= General Call Mode ===================== */
/* ========================================================= */

typedef enum
{
    I2C_GENERAL_CALL_DISABLE = 0U,  /* I2C-I01: explicit U-suffixed value */
    I2C_GENERAL_CALL_ENABLE  = 1U   /* I2C-I01: explicit U-suffixed value */
} I2C_GeneralCall_t;

/* ========================================================= */
/* ================= Clock Stretching ====================== */
/* ========================================================= */

typedef enum
{
    I2C_CLOCK_STRETCH_ENABLE  = 0U,  /* I2C-I01: NOSTRETCH = 0 */
    I2C_CLOCK_STRETCH_DISABLE = 1U   /* I2C-I01: NOSTRETCH = 1 */
} I2C_ClockStretch_t;

/* ========================================================= */
/* ================= Analog Filter ========================= */
/* ========================================================= */

typedef enum
{
    I2C_ANALOG_FILTER_ENABLE  = 0U,  /* I2C-I01: ANOFF = 0 */
    I2C_ANALOG_FILTER_DISABLE = 1U   /* I2C-I01: ANOFF = 1 */
} I2C_AnalogFilter_t;

/* ========================================================= */
/* ================= Digital Filter ======================== */
/* ========================================================= */

typedef enum
{
    I2C_DIGITALFILTER_0  = 0U,   /* I2C-I01: explicit U-suffixed value */
    I2C_DIGITALFILTER_1  = 1U,   /* I2C-I01: explicit U-suffixed value */
    I2C_DIGITALFILTER_2  = 2U,   /* I2C-I01: explicit U-suffixed value */
    I2C_DIGITALFILTER_3  = 3U,   /* I2C-I01: explicit U-suffixed value */
    I2C_DIGITALFILTER_4  = 4U,   /* I2C-I01: explicit U-suffixed value */
    I2C_DIGITALFILTER_5  = 5U,   /* I2C-I01: explicit U-suffixed value */
    I2C_DIGITALFILTER_6  = 6U,   /* I2C-I01: explicit U-suffixed value */
    I2C_DIGITALFILTER_7  = 7U,   /* I2C-I01: explicit U-suffixed value */
    I2C_DIGITALFILTER_8  = 8U,   /* I2C-I01: explicit U-suffixed value */
    I2C_DIGITALFILTER_9  = 9U,   /* I2C-I01: explicit U-suffixed value */
    I2C_DIGITALFILTER_10 = 10U,  /* I2C-I01: explicit U-suffixed value */
    I2C_DIGITALFILTER_11 = 11U,  /* I2C-I01: explicit U-suffixed value */
    I2C_DIGITALFILTER_12 = 12U,  /* I2C-I01: explicit U-suffixed value */
    I2C_DIGITALFILTER_13 = 13U,  /* I2C-I01: explicit U-suffixed value */
    I2C_DIGITALFILTER_14 = 14U,  /* I2C-I01: explicit U-suffixed value */
    I2C_DIGITALFILTER_15 = 15U   /* I2C-I01: explicit U-suffixed value */
} I2C_DigitalFilter_t;

/* ========================================================= */
/* ================= Transfer Mode ========================= */
/* ========================================================= */

typedef enum
{
    I2C_MODE_POLLING = 0U,  /* I2C-I01: explicit U-suffixed value */
    I2C_MODE_IRQ     = 1U,  /* I2C-I01: explicit U-suffixed value */
    I2C_MODE_DMA     = 2U   /* I2C-I01: explicit U-suffixed value */
} I2C_TransferMode_t;

/* ========================================================= */
/* ================= I2C State ============================= */
/* ========================================================= */

typedef enum
{
    I2C_STATE_IDLE      = 0U,  /* I2C-I01: explicit U-suffixed value */
    I2C_STATE_BUSY_TX   = 1U,  /* I2C-I01: explicit U-suffixed value */
    I2C_STATE_BUSY_RX   = 2U,  /* I2C-I01: explicit U-suffixed value */
    I2C_STATE_BUSY_TXRX = 3U   /* I2C-I01: explicit U-suffixed value */
} I2C_State_t;

/* Transfer result/status for async */
typedef enum
{
    I2C_STATUS_IDLE  = 0U,  /* I2C-I01: explicit U-suffixed value */
    I2C_STATUS_BUSY  = 1U,  /* I2C-I01: explicit U-suffixed value */
    I2C_STATUS_DONE  = 2U,  /* I2C-I01: explicit U-suffixed value */
    I2C_STATUS_ERROR = 3U   /* I2C-I01: explicit U-suffixed value */
} I2C_Status_t;

/* ========================================================= */
/* ================= I2C Errors ============================ */
/* ========================================================= */

typedef enum
{
    I2C_OK                  = 0x00U,  /* I2C-I01: explicit U-suffixed value */
    I2C_ERROR_INVALID_PARAM = 0x01U,  /* I2C-I01: explicit U-suffixed value */
    I2C_ERROR_INVALID_I2C   = 0x02U,  /* I2C-I01: explicit U-suffixed value */
    I2C_ERROR_BUSY          = 0x03U,  /* I2C-I01: explicit U-suffixed value */
    I2C_ERROR_UNSUPPORTED   = 0x04U,  /* I2C-I01: explicit U-suffixed value */
    I2C_ERROR_NACK          = 0x05U,  /* I2C-I01: explicit U-suffixed value */
    I2C_ERROR_BERR          = 0x06U,  /* I2C-I01: Bus error                 */
    I2C_ERROR_ARLO          = 0x07U,  /* I2C-I01: Arbitration lost          */
    I2C_ERROR_OVR           = 0x08U,  /* I2C-I01: Overrun/Underrun          */
    I2C_ERROR_TIMEOUT       = 0x09U   /* I2C-I01: explicit U-suffixed value */
} I2C_Error_t;

/* ========================================================= */
/* ================= I2C Events ============================ */
/* ========================================================= */

typedef enum
{
    I2C_EVENT_TXE         = 0U,   /* I2C-I01: TX data register empty       */
    I2C_EVENT_RXNE        = 1U,   /* I2C-I01: RX data register not empty   */
    I2C_EVENT_SB          = 2U,   /* I2C-I01: Start bit generated          */
    I2C_EVENT_ADDR        = 3U,   /* I2C-I01: Address sent/matched         */
    I2C_EVENT_BTF         = 4U,   /* I2C-I01: Byte transfer finished       */
    I2C_EVENT_STOPF       = 5U,   /* I2C-I01: Stop detected (slave)        */
    I2C_EVENT_TX_COMPLETE = 6U,   /* I2C-I01: TX transfer complete         */
    I2C_EVENT_RX_COMPLETE = 7U,   /* I2C-I01: RX transfer complete         */
    I2C_EVENT_ERROR       = 8U    /* I2C-I01: NACK / BERR / ARLO / OVR    */
} I2C_Event_t;

/* ========================================================= */
/* ================= Stronger Types ======================== */
/* ========================================================= */

/* 7-bit I2C device address (0..127). Keep as uint16_t for compatibility. */
typedef uint16_t I2C_DevAddr7_t;

/* Timeout in ticks. */
typedef uint32_t I2C_Timeout_t;

typedef enum
{
    I2C_STOP_DISABLE = 0U,  /* I2C-I01: explicit U-suffixed value */
    I2C_STOP_ENABLE  = 1U   /* I2C-I01: explicit U-suffixed value */
} I2C_Stop_t;

/* I2C-I05: Removed I2C_Direction_t — was defined but never used in driver code.
 *          I2C_SendAddress() uses raw 0U/1U literals internally.
 *          Either use the enum inside I2C_SendAddress or remove it.
 *          Decision: removed to avoid dead / misleading API surface.          */

/* ========================================================= */
/* ================= Transaction Descriptors =============== */
/* ========================================================= */

/* I2C-I04: Already well-typed structs — no change required */

typedef struct
{
    I2C_DevAddr7_t  dev_addr;   /* 7-bit address */
    const uint8_t  *tx_buf;
    uint16_t        tx_len;
    I2C_Stop_t      stop;       /* stop after tx */
} I2C_TxRequest_t;

typedef struct
{
    I2C_DevAddr7_t  dev_addr;   /* 7-bit address */
    uint8_t        *rx_buf;
    uint16_t        rx_len;
    I2C_Stop_t      stop;       /* stop after rx */
} I2C_RxRequest_t;

/* Combined: TX then repeated-start then RX */
typedef struct
{
    I2C_DevAddr7_t  dev_addr;
    const uint8_t  *tx_buf;
    uint16_t        tx_len;
    uint8_t        *rx_buf;
    uint16_t        rx_len;
} I2C_TxRxRequest_t;

/* ========================================================= */
/* ================= Callback Type ========================= */
/* ========================================================= */

typedef void (*I2C_Callback_t)(I2C_Event_t event, void *context);

/* ========================================================= */
/* ================= I2C Configuration ===================== */
/* ========================================================= */

typedef struct
{
    I2C_Id_t              id;

    /* Clock Configuration */
    I2C_Speed_t           speed;
    I2C_DutyCycle_t       duty_cycle;        /* Fast mode only */

    /* Address Configuration */
    I2C_AddressingMode_t  addressing_mode;
    uint16_t              own_address1;      /* Primary slave address */
    uint16_t              own_address2;      /* Secondary slave address (7-bit only) */
    I2C_DualAddr_t        dual_address_mode;
    I2C_GeneralCall_t     general_call_mode;

    /* Slave Configuration */
    I2C_ClockStretch_t    clock_stretching;

    /* Filter Configuration */
    I2C_AnalogFilter_t    analog_filter;
    I2C_DigitalFilter_t   digital_filter;    /* 0-15 */

    /* Transfer Mode */
    I2C_TransferMode_t    transfer_mode;

} I2C_Config_t;

/* ========================================================= */
/* ================= Initialization ======================== */
/* ========================================================= */

I2C_Error_t I2C_Init(const I2C_Config_t *cfg);

/* ========================================================= */
/* ================= Data Register Access ================== */
/* ========================================================= */

I2C_Error_t I2C_WriteDR(I2C_Id_t i2c_id, uint8_t data);
I2C_Error_t I2C_ReadDR(I2C_Id_t i2c_id, uint8_t *data);
uint32_t    I2C_GetDRAddress(I2C_Id_t i2c_id);

/* ========================================================= */
/* ================= Status Flags (for polling) ============ */
/* ========================================================= */

/* I2C-I02: All status flag functions now return I2C_Error_t with a
 *          uint8_t *state output parameter. Previously returned raw uint8_t
 *          where 0 was ambiguous between "flag not set" and "invalid ID".   */

/**
 * @brief Check if TX data register is empty
 * @param i2c_id  I2C instance
 * @param state   Set to 1U if TXE=1, 0U otherwise
 * @return I2C_OK on success; I2C_ERROR_INVALID_PARAM / I2C_ERROR_INVALID_I2C
 */
I2C_Error_t I2C_IsTxEmpty(I2C_Id_t i2c_id, uint8_t *state);

/**
 * @brief Check if RX data register is not empty
 * @param i2c_id  I2C instance
 * @param state   Set to 1U if RXNE=1, 0U otherwise
 * @return I2C_OK on success; I2C_ERROR_INVALID_PARAM / I2C_ERROR_INVALID_I2C
 */
I2C_Error_t I2C_IsRxNotEmpty(I2C_Id_t i2c_id, uint8_t *state);

/**
 * @brief Check if I2C bus is busy
 * @param i2c_id  I2C instance
 * @param state   Set to 1U if BUSY=1, 0U otherwise
 * @return I2C_OK on success; I2C_ERROR_INVALID_PARAM / I2C_ERROR_INVALID_I2C
 */
I2C_Error_t I2C_IsBusy(I2C_Id_t i2c_id, uint8_t *state);

/**
 * @brief Check if START condition has been generated
 * @param i2c_id  I2C instance
 * @param state   Set to 1U if SB=1, 0U otherwise
 * @return I2C_OK on success; I2C_ERROR_INVALID_PARAM / I2C_ERROR_INVALID_I2C
 */
I2C_Error_t I2C_IsStartGenerated(I2C_Id_t i2c_id, uint8_t *state);

/**
 * @brief Check if address has been sent/matched
 * @param i2c_id  I2C instance
 * @param state   Set to 1U if ADDR=1, 0U otherwise
 * @return I2C_OK on success; I2C_ERROR_INVALID_PARAM / I2C_ERROR_INVALID_I2C
 */
I2C_Error_t I2C_IsAddrSent(I2C_Id_t i2c_id, uint8_t *state);

/**
 * @brief Check if byte transfer is finished
 * @param i2c_id  I2C instance
 * @param state   Set to 1U if BTF=1, 0U otherwise
 * @return I2C_OK on success; I2C_ERROR_INVALID_PARAM / I2C_ERROR_INVALID_I2C
 */
I2C_Error_t I2C_IsBTF(I2C_Id_t i2c_id, uint8_t *state);

/**
 * @brief Check if STOP condition has been detected (Slave mode)
 * @param i2c_id  I2C instance
 * @param state   Set to 1U if STOPF=1, 0U otherwise
 * @return I2C_OK on success; I2C_ERROR_INVALID_PARAM / I2C_ERROR_INVALID_I2C
 */
I2C_Error_t I2C_IsStopDetected(I2C_Id_t i2c_id, uint8_t *state);

/* ========================================================= */
/* ================= Bus Control =========================== */
/* ========================================================= */

I2C_Error_t I2C_GenerateStart(I2C_Id_t i2c_id);
I2C_Error_t I2C_GenerateStop(I2C_Id_t i2c_id);
I2C_Error_t I2C_SendAddress(I2C_Id_t i2c_id, uint16_t addr, uint8_t is_read);
I2C_Error_t I2C_ClearAddrFlag(I2C_Id_t i2c_id);
I2C_Error_t I2C_EnableACK(I2C_Id_t i2c_id);
I2C_Error_t I2C_DisableACK(I2C_Id_t i2c_id);

/* ========================================================= */
/* ================= Interrupt Control ===================== */
/* ========================================================= */

I2C_Error_t I2C_EnableEventIRQ(I2C_Id_t i2c_id);
I2C_Error_t I2C_DisableEventIRQ(I2C_Id_t i2c_id);

I2C_Error_t I2C_EnableBufferIRQ(I2C_Id_t i2c_id);
I2C_Error_t I2C_DisableBufferIRQ(I2C_Id_t i2c_id);

I2C_Error_t I2C_EnableErrorIRQ(I2C_Id_t i2c_id);
I2C_Error_t I2C_DisableErrorIRQ(I2C_Id_t i2c_id);

/* ========================================================= */
/* ================= DMA Request Control =================== */
/* ========================================================= */

I2C_Error_t I2C_EnableDMA(I2C_Id_t i2c_id);
I2C_Error_t I2C_DisableDMA(I2C_Id_t i2c_id);

I2C_Error_t I2C_EnableDMALast(I2C_Id_t i2c_id);
I2C_Error_t I2C_DisableDMALast(I2C_Id_t i2c_id);

/* ========================================================= */
/* ================= Callback Registration ================= */
/* ========================================================= */

I2C_Error_t I2C_RegisterCallback(
    I2C_Id_t i2c_id,
    I2C_Callback_t cb,
    void *context
);

/* ========================================================= */
/* ================= State & Status Query ================== */
/* ========================================================= */

I2C_State_t I2C_GetState(I2C_Id_t i2c_id);

/* I2C-I03: Added — implemented in I2C.c (line 313) but had no prototype here.
 *          I2C_GetXferStatus and I2C_GetStatus are two different functions
 *          with different semantics; both must be declared.                    */

/**
 * @brief  Get the current transfer status of an I2C instance
 * @param  id  I2C instance
 * @return Current I2C_Status_t value
 */
I2C_Status_t I2C_GetStatus(I2C_Id_t id);

/* ========================================================= */
/* =========== Master Transactions (IRQ / StateMachine) ===== */
/* ========================================================= */

I2C_Error_t I2C_MasterTransmit_IT(I2C_Id_t id, const I2C_TxRequest_t *tx);
I2C_Error_t I2C_MasterReceive_IT(I2C_Id_t id, const I2C_RxRequest_t *rx);
I2C_Error_t I2C_MasterWriteRead_IT(I2C_Id_t id, const I2C_TxRxRequest_t *xfer);

I2C_Error_t I2C_Abort_IT(I2C_Id_t id);

I2C_Error_t I2C_ResetHandle(I2C_Id_t id);

/* Optional: query last error and transfer status */
I2C_Status_t I2C_GetXferStatus(I2C_Id_t id);
I2C_Error_t  I2C_GetLastError(I2C_Id_t id);

/* ========================================================= */
/* ================= IRQ Handler Prototypes ================ */
/* ========================================================= */

/* I2C-I06: All six IRQ handlers are defined in I2C.c but had no prototypes
 *          in this header. Declared here for startup-file and linker visibility. */

void I2C1_EV_IRQHandler(void);
void I2C1_ER_IRQHandler(void);
void I2C2_EV_IRQHandler(void);
void I2C2_ER_IRQHandler(void);
void I2C3_EV_IRQHandler(void);
void I2C3_ER_IRQHandler(void);

#endif /* I2C_INTERFACE_H */