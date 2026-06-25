/* ================================================================
 *  I2C_SERVICE.c
 *  Lightweight service layer above I2C driver for STM32F401CC.
 *  Supports: blocking IRQ TX (register writes) and non-blocking
 *  DMA burst RX (100 Hz MPU6050 sensor loop).
 * ================================================================ */

#include "I2C_SERVICE.h"
#include "I2C_INTERFACE.h"
#include "I2C_REGS.h"
#include "DMA_INTERFACE.h"
#include "NVIC_INTERFACE.h"
#include "RCC_INTERFACE.h"
#include "STD_TYPES.h"

/* ================================================================
 *  Internal instance struct — one per I2C peripheral (3 total).
 *  All state is static; no dynamic allocation anywhere.
 * ================================================================ */
typedef struct {
    volatile I2C_SVC_State_t  state;           /* current service-layer state          */
    I2C_SVC_Callback_t        user_callback;   /* user's completion/error callback     */
    void                     *user_ctx;        /* opaque context passed back to user   */
    DMA_Id_t                  dma_id;          /* DMA controller ID for RX             */
    DMA_StreamId_t            dma_stream;      /* DMA stream ID for RX                 */
    DMA_Channel_t             dma_channel;     /* DMA channel selection for RX         */
    uint8_t                  *rx_buf_pending;  /* pointer to caller's RX destination   */
    uint16_t                  rx_len_pending;  /* number of bytes DMA must fill        */
    uint8_t                   reg_addr_buf;    /* single-byte TX buf for register addr */
    I2C_DevAddr7_t            dev_addr;        /* stored slave addr for Phase 2 RS     */
    uint8_t                   initialized;     /* 1U after successful Init             */
} I2C_SVC_Instance_t;

static I2C_SVC_Instance_t svc[3];  /* one entry per I2C_ID_1 / _2 / _3 */

/* ================================================================
 *  NVIC IRQ look-up tables
 *  Maps I2C instance / DMA stream to the encoded IRQn_t values
 *  defined in NVIC_INTERFACE.h.
 * ================================================================ */

/* I2C event IRQ: I2C_ID_x → IRQn_t */
static const IRQn_t i2c_ev_lut[3U] = {
    I2C1_EV,   /* I2C_ID_1 */
    I2C2_EV,   /* I2C_ID_2 */
    I2C3_EV    /* I2C_ID_3 */
};

/* I2C error IRQ: I2C_ID_x → IRQn_t */
static const IRQn_t i2c_er_lut[3U] = {
    I2C1_ER,   /* I2C_ID_1 */
    I2C2_ER,   /* I2C_ID_2 */
    I2C3_ER    /* I2C_ID_3 */
};

/* DMA stream IRQ: [dma_id][stream] → IRQn_t */
static const IRQn_t dma_irq_lut[2U][8U] = {
    /* DMA_1, streams 0–7 */
    {
        DMA1_Stream0_IRQ, DMA1_Stream1_IRQ, DMA1_Stream2_IRQ, DMA1_Stream3_IRQ,
        DMA1_Stream4_IRQ, DMA1_Stream5_IRQ, DMA1_Stream6_IRQ, DMA1_Stream7
    },
    /* DMA_2, streams 0–7 */
    {
        DMA2_Stream0, DMA2_Stream1, DMA2_Stream2, DMA2_Stream3,
        DMA2_Stream4, DMA2_Stream5, DMA2_Stream6, DMA2_Stream7
    }
};

/* ================================================================
 *  NVIC IRQ mapping helpers
 * ================================================================ */

/* Return the NVIC event-IRQ number for the given I2C instance */
static IRQn_t i2c_svc_ev_irqn(I2C_Id_t id)
{
    return i2c_ev_lut[id];  /* id is validated by caller */
}

/* Return the NVIC error-IRQ number for the given I2C instance */
static IRQn_t i2c_svc_er_irqn(I2C_Id_t id)
{
    return i2c_er_lut[id];  /* id is validated by caller */
}

/* Return the NVIC IRQ number for a specific DMA stream */
static IRQn_t i2c_svc_dma_irqn(DMA_Id_t dma_id, DMA_StreamId_t stream)
{
    return dma_irq_lut[dma_id][stream];  /* both indices validated by caller */
}

/* ================================================================
 *  DMA RX complete / error callback
 *  Called from DMA ISR context — must be short, no blocking.
 * ================================================================ */
