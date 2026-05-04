#include "UART_INTERFACE.h"
#include "UART_REGS.h"
#include "RCC_INTERFACE.h"
#include "STD_BUFFER.h"  /* changed: Buffer_t used in polling transfer implementations */

/* ============================================================
 *  Constants
 * ============================================================ */
#define UART_MAX_INSTANCES  3U

/* ============================================================
 *  Helpers
 * ============================================================ */
#define UART_REG(id)                                             \
    ( ((id) == UART1_ID) ? UART1 :                               \
      ((id) == UART2_ID) ? UART2 :                               \
      ((id) == UART6_ID) ? UART6 :                               \
      (volatile UART_REGS_t *)0 )    /* U-02: cast updated to volatile to match macro type */

static inline uint8_t UART_Index(UART_Id_t id)
{
    return (uint8_t)id; /* UART1=0, UART2=1, UART6=2 */
}

/* ============================================================
 *  UART internal context
 * ============================================================ */
typedef struct {
    UART_TransferMode_t tx_mode;
    UART_TransferMode_t rx_mode;
    UART_State_t        state;
    UART_Callback_t     callback;
    void               *callback_ctx;
} UART_Context_t;

static UART_Context_t uart_ctx[UART_MAX_INSTANCES] = {
    { .tx_mode = UART_MODE_POLLING, .rx_mode = UART_MODE_POLLING, .state = UART_STATE_IDLE, .callback = NULL, .callback_ctx = NULL },
    { .tx_mode = UART_MODE_POLLING, .rx_mode = UART_MODE_POLLING, .state = UART_STATE_IDLE, .callback = NULL, .callback_ctx = NULL },
    { .tx_mode = UART_MODE_POLLING, .rx_mode = UART_MODE_POLLING, .state = UART_STATE_IDLE, .callback = NULL, .callback_ctx = NULL }
};

/* ============================================================
 *  Baudrate helper
 * ============================================================ */
static UART_Error_t UART_setBRR(
    volatile UART_REGS_t *uart,        /* U-02: volatile — matches updated macro type */
    UART_Id_t uart_id,
    UART_Baudrate_t baudrate,
    UART_OverSampling_t oversampling
)
{
    RCC_Peripheral_t peri;
    uint32_t fck;

    switch (uart_id) {
        case UART1_ID: peri = PERIPH_USART1; break;
        case UART2_ID: peri = PERIPH_USART2; break;
        case UART6_ID: peri = PERIPH_USART6; break;
        default: return UART_ERROR_INVALID_UART;
    }

    if (GET_PCLK_FREQ(peri, &fck) != RCC_OK) {
        return UART_ERROR_UNSUPPORTED;
    }

    /* Configure oversampling */
    uart->CR1.BITS.OVER8 =
        (oversampling == UART_OVER8_ENABLE) ? 1U : 0U;

    uint32_t usartdiv;
    uint32_t mantissa;
    uint32_t fraction;

    if (uart->CR1.BITS.OVER8 == 0U)
    {
        /* Oversampling by 16 */
        usartdiv = (fck + baudrate / 2U) / baudrate;

        mantissa = usartdiv / 16U;
        fraction = usartdiv % 16U;
    }
    else
    {
        /* Oversampling by 8 */
        usartdiv = (2U * fck + baudrate / 2U) / baudrate;

        mantissa = usartdiv / 16U;
        /* U-12: OVER8=1: fraction is 3 bits only (BRR[2:0]), BRR[3] must be 0.
           Correct encoding: take modulo 8, shift left by 1 to place in bits[3:1].
           Previous code used (% 16U) & 0xEU which gave wrong baud at OVER8=1. */
        fraction = ((usartdiv % 8U) & 0x7U) << 1U;   /* U-12: fixed fraction formula */
    }

    uart->BRR.ALL = (mantissa << 4U) | fraction;   /* U-MISRA: U suffix on shift constant */

    return UART_OK;
}

/* ============================================================
 *  Initialization
 *  NOTE: GPIO must be configured by upper layer (HSerial/App)
 *        before calling UART_Init
 * ============================================================ */
