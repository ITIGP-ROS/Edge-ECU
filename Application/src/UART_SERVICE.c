#include "UART_SERVICE.h"   /* includes UART_INTERFACE.h and STD_BUFFER.h transitively */
#include "UART_INTERFACE.h"
#include "STD_BUFFER.h"     /* changed: explicit include — do not rely on transitive inclusion */
#include "UART_REGS.h"
#include "NVIC_INTERFACE.h"
#include "STD_TYPES.h"
#include "DMA_INTERFACE.h"


/* ============================================================
 *  Internal ring buffer type
 * ============================================================ */
typedef struct {
    uint8_t  buf[UART_SVC_TX_BUF_SIZE];
    volatile uint16_t head;  /* write index — modified by app/ISR */
    volatile uint16_t tail;  /* read  index — modified by ISR only */
} UART_SVC_TxRing_t;

typedef struct {
    uint8_t  buf[UART_SVC_RX_BUF_SIZE];
    volatile uint16_t head;  /* write index — modified by ISR only */
    volatile uint16_t tail;  /* read  index — modified by app */
} UART_SVC_RxRing_t;

/* ============================================================
 *  Per-instance context
 * ============================================================ */
typedef struct {
    UART_SVC_TxRing_t  tx;
    UART_SVC_RxRing_t  rx;
    volatile uint8_t   tx_active;    /* 1 = TX in progress (IRQ or DMA) */
    volatile uint8_t   error;        /* 1 = FE/NF/ORE/PE occurred */
    uint8_t            initialized;
    UART_SVC_TxMode_t  tx_mode;      /* IRQ or DMA */
    DMA_Id_t           dma_id;       /* DMA controller for TX */
    DMA_StreamId_t     dma_stream;   /* DMA stream for TX */
    DMA_Channel_t      dma_ch;       /* DMA channel for TX */

    /* ---- new fields for DMA TX done callback ---- */
    UART_SVC_TxDoneCb_t tx_done_cb;  /* user callback, NULL if none */
    void               *tx_done_ctx; /* context pointer for callback */

    /* ---- RX notify task: woken from ISR on each received byte ---- */
    TaskHandle_t        rx_notify_task;  /* NULL if unused */
} UART_SVC_Instance_t;

static UART_SVC_Instance_t svc[3];  /* one per UART instance */

/* ============================================================
 *  Ring buffer helpers — all inline, no dynamic allocation
 * ============================================================ */
static inline uint16_t tx_count(const UART_SVC_TxRing_t *r)
{
    return (r->head - r->tail) & UART_SVC_TX_MASK;
}

static inline uint16_t tx_free(const UART_SVC_TxRing_t *r)
{
    return (UART_SVC_TX_BUF_SIZE - 1U) - tx_count(r);
}

static inline uint8_t tx_empty(const UART_SVC_TxRing_t *r)
{
    return (r->head == r->tail) ? 1U : 0U;
}

static inline void tx_push(UART_SVC_TxRing_t *r, uint8_t byte)
{
    r->buf[r->head & UART_SVC_TX_MASK] = byte;
    r->head = (r->head + 1U) & UART_SVC_TX_MASK;
}

static inline uint8_t tx_pop(UART_SVC_TxRing_t *r)
{
    uint8_t b = r->buf[r->tail & UART_SVC_TX_MASK];
    r->tail = (r->tail + 1U) & UART_SVC_TX_MASK;
    return b;
}

static inline uint16_t rx_count(const UART_SVC_RxRing_t *r)
{
    return (r->head - r->tail) & UART_SVC_RX_MASK;
}

static inline uint8_t rx_empty(const UART_SVC_RxRing_t *r)
{
    return (r->head == r->tail) ? 1U : 0U;
}

static inline void rx_push(UART_SVC_RxRing_t *r, uint8_t byte)
{
    /* Silently drop on overflow — head never overwrites tail */
    uint16_t next = (r->head + 1U) & UART_SVC_RX_MASK;
    if (next != r->tail)
    {
        r->buf[r->head] = byte;
        r->head = next;
    }
}

static inline uint8_t rx_pop(UART_SVC_RxRing_t *r)
{
    uint8_t b = r->buf[r->tail & UART_SVC_RX_MASK];
    r->tail = (r->tail + 1U) & UART_SVC_RX_MASK;
    return b;
}

/* ============================================================
 *  NVIC IRQ token per UART instance
 * ============================================================ */
