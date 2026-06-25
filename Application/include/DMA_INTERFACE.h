#ifndef DMA_INTERFACE_H
#define DMA_INTERFACE_H

#include "STD_TYPES.h"

typedef enum {
    DMA_OK                   = 0x00U,  /* D-07: explicit value — MISRA Rule 8.12 */
    DMA_ERROR_INVALID_PARAM  = 0x01U,  /* D-07 */
    DMA_ERROR_STREAM_BUSY    = 0x02U,  /* D-07 */
    DMA_ERROR_UNSUPPORTED    = 0x03U,  /* D-07 */
    DMA_ERROR_HW_FAILURE     = 0x04U   /* D-07 */
} DMA_Error_t;

/* DMA_State_t — reserved for future DMA_GetState implementation.
   DMA_GetTransferStatus() provides equivalent functionality via DMA_TransferStatus_t. */
typedef enum {
    DMA_STATE_IDLE  = 0x00U,  /* D-07: explicit value — MISRA Rule 8.12 */
    DMA_STATE_BUSY  = 0x01U,  /* D-07 */
    DMA_STATE_ERROR = 0x02U   /* D-07 */
} DMA_State_t;

typedef enum {
    DMA_1 = 0U,  /* D-07: explicit value — MISRA Rule 8.12 */
    DMA_2 = 1U   /* D-07 */
} DMA_Id_t;

typedef enum {
    DMA_STREAM_0 = 0U,  /* D-07: explicit value — MISRA Rule 8.12 */
    DMA_STREAM_1 = 1U,  /* D-07 */
    DMA_STREAM_2 = 2U,  /* D-07 */
    DMA_STREAM_3 = 3U,  /* D-07 */
    DMA_STREAM_4 = 4U,  /* D-07 */
    DMA_STREAM_5 = 5U,  /* D-07 */
    DMA_STREAM_6 = 6U,  /* D-07 */
    DMA_STREAM_7 = 7U   /* D-07 */
} DMA_StreamId_t;

typedef enum {
    DMA_DIR_PERIPH_TO_MEM = 0U,  /* D-07: explicit value — MISRA Rule 8.12 */
    DMA_DIR_MEM_TO_PERIPH = 1U,  /* D-07 */
    DMA_DIR_MEM_TO_MEM    = 2U   /* D-07 */
} DMA_Direction_t;

typedef enum {
    DMA_PRIORITY_LOW       = 0U,  /* D-07: explicit value — MISRA Rule 8.12 */
    DMA_PRIORITY_MEDIUM    = 1U,  /* D-07 */
    DMA_PRIORITY_HIGH      = 2U,  /* D-07 */
    DMA_PRIORITY_VERY_HIGH = 3U   /* D-07 */
} DMA_Priority_t;

typedef enum {
    DMA_WIDTH_BYTE     = 0U,  /* D-07: explicit value — MISRA Rule 8.12 */
    DMA_WIDTH_HALFWORD = 1U,  /* D-07 */
    DMA_WIDTH_WORD     = 2U   /* D-07 */
} DMA_DataWidth_t;

typedef enum {
    DMA_INC_DISABLE = 0U,  /* D-07: explicit value — MISRA Rule 8.12 */
    DMA_INC_ENABLE  = 1U   /* D-07 */
} DMA_Increment_t;

typedef enum {
    DMA_MODE_NORMAL        = 0U,  /* D-07: explicit value — MISRA Rule 8.12 */
    DMA_MODE_CIRCULAR      = 1U,  /* D-07 */
    DMA_MODE_DOUBLE_BUFFER = 2U   /* D-07 */
} DMA_Mode_t;

typedef enum {
    DMA_TRANSFER_IDLE        = 0U,  /* D-07: explicit value — MISRA Rule 8.12; no transfer running */
    DMA_TRANSFER_IN_PROGRESS = 1U,  /* D-07: EN=1, no terminal flags */
    DMA_TRANSFER_HALF        = 2U,  /* D-07: HTIF set */
    DMA_TRANSFER_COMPLETE    = 3U,  /* D-07: TCIF set */
    DMA_TRANSFER_ERROR       = 4U   /* D-07: TEIF / FEIF / DMEIF set */
} DMA_TransferStatus_t;

