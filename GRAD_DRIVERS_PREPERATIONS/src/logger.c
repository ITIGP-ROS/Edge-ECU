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
#define LOGGER_QUEUE_DEPTH       8U
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
    Log_Payload_t entry;
    BaseType_t    status;

    entry.code         = code;
    entry.severity     = severity;
    entry.timestamp_ms = (uint32_t)xTaskGetTickCount();
    entry.aux_data     = aux_data;

    if (xPortIsInsideInterrupt() != pdFALSE)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        status = xQueueSendFromISR(s_logger_queue, &entry, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    else
    {
        status = xQueueSend(s_logger_queue, &entry, 0U);
    }

    if (status != pdTRUE)
    {
        s_drop_count++;
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
        }
    }
}