UART_Error_t UART_Init(const UART_Config_t *cfg)
{
    volatile UART_REGS_t *uart;    /* U-02: volatile — receives pointer from updated macros */

    if (cfg == NULL) {
        return UART_ERROR_INVALID_PARAM;
    }

    switch (cfg->uart_id)
    {
        case UART1_ID:
            uart = UART1;
            break;

        case UART2_ID:
            uart = UART2;
            break;

        case UART6_ID:
            uart = UART6;
            break;

        default:
            return UART_ERROR_INVALID_UART;
    }

    /* Disable UART for configuration */
    uart->CR1.BITS.UE = 0U;

    /* Frame config */
    uart->CR1.BITS.M   = (cfg->data_bits == UART_DATA_9BIT);
    uart->CR1.BITS.PCE = (cfg->parity != UART_PARITY_NONE);
    uart->CR1.BITS.PS  = (cfg->parity == UART_PARITY_ODD);
    uart->CR2.BITS.STOP =
        (cfg->stop_bits == UART_STOP_2) ? 2U : 0U;

    /* Baudrate */
    if (UART_setBRR(
        uart,
        cfg->uart_id,
        cfg->baudrate,
        cfg->oversampling
    ) != UART_OK) {
        return UART_ERROR_UNSUPPORTED;
    }

    /* Save modes and initialize state */
    uart_ctx[UART_Index(cfg->uart_id)].tx_mode = cfg->tx_mode;
    uart_ctx[UART_Index(cfg->uart_id)].rx_mode = cfg->rx_mode;
    uart_ctx[UART_Index(cfg->uart_id)].state = UART_STATE_IDLE;
    uart_ctx[UART_Index(cfg->uart_id)].callback = NULL;
    uart_ctx[UART_Index(cfg->uart_id)].callback_ctx = NULL;

    /* Disable all IRQ & DMA requests */
    uart->CR1.BITS.TXEIE  = 0U;
    uart->CR1.BITS.RXNEIE = 0U;
    uart->CR1.BITS.TCIE   = 0U;
    uart->CR1.BITS.IDLEIE = 0U;
    uart->CR3.BITS.DMAT   = 0U;
    uart->CR3.BITS.DMAR   = 0U;

    /* Step 1: Enable UART first per RM0368 §19.3.2 */      /* U-09: UE must be set before TE/RE */
    uart->CR1.BITS.UE = 1U;

    /* Step 2: Enable TX and RE after UE is set */           /* U-09: correct RM0368 §19.3.2 sequence */
    uart->CR1.BITS.TE = 1U;
    uart->CR1.BITS.RE = 1U;

    /* Step 3: Small settle delay — critical at 921600 baud */  /* U-09: allow first idle frame to complete */
    for (volatile uint32_t i = 0U; i < 100U; i++);

    /* Step 4: Clear any startup noise flags */              /* U-09: SR+DR read clears PE/FE/NF/ORE/RXNE */
    (void)uart->SR.ALL;
    (void)uart->DR.ALL;

    return UART_OK;
}

/* ============================================================
 *  Status Flags (for polling mode)
 * ============================================================ */
UART_Error_t UART_IsTxEmpty(UART_Id_t uart, uint8_t *state)  /* changed: out-param replaces raw return */
{
    if (state == NULL) return UART_ERROR_INVALID_PARAM;      /* changed: NULL guard on output pointer */
    volatile UART_REGS_t *u = UART_REG(uart);
    if (u == NULL) return UART_ERROR_INVALID_UART;           /* changed: error code replaces silent 0U */
    *state = (uint8_t)(u->SR.BITS.TXE);                     /* changed: write flag to caller's variable */
    return UART_OK;
}

UART_Error_t UART_IsRxNotEmpty(UART_Id_t uart, uint8_t *state)  /* changed: out-param replaces raw return */
{
    if (state == NULL) return UART_ERROR_INVALID_PARAM;          /* changed: NULL guard on output pointer */
    volatile UART_REGS_t *u = UART_REG(uart);
    if (u == NULL) return UART_ERROR_INVALID_UART;               /* changed: error code replaces silent 0U */
    *state = (uint8_t)(u->SR.BITS.RXNE);                        /* changed: write flag to caller's variable */
    return UART_OK;
}