static IRQn_t uart_irqn(UART_Id_t id)
{
    switch (id)
    {
        case UART1_ID: return USART1;
        case UART2_ID: return USART2;
        case UART6_ID: return USART6;
        default:       return USART1;  /* unreachable — ID validated before call */
    }
}
/* Map DMA controller + stream to the correct NVIC IRQ token */
static IRQn_t uart_svc_dma_irqn(DMA_Id_t dma_id, DMA_StreamId_t stream)
{
    if (dma_id == DMA_1)
    {
        switch (stream)
        {
            case DMA_STREAM_0: return DMA1_Stream0_IRQ;
            case DMA_STREAM_1: return DMA1_Stream1_IRQ;
            case DMA_STREAM_2: return DMA1_Stream2_IRQ;
            case DMA_STREAM_3: return DMA1_Stream3_IRQ;
            case DMA_STREAM_4: return DMA1_Stream4_IRQ;
            case DMA_STREAM_5: return DMA1_Stream5_IRQ;
            case DMA_STREAM_6: return DMA1_Stream6_IRQ;
            case DMA_STREAM_7: return DMA1_Stream7;
            default:           return DMA1_Stream0_IRQ;
        }
    }
    else
    {
        switch (stream)
        {
            case DMA_STREAM_0: return DMA2_Stream0;
            case DMA_STREAM_1: return DMA2_Stream1;
            case DMA_STREAM_2: return DMA2_Stream2;
            case DMA_STREAM_3: return DMA2_Stream3;
            case DMA_STREAM_4: return DMA2_Stream4;
            case DMA_STREAM_5: return DMA2_Stream5;
            case DMA_STREAM_6: return DMA2_Stream6;
            case DMA_STREAM_7: return DMA2_Stream7;
            default:           return DMA2_Stream0;
        }
    }
}
/* ============================================================
 *  Internal callback — registered for all instances
 *  Handles TXE, TC, RXNE, ERROR events
 * ============================================================ */
static void uart_svc_callback(UART_Event_t event, void *ctx)
{
    UART_Id_t id = (UART_Id_t)(uint32_t)ctx;
    UART_SVC_Instance_t *inst = &svc[(uint8_t)id];

    switch (event)
    {
        /* ---- TX data register empty ---- */
        case UART_EVENT_TXE:
            if (!tx_empty(&inst->tx))
            {
                /* More bytes to send — write next byte to DR */
                uint8_t byte = tx_pop(&inst->tx);
                UART_WriteDR(id, (uint16_t)byte);
            }
            else
            {
                /* Buffer drained — disable TXE, enable TC */
                UART_DisableTxIRQ(id);
                UART_EnableTxCompleteIRQ(id);
            }
            break;

        /* ---- Transmission complete ---- */
        case UART_EVENT_TC:
            UART_DisableTxCompleteIRQ(id);
            inst->tx_active = 0U;
            break;

        /* ---- RX byte received ---- */
        case UART_EVENT_RXNE:
        {
            uint16_t data = 0U;
            UART_ReadDR(id, &data);
            rx_push(&inst->rx, (uint8_t)(data & 0xFFU));

            /* Wake any registered task without context switch — let scheduler decide */
            if (inst->rx_notify_task != NULL)
            {
                BaseType_t woken = pdFALSE;
                vTaskNotifyGiveFromISR(inst->rx_notify_task, &woken);
                portYIELD_FROM_ISR(woken);
            }
            break;
        }

        /* ---- Error (FE / NF / ORE / PE) ---- */
        case UART_EVENT_ERROR:
            inst->error = 1U;
            UART_ClearErrors(id);
            break;

        /* ---- IDLE / BREAK — not used by service layer ---- */
        case UART_EVENT_IDLE:
        case UART_EVENT_BREAK:
        default:
            break;
    }
}
/* Internal DMA TX complete callback — called from DMA IRQ when TX finishes.
   Disables UART DMA TX request and clears tx_active flag. */
static void uart_svc_dma_tx_complete(DMA_Event_t event, void *ctx)
{
    UART_Id_t id = (UART_Id_t)(uint32_t)ctx;

    if (event == DMA_EVENT_TRANSFER_COMPLETE)
    {
        /* Disable DMA TX request in UART peripheral */
        UART_DisableTxDMA(id);

        /* Mark TX as idle */
        svc[(uint8_t)id].tx_active = 0U;

        /* ---- new: notify registered user callback (if any) ---- */
        if (svc[(uint8_t)id].tx_done_cb != NULL)
        {
            svc[(uint8_t)id].tx_done_cb(svc[(uint8_t)id].tx_done_ctx);
        }
    }
    else if (event == DMA_EVENT_TRANSFER_ERROR)
    {
        /* Mark error and release TX */
        svc[(uint8_t)id].error     = 1U;
        svc[(uint8_t)id].tx_active = 0U;
        UART_DisableTxDMA(id);

        /* ---- new: also notify on error so task can handle abort ---- */
        if (svc[(uint8_t)id].tx_done_cb != NULL)
        {
            svc[(uint8_t)id].tx_done_cb(svc[(uint8_t)id].tx_done_ctx);
        }
    }
}
/* ============================================================
 *  Public API implementation
 * ============================================================ */