static void i2c_svc_dma_rx_complete(DMA_Event_t event, void *ctx)
{
    I2C_Id_t id = (I2C_Id_t)(uint32_t)ctx;

    if (event == DMA_EVENT_TRANSFER_COMPLETE)
    {
        I2C_DisableDMA(id);
        I2C_DisableDMALast(id);
        
           I2C_GenerateStop(id);

        /* I2C_ResetHandle
 * Resets the driver handle state machine to IDLE without touching
 * the I2C bus. The caller is responsible for generating STOP
 * before calling this function if the bus transaction is complete.
 */

        I2C_ResetHandle(id);
        /* ---- END FIX ---- */

        svc[id].state = I2C_SVC_STATE_IDLE;
        svc[id].rx_buf_pending = NULL;

        if (svc[id].user_callback != NULL)
        {
            svc[id].user_callback(I2C_SVC_OK, svc[id].user_ctx);
        }
    }
    else if (event == DMA_EVENT_TRANSFER_ERROR)
    {
        I2C_DisableDMA(id);
        I2C_DisableDMALast(id);
        
         I2C_GenerateStop(id);

        /* Reset driver handle on error path too — same reasoning */
        I2C_ResetHandle(id);

        svc[id].state = I2C_SVC_STATE_ERROR;
        svc[id].rx_buf_pending = NULL;

        if (svc[id].user_callback != NULL)
        {
            svc[id].user_callback(I2C_SVC_ERROR_DMA, svc[id].user_ctx);
        }
    }
}

/* ================================================================
 *  Internal I2C event callback
 *  Registered once during Init — handles TX_COMPLETE to launch
 *  DMA RX (Phase 2), and ERROR to abort.
 *  For blocking WriteReg, rx_buf_pending == NULL so TX_COMPLETE
 *  simply sets state IDLE without launching DMA.
 * ================================================================ */
static void i2c_svc_ev_callback(I2C_Event_t event, void *ctx)
{
    I2C_Id_t id = (I2C_Id_t)(uint32_t)ctx;  /* recover I2C instance */

    if (event == I2C_EVENT_TX_COMPLETE)
    {
        /* ---- Guard for blocking WriteReg path ----
         * WriteReg does not set rx_buf_pending, so if it is NULL
         * we just mark idle and return — no DMA launch needed.   */
        if (svc[id].rx_buf_pending == NULL)
        {
            svc[id].state = I2C_SVC_STATE_IDLE;  /* TX done, no RX phase */
            return;
        }

        /* ---- Phase 2: launch DMA RX with repeated START ---- */
        svc[id].state = I2C_SVC_STATE_RX_DMA;  /* transition to RX_DMA state */

        /* Build DMA configuration for peripheral-to-memory byte transfer */
        DMA_Config_t dma_cfg;
        dma_cfg.dma_id              = svc[id].dma_id;              /* e.g. DMA_1           */
        dma_cfg.stream_id           = svc[id].dma_stream;          /* e.g. DMA_STREAM_0    */
        dma_cfg.channel             = svc[id].dma_channel;         /* e.g. DMA_CHANNEL_1   */
        dma_cfg.direction           = DMA_DIR_PERIPH_TO_MEM;       /* I2C DR → SRAM        */
        dma_cfg.periph_width        = DMA_WIDTH_BYTE;              /* DR is 8-bit          */
        dma_cfg.mem_width           = DMA_WIDTH_BYTE;              /* rx_buf is uint8_t[]  */
        dma_cfg.periph_inc          = DMA_INC_DISABLE;             /* DR address is fixed  */
        dma_cfg.mem_inc             = DMA_INC_ENABLE;              /* advance through buf  */
        dma_cfg.mode                = DMA_MODE_NORMAL;             /* one-shot, not circ   */
        dma_cfg.priority            = DMA_PRIORITY_HIGH;           /* sensor data is prio  */
        dma_cfg.flow_controller     = DMA_FLOW_DMA;               /* DMA controls length  */
        dma_cfg.peripheral_address  = I2C_GetDRAddress(id);        /* &I2Cx->DR            */
        dma_cfg.memory0_address     = (uint32_t)svc[id].rx_buf_pending;  /* caller's buffer */
        dma_cfg.memory1_address     = 0U;                          /* no double-buffer     */
        dma_cfg.length              = svc[id].rx_len_pending;      /* e.g. 14 for MPU6050  */
        dma_cfg.complete_callback   = i2c_svc_dma_rx_complete;     /* TC fires here        */
        dma_cfg.half_callback       = NULL;                        /* not needed           */
        dma_cfg.error_callback      = i2c_svc_dma_rx_complete;     /* errors also here     */
        dma_cfg.user_context        = (void *)(uint32_t)id;        /* pass id to DMA cb    */

        /* Initialise DMA stream with the config */
        if (DMA_Init(&dma_cfg) != DMA_OK)
        {
            svc[id].state = I2C_SVC_STATE_ERROR;  /* DMA init failed */
            if (svc[id].user_callback != NULL)
            {
                svc[id].user_callback(I2C_SVC_ERROR_DMA, svc[id].user_ctx);
            }
            return;  /* abort — do not proceed to RX */
        }

        I2C_EnableDMALast(id);  /* set CR2.LAST — NACK + STOP after last DMA byte */
        I2C_EnableDMA(id);      /* set CR2.DMAEN — I2C will issue DMA requests     */

        /* Build RX request for repeated START + slave read address */
        I2C_RxRequest_t rx_req;
        rx_req.dev_addr = svc[id].dev_addr;        /* 7-bit slave address         */
        rx_req.rx_buf   = svc[id].rx_buf_pending;  /* destination buffer           */
        rx_req.rx_len   = svc[id].rx_len_pending;  /* number of bytes to receive   */
        rx_req.stop     = I2C_STOP_ENABLE;          /* generate STOP after last byte */

        /* Issue repeated START — I2C HW sends addr+R, DMA handles data */
        I2C_MasterReceive_IT(id, &rx_req);

        I2C_DisableBufferIRQ(id);  /* disable buffer IRQs — DMA will handle RX, we don't want IRQs */

        /* Enable DMA stream — begins filling rx_buf from DR */
        DMA_Start(svc[id].dma_id, svc[id].dma_stream);
    }
    else if (event == I2C_EVENT_ERROR)
    {
        svc[id].state = I2C_SVC_STATE_ERROR;  /* latch error state */

        if (svc[id].user_callback != NULL)    /* notify user of NACK/bus error */
        {
            svc[id].user_callback(I2C_SVC_ERROR_NACK, svc[id].user_ctx);
        }
    }
    else
    {
        /* All other events (SB, ADDR, TXE, RXNE, BTF, STOPF, RX_COMPLETE):
         * ignored — handled internally by the I2C driver state machine.     */
    }
}