UART_Error_t UART_IsTxComplete(UART_Id_t uart, uint8_t *state)  /* changed: out-param replaces raw return */
{
    if (state == NULL) return UART_ERROR_INVALID_PARAM;          /* changed: NULL guard on output pointer */
    volatile UART_REGS_t *u = UART_REG(uart);
    if (u == NULL) return UART_ERROR_INVALID_UART;               /* changed: error code replaces silent 0U */
    *state = (uint8_t)(u->SR.BITS.TC);                          /* changed: write flag to caller's variable */
    return UART_OK;
}

/* ============================================================
 *  IRQ control
 * ============================================================ */
UART_Error_t UART_EnableTxIRQ(UART_Id_t uart)
{
    if (uart_ctx[UART_Index(uart)].tx_mode != UART_MODE_IRQ)
        return UART_ERROR_UNSUPPORTED;

    UART_REG(uart)->CR1.BITS.TXEIE = 1U;
    return UART_OK;
}

UART_Error_t UART_DisableTxIRQ(UART_Id_t uart)
{
    /* U-16: Unconditional disable — safe regardless of configured mode */
    UART_REG(uart)->CR1.BITS.TXEIE = 0U;
    return UART_OK;
}

UART_Error_t UART_EnableTxCompleteIRQ(UART_Id_t uart)
{
    UART_REG(uart)->CR1.BITS.TCIE = 1U;
    return UART_OK;
}

UART_Error_t UART_DisableTxCompleteIRQ(UART_Id_t uart)
{
    UART_REG(uart)->CR1.BITS.TCIE = 0U;
    return UART_OK;
}

UART_Error_t UART_EnableRxIRQ(UART_Id_t uart)
{
    if (uart_ctx[UART_Index(uart)].rx_mode != UART_MODE_IRQ)
        return UART_ERROR_UNSUPPORTED;

    UART_REG(uart)->CR1.BITS.RXNEIE = 1U;
    return UART_OK;
}

UART_Error_t UART_DisableRxIRQ(UART_Id_t uart)
{
    /* U-16: Unconditional disable — safe regardless of configured mode */
    UART_REG(uart)->CR1.BITS.RXNEIE = 0U;
    return UART_OK;
}

UART_Error_t UART_EnableIdleIRQ(UART_Id_t uart)
{
    UART_REG(uart)->CR1.BITS.IDLEIE = 1U;
    return UART_OK;
}

UART_Error_t UART_DisableIdleIRQ(UART_Id_t uart)
{
    UART_REG(uart)->CR1.BITS.IDLEIE = 0U;
    return UART_OK;
}

/* ============================================================
 *  DMA request control
 * ============================================================ */
UART_Error_t UART_EnableTxDMA(UART_Id_t uart)
{
    if (uart_ctx[UART_Index(uart)].tx_mode != UART_MODE_DMA)
        return UART_ERROR_UNSUPPORTED;

    volatile UART_REGS_t *u = UART_REG(uart);  /* U-02: volatile — matches updated macro type */

    u->CR1.BITS.TXEIE = 0U;
    /* U-13: TC can be cleared by writing 0 directly (RM0368 §19.3.9)
       or by SR read + DR write. Direct write used here for DMA setup. */
    u->SR.BITS.TC = 0U;                        /* U-13: comment above explains this direct-write clear */
    u->CR3.BITS.DMAT = 1U;

    return UART_OK;
}

UART_Error_t UART_DisableTxDMA(UART_Id_t uart)
{
    UART_REG(uart)->CR3.BITS.DMAT = 0U;
    return UART_OK;
}

UART_Error_t UART_EnableRxDMA(UART_Id_t uart)
{
    if (uart_ctx[UART_Index(uart)].rx_mode != UART_MODE_DMA)
        return UART_ERROR_UNSUPPORTED;

    volatile UART_REGS_t *u = UART_REG(uart);  /* U-02: volatile — matches updated macro type */

    u->CR1.BITS.RXNEIE = 0U;

    /* Clear any pending data */
    if (u->SR.BITS.RXNE || u->SR.BITS.ORE) {
        (void)u->DR.ALL;
    }

    u->CR3.BITS.DMAR = 1U;

    return UART_OK;
}

