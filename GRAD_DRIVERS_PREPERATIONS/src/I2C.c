/* ========================================================= */
/* ===================== Includes ========================== */
/* ========================================================= */

#include "I2C_INTERFACE.h"
#include "I2C_REGS.h"
#include "RCC_INTERFACE.h"

/* ========================================================= */
/* ================= Private Macros ======================== */
/* ========================================================= */

#define I2C_MAX_INSTANCES       3U

#define I2C_SPEED_SM_HZ         100000U     /* 100 kHz */
#define I2C_SPEED_FM_HZ         400000U     /* 400 kHz */

#define I2C_RISE_TIME_SM_NS     1000U       /* 1000 ns for Standard Mode */
#define I2C_RISE_TIME_FM_NS     300U        /* 300 ns for Fast Mode */

/* APB1 clock limits for I2C per RM0368 */
#define I2C_PCLK1_MIN_MHZ      2U
#define I2C_PCLK1_MAX_MHZ      42U

/* SR1 bit positions */
#define I2C_SR1_SB              0U
#define I2C_SR1_ADDR            1U
#define I2C_SR1_BTF             2U
#define I2C_SR1_STOPF           4U
#define I2C_SR1_RXNE            6U
#define I2C_SR1_TXE             7U
#define I2C_SR1_BERR            8U
#define I2C_SR1_ARLO            9U
#define I2C_SR1_AF              10U
#define I2C_SR1_OVR             11U

/* ========================================================= */
/* ================= Private Types ========================= */
/* ========================================================= */

typedef enum
{
    I2C_IT_OP_NONE = 0U,   /* I2C-I01 pattern: explicit U-suffixed values */
    I2C_IT_OP_TX   = 1U,
    I2C_IT_OP_RX   = 2U,
    I2C_IT_OP_TXRX = 3U
} I2C_IT_Op_t;

typedef enum
{
    I2C_IT_PHASE_IDLE     = 0U,

    /* TX phases */
    I2C_IT_PHASE_START_W  = 1U,
    I2C_IT_PHASE_ADDR_W   = 2U,
    I2C_IT_PHASE_TX       = 3U,

    /* Repeated start for TXRX */
    I2C_IT_PHASE_REPSTART = 4U,
    I2C_IT_PHASE_ADDR_R   = 5U,

    /* RX phases */
    I2C_IT_PHASE_START_R  = 6U,   /* I2C-C13: dedicated RX start phase for clarity */
    I2C_IT_PHASE_RX       = 7U,

    I2C_IT_PHASE_DONE     = 8U,
    I2C_IT_PHASE_ERROR    = 9U
} I2C_IT_Phase_t;

typedef struct {
    I2C_Callback_t        callback;
    void                 *context;
    volatile I2C_State_t  state;
    I2C_TransferMode_t    transfer_mode;

    volatile uint8_t      it_active;
    volatile I2C_IT_Op_t  it_op;        /* volatile — read/written across ISR boundaries */
    volatile I2C_IT_Phase_t it_phase;   /* volatile — set by ADDR handler, read by TXE/BTF */

    volatile I2C_Status_t it_status;
    volatile I2C_Error_t  it_last_error;

    I2C_DevAddr7_t        dev_addr;
    I2C_Stop_t            stop_after;

    const uint8_t        *tx_buf;
    volatile uint16_t     tx_len;       /* volatile — read by TXE handler */
    volatile uint16_t     tx_idx;       /* volatile — incremented by TXE handler */

    uint8_t              *rx_buf;
    volatile uint16_t     rx_len;       /* volatile — read by RXNE/BTF handlers */
    volatile uint16_t     rx_idx;       /* volatile — incremented by RXNE handler */
    volatile uint16_t     rx_remaining; /* volatile — decremented by RXNE/BTF handlers */
} I2C_Handle_t;


/* ========================================================= */
/* ================= Private Variables ===================== */
/* ========================================================= */

/*
 * I2C-C01: Array stores volatile pointers — matches the volatile cast
 * in I2C1/I2C2/I2C3 macros after I2C-R03 fix.
 */
static volatile I2C_REGS_t* const I2C_Instances[I2C_MAX_INSTANCES] = {
    I2C1,   /* I2C_ID_1 = 0 */
    I2C2,   /* I2C_ID_2 = 1 */
    I2C3    /* I2C_ID_3 = 2 */
};

/* Maps I2C_Id_t to RCC peripheral enum for clock queries */
static const RCC_Peripheral_t I2C_RCC_Periph[I2C_MAX_INSTANCES] = {
    PERIPH_I2C1,   /* I2C_ID_1 = 0 */
    PERIPH_I2C2,   /* I2C_ID_2 = 1 */
    PERIPH_I2C3    /* I2C_ID_3 = 2 */
};

/* Runtime data for each I2C instance */
static I2C_Handle_t I2C_Handles[I2C_MAX_INSTANCES] = {
    { .callback = NULL, .context = NULL, .state = I2C_STATE_IDLE, .transfer_mode = I2C_MODE_POLLING },
    { .callback = NULL, .context = NULL, .state = I2C_STATE_IDLE, .transfer_mode = I2C_MODE_POLLING },
    { .callback = NULL, .context = NULL, .state = I2C_STATE_IDLE, .transfer_mode = I2C_MODE_POLLING }
};

/* ========================================================= */
/* ================= Private Helper Functions ============== */
/* ========================================================= */

/*
 * I2C-C02: Returns volatile I2C_REGS_t* so all downstream register
 * accesses through this pointer are treated as volatile by the compiler.
 */
static volatile I2C_REGS_t* GetI2CInstance(I2C_Id_t id)
{
    if (id >= I2C_MAX_INSTANCES) return NULL;
    return I2C_Instances[id];
}

static void I2C_IT_EnableIRQs(I2C_Id_t id)
{
    volatile I2C_REGS_t* i2c = GetI2CInstance(id);   /* I2C-C02 */
    if (i2c == NULL) return;

    i2c->CR2.BITS.ITEVTEN = 1U;
    i2c->CR2.BITS.ITBUFEN = 1U;
    i2c->CR2.BITS.ITERREN = 1U;
}

