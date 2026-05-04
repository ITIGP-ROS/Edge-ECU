#include "STD_TYPES.h"
#include "DMA_INTERFACE.h"
#include "DMA_REGS.h"
/* core_cm4.h intentionally NOT included — conflicts with STD_TYPES.h uint32_t.
   Barriers implemented via inline assembly below. */

/*============================================================================
 *                    INTERNAL STATE
 *============================================================================*/

static DMA_Config_t dma_cfg[2][8];
static uint8_t dma_cfg_valid[2][8] = {0};

/*============================================================================
 *                    FLAG LOOKUP TABLES
 *============================================================================
 * D-04: Moved from DMA_REGS.h to prevent duplicate instantiation in every
 *       translation unit that includes the header.
 *============================================================================*/

/* Base bit positions: Stream 0/4=0, 1/5=6, 2/6=16, 3/7=22
   Not stored as an array — computed via DMA_GetFlagIndex() + offset macros.
   D-04: removed static const array — was unused and generated a compiler warning. */

/* Pre-computed masks — all 5 flag bits set for a given stream (clear-all) */
/* D-04: moved from DMA_REGS.h */
static const uint32_t DMA_STREAM_ALL_FLAGS[4] = {
    (DMA_ALL_FLAGS_MASK << 0U),   /* Stream 0/4 */
    (DMA_ALL_FLAGS_MASK << 6U),   /* Stream 1/5 */
    (DMA_ALL_FLAGS_MASK << 16U),  /* Stream 2/6 */
    (DMA_ALL_FLAGS_MASK << 22U)   /* Stream 3/7 */
};

/* D-04: moved from DMA_REGS.h */
static const uint32_t DMA_FEIF_MASK[4] = {
    (1U << (0U  + DMA_FLAG_FEIF_OFFSET)),
    (1U << (6U  + DMA_FLAG_FEIF_OFFSET)),
    (1U << (16U + DMA_FLAG_FEIF_OFFSET)),
    (1U << (22U + DMA_FLAG_FEIF_OFFSET))
};

/* D-04: moved from DMA_REGS.h */
static const uint32_t DMA_DMEIF_MASK[4] = {
    (1U << (0U  + DMA_FLAG_DMEIF_OFFSET)),
    (1U << (6U  + DMA_FLAG_DMEIF_OFFSET)),
    (1U << (16U + DMA_FLAG_DMEIF_OFFSET)),
    (1U << (22U + DMA_FLAG_DMEIF_OFFSET))
};

/* D-04: moved from DMA_REGS.h */
static const uint32_t DMA_TEIF_MASK[4] = {
    (1U << (0U  + DMA_FLAG_TEIF_OFFSET)),
    (1U << (6U  + DMA_FLAG_TEIF_OFFSET)),
    (1U << (16U + DMA_FLAG_TEIF_OFFSET)),
    (1U << (22U + DMA_FLAG_TEIF_OFFSET))
};

/* D-04: moved from DMA_REGS.h */
static const uint32_t DMA_HTIF_MASK[4] = {
    (1U << (0U  + DMA_FLAG_HTIF_OFFSET)),
    (1U << (6U  + DMA_FLAG_HTIF_OFFSET)),
    (1U << (16U + DMA_FLAG_HTIF_OFFSET)),
    (1U << (22U + DMA_FLAG_HTIF_OFFSET))
};

/* D-04: moved from DMA_REGS.h */
static const uint32_t DMA_TCIF_MASK[4] = {
    (1U << (0U  + DMA_FLAG_TCIF_OFFSET)),
    (1U << (6U  + DMA_FLAG_TCIF_OFFSET)),
    (1U << (16U + DMA_FLAG_TCIF_OFFSET)),
    (1U << (22U + DMA_FLAG_TCIF_OFFSET))
};