UART_Error_t UART_DisableRxDMA(UART_Id_t uart)
{
    UART_REG(uart)->CR3.BITS.DMAR = 0U;
    return UART_OK;
}

/* ============================================================
 *  DR access (used by HSerial)
 * ============================================================ */
UART_Error_t UART_WriteDR(UART_Id_t uart, uint16_t data)
{
    UART_REG(uart)->DR.ALL = data;
    return UART_OK;
}

UART_Error_t UART_ReadDR(UART_Id_t uart, uint16_t *data)
{
    if (data == NULL)
        return UART_ERROR_INVALID_PARAM;

    *data = (uint16_t)UART_REG(uart)->DR.ALL;
    return UART_OK;
}

uint32_t UART_GetDRAddress(UART_Id_t uart)
{
    switch (uart) {
        case UART1_ID: return (uint32_t)&UART1->DR;
        case UART2_ID: return (uint32_t)&UART2->DR;
        case UART6_ID: return (uint32_t)&UART6->DR;
        default: return 0U;
    }
}

/* ============================================================
 *  Polling Transfer Functions
 * ============================================================ */

/* U-19: UART_Transmit_Polling — missing function, added per spec */
UART_Error_t UART_Transmit_Polling(UART_Id_t uart_id,
                                    const Buffer_t *buf,   /* changed: Buffer_t replaces (buf, len) */
                                    uint32_t timeout)
{
    if (!BUFFER_IS_VALID(buf)) return UART_ERROR_INVALID_PARAM;  /* changed: validates data, size, length, index in one call */

    volatile UART_REGS_t *uart = UART_REG(uart_id);  /* U-19: get register pointer via macro */
    /* U-19: NULL means invalid uart_id — must not dereference */
    if (uart == NULL) return UART_ERROR_INVALID_UART;

    uart_ctx[UART_Index(uart_id)].state = UART_STATE_BUSY_TX;  /* U-15: mark busy at entry */

    for (uint16_t i = 0U; i < buf->length; i++)  /* changed: loop bound uses buf->length */
    {
        uint32_t tmo = timeout;              /* U-19: fresh countdown per byte */
        while (uart->SR.BITS.TXE == 0U)     /* U-19: wait for TX data register empty */
        {
            if (tmo == 0U)
            {
                uart_ctx[UART_Index(uart_id)].state = UART_STATE_IDLE;  /* U-15: IDLE on timeout */
                return UART_ERROR_TIMEOUT;   /* U-19: countdown expired */
            }
            tmo--;                           /* U-19: decrement once per poll iteration (not ms) */
        }
        (void)UART_WriteDR(uart_id, (uint16_t)buf->data[i]);  /* changed: byte access via buf->data[i] */
    }

    /* U-19: wait for TC after the last byte to ensure shift register is empty */
    uint32_t tc_tmo = timeout;
    while (uart->SR.BITS.TC == 0U)          /* U-19: wait for transmission complete flag */
    {
        if (tc_tmo == 0U)
        {
            uart_ctx[UART_Index(uart_id)].state = UART_STATE_IDLE;  /* U-15: IDLE on timeout */
            return UART_ERROR_TIMEOUT;       /* U-19: TC wait timed out */
        }
        tc_tmo--;                               /* U-19: decrement TC wait counter */
    }

    uart_ctx[UART_Index(uart_id)].state = UART_STATE_IDLE;  /* U-15: IDLE on success */
    return UART_OK;
}