static void I2C_IT_DisableIRQs(I2C_Id_t id)
{
    volatile I2C_REGS_t* i2c = GetI2CInstance(id);   /* I2C-C02 */
    if (i2c == NULL) return;

    i2c->CR2.BITS.ITEVTEN = 0U;
    i2c->CR2.BITS.ITBUFEN = 0U;
    i2c->CR2.BITS.ITERREN = 0U;
}

static void I2C_IT_Complete(I2C_Id_t id, I2C_Event_t evt, I2C_Error_t err)
{
    I2C_Handle_t* h = &I2C_Handles[id];

    h->it_active = 0U;
    h->it_op = I2C_IT_OP_NONE;
    h->it_phase = (err == I2C_OK) ? I2C_IT_PHASE_DONE : I2C_IT_PHASE_ERROR;

    h->it_last_error = err;
    h->it_status = (err == I2C_OK) ? I2C_STATUS_DONE : I2C_STATUS_ERROR;

    h->state = I2C_STATE_IDLE;

    I2C_IT_DisableIRQs(id);

    /* Release bus if needed */
    if (err != I2C_OK)
    {
        (void)I2C_GenerateStop(id);
    }

    if (h->callback != NULL)
    {
        h->callback(evt, h->context);
    }
}

/* ========================================================= */
/* ================= Initialization ======================== */
/* ========================================================= */

I2C_Error_t I2C_Init(const I2C_Config_t *cfg)
{
    /* Step 1: Validate config pointer */
    if (cfg == NULL) return I2C_ERROR_INVALID_PARAM;

    /* Step 2: Get hardware pointer */
    volatile I2C_REGS_t* I2Cx = GetI2CInstance(cfg->id);
    if (I2Cx == NULL) return I2C_ERROR_INVALID_I2C;

    /* Step 3: Get PCLK1 frequency from RCC */
    uint32_t pclk1_freq;
    if (GET_PCLK_FREQ(I2C_RCC_Periph[cfg->id], &pclk1_freq) != RCC_OK)
    {
        return I2C_ERROR_INVALID_PARAM;
    }
    uint32_t pclk1_mhz = pclk1_freq / 1000000U;

    if (pclk1_mhz < I2C_PCLK1_MIN_MHZ || pclk1_mhz > I2C_PCLK1_MAX_MHZ)
    {
        return I2C_ERROR_INVALID_PARAM;
    }

    /* Step 4: Disable peripheral before configuration
     * Per RM0368 §27.3.3: CCR and TRISE must only be configured when PE=0. */
    I2Cx->CR1.BITS.PE = 0U;

    /* Small delay after PE=0 — peripheral needs time to release bus */
    {
        volatile uint32_t pe_delay;
        for (pe_delay = 0U; pe_delay < 1000U; pe_delay++)
        {
            __asm volatile ("nop");
        }
    }

    /* Step 5: Software reset — STM32F4 BUSY-stuck errata workaround
     *
     * ES0182 Section 2.14.7: The I2C analog filters can latch a
     * false BUSY flag if SWRST is released too quickly. A delay
     * between SWRST=1 and SWRST=0 ensures the internal state
     * machine and analog filters fully reset.
     *
     * After releasing SWRST, a second delay allows the filters to
     * sample the idle (HIGH) bus state before any configuration.
     */
  I2Cx->CR1.BITS.SWRST = 1U;
    { volatile uint32_t d; for (d = 0U; d < 5000U; d++) __asm volatile("nop"); }
    I2Cx->CR1.BITS.SWRST = 0U;
    { volatile uint32_t d; for (d = 0U; d < 5000U; d++) __asm volatile("nop"); }

    /* Step 6: Configure CR2.FREQ (PCLK1 frequency in MHz) */
    I2Cx->CR2.BITS.FREQ = pclk1_mhz;

    /* Step 7: Configure CCR (Clock Control Register) */
    uint32_t ccr_value = 0U;

    if (cfg->speed == I2C_SPEED_STANDARD)
    {
        ccr_value = pclk1_freq / (2U * I2C_SPEED_SM_HZ);
        if (ccr_value < 4U) ccr_value = 4U;
        I2Cx->CCR.BITS.FS = 0U;
        I2Cx->CCR.BITS.DUTY = 0U;
    }
    else
    {
        I2Cx->CCR.BITS.FS = 1U;

        if (cfg->duty_cycle == I2C_DUTY_2)
        {
            ccr_value = pclk1_freq / (3U * I2C_SPEED_FM_HZ);
            I2Cx->CCR.BITS.DUTY = 0U;
        }
        else
        {
            ccr_value = pclk1_freq / (25U * I2C_SPEED_FM_HZ);
            I2Cx->CCR.BITS.DUTY = 1U;
        }

        if (ccr_value < 1U) ccr_value = 1U;
    }

    I2Cx->CCR.BITS.CCR = ccr_value;

    /* Step 8: Configure TRISE */
    uint32_t trise_value;

    if (cfg->speed == I2C_SPEED_STANDARD)
    {
        trise_value = pclk1_mhz + 1U;
    }
    else
    {
        trise_value = ((pclk1_mhz * 300U) / 1000U) + 1U;
    }

    I2Cx->TRISE.BITS.TRISE = trise_value;

    /* Step 9: Configure FLTR (Filters) */
    I2Cx->FLTR.BITS.ANOFF = (cfg->analog_filter == I2C_ANALOG_FILTER_DISABLE) ? 1U : 0U;
    I2Cx->FLTR.BITS.DNF = cfg->digital_filter;

    /* Step 10: Configure OAR1 (Own Address Register 1) */
    I2Cx->OAR1.BITS.SHOULDBE1 = 1U;
    I2Cx->OAR1.BITS.ADDMODE = (cfg->addressing_mode == I2C_ADDR_10BIT) ? 1U : 0U;

    if (cfg->addressing_mode == I2C_ADDR_7BIT)
    {
        I2Cx->OAR1.BITS.ADD7_1 = cfg->own_address1 & 0x7FU;
    }
    else
    {
        I2Cx->OAR1.BITS.ADD0 = cfg->own_address1 & 0x01U;
        I2Cx->OAR1.BITS.ADD7_1 = (cfg->own_address1 >> 1U) & 0x7FU;
        I2Cx->OAR1.BITS.ADD9_8 = (cfg->own_address1 >> 8U) & 0x03U;
    }

    /* Step 11: Configure OAR2 (Own Address Register 2) */
    if (cfg->dual_address_mode == I2C_DUAL_ADDR_ENABLE)
    {
        I2Cx->OAR2.BITS.ENDUAL = 1U;
        I2Cx->OAR2.BITS.ADD2 = cfg->own_address2 & 0x7FU;
    }
    else
    {
        I2Cx->OAR2.BITS.ENDUAL = 0U;
    }

    /* Step 12: Configure CR1 options */
    I2Cx->CR1.BITS.ENGC = (cfg->general_call_mode == I2C_GENERAL_CALL_ENABLE) ? 1U : 0U;
    I2Cx->CR1.BITS.NOSTRETCH = (cfg->clock_stretching == I2C_CLOCK_STRETCH_DISABLE) ? 1U : 0U;

    /* Step 13: Disable all interrupts and DMA */
    I2Cx->CR2.BITS.ITEVTEN = 0U;
    I2Cx->CR2.BITS.ITBUFEN = 0U;
    I2Cx->CR2.BITS.ITERREN = 0U;
    I2Cx->CR2.BITS.DMAEN = 0U;
    I2Cx->CR2.BITS.LAST = 0U;

    /* Step 14: Store runtime data in handle */
    I2C_Handles[cfg->id].state = I2C_STATE_IDLE;
    I2C_Handles[cfg->id].transfer_mode = cfg->transfer_mode;
    I2C_Handles[cfg->id].callback = NULL;
    I2C_Handles[cfg->id].context = NULL;

    /* Step 15: Enable peripheral */
    I2Cx->CR1.BITS.PE = 1U;

    /* Step 16: Verify BUSY flag is clear — ES0182 errata guard
     *
     * If BUSY=1 immediately after PE=1 with idle bus, the analog
     * filter glitch occurred. Retry SWRST with full delays.
     * Limited to 3 retries to avoid infinite loop.
     */
    {
        uint8_t retry;
        for (retry = 0U; retry < 3U; retry++)
        {
            volatile uint32_t settle;
            for (settle = 0U; settle < 1000U; settle++)
            {
                __asm volatile ("nop");
            }

            if (I2Cx->SR2.BITS.BUSY == 0U)
            {
                break;  /* BUSY clear — success */
            }

            /* BUSY stuck — retry full SWRST sequence */
            I2Cx->CR1.BITS.PE = 0U;

            for (settle = 0U; settle < 1000U; settle++)
            {
                __asm volatile ("nop");
            }

            I2Cx->CR1.BITS.SWRST = 1U;

            for (settle = 0U; settle < 5000U; settle++)
            {
                __asm volatile ("nop");
            }

            I2Cx->CR1.BITS.SWRST = 0U;

            for (settle = 0U; settle < 5000U; settle++)
            {
                __asm volatile ("nop");
            }

            /* Re-enable peripheral */
            I2Cx->CR1.BITS.PE = 1U;
        }

        if (I2Cx->SR2.BITS.BUSY != 0U)
        {
            return I2C_ERROR_BUSY;
        }
    }

    return I2C_OK;
}