/* Combined error flags mask per stream (FEIF | DMEIF | TEIF) */
/* D-04: moved from DMA_REGS.h */
static const uint32_t DMA_ERROR_FLAGS_MASK[4] = {
    (1U << (0U  + DMA_FLAG_FEIF_OFFSET)) | (1U << (0U  + DMA_FLAG_DMEIF_OFFSET)) | (1U << (0U  + DMA_FLAG_TEIF_OFFSET)),
    (1U << (6U  + DMA_FLAG_FEIF_OFFSET)) | (1U << (6U  + DMA_FLAG_DMEIF_OFFSET)) | (1U << (6U  + DMA_FLAG_TEIF_OFFSET)),
    (1U << (16U + DMA_FLAG_FEIF_OFFSET)) | (1U << (16U + DMA_FLAG_DMEIF_OFFSET)) | (1U << (16U + DMA_FLAG_TEIF_OFFSET)),
    (1U << (22U + DMA_FLAG_FEIF_OFFSET)) | (1U << (22U + DMA_FLAG_DMEIF_OFFSET)) | (1U << (22U + DMA_FLAG_TEIF_OFFSET))
};

/*============================================================================
 *                    HELPER FUNCTIONS
 *============================================================================*/

/**
 * @brief Get the index (0-3) for flag lookup tables
 * @param stream Stream ID (0-7)
 * @return Index into flag lookup tables (0-3)
 */
static inline uint8_t DMA_GetFlagIndex(DMA_StreamId_t stream)
{
    return (uint8_t)(stream % 4U);
}

/**
 * @brief Check if stream uses LISR/LIFCR (true) or HISR/HIFCR (false)
 */
static inline uint8_t DMA_UsesLowReg(DMA_StreamId_t stream)
{
    return (stream <= DMA_STREAM_3) ? 1U : 0U;
}

/**
 * @brief Read the appropriate ISR for a stream
 */
/* D-03: parameter changed to volatile DMA_REGS_t* — DMA1/DMA2 macros now yield volatile pointer */
static inline uint32_t DMA_ReadISR(volatile DMA_REGS_t *dma, DMA_StreamId_t stream)
{
    return DMA_UsesLowReg(stream) ? dma->LISR.ALL : dma->HISR.ALL;
}

/**
 * @brief Write to the appropriate IFCR for a stream
 */
/* D-03: parameter changed to volatile DMA_REGS_t* */
static inline void DMA_WriteIFCR(volatile DMA_REGS_t *dma, DMA_StreamId_t stream, uint32_t value)
{
    if (DMA_UsesLowReg(stream)) {
        dma->LIFCR.ALL = value;
    } else {
        dma->HIFCR.ALL = value;
    }
}

/**
 * @brief Clear all flags for a stream
 */
/* D-03: parameter changed to volatile DMA_REGS_t* */
static inline void DMA_ClearAllFlags(volatile DMA_REGS_t *dma, DMA_StreamId_t stream)
{
    uint8_t idx = DMA_GetFlagIndex(stream);
    DMA_WriteIFCR(dma, stream, DMA_STREAM_ALL_FLAGS[idx]);
}

/**
 * @brief Clear error flags for a stream
 */
/* D-03: parameter changed to volatile DMA_REGS_t* */
static inline void DMA_ClearErrorFlags(volatile DMA_REGS_t *dma, DMA_StreamId_t stream)
{
    uint8_t idx = DMA_GetFlagIndex(stream);
    DMA_WriteIFCR(dma, stream, DMA_ERROR_FLAGS_MASK[idx]);
}

/**
 * @brief Clear TCIF for a stream
 */
/* D-03: parameter changed to volatile DMA_REGS_t* */
static inline void DMA_ClearTCIF(volatile DMA_REGS_t *dma, DMA_StreamId_t stream)
{
    uint8_t idx = DMA_GetFlagIndex(stream);
    DMA_WriteIFCR(dma, stream, DMA_TCIF_MASK[idx]);
}

/**
 * @brief Clear HTIF for a stream
 */
/* D-03: parameter changed to volatile DMA_REGS_t* */
static inline void DMA_ClearHTIF(volatile DMA_REGS_t *dma, DMA_StreamId_t stream)
{
    uint8_t idx = DMA_GetFlagIndex(stream);
    DMA_WriteIFCR(dma, stream, DMA_HTIF_MASK[idx]);
}

/**
 * @brief Extract flag states from ISR value
 */
typedef struct {
    uint8_t feif;
    uint8_t dmeif;
    uint8_t teif;
    uint8_t htif;
    uint8_t tcif;
} DMA_FlagState_t;

