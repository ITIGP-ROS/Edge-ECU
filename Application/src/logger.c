/*****************************************************************************
 * logger.c — Logger task implementation
 *
 * Static allocation only. Batches log entries from s_logger_queue into
 * FrameRequest_t items and forwards them to g_frame_queue.
 *****************************************************************************/

#include "logger.h"
#include "log_payload.h"
#include "FRAME.h"
#include "frame_request.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/*-------------------------------------------------------------
 * Constants
 *-------------------------------------------------------------*/
#define LOGGER_QUEUE_DEPTH       16U
#define LOGGER_TASK_STACK_WORDS  256U
#define LOGGER_TASK_PRIORITY     0U     /* lowest priority */

/*-------------------------------------------------------------
 * Static storage
 *-------------------------------------------------------------*/
static StaticQueue_t s_logger_queue_storage;
static uint8_t       s_logger_queue_buffer[LOGGER_QUEUE_DEPTH * sizeof(Log_Payload_t)];
static QueueHandle_t s_logger_queue = NULL;

static StaticTask_t  s_logger_task_tcb;
static StackType_t   s_logger_task_stack[LOGGER_TASK_STACK_WORDS];
static TaskHandle_t  s_logger_task_handle = NULL;

static volatile uint32_t s_drop_count         = 0U;  /* logger queue full  */
static volatile uint32_t s_forward_drop_count = 0U;  /* g_frame_queue full */

/* Deduplication array: 1 if code is currently in the queue, 0 otherwise */
static volatile uint8_t  s_is_pending[256]    = {0};

/* State tracking array: Stores the last severity logged for a given code. 0xFF means never logged. */
static volatile uint8_t  s_last_severity[256];

/* Debounce tracking */
static volatile uint8_t  s_candidate_severity[256];
static volatile uint8_t  s_candidate_count[256];

/*-------------------------------------------------------------
 * External symbols
 *-------------------------------------------------------------*/
extern QueueHandle_t g_frame_queue;

/*-------------------------------------------------------------
 * Forward declarations
 *-------------------------------------------------------------*/
static void Logger_Task(void *arg);

/*-------------------------------------------------------------
 * Public API
 *-------------------------------------------------------------*/
uint8_t Logger_Init(void)
{
    for(int i = 0; i < 256; i++) {
        s_last_severity[i] = 0xFF;
        s_candidate_severity[i] = 0xFF;
        s_candidate_count[i] = 0;
    }

    s_logger_queue = xQueueCreateStatic(
        LOGGER_QUEUE_DEPTH,
        sizeof(Log_Payload_t),
        s_logger_queue_buffer,
        &s_logger_queue_storage
    );

    if (s_logger_queue == NULL)
    {
        return 1U;
    }

    s_logger_task_handle = xTaskCreateStatic(
        Logger_Task,
        "Logger",
        LOGGER_TASK_STACK_WORDS,
        NULL,
        LOGGER_TASK_PRIORITY,
        s_logger_task_stack,
        &s_logger_task_tcb
    );

    if (s_logger_task_handle == NULL)
    {
        return 1U;
    }

    return 0U;
}

void Logger_Log(uint8_t code, uint8_t severity, uint32_t aux_data)
{
    /* If the state matches the already committed state, reset debounce and ignore */
    if (s_last_severity[code] == severity)
    {
        s_candidate_severity[code] = 0xFF;
        s_candidate_count[code] = 0;
        return;
    }

    /* If it's a new state candidate (or first time seeing it) */
    if (s_candidate_severity[code] != severity)
    {
        s_candidate_severity[code] = severity;
        s_candidate_count[code] = 1;
        
        /* If it's the very first log for this code, accept it immediately without debouncing */
        if (s_last_severity[code] == 0xFF) {
            s_candidate_count[code] = 2; // Force pass
        } else {
            return; // Wait for next reading to confirm
        }
    }
    else
    {
        /* We saw the same candidate again! */
        s_candidate_count[code]++;
    }

    /* Must be constant for at least 2 readings */
    if (s_candidate_count[code] < 2)
    {
        return;
    }

    /* Deduplication: if this log code is already pending in the queue, drop it */
    if (s_is_pending[code] != 0U)
    {
        return;
    }

    Log_Payload_t entry;
    BaseType_t    status;

    entry.code         = code;
    entry.severity     = severity;
    entry.timestamp_ms = (uint32_t)xTaskGetTickCount();
    entry.aux_data     = aux_data;

    /* Mark as pending before attempting to queue */
    s_is_pending[code] = 1U;

    if (xPortIsInsideInterrupt() != pdFALSE)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        status = xQueueSendFromISR(s_logger_queue, &entry, &xHigherPriorityTaskWoken);
        if (status != pdTRUE)
        {
            s_is_pending[code] = 0U; /* Clear if queue was full so it can retry later */
            s_drop_count++;
        }
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    else
    {
        status = xQueueSend(s_logger_queue, &entry, 0U);
        if (status != pdTRUE)
        {
            s_is_pending[code] = 0U; /* Clear if queue was full so it can retry later */
            s_drop_count++;
        }
    }
}

/*-------------------------------------------------------------
 * Logger task
 *-------------------------------------------------------------*/
static void Logger_Task(void *arg)
{
    (void)arg;
    Log_Payload_t  log_entry;
    FrameRequest_t req;

    for (;;)
    {
        if (xQueueReceive(s_logger_queue, &log_entry, portMAX_DELAY) == pdTRUE)
        {
            req.type   = FRAME_TYPE_LOG;
            req.ecu_id = ECU_ID_STM32_NODE1;
            req.length = LOG_PAYLOAD_SIZE;

            /* Pack 10-byte payload BIG-ENDIAN */
            req.payload[0] = log_entry.code;
            req.payload[1] = log_entry.severity;
            req.payload[2] = (uint8_t)(log_entry.timestamp_ms >> 24);
            req.payload[3] = (uint8_t)(log_entry.timestamp_ms >> 16);
            req.payload[4] = (uint8_t)(log_entry.timestamp_ms >>  8);
            req.payload[5] = (uint8_t)(log_entry.timestamp_ms);
            req.payload[6] = (uint8_t)(log_entry.aux_data >> 24);
            req.payload[7] = (uint8_t)(log_entry.aux_data >> 16);
            req.payload[8] = (uint8_t)(log_entry.aux_data >>  8);
            req.payload[9] = (uint8_t)(log_entry.aux_data);

            if (xQueueSend(g_frame_queue, &req, 0U) != pdTRUE)
            {
                s_forward_drop_count++;
            }
            else
            {
                /* Successfully pushed to UART frame queue. Update the last severity. */
                s_last_severity[log_entry.code] = log_entry.severity;
            }

            /* Clear pending flag so this log code can be queued again */
            s_is_pending[log_entry.code] = 0U;

            /* Enforce strict rate limit: 1 log every 4 seconds maximum */
            vTaskDelay(pdMS_TO_TICKS(4000));
        }
    }
}