/* ================================================================
 *  I2C_SVC_Init
 *  Initialises one service-layer instance. Call AFTER I2C_Init()
 *  and GPIO AF config for SCL/SDA.
 *
 *  IMPORTANT: The underlying I2C peripheral MUST be initialised with
 *  transfer_mode = I2C_MODE_DMA in I2C_Config_t, otherwise
 *  I2C_EnableDMA() will fail (it checks transfer_mode internally).
 * ================================================================ */
I2C_SVC_Error_t I2C_SVC_Init(
    I2C_Id_t        id,
    uint8_t         i2c_ev_priority,
    uint8_t         i2c_er_priority,
    DMA_Id_t        dma_id,
    DMA_StreamId_t  dma_stream,
    DMA_Channel_t   dma_channel,
    uint8_t         dma_priority
)
{
    /* 1. Validate I2C instance range */
    if ((uint32_t)id >= 3U)
    {
        return I2C_SVC_ERROR_PARAM;  /* bad instance ID */
    }

    /* 2. Zero-initialise the instance — all fields become 0 / NULL */
    svc[id].state          = I2C_SVC_STATE_IDLE;
    svc[id].user_callback  = NULL;
    svc[id].user_ctx       = NULL;
    svc[id].rx_buf_pending = NULL;
    svc[id].rx_len_pending = 0U;
    svc[id].reg_addr_buf   = 0U;
    svc[id].dev_addr       = 0U;
    svc[id].initialized    = 0U;  /* will be set to 1 at end on success */

    /* 3. Store DMA parameters for later use in RX phase */
    svc[id].dma_id      = dma_id;       /* e.g. DMA_1 for I2C1         */
    svc[id].dma_stream  = dma_stream;   /* e.g. DMA_STREAM_0 for I2C1  */
    svc[id].dma_channel = dma_channel;  /* e.g. DMA_CHANNEL_1 for I2C1 */

    /* 4. Register the internal event callback with the I2C driver.
     *    This callback stays active for both WriteReg and ReadBurst_DMA.
     *    The rx_buf_pending NULL check inside the callback makes it safe
     *    for the blocking write path (no DMA launch on TX_COMPLETE).     */
    I2C_RegisterCallback(id, i2c_svc_ev_callback, (void *)(uint32_t)id);

    /* 5. Enable NVIC for I2C event IRQ */
    NVIC_SetPriority(i2c_svc_ev_irqn(id), (uint32_t)i2c_ev_priority);
    NVIC_EnableIRQ(i2c_svc_ev_irqn(id));

    /* 6. Enable NVIC for I2C error IRQ */
    NVIC_SetPriority(i2c_svc_er_irqn(id), (uint32_t)i2c_er_priority);
    NVIC_EnableIRQ(i2c_svc_er_irqn(id));

    /* 7. Enable NVIC for DMA stream IRQ.
     *    DMA priority must be numerically higher (= lower urgency) than
     *    I2C EV priority so the EV handler finishes before DMA preempts.
     *    The passed dma_priority parameter is overridden by this rule.   */
    uint32_t computed_dma_prio = (i2c_ev_priority < 15U)
                                 ? ((uint32_t)i2c_ev_priority + 1U)
                                 : 15U;
    (void)dma_priority;  /* intentionally unused — computed from ev priority */
    NVIC_SetPriority(i2c_svc_dma_irqn(dma_id, dma_stream), computed_dma_prio);
    NVIC_EnableIRQ(i2c_svc_dma_irqn(dma_id, dma_stream));

    /* 8. Mark instance as initialised */
    svc[id].initialized = 1U;

    /* 9. Success */
    return I2C_SVC_OK;
}