static inline DMA_FlagState_t DMA_ExtractFlags(uint32_t isr, DMA_StreamId_t stream)
{
    DMA_FlagState_t flags;
    uint8_t idx = DMA_GetFlagIndex(stream);

    flags.feif  = (isr & DMA_FEIF_MASK[idx])  ? 1U : 0U;
    flags.dmeif = (isr & DMA_DMEIF_MASK[idx]) ? 1U : 0U;
    flags.teif  = (isr & DMA_TEIF_MASK[idx])  ? 1U : 0U;
    flags.htif  = (isr & DMA_HTIF_MASK[idx])  ? 1U : 0U;
    flags.tcif  = (isr & DMA_TCIF_MASK[idx])  ? 1U : 0U;

    return flags;
}

/**
 * @brief Check if any error flag is set
 */
static inline uint8_t DMA_HasError(uint32_t isr, DMA_StreamId_t stream)
{
    uint8_t idx = DMA_GetFlagIndex(stream);
    return (isr & DMA_ERROR_FLAGS_MASK[idx]) ? 1U : 0U;
}

/*============================================================================
 *                    PUBLIC API IMPLEMENTATION
 *============================================================================*/

DMA_Error_t DMA_Init(const DMA_Config_t *cfg)
{
    volatile DMA_REGS_t   *dma;    /* D-03: volatile — DMA1/DMA2 macros yield volatile pointer */
    volatile DMA_Stream_t *stream; /* D-03: volatile — stream is a sub-region of the volatile register map */

    /* ===================== 1. Validate parameters ===================== */

    if (cfg == NULL) {
        return DMA_ERROR_INVALID_PARAM;
    }

    if ((cfg->dma_id > DMA_2) || (cfg->stream_id > DMA_STREAM_7)) {
        return DMA_ERROR_INVALID_PARAM;
    }

    if (cfg->length == 0U)  /* D-16: upper bound check removed — uint16_t cannot exceed 65535 by type */
    {
        return DMA_ERROR_INVALID_PARAM;
    }

    if ((cfg->direction == DMA_DIR_MEM_TO_MEM) &&
        (cfg->mode != DMA_MODE_NORMAL)) {
        return DMA_ERROR_UNSUPPORTED;
    }

    /* ===================== 2. Resolve DMA & Stream ===================== */

    dma    = (cfg->dma_id == DMA_1) ? DMA1 : DMA2;  /* D-03: volatile pointer assigned */
    stream = &dma->STREAM[cfg->stream_id];            /* D-03: volatile pointer assigned */

    /* ===================== 3. Disable stream ===================== */

    stream->CR.BITS.EN = 0U;
    /* D-11: timeout added — per RM0368 §10.3.9 stream may take several AHB cycles to stop.
       On FIFO error the stream can stall — infinite loop here would hang forever. */
    uint32_t dis_timeout = 100000U;
    while (stream->CR.BITS.EN != 0U)
    {
        if (--dis_timeout == 0U)
            return DMA_ERROR_HW_FAILURE;
    }

    /* ===================== 4. Clear ALL flags for THIS stream ===================== */

    DMA_ClearAllFlags(dma, cfg->stream_id);

    /* ===================== 5. Program addresses & length ===================== */

    stream->NDTR = cfg->length;
    stream->PAR  = cfg->peripheral_address;
    stream->M0AR = cfg->memory0_address;

    if (cfg->mode == DMA_MODE_DOUBLE_BUFFER) {
        stream->M1AR = cfg->memory1_address;
    }

    /* ===================== 6. FIFO configuration ===================== */

    stream->FCR.ALL = 0U;
    stream->FCR.BITS.DMDIS = 0U; /* Direct mode enable */
    stream->FCR.BITS.FEIE  = 0U; /* FIFO error interrupt disabled */

    /* ===================== 7. Configure CR ===================== */

    stream->CR.ALL = 0U;

    stream->CR.BITS.CHSEL = cfg->channel;
    stream->CR.BITS.PL    = cfg->priority;
    stream->CR.BITS.DIR   = cfg->direction;

    stream->CR.BITS.PSIZE = cfg->periph_width;
    stream->CR.BITS.MSIZE = cfg->mem_width;

    stream->CR.BITS.PINC  = cfg->periph_inc;
    stream->CR.BITS.MINC  = cfg->mem_inc;

    stream->CR.BITS.PFCTRL = (cfg->flow_controller == DMA_FLOW_PERIPHERAL) ? 1U : 0U;

    if (cfg->mode == DMA_MODE_CIRCULAR) {
        stream->CR.BITS.CIRC = 1U;
    } else if (cfg->mode == DMA_MODE_DOUBLE_BUFFER) {
        stream->CR.BITS.DBM = 1U;
    }

    /* ===================== 8. Interrupt enables ===================== */

    stream->CR.BITS.TCIE  = (cfg->complete_callback != NULL) ? 1U : 0U;
    stream->CR.BITS.HTIE  = (cfg->half_callback     != NULL) ? 1U : 0U;
    stream->CR.BITS.TEIE  = (cfg->error_callback    != NULL) ? 1U : 0U;
    stream->CR.BITS.DMEIE = (cfg->error_callback    != NULL) ? 1U : 0U;

    /* ===================== 9. Store internal config ===================== */

    dma_cfg[cfg->dma_id][cfg->stream_id] = *cfg;
    dma_cfg_valid[cfg->dma_id][cfg->stream_id] = 1U;

    return DMA_OK;
}