UART_SVC_Error_t UART_SVC_Init(UART_Id_t         id,
                                uint8_t           priority,
                                UART_SVC_TxMode_t tx_mode,
                                DMA_Id_t          dma_id,
                                DMA_StreamId_t    dma_stream,
                                DMA_Channel_t     dma_ch)
{
    if (id > UART6_ID)
        return UART_SVC_ERROR_PARAM;

    UART_SVC_Instance_t *inst = &svc[(uint8_t)id];

    /* Zero ring buffers and flags */
    inst->tx.head     = 0U;
    inst->tx.tail     = 0U;
    inst->rx.head     = 0U;
    inst->rx.tail     = 0U;
    inst->tx_active   = 0U;
    inst->error       = 0U;
    inst->tx_mode     = tx_mode;       /* store TX mode for TransmitDMA guard */
    inst->dma_id      = dma_id;        /* store DMA config for TransmitDMA */
    inst->dma_stream  = dma_stream;
    inst->dma_ch      = dma_ch;
    inst->initialized = 1U;

    /* ---- new: clear callback fields ---- */
    inst->tx_done_cb  = NULL;
    inst->tx_done_ctx = NULL;
    inst->rx_notify_task = NULL;

    /* Register UART IRQ callback for RX and error handling */
    UART_RegisterCallback(id, uart_svc_callback, (void *)(uint32_t)id);

    /* Always enable RX IRQ — RX always uses IRQ ring buffer */
    UART_EnableRxIRQ(id);

    /* Configure and enable NVIC for this UART */
    NVIC_SetPriority(uart_irqn(id), (uint32_t)priority);
    NVIC_EnableIRQ(uart_irqn(id));

    /* If DMA TX mode: configure DMA stream now, callback registered here */
    if (tx_mode == UART_SVC_TX_MODE_DMA)
    {
        /* Map UART DR address for DMA peripheral address */
        uint32_t dr_addr = UART_GetDRAddress(id);

        DMA_Config_t dma_cfg;
        dma_cfg.dma_id             = dma_id;
        dma_cfg.stream_id          = dma_stream;
        dma_cfg.channel            = dma_ch;
        dma_cfg.direction          = DMA_DIR_MEM_TO_PERIPH;
        dma_cfg.periph_width       = DMA_WIDTH_BYTE;
        dma_cfg.mem_width          = DMA_WIDTH_BYTE;
        dma_cfg.periph_inc         = DMA_INC_DISABLE;
        dma_cfg.mem_inc            = DMA_INC_ENABLE;
        dma_cfg.mode               = DMA_MODE_NORMAL;
        dma_cfg.priority           = DMA_PRIORITY_HIGH;
        dma_cfg.flow_controller    = DMA_FLOW_DMA;
        dma_cfg.peripheral_address = dr_addr;
        dma_cfg.memory0_address    = 0U;  /* updated per-transfer in TransmitDMA */
        dma_cfg.memory1_address    = 0U;
        dma_cfg.length             = 1U;  /* placeholder — updated per-transfer */
        dma_cfg.complete_callback  = uart_svc_dma_tx_complete;
        dma_cfg.half_callback      = NULL;
        dma_cfg.error_callback     = uart_svc_dma_tx_complete; /* reuse — checks event type */
        dma_cfg.user_context       = (void *)(uint32_t)id;

        if (DMA_Init(&dma_cfg) != DMA_OK)
            return UART_SVC_ERROR_DMA;

        /* Enable NVIC for the DMA stream */
        IRQn_t dma_irqn = uart_svc_dma_irqn(dma_id, dma_stream);
        NVIC_SetPriority(dma_irqn, (uint32_t)(priority + 1U));
        NVIC_EnableIRQ(dma_irqn);
    }

    return UART_SVC_OK;
}