/*
 * I2C-C12: Renamed from I2C_GetStatus to I2C_GetXferStatus to match
 * the prototype declared in I2C_INTERFACE.h. The old name had no
 * corresponding header declaration, causing linker failures.
 */
I2C_Status_t I2C_GetXferStatus(I2C_Id_t id)
{
    if (id >= I2C_MAX_INSTANCES) return I2C_STATUS_ERROR;
    return I2C_Handles[id].it_status;
}

I2C_Error_t I2C_GetLastError(I2C_Id_t id)
{
    if (id >= I2C_MAX_INSTANCES) return I2C_ERROR_INVALID_I2C;
    return I2C_Handles[id].it_last_error;
}

I2C_Error_t I2C_Abort_IT(I2C_Id_t id)
{
    if (id >= I2C_MAX_INSTANCES) return I2C_ERROR_INVALID_I2C;

    I2C_Handle_t* h = &I2C_Handles[id];
    h->it_last_error = I2C_OK;

    I2C_IT_DisableIRQs(id);
    (void)I2C_GenerateStop(id);

    h->it_active = 0U;
    h->it_op = I2C_IT_OP_NONE;
    h->it_phase = I2C_IT_PHASE_IDLE;
    h->it_status = I2C_STATUS_IDLE;

    h->state = I2C_STATE_IDLE;

    return I2C_OK;
}
/* ================================================================
 * I2C_ResetHandle
 * Resets the driver handle state machine to IDLE without touching
 * the I2C bus (no STOP generated). Used by the service layer after
 * DMA RX completes — the hardware has already issued NACK+STOP via
 * CR2.LAST, so generating another STOP would be incorrect.
 *
 * Contrast with I2C_Abort_IT which generates a STOP on the bus.
 * ================================================================ */
I2C_Error_t I2C_ResetHandle(I2C_Id_t id)
{
    if (id >= I2C_MAX_INSTANCES) return I2C_ERROR_INVALID_I2C;

    I2C_Handle_t* h = &I2C_Handles[id];

    I2C_IT_DisableIRQs(id);       /* disable ITEVTEN/ITBUFEN/ITERREN */

    h->it_active    = 0U;
    h->it_op        = I2C_IT_OP_NONE;
    h->it_phase     = I2C_IT_PHASE_IDLE;
    h->it_status    = I2C_STATUS_IDLE;
    h->it_last_error = I2C_OK;
    h->state        = I2C_STATE_IDLE;

    return I2C_OK;
}
I2C_Error_t I2C_MasterTransmit_IT(I2C_Id_t id, const I2C_TxRequest_t *req)
{
    if (req == NULL || req->tx_buf == NULL || req->tx_len == 0U) return I2C_ERROR_INVALID_PARAM;
    if (id >= I2C_MAX_INSTANCES) return I2C_ERROR_INVALID_I2C;

    I2C_Handle_t* h = &I2C_Handles[id];
    volatile I2C_REGS_t* i2c = GetI2CInstance(id);   /* I2C-C02 */
    if (i2c == NULL) return I2C_ERROR_INVALID_I2C;

    /* Allow IRQ TX in both I2C_MODE_IRQ and I2C_MODE_DMA.
     * DMA mode controls RX burst reads only — small TX writes
     * (e.g. 2-byte register config) always use interrupts
     * regardless of the configured RX transfer mode.
     * Only reject I2C_MODE_POLLING which has its own blocking path. */
    if (h->transfer_mode == I2C_MODE_POLLING) return I2C_ERROR_UNSUPPORTED;
    if (h->it_active || h->state != I2C_STATE_IDLE) return I2C_ERROR_BUSY;

    h->it_active = 1U;
    h->it_op = I2C_IT_OP_TX;
    h->it_phase = I2C_IT_PHASE_START_W;

    h->it_status = I2C_STATUS_BUSY;
    h->it_last_error = I2C_OK;

    h->dev_addr = req->dev_addr;
    h->stop_after = req->stop;

    h->tx_buf = req->tx_buf;
    h->tx_len = req->tx_len;
    h->tx_idx = 0U;

    h->rx_buf = NULL;
    h->rx_len = 0U;
    h->rx_idx = 0U;
    h->rx_remaining = 0U;

    h->state = I2C_STATE_BUSY_TX;

    /* Safe defaults */
    i2c->CR1.BITS.POS = 0U;
    I2C_EnableACK(id); /* doesn't hurt in TX */

    I2C_IT_EnableIRQs(id);
    I2C_GenerateStart(id);

    return I2C_OK;
}