/* D-14: made static — only called from IRQ entry points within this file;
   external linkage is not needed and would allow accidental external calls. */
static void DMA_IRQHandler(DMA_Id_t dma_id, DMA_StreamId_t stream)
{
    volatile DMA_REGS_t *dma;  /* D-03: volatile — DMA1/DMA2 macros yield volatile pointer */
    DMA_Config_t        *cfg;
    uint32_t             isr;
    DMA_FlagState_t      flags;

    /* ===================== Resolve DMA ===================== */

    dma = (dma_id == DMA_1) ? DMA1 : DMA2;  /* D-03: volatile pointer assigned */

    if (!dma_cfg_valid[dma_id][stream]) {
        return;
    }

    cfg = &dma_cfg[dma_id][stream];

    /* ===================== Read ISR and extract flags ===================== */

    isr = DMA_ReadISR(dma, stream);
    flags = DMA_ExtractFlags(isr, stream);

    /* ===================== ERROR flags (highest priority) ===================== */

    if (flags.feif || flags.dmeif || flags.teif) {
        DMA_ClearErrorFlags(dma, stream);

        if (cfg->error_callback != NULL) {
            cfg->error_callback(DMA_EVENT_TRANSFER_ERROR, cfg->user_context);  /* D-08: event passed to callback */
        }
        return;
    }

    /* ===================== Transfer complete ===================== */

    if (flags.tcif) {
        DMA_ClearTCIF(dma, stream);

        if (cfg->complete_callback != NULL) {
            cfg->complete_callback(DMA_EVENT_TRANSFER_COMPLETE, cfg->user_context);  /* D-08: event passed to callback */
        }
        return;
    }

    /* ===================== Half transfer ===================== */

    if (flags.htif) {
        DMA_ClearHTIF(dma, stream);

        if (cfg->half_callback != NULL) {
            cfg->half_callback(DMA_EVENT_HALF_TRANSFER, cfg->user_context);  /* D-08: event passed to callback */
        }
    }
}

DMA_Error_t DMA_Start(DMA_Id_t dma, DMA_StreamId_t stream)
{
    volatile DMA_REGS_t   *dma_regs;   /* D-03: volatile — DMA1/DMA2 macros yield volatile pointer */
    volatile DMA_Stream_t *dma_stream; /* D-03: volatile — stream is a sub-region of the volatile register map */

    /* ===================== 1. Validate parameters ===================== */

    if ((dma > DMA_2) || (stream > DMA_STREAM_7)) {
        return DMA_ERROR_INVALID_PARAM;
    }

    if (!dma_cfg_valid[dma][stream]) {
        return DMA_ERROR_INVALID_PARAM;
    }

    /* ===================== 2. Resolve DMA and stream ===================== */

    dma_regs   = (dma == DMA_1) ? DMA1 : DMA2;  /* D-03: volatile pointer assigned */
    dma_stream = &dma_regs->STREAM[stream];       /* D-03: volatile pointer assigned */

    /* ===================== 3. Check if stream already enabled ===================== */

    if (dma_stream->CR.BITS.EN != 0U) {
        return DMA_ERROR_STREAM_BUSY;
    }

    /* ===================== 4. Reload NDTR ===================== */

    /* D-15: Only NDTR is reloaded here. Memory/peripheral addresses are NOT reloaded.
       For address changes between transfers, call DMA_UpdateMemoryAddress() or DMA_Init()
       before DMA_Start(). This is intentional — reloading addresses from stored config
       would silently override any address changes made by the caller. */
    dma_stream->NDTR = dma_cfg[dma][stream].length;

    /* ===================== 5. Clear flags for THIS stream ===================== */

    DMA_ClearAllFlags(dma_regs, stream);

    /* ===================== 6. Memory barrier ===================== */

    // __DSB();  /* D-18: CMSIS intrinsic replaces raw inline ASM — portable and readable */
    // __ISB();  /* D-18: instruction sync barrier after DSB */
      __asm volatile ("dsb" ::: "memory");
      __asm volatile ("isb");
    /* ===================== 7. Enable the stream ===================== */

    dma_stream->CR.BITS.EN = 1U;

    return DMA_OK;
}