UART_SVC_Error_t UART_SVC_Transmit(UART_Id_t id,
                                    const Buffer_t *buf)  /* changed: Buffer_t replaces (buf, len) */
{
    if (id > UART6_ID || !BUFFER_IS_VALID(buf))  /* changed: BUFFER_IS_VALID replaces NULL+len checks */
        return UART_SVC_ERROR_PARAM;

    UART_SVC_Instance_t *inst = &svc[(uint8_t)id];

    if (!inst->initialized)
        return UART_SVC_ERROR_INIT;

    /* Check space */
    if (tx_free(&inst->tx) < buf->length)  /* changed: buf->length replaces len */
        return UART_SVC_ERROR_FULL;

    /* Load bytes into ring buffer */
    for (uint16_t i = 0U; i < buf->length; i++)  /* changed: loop bound uses buf->length */
        tx_push(&inst->tx, buf->data[i]);          /* changed: byte access via buf->data[i] */

    /* Kick off TX if not already running */
    if (inst->tx_active == 0U)
    {
        inst->tx_active = 1U;

        /* Write first byte directly — this triggers TXE chain */
        uint8_t first = tx_pop(&inst->tx);
        UART_WriteDR(id, (uint16_t)first);

        /* Enable TXE IRQ — callback handles remaining bytes */
        UART_EnableTxIRQ(id);
    }
    /* If tx_active == 1, bytes are already in the ring buffer
       and the running TXE ISR will pick them up automatically */

    return UART_SVC_OK;
}
UART_SVC_Error_t UART_SVC_TransmitDMA(UART_Id_t id, const Buffer_t *buf)
{
    /* Validate buffer — TX buffer must have valid data and non-zero length */
    if (!BUFFER_IS_VALID(buf) || buf->length == 0U)
        return UART_SVC_ERROR_PARAM;

    if (id > UART6_ID)
        return UART_SVC_ERROR_PARAM;

    UART_SVC_Instance_t *inst = &svc[(uint8_t)id];

    if (!inst->initialized)
        return UART_SVC_ERROR_INIT;

    /* Only available in DMA TX mode */
    if (inst->tx_mode != UART_SVC_TX_MODE_DMA)
        return UART_SVC_ERROR_NO_DMA;

    /* Reject if previous DMA transfer still running */
    if (inst->tx_active != 0U)
        return UART_SVC_ERROR_BUSY;

    /* Mark TX busy before starting DMA — prevents re-entry */
    inst->tx_active = 1U;

    /* Update DMA memory address and length for this transfer */
    DMA_UpdateMemoryAddress(inst->dma_id,
                            inst->dma_stream,
                            (uint32_t)buf->data);

    /* Re-init DMA with correct length — DMA_Start only reloads NDTR from stored config.
       We must update stored config length before calling DMA_Start. */
    DMA_Config_t dma_cfg;
    dma_cfg.dma_id             = inst->dma_id;
    dma_cfg.stream_id          = inst->dma_stream;
    dma_cfg.channel            = inst->dma_ch;
    dma_cfg.direction          = DMA_DIR_MEM_TO_PERIPH;
    dma_cfg.periph_width       = DMA_WIDTH_BYTE;
    dma_cfg.mem_width          = DMA_WIDTH_BYTE;
    dma_cfg.periph_inc         = DMA_INC_DISABLE;
    dma_cfg.mem_inc            = DMA_INC_ENABLE;
    dma_cfg.mode               = DMA_MODE_NORMAL;
    dma_cfg.priority           = DMA_PRIORITY_HIGH;
    dma_cfg.flow_controller    = DMA_FLOW_DMA;
    dma_cfg.peripheral_address = UART_GetDRAddress(id);
    dma_cfg.memory0_address    = (uint32_t)buf->data;
    dma_cfg.memory1_address    = 0U;
    dma_cfg.length             = buf->length;
    dma_cfg.complete_callback  = uart_svc_dma_tx_complete;
    dma_cfg.half_callback      = NULL;
    dma_cfg.error_callback     = uart_svc_dma_tx_complete;
    dma_cfg.user_context       = (void *)(uint32_t)id;

    if (DMA_Init(&dma_cfg) != DMA_OK)
    {
        inst->tx_active = 0U;
        return UART_SVC_ERROR_DMA;
    }

    /* Enable DMA TX request in UART peripheral */
    UART_EnableTxDMA(id);

    /* Start DMA transfer */
    if (DMA_Start(inst->dma_id, inst->dma_stream) != DMA_OK)
    {
        inst->tx_active = 0U;
        UART_DisableTxDMA(id);
        return UART_SVC_ERROR_DMA;
    }

    return UART_SVC_OK;
}
UART_SVC_Error_t UART_SVC_TxPending(UART_Id_t id, uint16_t *pending)  /* changed: out-param replaces raw return */
{
    if (pending == NULL) return UART_SVC_ERROR_PARAM;   /* changed: NULL guard on output pointer */
    if (id > UART6_ID)   return UART_SVC_ERROR_PARAM;
    *pending = tx_count(&svc[(uint8_t)id].tx);          /* changed: write count to caller's variable */
    return UART_SVC_OK;
}