/* ================================================================
 *  I2C_SVC_WriteReg
 *  Blocking register write via IRQ TX. Polls for completion.
 *  Used at startup for MPU6050 configuration registers.
 *  tx_buf layout: [reg_addr, value0, value1, ...].
 * ================================================================ */
I2C_SVC_Error_t I2C_SVC_WriteReg(
    I2C_Id_t        id,
    I2C_DevAddr7_t  dev_addr,
    const uint8_t  *tx_buf,
    uint16_t        tx_len,
    uint32_t        timeout
)
{
    /* 1. Validate parameters */
    if (((uint32_t)id >= 3U) || (tx_buf == NULL) || (tx_len == 0U))
    {
        return I2C_SVC_ERROR_PARAM;  /* bad args */
    }

    /* 2. Check that Init was called */
    if (svc[id].initialized != 1U)
    {
        return I2C_SVC_ERROR_INIT;  /* not initialised */
    }

    /* 3. Busy guard — only one transfer at a time */
    if (svc[id].state != I2C_SVC_STATE_IDLE)
    {
        return I2C_SVC_ERROR_BUSY;  /* previous transfer still active */
    }

    /* 4. Transition to TX state */
    svc[id].state = I2C_SVC_STATE_TX;

    /* Ensure rx_buf_pending is NULL so the event callback does NOT
     * launch DMA on TX_COMPLETE — it will just set state to IDLE.  */
    svc[id].rx_buf_pending = NULL;

    /* 5. Build TX request descriptor */
    I2C_TxRequest_t tx_req;
    tx_req.dev_addr = dev_addr;          /* 7-bit slave address (e.g. 0x68) */
    tx_req.tx_buf   = tx_buf;            /* caller's buffer [reg, data...]  */
    tx_req.tx_len   = tx_len;            /* total bytes to send             */
    tx_req.stop     = I2C_STOP_ENABLE;   /* generate STOP after write       */

    /* 6. Start IRQ-driven transmit */
    if (I2C_MasterTransmit_IT(id, &tx_req) != I2C_OK)
    {
        svc[id].state = I2C_SVC_STATE_IDLE;  /* roll back state on failure */
        return I2C_SVC_ERROR_NACK;            /* likely NACK or bus error   */
    }

    
    /* 7. Poll transfer status with decrement timeout */
    I2C_Status_t xfer_status;
    uint32_t cnt = timeout;
    while (cnt > 0U)
    {
        xfer_status = I2C_GetXferStatus(id);
        if (xfer_status == I2C_STATUS_DONE)  break;
        if (xfer_status == I2C_STATUS_ERROR)
        {
            svc[id].state = I2C_SVC_STATE_ERROR;
            return I2C_SVC_ERROR_NACK;
        }
        cnt--;
    }

    if (cnt == 0U)
    {
        svc[id].state = I2C_SVC_STATE_ERROR;
        return I2C_SVC_ERROR_TIMEOUT;
    }

    /* 8. Wait for STOP to propagate — BUSY must clear before next START.
     * Without this, the next I2C_MasterTransmit_IT fires START while
     * the hardware BUSY flag is still set, causing a silent misfire.   */
    uint8_t  busy_flag = 1U;
    uint32_t busy_cnt  = 100000U;
    while (busy_cnt > 0U)
    {
        I2C_IsBusy(id, &busy_flag);
        if (busy_flag == 0U) break;
        busy_cnt--;
    }

    /* 9. Transfer complete — return to idle */
    svc[id].state = I2C_SVC_STATE_IDLE;
    return I2C_SVC_OK;
}

