/*****************************************************************************
 * logger.h — Event-driven logging module
 *
 * Architecture:
 *   Tasks/ISRs call LOG_*() macros → Logger_Log() pushes to internal queue
 *   → Logger task drains queue → builds FrameRequest_t → pushes to g_frame_queue
 *   → Thread 3 (UART TX) transmits as TYPE=0x04 frame.
 *
 * The Logger task runs at lowest priority (0) so logs never preempt
 * real-time tasks.
 *
 * Logger_Log() is ISR-safe — detects context and uses *FromISR variant.
 *****************************************************************************/

#ifndef LOGGER_H
#define LOGGER_H

#include "STD_TYPES.h"
#include "log_payload.h"

/*-------------------------------------------------------------
 * Log codes — group ranges:
 *   0x10..0x1F = Hardware errors
 *   0x20..0x2F = Software errors
 *   0x30..0x3F = Status events
 *   0x40..0x4F = Warnings
 *-------------------------------------------------------------*/
typedef enum {
    /* Hardware */
    LOG_CODE_MPU6050_TIMEOUT      = 0x10,
    LOG_CODE_I2C_BUS_STUCK        = 0x11,
    LOG_CODE_RING_BUFFER_DROP     = 0x12,
    LOG_CODE_DMA_ERROR            = 0x13,
    LOG_CODE_ADC_TIMEOUT          = 0x14,
    LOG_CODE_ULTRASONIC1_TIMEOUT  = 0x15,
    LOG_CODE_ULTRASONIC2_TIMEOUT  = 0x16,

    /* Software */
    LOG_CODE_INFERENCE_FAIL       = 0x20,
    LOG_CODE_QUEUE_FULL           = 0x21,
    LOG_CODE_FRAME_BUILD_FAIL     = 0x22,
    LOG_CODE_LOGGER_DROP          = 0x23,  /* logger queue overflowed */

    /* Status */
    LOG_CODE_BOOT                 = 0x30,
    LOG_CODE_THREAD_STARTED       = 0x31,
    LOG_CODE_RESET_DETECTED       = 0x32,

    /* Warnings */
    LOG_CODE_LOW_CONFIDENCE       = 0x40,
    LOG_CODE_STACK_HIGH_WATER     = 0x41
} Log_Code_t;

/*-------------------------------------------------------------
 * Severity levels
 *-------------------------------------------------------------*/
#define LOG_SEV_INFO    0U
#define LOG_SEV_WARN    1U
#define LOG_SEV_ERROR   2U
#define LOG_SEV_FATAL   3U

/*-------------------------------------------------------------
 * Convenience macros — call from any task or ISR
 *-------------------------------------------------------------*/
#define LOG_INFO(code, aux)    Logger_Log((uint8_t)(code), LOG_SEV_INFO,  (uint32_t)(aux))
#define LOG_WARN(code, aux)    Logger_Log((uint8_t)(code), LOG_SEV_WARN,  (uint32_t)(aux))
#define LOG_ERROR(code, aux)   Logger_Log((uint8_t)(code), LOG_SEV_ERROR, (uint32_t)(aux))
#define LOG_FATAL(code, aux)   Logger_Log((uint8_t)(code), LOG_SEV_FATAL, (uint32_t)(aux))

/*-------------------------------------------------------------
 * API
 *-------------------------------------------------------------*/

/**
 * @brief  Initialise the logger queue and task.
 *         Must be called BEFORE vTaskStartScheduler().
 * @return 0 on success, non-zero on failure.
 */
uint8_t Logger_Init(void);

/**
 * @brief  Push a log entry onto the logger queue. Non-blocking.
 *
 * ISR-safe: detects calling context via xPortIsInsideInterrupt() and
 * uses xQueueSendFromISR if called from an ISR.
 *
 * @param  code      Log_Code_t value
 * @param  severity  LOG_SEV_INFO / WARN / ERROR / FATAL
 * @param  aux_data  Caller-defined context value
 */
void Logger_Log(uint8_t code, uint8_t severity, uint32_t aux_data);

#endif /* LOGGER_H */