I2C_Error_t I2C_MasterReceive_IT(I2C_Id_t id, const I2C_RxRequest_t *req)
{
    if (req == NULL || req->rx_buf == NULL || req->rx_len == 0U) return I2C_ERROR_INVALID_PARAM;
    if (id >= I2C_MAX_INSTANCES) return I2C_ERROR_INVALID_I2C;

    I2C_Handle_t* h = &I2C_Handles[id];
    volatile I2C_REGS_t* i2c = GetI2CInstance(id);   /* I2C-C02 */
    if (i2c == NULL) return I2C_ERROR_INVALID_I2C;

    /* Allow IRQ RX in both I2C_MODE_IRQ and I2C_MODE_DMA.
     * IRQ-driven receives (repeated START phase in WriteRead sequence)
     * must work when the handle is in DMA mode. DMA mode only changes
     * how data bytes are pulled from DR — the START/ADDR/STOP phases
     * are always interrupt-driven. Only reject I2C_MODE_POLLING. */
    if (h->transfer_mode == I2C_MODE_POLLING) return I2C_ERROR_UNSUPPORTED;
    if (h->it_active || h->state != I2C_STATE_IDLE) return I2C_ERROR_BUSY;

    h->it_active = 1U;
    h->it_op = I2C_IT_OP_RX;

    /*
     * I2C-C13: Use dedicated I2C_IT_PHASE_START_R instead of reusing
     * I2C_IT_PHASE_START_W. The SB handler differentiates TX vs RX by
     * checking it_op, but using a dedicated phase value makes the state
     * machine self-documenting and easier to debug.
     */
    h->it_phase = I2C_IT_PHASE_START_R;

    h->it_status = I2C_STATUS_BUSY;
    h->it_last_error = I2C_OK;

    h->dev_addr = req->dev_addr;
    h->stop_after = req->stop;

    h->rx_buf = req->rx_buf;
    h->rx_len = req->rx_len;
    h->rx_idx = 0U;
    h->rx_remaining = req->rx_len;

    h->tx_buf = NULL;
    h->tx_len = 0U;
    h->tx_idx = 0U;

    h->state = I2C_STATE_BUSY_RX;

    /* RX defaults */
    i2c->CR1.BITS.POS = 0U;
    if (req->rx_len > 1U) I2C_EnableACK(id);
    else                   I2C_DisableACK(id);

    I2C_IT_EnableIRQs(id);
    I2C_GenerateStart(id);

    return I2C_OK;
}

I2C_Error_t I2C_MasterWriteRead_IT(I2C_Id_t id, const I2C_TxRxRequest_t *req)
{
    if (req == NULL || req->tx_buf == NULL || req->tx_len == 0U || req->rx_buf == NULL || req->rx_len == 0U)
        return I2C_ERROR_INVALID_PARAM;

    if (id >= I2C_MAX_INSTANCES) return I2C_ERROR_INVALID_I2C;

    I2C_Handle_t* h = &I2C_Handles[id];
    volatile I2C_REGS_t* i2c = GetI2CInstance(id);   /* I2C-C02 */
    if (i2c == NULL) return I2C_ERROR_INVALID_I2C;

    if (h->transfer_mode != I2C_MODE_IRQ) return I2C_ERROR_UNSUPPORTED;
    if (h->it_active || h->state != I2C_STATE_IDLE) return I2C_ERROR_BUSY;

    h->it_active = 1U;
    h->it_op = I2C_IT_OP_TXRX;
    h->it_phase = I2C_IT_PHASE_START_W;

    h->it_status = I2C_STATUS_BUSY;
    h->it_last_error = I2C_OK;

    h->dev_addr = req->dev_addr;
    h->stop_after = I2C_STOP_ENABLE; /* for TXRX we typically stop at the end */

    h->tx_buf = req->tx_buf;
    h->tx_len = req->tx_len;
    h->tx_idx = 0U;

    h->rx_buf = req->rx_buf;
    h->rx_len = req->rx_len;
    h->rx_idx = 0U;
    h->rx_remaining = req->rx_len;

    h->state = I2C_STATE_BUSY_TXRX;

    i2c->CR1.BITS.POS = 0U;
    I2C_EnableACK(id); /* will be adjusted when RX starts */

    I2C_IT_EnableIRQs(id);
    I2C_GenerateStart(id);

    return I2C_OK;
}

/* ========================================================= */
/* ================= Status Flag Functions ================= */
/* ========================================================= */

/*
 * I2C-I02 / I2C-C02: All status flag functions now return I2C_Error_t
 * with a uint8_t *state output parameter. A return of 0 no longer
 * conflates "flag not set" with "invalid I2C ID".
 */

I2C_Error_t I2C_IsTxEmpty(I2C_Id_t i2c_id, uint8_t *state)
{
    if (state == NULL) return I2C_ERROR_INVALID_PARAM;
    volatile I2C_REGS_t* I2Cx = GetI2CInstance(i2c_id);   /* I2C-C02 */
    if (I2Cx == NULL) return I2C_ERROR_INVALID_I2C;
    *state = (uint8_t)(I2Cx->SR1.BITS.TXE);
    return I2C_OK;
}