/* ================================================================
 *  I2C_SVC_ReadBurst_DMA
 *  Non-blocking two-phase DMA burst read.
 *  Phase 1 (IRQ TX): sends reg_addr with NO STOP (holds bus).
 *  Phase 2 (DMA RX): repeated START + DMA fills rx_buf.
 *  Returns immediately after starting Phase 1.
 *  Callback fires from DMA TC ISR when rx_buf is fully filled.
 * ================================================================ */
I2C_SVC_Error_t I2C_SVC_ReadBurst_DMA(
    I2C_Id_t           id,
    I2C_DevAddr7_t     dev_addr,
    uint8_t            reg_addr,
    uint8_t           *rx_buf,
    uint16_t           rx_len,
    I2C_SVC_Callback_t callback,
    void              *ctx
)
{
    /* 1. Validate parameters */
    if (((uint32_t)id >= 3U) || (rx_buf == NULL) ||
        (rx_len == 0U)       || (callback == NULL))
    {
        return I2C_SVC_ERROR_PARAM;  /* bad args */
    }

    /* 2. Check that Init was called */
    if (svc[id].initialized != 1U)
    {
        return I2C_SVC_ERROR_INIT;  /* not initialised */
    }

    /* 3. Busy guard — reject if previous transfer still active */
    if (svc[id].state != I2C_SVC_STATE_IDLE)
    {
        return I2C_SVC_ERROR_BUSY;  /* caller must wait for callback */
    }

    /* 4. Store all parameters for use by the event callback (Phase 2) */
    svc[id].user_callback  = callback;   /* completion/error notification */
    svc[id].user_ctx       = ctx;        /* opaque user context           */
    svc[id].rx_buf_pending = rx_buf;     /* DMA destination buffer        */
    svc[id].rx_len_pending = rx_len;     /* bytes to read (e.g. 14)       */
    svc[id].dev_addr       = dev_addr;   /* slave addr for Phase 2 RS     */
    svc[id].reg_addr_buf   = reg_addr;   /* stored in SRAM — survives ISR */

    /* 5. Transition to TX state (Phase 1) */
    svc[id].state = I2C_SVC_STATE_TX;

    /* 6. Build TX request — single byte (register address), NO STOP
     *    so the bus is held for the repeated START in Phase 2.
     *    tx_buf points to the instance's reg_addr_buf field in SRAM,
     *    not a local variable, so it remains valid through the ISR.   */
    I2C_TxRequest_t tx_req;
    tx_req.dev_addr = dev_addr;                /* 7-bit slave address         */
    tx_req.tx_buf   = &svc[id].reg_addr_buf;  /* single byte in SRAM         */
    tx_req.tx_len   = 1U;                      /* just the register address   */
    tx_req.stop     = I2C_STOP_DISABLE;        /* NO STOP — hold bus for RS   */

    /* 7. Start IRQ-driven transmit of register address (Phase 1).
     *    On TX_COMPLETE, i2c_svc_ev_callback will launch DMA RX.  */
    if (I2C_MasterTransmit_IT(id, &tx_req) != I2C_OK)
    {
        svc[id].state = I2C_SVC_STATE_IDLE;  /* roll back on failure   */
        svc[id].rx_buf_pending = NULL;        /* clear pending pointer  */
        return I2C_SVC_ERROR_NACK;            /* likely NACK at address */
    }

    /* 8. Return immediately — Phase 2 happens in ISR context */
    return I2C_SVC_OK;
}

/* ================================================================
 *  I2C_SVC_GetState
 *  Returns the current service-layer state for the given instance.
 * ================================================================ */
I2C_SVC_Error_t I2C_SVC_GetState(I2C_Id_t id, I2C_SVC_State_t *state)
{
    /* 1. Validate parameters */
    if ((state == NULL) || ((uint32_t)id >= 3U))
    {
        return I2C_SVC_ERROR_PARAM;  /* NULL output or bad ID */
    }

    /* 2. Read current state */
    *state = svc[id].state;  /* volatile read */

    /* 3. Success */
    return I2C_SVC_OK;
}
