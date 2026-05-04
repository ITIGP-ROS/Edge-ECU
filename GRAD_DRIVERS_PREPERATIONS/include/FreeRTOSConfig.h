#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*****************************************************************************
 * FreeRTOSConfig.h — STM32F401CC @ 84 MHz
 *
 * Application: Road surface classifier (3 tasks at priorities 1/2/3)
 * Allocation:  Static only (configSUPPORT_DYNAMIC_ALLOCATION = 0)
 * Tick rate:   1 kHz (1 ms granularity)
 *
 * Settings disabled because we don't use the feature:
 *   - software timers (hardware TIM2 used instead)
 *   - time slicing   (tasks have distinct priorities)
 *   - mutexes        (binary semaphores only)
 *   - counting sems  (binary semaphores only)
 *   - trace facility (no tracing tool in use)
 *   - queue registry (no debug inspection)
 *****************************************************************************/

#include <stdint.h>
extern uint32_t SystemCoreClock;

/*============================================================================
 * Core scheduler
 *============================================================================*/
#define configUSE_PREEMPTION                          1
#define configUSE_TIME_SLICING                        0   /* distinct priorities */
#define configUSE_PORT_OPTIMISED_TASK_SELECTION       1   /* CLZ-based, fast    */
#define configUSE_TICKLESS_IDLE                       0

#define configCPU_CLOCK_HZ                            ((uint32_t)84000000U)
#define configTICK_RATE_HZ                            ((TickType_t)1000U)

/*============================================================================
 * Tasks
 *============================================================================*/
#define configMAX_PRIORITIES                          5     /* 0=idle..4 reserved */
#define configMINIMAL_STACK_SIZE                      ((uint16_t)128U)
#define configMAX_TASK_NAME_LEN                       16
#define configUSE_16_BIT_TICKS                        0     /* 32-bit on M4    */
#define configIDLE_SHOULD_YIELD                       0     /* no priority-0 tasks */

/*============================================================================
 * Memory — STATIC ONLY
 *============================================================================*/
#define configSUPPORT_STATIC_ALLOCATION               1
#define configSUPPORT_DYNAMIC_ALLOCATION              0
/* No configTOTAL_HEAP_SIZE — no heap exists */

/*============================================================================
 * Synchronization (binary semaphores only)
 *============================================================================*/
#define configUSE_MUTEXES                             0
#define configUSE_RECURSIVE_MUTEXES                   0
#define configUSE_COUNTING_SEMAPHORES                 0
#define configUSE_TASK_NOTIFICATIONS                  1
#define configUSE_QUEUE_SETS                          0
#define configQUEUE_REGISTRY_SIZE                     0

/*============================================================================
 * Software timers — disabled (using hardware TIM2)
 *============================================================================*/
#define configUSE_TIMERS                              0
/* When configUSE_TIMERS = 0, timer-related macros are unused — leave them */

/*============================================================================
 * Hooks
 *============================================================================*/
#define configUSE_IDLE_HOOK                           1   /* IWDG feed goes here */
#define configUSE_TICK_HOOK                           0
#define configUSE_MALLOC_FAILED_HOOK                  0   /* no malloc anyway   */
/* Stack overflow hook is automatic when CHECK_FOR_STACK_OVERFLOW > 0 */

/*============================================================================
 * Debug & safety
 *============================================================================*/
#define configCHECK_FOR_STACK_OVERFLOW                2
#define configUSE_TRACE_FACILITY                      1
#define configUSE_STATS_FORMATTING_FUNCTIONS          0
#define configGENERATE_RUN_TIME_STATS                 1

extern void     vConfigureTimerForRunTimeStats(void);
extern uint32_t ulGetRunTimeCounterValue(void);
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()  vConfigureTimerForRunTimeStats()
#define portGET_RUN_TIME_COUNTER_VALUE()           ulGetRunTimeCounterValue()

#define configASSERT(x)                                              \
    do {                                                             \
        if ((x) == 0) {                                              \
            taskDISABLE_INTERRUPTS();                                \
            for (;;) { __asm volatile ("bkpt #0"); }                 \
        }                                                            \
    } while (0)

/*============================================================================
 * Cortex-M4 interrupt priorities
 *
 * STM32F4 implements 4 priority bits → 16 levels (0=highest, 15=lowest).
 * Lower numerical value = higher hardware priority.
 *
 *   Kernel (SysTick, PendSV)   = 15     (lowest possible)
 *   Application ISRs           = 5..14  (CAN call FromISR APIs)
 *   "Ultra-fast" ISRs          = 0..4   (CANNOT call FreeRTOS APIs)
 *
 * Any ISR that calls xSemaphoreGiveFromISR / xQueueSendFromISR / etc.
 * MUST have priority value >= 5 (numerically).
 *============================================================================*/
#define configPRIO_BITS                               4

#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY       15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY  5

#define configKERNEL_INTERRUPT_PRIORITY               \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

#define configMAX_SYSCALL_INTERRUPT_PRIORITY          \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/*============================================================================
 * Vector table aliases — let port.c implement these directly
 *============================================================================*/
#define vPortSVCHandler                               SVC_Handler
#define xPortPendSVHandler                            PendSV_Handler
#define xPortSysTickHandler                           SysTick_Handler

/*============================================================================
 * INCLUDE_* — pull in only the API functions we actually call
 *============================================================================*/
#define INCLUDE_vTaskPrioritySet                      0
#define INCLUDE_uxTaskPriorityGet                     0
#define INCLUDE_vTaskDelete                           0
#define INCLUDE_vTaskSuspend                          0
#define INCLUDE_vTaskDelayUntil                       1   /* periodic helper */
#define INCLUDE_vTaskDelay                            1
#define INCLUDE_xTaskGetSchedulerState                0
#define INCLUDE_xTaskGetCurrentTaskHandle             0
#define INCLUDE_uxTaskGetStackHighWaterMark           1   /* stack tuning    */
#define INCLUDE_xTimerPendFunctionCall                0
#define INCLUDE_eTaskGetState                         0

#endif /* FREERTOS_CONFIG_H */