I2C_Error_t I2C_IsRxNotEmpty(I2C_Id_t i2c_id, uint8_t *state)
{
    if (state == NULL) return I2C_ERROR_INVALID_PARAM;
    volatile I2C_REGS_t* I2Cx = GetI2CInstance(i2c_id);   /* I2C-C02 */
    if (I2Cx == NULL) return I2C_ERROR_INVALID_I2C;
    *state = (uint8_t)(I2Cx->SR1.BITS.RXNE);
    return I2C_OK;
}

I2C_Error_t I2C_IsBusy(I2C_Id_t i2c_id, uint8_t *state)
{
    if (state == NULL) return I2C_ERROR_INVALID_PARAM;
    volatile I2C_REGS_t* I2Cx = GetI2CInstance(i2c_id);   /* I2C-C02 */
    if (I2Cx == NULL) return I2C_ERROR_INVALID_I2C;
    *state = (uint8_t)(I2Cx->SR2.BITS.BUSY);
    return I2C_OK;
}

I2C_Error_t I2C_IsStartGenerated(I2C_Id_t i2c_id, uint8_t *state)
{
    if (state == NULL) return I2C_ERROR_INVALID_PARAM;
    volatile I2C_REGS_t* I2Cx = GetI2CInstance(i2c_id);   /* I2C-C02 */
    if (I2Cx == NULL) return I2C_ERROR_INVALID_I2C;
    *state = (uint8_t)(I2Cx->SR1.BITS.SB);
    return I2C_OK;
}

I2C_Error_t I2C_IsAddrSent(I2C_Id_t i2c_id, uint8_t *state)
{
    if (state == NULL) return I2C_ERROR_INVALID_PARAM;
    volatile I2C_REGS_t* I2Cx = GetI2CInstance(i2c_id);   /* I2C-C02 */
    if (I2Cx == NULL) return I2C_ERROR_INVALID_I2C;
    *state = (uint8_t)(I2Cx->SR1.BITS.ADDR);
    return I2C_OK;
}

I2C_Error_t I2C_IsBTF(I2C_Id_t i2c_id, uint8_t *state)
{
    if (state == NULL) return I2C_ERROR_INVALID_PARAM;
    volatile I2C_REGS_t* I2Cx = GetI2CInstance(i2c_id);   /* I2C-C02 */
    if (I2Cx == NULL) return I2C_ERROR_INVALID_I2C;
    *state = (uint8_t)(I2Cx->SR1.BITS.BTF);
    return I2C_OK;
}

I2C_Error_t I2C_IsStopDetected(I2C_Id_t i2c_id, uint8_t *state)
{
    if (state == NULL) return I2C_ERROR_INVALID_PARAM;
    volatile I2C_REGS_t* I2Cx = GetI2CInstance(i2c_id);   /* I2C-C02 */
    if (I2Cx == NULL) return I2C_ERROR_INVALID_I2C;
    *state = (uint8_t)(I2Cx->SR1.BITS.STOPF);
    return I2C_OK;
}

/* ========================================================= */
/* ================= Data Register Access ================== */
/* ========================================================= */

I2C_Error_t I2C_WriteDR(I2C_Id_t i2c_id, uint8_t data)
{
    volatile I2C_REGS_t* I2Cx = GetI2CInstance(i2c_id);   /* I2C-C02 */
    if (I2Cx == NULL) return I2C_ERROR_INVALID_I2C;

    I2Cx->DR.BITS.DR = data;
    return I2C_OK;
}

I2C_Error_t I2C_ReadDR(I2C_Id_t i2c_id, uint8_t *data)
{
    volatile I2C_REGS_t* I2Cx = GetI2CInstance(i2c_id);   /* I2C-C02 */
    if (I2Cx == NULL) return I2C_ERROR_INVALID_I2C;
    if (data == NULL) return I2C_ERROR_INVALID_PARAM;

    *data = (uint8_t)I2Cx->DR.BITS.DR;
    return I2C_OK;
}

uint32_t I2C_GetDRAddress(I2C_Id_t i2c_id)
{
    volatile I2C_REGS_t* I2Cx = GetI2CInstance(i2c_id);   /* I2C-C02 */
    if (I2Cx == NULL) return 0U;

    return (uint32_t)&(I2Cx->DR.ALL);
}

/* ========================================================= */
/* ================= Bus Control Functions ================= */
/* ========================================================= */

/*
 * I2C-C09: I2C_GenerateStart and I2C_GenerateStop are intentionally
 * non-blocking — they set the hardware bit and return immediately.
 * In IRQ mode the caller must NOT poll; the SB/STOPF flag will fire
 * an interrupt. In polling context the caller is responsible for
 * subsequently polling the appropriate SR1 flag with a timeout.
 */

I2C_Error_t I2C_GenerateStart(I2C_Id_t i2c_id)
{
    volatile I2C_REGS_t* I2Cx = GetI2CInstance(i2c_id);   /* I2C-C02 */
    if (I2Cx == NULL) return I2C_ERROR_INVALID_I2C;

    I2Cx->CR1.BITS.START = 1U;
    return I2C_OK;
}

I2C_Error_t I2C_GenerateStop(I2C_Id_t i2c_id)
{
    volatile I2C_REGS_t* I2Cx = GetI2CInstance(i2c_id);   /* I2C-C02 */
    if (I2Cx == NULL) return I2C_ERROR_INVALID_I2C;

    I2Cx->CR1.BITS.STOP = 1U;
    return I2C_OK;
}

I2C_Error_t I2C_SendAddress(I2C_Id_t i2c_id, uint16_t addr, uint8_t is_read)
{
    volatile I2C_REGS_t* I2Cx = GetI2CInstance(i2c_id);   /* I2C-C02 */
    if (I2Cx == NULL) return I2C_ERROR_INVALID_I2C;

    /* 7-bit addressing: shift address left by 1 and add R/W bit */
    uint8_t address_byte = (uint8_t)((addr << 1U) | (is_read & 0x01U));

    I2Cx->DR.BITS.DR = address_byte;
    return I2C_OK;
}