DMA_Error_t DMA_Stop(DMA_Id_t dma, DMA_StreamId_t stream)
{
    volatile DMA_REGS_t   *dma_regs;   /* D-03: volatile — DMA1/DMA2 macros yield volatile pointer */
    volatile DMA_Stream_t *dma_stream; /* D-03: volatile — stream is a sub-region of the volatile register map */

    /* ===================== 1. Validate parameters ===================== */

    if ((dma > DMA_2) || (stream > DMA_STREAM_7)) {
        return DMA_ERROR_INVALID_PARAM;
    }

    if (!dma_cfg_valid[dma][stream]) {
        return DMA_ERROR_INVALID_PARAM;
    }

    /* ===================== 2. Resolve DMA and stream ===================== */

    dma_regs   = (dma == DMA_1) ? DMA1 : DMA2;  /* D-03: volatile pointer assigned */
    dma_stream = &dma_regs->STREAM[stream];       /* D-03: volatile pointer assigned */

    /* ===================== 3. Disable the stream ===================== */

    dma_stream->CR.BITS.EN = 0U;
    /* D-12: timeout added — same reasoning as DMA_Init (D-11) */
    uint32_t stop_timeout = 100000U;
    while (dma_stream->CR.BITS.EN != 0U)
    {
        if (--stop_timeout == 0U)
            return DMA_ERROR_HW_FAILURE;
    }

    /* ===================== 4. Clear flags for THIS stream ===================== */

    DMA_ClearAllFlags(dma_regs, stream);

    /* ===================== 5. Memory barrier ===================== */

    // __DSB();  /* D-18: CMSIS intrinsic replaces raw inline ASM — portable and readable */
    // __ISB();  /* D-18: instruction sync barrier after DSB */
    
    __asm volatile ("dsb" ::: "memory");
     __asm volatile ("isb");
    return DMA_OK;
}

DMA_Error_t DMA_GetRemaining(
    DMA_Id_t dma,
    DMA_StreamId_t stream,
    uint16_t *remaining
)
{
    volatile DMA_REGS_t   *dma_regs;   /* D-03: volatile — DMA1/DMA2 macros yield volatile pointer */
    volatile DMA_Stream_t *dma_stream; /* D-03: volatile — stream is a sub-region of the volatile register map */

    /* ===================== 1. Validate parameters ===================== */

    if (remaining == NULL) {
        return DMA_ERROR_INVALID_PARAM;
    }

    if ((dma > DMA_2) || (stream > DMA_STREAM_7)) {
        return DMA_ERROR_INVALID_PARAM;
    }

    if (!dma_cfg_valid[dma][stream]) {
        return DMA_ERROR_INVALID_PARAM;
    }

    /* ===================== 2. Resolve DMA and stream ===================== */

    dma_regs   = (dma == DMA_1) ? DMA1 : DMA2;  /* D-03: volatile pointer assigned */
    dma_stream = &dma_regs->STREAM[stream];       /* D-03: volatile pointer assigned */

    /* ===================== 3. Validate meaningful read ===================== */

    if (dma_stream->CR.BITS.EN == 0U) {
        *remaining = 0U;
        return DMA_OK;
    }

    /* ===================== 4. Read remaining count ===================== */

    *remaining = (uint16_t)(dma_stream->NDTR);

    return DMA_OK;
}