/* U-20: UART_Receive_Polling — missing function, added per spec */
UART_Error_t UART_Receive_Polling(UART_Id_t uart_id,
                                   Buffer_t *buf,           /* changed: Buffer_t replaces (buf, len) */
                                   uint32_t timeout)
{
    /* changed: RX validation — length/index will be 0 on empty buffer so BUFFER_IS_VALID not used */
    if (buf == NULL || buf->data == NULL || buf->size == 0U)
        return UART_ERROR_INVALID_PARAM;

    volatile UART_REGS_t *uart = UART_REG(uart_id);  /* U-20: get register pointer via macro */
    /* U-20: NULL means invalid uart_id — must not dereference */
    if (uart == NULL) return UART_ERROR_INVALID_UART;

    uart_ctx[UART_Index(uart_id)].state = UART_STATE_BUSY_RX;  /* U-15: mark busy at entry */

    for (uint16_t i = 0U; i < buf->size; i++)  /* changed: loop bound uses buf->size (capacity to fill) */
    {
        uint32_t tmo = timeout;              /* U-20: fresh countdown per byte */
        while (uart->SR.BITS.RXNE == 0U)    /* U-20: wait for RX data register not empty */
        {
            if (tmo == 0U)
            {
                uart_ctx[UART_Index(uart_id)].state = UART_STATE_IDLE;  /* U-15: IDLE on timeout */
                return UART_ERROR_TIMEOUT;   /* U-20: countdown expired */
            }
            tmo--;                           /* U-20: decrement once per poll iteration */
        }

        /* U-20: check error flags before reading DR — errors take priority over data */
        if (uart->SR.BITS.ORE != 0U)
        {
            (void)uart->DR.ALL;              /* U-20: ORE cleared by SR read + DR read sequence */
            uart_ctx[UART_Index(uart_id)].state = UART_STATE_IDLE;  /* U-15: IDLE on error exit */
            return UART_ERROR_OVERRUN;
        }
        if (uart->SR.BITS.FE != 0U)
        {
            (void)uart->DR.ALL;              /* U-20: FE cleared by SR read + DR read sequence */
            uart_ctx[UART_Index(uart_id)].state = UART_STATE_IDLE;  /* U-15: IDLE on error exit */
            return UART_ERROR_FRAMING;
        }
        if (uart->SR.BITS.NF != 0U)
        {
            (void)uart->DR.ALL;              /* U-20: NF cleared by SR read + DR read sequence */
            uart_ctx[UART_Index(uart_id)].state = UART_STATE_IDLE;  /* U-15: IDLE on error exit */
            return UART_ERROR_NOISE;
        }
        if (uart->SR.BITS.PE != 0U)
        {
            (void)uart->DR.ALL;              /* U-20: PE cleared by SR read + DR read sequence */
            uart_ctx[UART_Index(uart_id)].state = UART_STATE_IDLE;  /* U-15: IDLE on error exit */
            return UART_ERROR_PARITY;
        }

        buf->data[i] = (uint8_t)(uart->DR.BITS.DR);  /* changed: store byte via buf->data[i] */
    }

    buf->length = buf->size;  /* changed: mark buffer full — length reflects bytes received */
    uart_ctx[UART_Index(uart_id)].state = UART_STATE_IDLE;  /* U-15: IDLE on success */
    return UART_OK;
}

/* ============================================================
 *  Error Utilities
 * ============================================================ */

/* U-17: UART_ClearErrors — missing function, added per spec */
UART_Error_t UART_ClearErrors(UART_Id_t uart_id)
{
    volatile UART_REGS_t *uart = UART_REG(uart_id);  /* U-17: get register pointer via macro */
    if (uart == NULL) return UART_ERROR_INVALID_UART; /* U-17: validate ID before dereference */

    /* U-17: SR+DR read sequence clears PE, FE, NF, ORE atomically (RM0368 §19.3.4) */
    (void)uart->SR.ALL;
    (void)uart->DR.ALL;
    return UART_OK;
}

/* U-18: UART_FlushRx — missing function, added per spec */
UART_Error_t UART_FlushRx(UART_Id_t uart_id)
{
    volatile UART_REGS_t *uart = UART_REG(uart_id);  /* U-18: get register pointer via macro */
    if (uart == NULL) return UART_ERROR_INVALID_UART; /* U-18: validate ID before dereference */

    /* U-18: drain RXNE with iteration cap to prevent infinite loop on continuous input */
    uint8_t limit = 32U;
    while ((uart->SR.BITS.RXNE != 0U) && (limit > 0U))
    {
        (void)uart->DR.ALL;                /* U-18: reading DR clears RXNE flag */
        limit--;
    }
    return UART_OK;
}