I2C_Error_t I2C_ClearAddrFlag(I2C_Id_t i2c_id)
{
    volatile I2C_REGS_t* I2Cx = GetI2CInstance(i2c_id);   /* I2C-C02 */
    if (I2Cx == NULL) return I2C_ERROR_INVALID_I2C;

    /*
     * I2C-C10: Clear ADDR flag by reading SR1 then SR2 per RM0368.
     * Note: In IRQ context the handler may have already read SR1 at the
     * top of the ISR — this re-read of SR1 is harmless but redundant.
     * Both reads are required here to guarantee the clear sequence works
     * correctly when called from non-IRQ paths (e.g. polling mode).
     */
    volatile uint32_t dummy;
    dummy = I2Cx->SR1.ALL;
    dummy = I2Cx->SR2.ALL;
    (void)dummy;

    return I2C_OK;
}

I2C_Error_t I2C_EnableACK(I2C_Id_t i2c_id)
{
    volatile I2C_REGS_t* I2Cx = GetI2CInstance(i2c_id);   /* I2C-C02 */
    if (I2Cx == NULL) return I2C_ERROR_INVALID_I2C;

    I2Cx->CR1.BITS.ACK = 1U;
    return I2C_OK;
}

I2C_Error_t I2C_DisableACK(I2C_Id_t i2c_id)
{
    volatile I2C_REGS_t* I2Cx = GetI2CInstance(i2c_id);   /* I2C-C02 */
    if (I2Cx == NULL) return I2C_ERROR_INVALID_I2C;

    I2Cx->CR1.BITS.ACK = 0U;
    return I2C_OK;
}

/* ========================================================= */
/* ================= Interrupt Control ===================== */
/* ========================================================= */

I2C_Error_t I2C_EnableEventIRQ(I2C_Id_t i2c_id)
{
    if (i2c_id >= I2C_MAX_INSTANCES) return I2C_ERROR_INVALID_I2C;

    if (I2C_Handles[i2c_id].transfer_mode != I2C_MODE_IRQ)
        return I2C_ERROR_UNSUPPORTED;

    volatile I2C_REGS_t* I2Cx = GetI2CInstance(i2c_id);   /* I2C-C02 */
    I2Cx->CR2.BITS.ITEVTEN = 1U;
    return I2C_OK;
}

I2C_Error_t I2C_DisableEventIRQ(I2C_Id_t i2c_id)
{
    volatile I2C_REGS_t* I2Cx = GetI2CInstance(i2c_id);   /* I2C-C02 */
    if (I2Cx == NULL) return I2C_ERROR_INVALID_I2C;

    I2Cx->CR2.BITS.ITEVTEN = 0U;
    return I2C_OK;
}

I2C_Error_t I2C_EnableBufferIRQ(I2C_Id_t i2c_id)
{
    if (i2c_id >= I2C_MAX_INSTANCES) return I2C_ERROR_INVALID_I2C;

    if (I2C_Handles[i2c_id].transfer_mode != I2C_MODE_IRQ)
        return I2C_ERROR_UNSUPPORTED;

    volatile I2C_REGS_t* I2Cx = GetI2CInstance(i2c_id);   /* I2C-C02 */
    I2Cx->CR2.BITS.ITBUFEN = 1U;
    return I2C_OK;
}

I2C_Error_t I2C_DisableBufferIRQ(I2C_Id_t i2c_id)
{
    volatile I2C_REGS_t* I2Cx = GetI2CInstance(i2c_id);   /* I2C-C02 */
    if (I2Cx == NULL) return I2C_ERROR_INVALID_I2C;

    I2Cx->CR2.BITS.ITBUFEN = 0U;
    return I2C_OK;
}

I2C_Error_t I2C_EnableErrorIRQ(I2C_Id_t i2c_id)
{
    volatile I2C_REGS_t* I2Cx = GetI2CInstance(i2c_id);   /* I2C-C02 */
    if (I2Cx == NULL) return I2C_ERROR_INVALID_I2C;

    I2Cx->CR2.BITS.ITERREN = 1U;
    return I2C_OK;
}

I2C_Error_t I2C_DisableErrorIRQ(I2C_Id_t i2c_id)
{
    volatile I2C_REGS_t* I2Cx = GetI2CInstance(i2c_id);   /* I2C-C02 */
    if (I2Cx == NULL) return I2C_ERROR_INVALID_I2C;

    I2Cx->CR2.BITS.ITERREN = 0U;
    return I2C_OK;
}

/* ========================================================= */
/* ================= DMA Control =========================== */
/* ========================================================= */

I2C_Error_t I2C_EnableDMA(I2C_Id_t i2c_id)
{
    if (i2c_id >= I2C_MAX_INSTANCES) return I2C_ERROR_INVALID_I2C;

    if (I2C_Handles[i2c_id].transfer_mode != I2C_MODE_DMA)
        return I2C_ERROR_UNSUPPORTED;

    volatile I2C_REGS_t* I2Cx = GetI2CInstance(i2c_id);   /* I2C-C02 */
    I2Cx->CR2.BITS.DMAEN = 1U;
    return I2C_OK;
}

I2C_Error_t I2C_DisableDMA(I2C_Id_t i2c_id)
{
    volatile I2C_REGS_t* I2Cx = GetI2CInstance(i2c_id);   /* I2C-C02 */
    if (I2Cx == NULL) return I2C_ERROR_INVALID_I2C;

    I2Cx->CR2.BITS.DMAEN = 0U;
    return I2C_OK;
}

I2C_Error_t I2C_EnableDMALast(I2C_Id_t i2c_id)
{
    if (i2c_id >= I2C_MAX_INSTANCES) return I2C_ERROR_INVALID_I2C;

    if (I2C_Handles[i2c_id].transfer_mode != I2C_MODE_DMA)
        return I2C_ERROR_UNSUPPORTED;

    volatile I2C_REGS_t* I2Cx = GetI2CInstance(i2c_id);   /* I2C-C02 */
    I2Cx->CR2.BITS.LAST = 1U;
    return I2C_OK;
}

I2C_Error_t I2C_DisableDMALast(I2C_Id_t i2c_id)
{
    volatile I2C_REGS_t* I2Cx = GetI2CInstance(i2c_id);   /* I2C-C02 */
    if (I2Cx == NULL) return I2C_ERROR_INVALID_I2C;

    I2Cx->CR2.BITS.LAST = 0U;
    return I2C_OK;
}

/* ========================================================= */
/* ================= Callback Registration ================= */
/* ========================================================= */