/* tx_active is set/cleared by both IRQ TC callback and DMA complete callback —
   this function works correctly for both UART_SVC_TX_MODE_IRQ and UART_SVC_TX_MODE_DMA */
UART_SVC_Error_t UART_SVC_IsTxIdle(UART_Id_t id, uint8_t *idle)  /* changed: out-param replaces raw return */
{
    if (idle == NULL)  return UART_SVC_ERROR_PARAM;     /* changed: NULL guard on output pointer */
    if (id > UART6_ID) return UART_SVC_ERROR_PARAM;
    UART_SVC_Instance_t *inst = &svc[(uint8_t)id];
    *idle = (inst->tx_active == 0U && tx_empty(&inst->tx)) ? 1U : 0U;  /* changed: write result to *idle */
    return UART_SVC_OK;
}

UART_SVC_Error_t UART_SVC_Receive(UART_Id_t id, Buffer_t *buf)  /* changed: Buffer_t replaces (buf, len); returns error code */
{
    if (!BUFFER_IS_VALID(buf)) return UART_SVC_ERROR_PARAM;  /* changed: validates data, size, length, index */
    if (id > UART6_ID)         return UART_SVC_ERROR_PARAM;

    UART_SVC_RxRing_t *rx = &svc[(uint8_t)id].rx;
    BUFFER_RESET(buf);  /* changed: reset index and length before filling */

    while (!BUFFER_IS_FULL(buf) && !rx_empty(rx))  /* changed: fill loop uses Buffer_t predicates */
    {
        buf->data[buf->length] = rx_pop(rx);  /* changed: write into buf->data at current length */
        buf->length++;
    }

    return (buf->length > 0U) ? UART_SVC_OK : UART_SVC_ERROR_EMPTY;  /* changed: return error if nothing received */
}

UART_SVC_Error_t UART_SVC_RxAvailable(UART_Id_t id, uint16_t *available)  /* changed: out-param replaces raw return */
{
    if (available == NULL) return UART_SVC_ERROR_PARAM;   /* changed: NULL guard on output pointer */
    if (id > UART6_ID)     return UART_SVC_ERROR_PARAM;
    *available = rx_count(&svc[(uint8_t)id].rx);          /* changed: write count to caller's variable */
    return UART_SVC_OK;
}

UART_SVC_Error_t UART_SVC_FlushRx(UART_Id_t id)
{
    if (id > UART6_ID) return UART_SVC_ERROR_PARAM;
    UART_SVC_RxRing_t *rx = &svc[(uint8_t)id].rx;
    rx->head = 0U;
    rx->tail = 0U;
    return UART_SVC_OK;
}

UART_SVC_Error_t UART_SVC_FlushTx(UART_Id_t id)
{
    if (id > UART6_ID) return UART_SVC_ERROR_PARAM;
    UART_DisableTxIRQ(id);
    UART_SVC_TxRing_t *tx = &svc[(uint8_t)id].tx;
    tx->head = 0U;
    tx->tail = 0U;
    svc[(uint8_t)id].tx_active = 0U;
    return UART_SVC_OK;
}

UART_SVC_Error_t UART_SVC_IsError(UART_Id_t id, uint8_t *error)  /* changed: out-param replaces raw return */
{
    if (error == NULL) return UART_SVC_ERROR_PARAM;   /* changed: NULL guard on output pointer */
    if (id > UART6_ID) return UART_SVC_ERROR_PARAM;
    *error = svc[(uint8_t)id].error;                  /* changed: write error flag to caller's variable */
    return UART_SVC_OK;
}

void UART_SVC_ClearError(UART_Id_t id)
{
    if (id > UART6_ID) return;
    svc[(uint8_t)id].error = 0U;
    UART_ClearErrors(id);
}

/* ============================================================
 *  New: DMA TX done callback registration
 * ============================================================ */
UART_SVC_Error_t UART_SVC_RegisterTxDoneCb(UART_Id_t           id,
                                            UART_SVC_TxDoneCb_t cb,
                                            void               *ctx)
{
    if (id > UART6_ID)
        return UART_SVC_ERROR_PARAM;

    svc[(uint8_t)id].tx_done_cb  = cb;
    svc[(uint8_t)id].tx_done_ctx = ctx;

    return UART_SVC_OK;
}

/* ============================================================
 *  RX notify task registration
 * ============================================================ */
void UART_SVC_SetRxNotifyTask(UART_Id_t id, TaskHandle_t task)
{
    if (id <= UART6_ID)
    {
        svc[(uint8_t)id].rx_notify_task = task;
    }
}