/* U-22: UART_IsError — missing convenience function, added per spec */
UART_Error_t UART_IsError(UART_Id_t uart_id, uint8_t *state)  /* changed: out-param replaces raw return */
{
    if (state == NULL) return UART_ERROR_INVALID_PARAM;           /* changed: NULL guard on output pointer */
    volatile UART_REGS_t *uart = UART_REG(uart_id);
    if (uart == NULL) return UART_ERROR_INVALID_UART;             /* changed: error code replaces silent 0U */
    *state = (uint8_t)(((uart->SR.BITS.FE  != 0U) ||             /* changed: write combined error flag to *state */
                        (uart->SR.BITS.NF  != 0U) ||
                        (uart->SR.BITS.ORE != 0U) ||
                        (uart->SR.BITS.PE  != 0U)) ? 1U : 0U);
    return UART_OK;
}

/* ============================
 *   LIN Mode Control
 * ============================ */

UART_Error_t UART_EnableLINMode(UART_Id_t uart)
{
    volatile UART_REGS_t *uart_instance = UART_REG(uart);  /* U-02: volatile — matches updated macro type */
    if (uart_instance == NULL) return UART_ERROR_INVALID_UART;

    /* Set 11-bit break detection length (required for LIN) */
    uart_instance->CR2.BITS.LBDL = 1U;

    /* Enable LIN mode */
    uart_instance->CR2.BITS.LINEN = 1U;

    return UART_OK;
}

UART_Error_t UART_DisableLINMode(UART_Id_t uart)
{
    volatile UART_REGS_t *uart_instance = UART_REG(uart);  /* U-02: volatile — matches updated macro type */

    if (uart_instance == NULL)
    {
        return UART_ERROR_INVALID_UART;
    }

    /* Disable LIN mode */
    uart_instance->CR2.BITS.LINEN = 0U;

    /* Disable break detection interrupt */
    uart_instance->CR2.BITS.LBDIE = 0U;

    return UART_OK;
}

UART_Error_t UART_SendBreak(UART_Id_t uart)
{
    volatile UART_REGS_t *uart_instance = UART_REG(uart);  /* U-02: volatile — matches updated macro type */

    if (uart_instance == NULL)
    {
        return UART_ERROR_INVALID_UART;
    }

    /* Check if previous transmission is complete */
    if (uart_instance->SR.BITS.TC == 0U)
    {
        return UART_ERROR_BUSY;
    }

    /* Clear TC flag before starting break */
    uart_instance->SR.BITS.TC = 0U;

    /* Send break - returns immediately */
    uart_instance->CR1.BITS.SBK = 1U;

    return UART_OK;
}

UART_Error_t UART_EnableBreakIRQ(UART_Id_t uart)
{
    volatile UART_REGS_t *uart_instance = UART_REG(uart);  /* U-02: volatile — matches updated macro type */

    if (uart_instance == NULL)
    {
        return UART_ERROR_INVALID_UART;
    }

    /* Clear any pending break flag first */
    uart_instance->SR.BITS.LBD = 0U;

    /* Enable LIN break detection interrupt */
    uart_instance->CR2.BITS.LBDIE = 1U;

    return UART_OK;
}

UART_Error_t UART_DisableBreakIRQ(UART_Id_t uart)
{
    volatile UART_REGS_t *uart_instance = UART_REG(uart);  /* U-02: volatile — matches updated macro type */

    if (uart_instance == NULL)
    {
        return UART_ERROR_INVALID_UART;
    }

    /* Disable LIN break detection interrupt */
    uart_instance->CR2.BITS.LBDIE = 0U;

    return UART_OK;
}

UART_Error_t UART_IsBreakDetected(UART_Id_t uart, uint8_t *state)  /* changed: out-param replaces raw return */
{
    if (state == NULL) return UART_ERROR_INVALID_PARAM;                /* changed: NULL guard on output pointer */
    volatile UART_REGS_t *uart_instance = UART_REG(uart);
    if (uart_instance == NULL) return UART_ERROR_INVALID_UART;         /* changed: error code replaces silent 0U */
    *state = (uint8_t)(uart_instance->SR.BITS.LBD);                    /* changed: write flag to caller's variable */
    return UART_OK;
}