I2C_Error_t I2C_RegisterCallback(I2C_Id_t i2c_id, I2C_Callback_t cb, void *context)
{
    if (i2c_id >= I2C_MAX_INSTANCES) return I2C_ERROR_INVALID_I2C;

    I2C_Handles[i2c_id].callback = cb;
    I2C_Handles[i2c_id].context = context;

    return I2C_OK;
}

/* ========================================================= */
/* ================= State Query =========================== */
/* ========================================================= */

I2C_State_t I2C_GetState(I2C_Id_t i2c_id)
{
    if (i2c_id >= I2C_MAX_INSTANCES) return I2C_STATE_IDLE;

    return I2C_Handles[i2c_id].state;
}

/* ========================================================= */
/* ================= IRQ Handlers ========================== */
/* ========================================================= */

static void I2C_EV_IRQHandler_Common(I2C_Id_t id)
{
    volatile I2C_REGS_t* i2c = GetI2CInstance(id);
    if (i2c == NULL) return;

    I2C_Handle_t* h = &I2C_Handles[id];

    uint32_t sr1 = i2c->SR1.ALL;

    /* ========================================================= */
    /* If IT engine active -> consume events internally           */
    /* ========================================================= */
    if (h->it_active)
    {
        /* --- SB: start condition generated --- */
        if (sr1 & (1U << I2C_SR1_SB))
        {
            if (h->it_phase == I2C_IT_PHASE_REPSTART)
            {
                I2C_SendAddress(id, h->dev_addr, 1U);
                h->it_phase = I2C_IT_PHASE_ADDR_R;
            }
            else if (h->it_op == I2C_IT_OP_TX || h->it_op == I2C_IT_OP_TXRX)
            {
                I2C_SendAddress(id, h->dev_addr, 0U);
                h->it_phase = I2C_IT_PHASE_ADDR_W;
            }
            else
            {
                I2C_SendAddress(id, h->dev_addr, 1U);
                h->it_phase = I2C_IT_PHASE_ADDR_R;
            }
            return;
        }

        /* --- ADDR: address sent/matched --- */
               
        if (sr1 & (1U << I2C_SR1_ADDR))
        {
            if (h->it_phase == I2C_IT_PHASE_ADDR_R)
            {
                /* ============================================================
                 * DMA MODE: DMA + CR2.LAST handles ACK/NACK/STOP automatically.
                 * Just clear ADDR, keep ACK enabled, then disable event IRQs
                 * so BTF/RXNE don't fire and interfere with DMA.
                 * Error IRQs (ITERREN) stay enabled for NACK/bus errors.
                 * DMA TC callback will call I2C_ResetHandle to clean up.
                 * ============================================================ */
                if (i2c->CR2.BITS.DMAEN)
                {
                    I2C_EnableACK(id);
                    i2c->CR1.BITS.POS = 0U;
                    I2C_ClearAddrFlag(id);

                    /* Kill event + buffer IRQs — DMA owns the bus now */
                    i2c->CR2.BITS.ITEVTEN = 0U;
                    i2c->CR2.BITS.ITBUFEN = 0U;

                    h->it_phase = I2C_IT_PHASE_RX;
                    return;
                }

                /* ============================================================
                 * IRQ MODE: Existing byte-by-byte RX logic (unchanged)
                 * ============================================================ */
                if (h->rx_len == 1U)
                {
                    I2C_DisableACK(id);
                    I2C_ClearAddrFlag(id);
                    if (h->stop_after == I2C_STOP_ENABLE) I2C_GenerateStop(id);
                }
                else if (h->rx_len == 2U)
                {
                    I2C_DisableACK(id);
                    i2c->CR1.BITS.POS = 0U;
                    I2C_ClearAddrFlag(id);
                }
                else
                {
                    I2C_EnableACK(id);
                    i2c->CR1.BITS.POS = 0U;
                    I2C_ClearAddrFlag(id);
                }

                h->it_phase = I2C_IT_PHASE_RX;
            }
            else
            {
                /* TX ADDR — unchanged */
                I2C_ClearAddrFlag(id);
                h->it_phase = I2C_IT_PHASE_TX;
            }
            return;
        }

        /* --- TXE: transmit buffer empty --- */
        if ((sr1 & (1U << I2C_SR1_TXE)) && i2c->CR2.BITS.ITBUFEN)
        {
            if (h->it_phase == I2C_IT_PHASE_TX)
            {
                if (h->tx_idx < h->tx_len)
                {
                    I2C_WriteDR(id, h->tx_buf[h->tx_idx++]);
                    return;  /* byte written — wait for next TXE or BTF */
                }
                else
                {
                    /* All bytes sent — disable ITBUFEN so TXE stops
                     * re-triggering the IRQ endlessly. Do NOT return:
                     * fall through to the BTF handler below which will
                     * detect tx_idx >= tx_len and complete the transfer
                     * with STOP + I2C_IT_Complete.
                     *
                     * Without this fix, TXE (which is set simultaneously
                     * with BTF after the last byte shifts out) wins the
                     * check, returns immediately, and BTF is never
                     * reached — causing an infinite ISR re-entry loop
                     * that starves main context.                          */
                    i2c->CR2.BITS.ITBUFEN = 0U;

                    /* Fall through to BTF check below ↓ */
                }
            }
            else
            {
                return;  /* TXE during non-TX phase — ignore */
            }
        }

        /* --- RXNE: receive buffer not empty --- */
        if ((sr1 & (1U << I2C_SR1_RXNE)) && i2c->CR2.BITS.ITBUFEN)
        {
            if (h->it_phase == I2C_IT_PHASE_RX)
            {
                if (h->rx_remaining > 0U)
                {
                    if (h->rx_len > 2U && h->rx_remaining <= 3U)
                    {
                        /* Leave last 3 bytes to BTF handler */
                        return;
                    }

                    I2C_ReadDR(id, &h->rx_buf[h->rx_idx++]);
                    h->rx_remaining--;

                    if (h->rx_len == 1U && h->rx_remaining == 0U)
                    {
                        I2C_IT_Complete(id, I2C_EVENT_RX_COMPLETE, I2C_OK);
                    }
                }
            }
            return;
        }

        /* --- BTF: byte transfer finished --- */
        if (sr1 & (1U << I2C_SR1_BTF))
        {
            /* TX completion */
            if (h->it_phase == I2C_IT_PHASE_TX)
            {
                if (h->tx_idx >= h->tx_len)
                {
                    if (h->it_op == I2C_IT_OP_TX)
                    {
                        if (h->stop_after == I2C_STOP_ENABLE) I2C_GenerateStop(id);
                        I2C_IT_Complete(id, I2C_EVENT_TX_COMPLETE, I2C_OK);
                        return;
                    }
                    else if (h->it_op == I2C_IT_OP_TXRX)
                    {
                        I2C_GenerateStart(id);
                        h->it_op = I2C_IT_OP_RX;
                        h->it_phase = I2C_IT_PHASE_REPSTART;

                        if (h->rx_len > 1U) I2C_EnableACK(id);
                        else                I2C_DisableACK(id);

                        return;
                    }
                }
            }

            /* RX completion paths for len>=2 (BTF is key) */
            if (h->it_phase == I2C_IT_PHASE_RX)
            {
                if (h->rx_len == 2U && h->rx_remaining == 2U)
                {
                    if (h->stop_after == I2C_STOP_ENABLE) I2C_GenerateStop(id);

                    I2C_ReadDR(id, &h->rx_buf[h->rx_idx++]);
                    I2C_ReadDR(id, &h->rx_buf[h->rx_idx++]);
                    h->rx_remaining = 0U;

                    I2C_IT_Complete(id, I2C_EVENT_RX_COMPLETE, I2C_OK);
                    return;
                }

                if (h->rx_len > 2U)
                {
                    if (h->rx_remaining == 3U)
                    {
                        I2C_DisableACK(id);

                        I2C_ReadDR(id, &h->rx_buf[h->rx_idx++]);
                        h->rx_remaining--;

                        return;
                    }
                    else if (h->rx_remaining == 2U)
                    {
                        if (h->stop_after == I2C_STOP_ENABLE) I2C_GenerateStop(id);

                        I2C_ReadDR(id, &h->rx_buf[h->rx_idx++]);
                        I2C_ReadDR(id, &h->rx_buf[h->rx_idx++]);
                        h->rx_remaining = 0U;

                        I2C_IT_Complete(id, I2C_EVENT_RX_COMPLETE, I2C_OK);
                        return;
                    }
                }
            }
        }

        return;
    }

    /* ========================================================= */
    /* Raw events mode (current behavior) for HSerial compatibility */
    /* ========================================================= */
    {
        I2C_Callback_t cb = h->callback;
        void* ctx = h->context;
        if (cb == NULL) return;

        if (sr1 & (1U << I2C_SR1_SB))   cb(I2C_EVENT_SB, ctx);
        if (sr1 & (1U << I2C_SR1_ADDR)) cb(I2C_EVENT_ADDR, ctx);
        if (sr1 & (1U << I2C_SR1_BTF))  cb(I2C_EVENT_BTF, ctx);

        if (sr1 & (1U << I2C_SR1_STOPF)) cb(I2C_EVENT_STOPF, ctx);

        if ((sr1 & (1U << I2C_SR1_RXNE)) && i2c->CR2.BITS.ITBUFEN) cb(I2C_EVENT_RXNE, ctx);
        if ((sr1 & (1U << I2C_SR1_TXE))  && i2c->CR2.BITS.ITBUFEN) cb(I2C_EVENT_TXE, ctx);
    }
}