DMA_Error_t DMA_UpdateMemoryAddress(
    DMA_Id_t dma,
    DMA_StreamId_t stream,
    uint32_t new_address
)
{
    volatile DMA_REGS_t   *dma_regs;   /* D-03: volatile — DMA1/DMA2 macros yield volatile pointer */
    volatile DMA_Stream_t *dma_stream; /* D-03: volatile — stream is a sub-region of the volatile register map */

    /* ===================== 1. Validate parameters ===================== */

    if ((dma > DMA_2) || (stream > DMA_STREAM_7) || (new_address == 0U)) {
        return DMA_ERROR_INVALID_PARAM;
    }

    if (!dma_cfg_valid[dma][stream]) {
        return DMA_ERROR_INVALID_PARAM;
    }

    /* ===================== 2. Resolve DMA and stream ===================== */

    dma_regs   = (dma == DMA_1) ? DMA1 : DMA2;  /* D-03: volatile pointer assigned */
    dma_stream = &dma_regs->STREAM[stream];       /* D-03: volatile pointer assigned */

    /* ===================== 3. Supported modes only ===================== */

    if ((dma_stream->CR.BITS.CIRC == 0U) &&
        (dma_stream->CR.BITS.DBM  == 0U)) {
        return DMA_ERROR_UNSUPPORTED;
    }

    /* ===================== 4. Safe update rules ===================== */

    /* Double buffer mode */
    if (dma_stream->CR.BITS.DBM != 0U) {
        /* Update only INACTIVE buffer */
        if (dma_stream->CR.BITS.CT == 0U) {
            dma_stream->M1AR = new_address;
            dma_cfg[dma][stream].memory1_address = new_address;
        } else {
            dma_stream->M0AR = new_address;
            dma_cfg[dma][stream].memory0_address = new_address;
        }
    }
    /* Circular mode (single buffer) */
    else {
        /* EN must be 0 or transfer at boundary */
        if (dma_stream->CR.BITS.EN != 0U) {
            return DMA_ERROR_STREAM_BUSY;
        }

        dma_stream->M0AR = new_address;
        dma_cfg[dma][stream].memory0_address = new_address;
    }

    return DMA_OK;
}

DMA_Error_t DMA_GetTransferStatus(
    DMA_Id_t dma,
    DMA_StreamId_t stream,
    DMA_TransferStatus_t *status
)
{
    volatile DMA_REGS_t   *dma_regs;   /* D-03: volatile — DMA1/DMA2 macros yield volatile pointer */
    volatile DMA_Stream_t *dma_stream; /* D-03: volatile — stream is a sub-region of the volatile register map */
    uint32_t               isr;
    DMA_FlagState_t        flags;

    /* ===================== 1. Validate parameters ===================== */

    if (status == NULL) {
        return DMA_ERROR_INVALID_PARAM;
    }

    if ((dma > DMA_2) || (stream > DMA_STREAM_7)) {
        return DMA_ERROR_INVALID_PARAM;
    }

    if (!dma_cfg_valid[dma][stream]) {
        return DMA_ERROR_INVALID_PARAM;
    }

    /* ===================== 2. Resolve DMA and stream ===================== */

    dma_regs   = (dma == DMA_1) ? DMA1 : DMA2;  /* D-03: volatile pointer assigned */
    dma_stream = &dma_regs->STREAM[stream];       /* D-03: volatile pointer assigned */

    /* ===================== 3. Read ISR and extract flags ===================== */

    isr = DMA_ReadISR(dma_regs, stream);
    flags = DMA_ExtractFlags(isr, stream);

    /* ===================== 4. ERROR flags (highest priority) ===================== */

    if (flags.feif || flags.dmeif || flags.teif) {
        *status = DMA_TRANSFER_ERROR;
        return DMA_OK;
    }

    /* ===================== 5. TRANSFER COMPLETE ===================== */

    if (flags.tcif) {
        *status = DMA_TRANSFER_COMPLETE;
        return DMA_OK;
    }

    /* ===================== 6. HALF TRANSFER ===================== */

    if (flags.htif) {
        *status = DMA_TRANSFER_HALF;
        return DMA_OK;
    }

    /* ===================== 7. IN PROGRESS / IDLE ===================== */

    if (dma_stream->CR.BITS.EN != 0U) {
        *status = DMA_TRANSFER_IN_PROGRESS;
    } else {
        *status = DMA_TRANSFER_IDLE;
    }

    return DMA_OK;
}