void UART_ClearBreakFlag(UART_Id_t uart)
{
    volatile UART_REGS_t *uart_instance = UART_REG(uart);  /* U-02: volatile — matches updated macro type */

    if (uart_instance != NULL)
    {
        /* Clear LBD flag by writing 0 */
        uart_instance->SR.BITS.LBD = 0U;
    }
}

/* ============================================================
 *  Callback registration
 * ============================================================ */
UART_Error_t UART_RegisterCallback(
    UART_Id_t uart,
    UART_Callback_t cb,
    void *context
)
{
    if (uart > UART6_ID)
        return UART_ERROR_INVALID_UART;

    uart_ctx[UART_Index(uart)].callback = cb;
    uart_ctx[UART_Index(uart)].callback_ctx = context;
    return UART_OK;
}

/* ============================================================
 *  State Query
 * ============================================================ */
UART_State_t UART_GetState(UART_Id_t uart)
{
    if (uart > UART6_ID)
        return UART_STATE_IDLE;

    return uart_ctx[UART_Index(uart)].state;
}

/* ============================================================
 *  IRQ handler (event-only)
 * ============================================================ */
static void UART_IRQHandler_Common(UART_Id_t id)
{
    volatile UART_REGS_t *uart = UART_REG(id);  /* U-02: volatile — matches updated macro type */
    UART_Callback_t cb = uart_ctx[UART_Index(id)].callback;
    void *ctx = uart_ctx[UART_Index(id)].callback_ctx;
    /* U-14: SR is snapshotted once to prevent race conditions between flag checks.
       CR1 IE bits are re-read live — acceptable since they only change from software.
       Error handling reads DR which clears RXNE — this is intentional: error takes
       priority over data-ready, caller must re-enable RX after error recovery. */
    uint32_t sr = uart->SR.ALL;                 /* U-14: snapshot SR once — prevents TOCTOU between checks */

    if (cb == NULL)
        return;

    /* ================= ERROR (highest priority) ================= */
    if (sr & (USART_SR_FE | USART_SR_NF | USART_SR_ORE | USART_SR_PE)) {
        (void)uart->DR.ALL;
        uart_ctx[UART_Index(id)].state = UART_STATE_IDLE;  /* U-15: error event → back to IDLE */
        cb(UART_EVENT_ERROR, ctx);
        return;
    }

    /* ================= LIN BREAK DETECTED ================= */
    if ((sr & USART_SR_LBD) && uart->CR2.BITS.LBDIE) {
        uart->SR.BITS.LBD = 0U;  /* Clear flag */
        cb(UART_EVENT_BREAK, ctx);
        /* Don't return - continue checking other flags */
    }

    /* ================= RX data ready ================= */
    if ((sr & USART_SR_RXNE) && uart->CR1.BITS.RXNEIE) {
        uart_ctx[UART_Index(id)].state = UART_STATE_BUSY_RX;  /* U-15: RXNE event → BUSY_RX */
        cb(UART_EVENT_RXNE, ctx);
    }

    /* ================= TX empty ================= */
    if ((sr & USART_SR_TXE) && uart->CR1.BITS.TXEIE) {
        uart_ctx[UART_Index(id)].state = UART_STATE_BUSY_TX;  /* U-15: TXE event → BUSY_TX */
        cb(UART_EVENT_TXE, ctx);
    }

    /* ================= TX complete ================= */
    if ((sr & USART_SR_TC) && uart->CR1.BITS.TCIE) {
        uart->SR.BITS.TC = 0U;
        uart_ctx[UART_Index(id)].state = UART_STATE_IDLE;     /* U-15: TC event → IDLE (transfer done) */
        cb(UART_EVENT_TC, ctx);
    }

    /* ================= IDLE line ================= */
    if ((sr & USART_SR_IDLE) && uart->CR1.BITS.IDLEIE) {
        (void)uart->DR.ALL;  /* Clear IDLE flag by reading DR */
        cb(UART_EVENT_IDLE, ctx);
    }
}

/* ============================================================
 *  IRQ entry points
 * ============================================================ */
void USART1_IRQHandler(void) { UART_IRQHandler_Common(UART1_ID); }
void USART2_IRQHandler(void) { UART_IRQHandler_Common(UART2_ID); }
void USART6_IRQHandler(void) { UART_IRQHandler_Common(UART6_ID); }