static void I2C_ER_IRQHandler_Common(I2C_Id_t id)
{
    volatile I2C_REGS_t* i2c = GetI2CInstance(id);   /* I2C-C02 */
    if (i2c == NULL) return;

    I2C_Handle_t* h = &I2C_Handles[id];
    uint32_t sr1 = i2c->SR1.ALL;

    /* If engine active: map error and finish */
    if (h->it_active)
    {
        if (sr1 & (1U << I2C_SR1_BERR)) { i2c->SR1.BITS.BERR = 0U; I2C_IT_Complete(id, I2C_EVENT_ERROR, I2C_ERROR_BERR); return; }
        if (sr1 & (1U << I2C_SR1_ARLO)) { i2c->SR1.BITS.ARLO = 0U; I2C_IT_Complete(id, I2C_EVENT_ERROR, I2C_ERROR_ARLO); return; }
        if (sr1 & (1U << I2C_SR1_AF))   { i2c->SR1.BITS.AF   = 0U; I2C_IT_Complete(id, I2C_EVENT_ERROR, I2C_ERROR_NACK); return; }
        if (sr1 & (1U << I2C_SR1_OVR))  { i2c->SR1.BITS.OVR  = 0U; I2C_IT_Complete(id, I2C_EVENT_ERROR, I2C_ERROR_OVR);  return; }
        return;
    }

    /* Raw events mode (current behavior) */
    {
        I2C_Callback_t cb = h->callback;
        void* ctx = h->context;

        if (sr1 & (1U << I2C_SR1_BERR)) { i2c->SR1.BITS.BERR = 0U; if (cb) cb(I2C_EVENT_ERROR, ctx); }
        if (sr1 & (1U << I2C_SR1_ARLO)) { i2c->SR1.BITS.ARLO = 0U; if (cb) cb(I2C_EVENT_ERROR, ctx); }
        if (sr1 & (1U << I2C_SR1_AF))   { i2c->SR1.BITS.AF   = 0U; if (cb) cb(I2C_EVENT_ERROR, ctx); }
        if (sr1 & (1U << I2C_SR1_OVR))  { i2c->SR1.BITS.OVR  = 0U; if (cb) cb(I2C_EVENT_ERROR, ctx); }
    }
}

/* ========================================================= */
/* ================= IRQ Entry Points ====================== */
/* ========================================================= */

void I2C1_EV_IRQHandler(void)
{
    I2C_EV_IRQHandler_Common(I2C_ID_1);
}

void I2C1_ER_IRQHandler(void)
{
    I2C_ER_IRQHandler_Common(I2C_ID_1);
}

void I2C2_EV_IRQHandler(void)
{
    I2C_EV_IRQHandler_Common(I2C_ID_2);
}

void I2C2_ER_IRQHandler(void)
{
    I2C_ER_IRQHandler_Common(I2C_ID_2);
}

void I2C3_EV_IRQHandler(void)
{
    I2C_EV_IRQHandler_Common(I2C_ID_3);
}

void I2C3_ER_IRQHandler(void)
{
    I2C_ER_IRQHandler_Common(I2C_ID_3);
}