typedef enum {
    DMA_EVENT_HALF_TRANSFER    = 0U,  /* D-07: explicit value — MISRA Rule 8.12 */
    DMA_EVENT_TRANSFER_COMPLETE = 1U, /* D-07 */
    DMA_EVENT_TRANSFER_ERROR   = 2U   /* D-07 */
} DMA_Event_t;

typedef enum {
    DMA_FLOW_DMA        = 0U,  /* D-07: explicit value — MISRA Rule 8.12 */
    DMA_FLOW_PERIPHERAL = 1U   /* D-07 */
} DMA_FlowController_t;

typedef enum {
    DMA_CHANNEL_0 = 0U,  /* D-07: explicit value — MISRA Rule 8.12 */
    DMA_CHANNEL_1 = 1U,  /* D-07 */
    DMA_CHANNEL_2 = 2U,  /* D-07 */
    DMA_CHANNEL_3 = 3U,  /* D-07 */
    DMA_CHANNEL_4 = 4U,  /* D-07 */
    DMA_CHANNEL_5 = 5U,  /* D-07 */
    DMA_CHANNEL_6 = 6U,  /* D-07 */
    DMA_CHANNEL_7 = 7U   /* D-07 */
} DMA_Channel_t;


/* D-08: event parameter added — callbacks now carry the triggering event so callers
   can distinguish half/complete/error without querying status separately. */
typedef void (*DMA_Callback_t)(DMA_Event_t event, void *context);


typedef struct {
    DMA_Id_t         dma_id;
    DMA_StreamId_t  stream_id;

    uint32_t        peripheral_address;
    uint32_t        memory0_address;
    uint32_t        memory1_address;   /* used only in Double Buffer Mode */

    uint16_t        length;            /* number of data items */

    DMA_Direction_t direction;

    DMA_DataWidth_t periph_width;
    DMA_DataWidth_t mem_width;

    DMA_Increment_t periph_inc;
    DMA_Increment_t mem_inc;

    DMA_Mode_t      mode;
    DMA_Priority_t  priority;

    DMA_Channel_t         channel;           /* CHSEL value */

    DMA_Callback_t  half_callback;
    DMA_Callback_t  complete_callback;
    DMA_Callback_t  error_callback;

    DMA_FlowController_t flow_controller;

    void           *user_context;

} DMA_Config_t;


DMA_Error_t DMA_Init(const DMA_Config_t *cfg);

DMA_Error_t DMA_Start(DMA_Id_t dma, DMA_StreamId_t stream);

DMA_Error_t DMA_Stop(DMA_Id_t dma, DMA_StreamId_t stream);

DMA_Error_t DMA_GetRemaining(
    DMA_Id_t dma,
    DMA_StreamId_t stream,
    uint16_t *remaining
);

DMA_Error_t DMA_UpdateMemoryAddress(
    DMA_Id_t dma,
    DMA_StreamId_t stream,
    uint32_t new_address
);

DMA_Error_t DMA_GetTransferStatus(
    DMA_Id_t dma,
    DMA_StreamId_t stream,
    DMA_TransferStatus_t *status
);

/* D-10: new function — allows updating callbacks after DMA_Init without full re-init */
DMA_Error_t DMA_SetCallback(
    DMA_Id_t        dma,
    DMA_StreamId_t  stream,
    DMA_Callback_t  complete_cb,
    DMA_Callback_t  error_cb,
    void           *ctx
);

/* ============================
 *   IRQ Entry Points
 *   Called by NVIC — do not call directly
 * ============================ */
/* D-17: prototypes added — IRQ handlers must be declared in the header so the
   linker can verify the definition matches the NVIC vector table symbol. */
void DMA1_Stream0_IRQHandler(void);
void DMA1_Stream1_IRQHandler(void);
void DMA1_Stream2_IRQHandler(void);
void DMA1_Stream3_IRQHandler(void);
void DMA1_Stream4_IRQHandler(void);
void DMA1_Stream5_IRQHandler(void);
void DMA1_Stream6_IRQHandler(void);
void DMA1_Stream7_IRQHandler(void);
void DMA2_Stream0_IRQHandler(void);
void DMA2_Stream1_IRQHandler(void);
void DMA2_Stream2_IRQHandler(void);
void DMA2_Stream3_IRQHandler(void);
void DMA2_Stream4_IRQHandler(void);
void DMA2_Stream5_IRQHandler(void);
void DMA2_Stream6_IRQHandler(void);
void DMA2_Stream7_IRQHandler(void);

#endif