/* D-10: new function — allows updating callbacks after DMA_Init without full re-init.
   Also syncs TCIE/TEIE/DMEIE hardware bits to match the new callback state. */
DMA_Error_t DMA_SetCallback(
    DMA_Id_t        dma,
    DMA_StreamId_t  stream,
    DMA_Callback_t  complete_cb,
    DMA_Callback_t  error_cb,
    void           *ctx
)
{
    volatile DMA_REGS_t   *dma_regs;   /* D-03: volatile — DMA1/DMA2 macros yield volatile pointer */
    volatile DMA_Stream_t *dma_stream; /* D-03: volatile — stream is a sub-region of the volatile register map */

    /* ===================== 1. Validate parameters ===================== */

    if ((dma > DMA_2) || (stream > DMA_STREAM_7)) {
        return DMA_ERROR_INVALID_PARAM;
    }

    /* D-10: guard matches all other functions — no config access before validity check */
    if (!dma_cfg_valid[dma][stream]) {
        return DMA_ERROR_INVALID_PARAM;
    }

    /* ===================== 2. Update stored callbacks and context ===================== */

    dma_cfg[dma][stream].complete_callback = complete_cb;  /* D-10: update complete callback */
    dma_cfg[dma][stream].error_callback    = error_cb;     /* D-10: update error callback */
    dma_cfg[dma][stream].user_context      = ctx;          /* D-10: update user context */

    /* ===================== 3. Sync interrupt enables in hardware ===================== */

    dma_regs   = (dma == DMA_1) ? DMA1 : DMA2;  /* D-03: volatile pointer assigned */
    dma_stream = &dma_regs->STREAM[stream];       /* D-03: volatile pointer assigned */

    dma_stream->CR.BITS.TCIE  = (complete_cb != NULL) ? 1U : 0U;  /* D-10: sync TCIE with new callback state */
    dma_stream->CR.BITS.TEIE  = (error_cb    != NULL) ? 1U : 0U;  /* D-10: sync TEIE with new callback state */
    dma_stream->CR.BITS.DMEIE = (error_cb    != NULL) ? 1U : 0U;  /* D-10: sync DMEIE with new callback state */

    return DMA_OK;
}

/*============================================================================
 *                    IRQ HANDLERS
 *============================================================================*/

void DMA1_Stream0_IRQHandler(void) { DMA_IRQHandler(DMA_1, DMA_STREAM_0); }
void DMA1_Stream1_IRQHandler(void) { DMA_IRQHandler(DMA_1, DMA_STREAM_1); }
void DMA1_Stream2_IRQHandler(void) { DMA_IRQHandler(DMA_1, DMA_STREAM_2); }
void DMA1_Stream3_IRQHandler(void) { DMA_IRQHandler(DMA_1, DMA_STREAM_3); }
void DMA1_Stream4_IRQHandler(void) { DMA_IRQHandler(DMA_1, DMA_STREAM_4); }
void DMA1_Stream5_IRQHandler(void) { DMA_IRQHandler(DMA_1, DMA_STREAM_5); }
void DMA1_Stream6_IRQHandler(void) { DMA_IRQHandler(DMA_1, DMA_STREAM_6); }
void DMA1_Stream7_IRQHandler(void) { DMA_IRQHandler(DMA_1, DMA_STREAM_7); }

void DMA2_Stream0_IRQHandler(void) { DMA_IRQHandler(DMA_2, DMA_STREAM_0); }
void DMA2_Stream1_IRQHandler(void) { DMA_IRQHandler(DMA_2, DMA_STREAM_1); }
void DMA2_Stream2_IRQHandler(void) { DMA_IRQHandler(DMA_2, DMA_STREAM_2); }
void DMA2_Stream3_IRQHandler(void) { DMA_IRQHandler(DMA_2, DMA_STREAM_3); }
void DMA2_Stream4_IRQHandler(void) { DMA_IRQHandler(DMA_2, DMA_STREAM_4); }
void DMA2_Stream5_IRQHandler(void) { DMA_IRQHandler(DMA_2, DMA_STREAM_5); }
void DMA2_Stream6_IRQHandler(void) { DMA_IRQHandler(DMA_2, DMA_STREAM_6); }
void DMA2_Stream7_IRQHandler(void) { DMA_IRQHandler(DMA_2, DMA_STREAM_7); }