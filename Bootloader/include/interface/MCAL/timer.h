#ifndef TIMER_H
#define TIMER_H

/** Note:
    Timers by default doesn't stop when the core is halted by the debugger,
    if you need to stop the timer when debugging,
    see the DBGMCU Register and set the corresponding bits for the timers you want to stop during debug halt. 
**/

#include "../../../lib/STD_Types.h"
#include "interface/MCAL/gpio.h"

typedef enum {
    TIMER_2,    // TIM2 is a 32-bit timer
    TIMER_3,    // TIM3 is a 16-bit timer
    TIMER_4,    // TIM4 is a 16-bit timer
    TIMER_5     // TIM5 is a 32-bit timer
} Timer_Instance_t;

typedef enum {
    TIMER_MODE_UP,
    TIMER_MODE_DOWN,
    TIMER_MODE_CENTER_ALIGNED_1,
    TIMER_MODE_CENTER_ALIGNED_2,
    TIMER_MODE_CENTER_ALIGNED_3
} Timer_Mode_t;

// All Four Channels share:
// - One counter (CNT)
// - Prescaler (PSC)
// - Auto-reload value (ARR)
// They differ in:
// - its own mode (Output Compare(PWM), Input Capture(input pwm)) which is configured through CCMR1 and CCMR2 registers
// - Capture/Compare values (CCR1, CCR2, CCR3, CCR4)
// - Interrupts for each channel (CC1IE, CC2IE, CC3IE, CC4IE)
typedef enum {
    TIMER_CHANNEL_1,
    TIMER_CHANNEL_2,
    TIMER_CHANNEL_3,
    TIMER_CHANNEL_4
} Timer_Channel_t;

typedef enum {
    TIMER_IC_FILTER_NONE = 0,
    TIMER_IC_FILTER_CONSCUTIVE_2 = 1,   // 2 consecutive samples must be 1 for a valid capture
    TIMER_IC_FILTER_CONSCUTIVE_4 = 2,   // 4 consecutive samples must be 1 for a valid capture
    TIMER_IC_FILTER_CONSCUTIVE_5 = 10,  // 5 consecutive samples must be 1 for a valid capture
    TIMER_IC_FILTER_CONSCUTIVE_6 = 4,   // 6 consecutive samples must be 1 for a valid capture
    TIMER_IC_FILTER_CONSCUTIVE_8 = 3    // 8 consecutive samples must be 1 for a valid capture
} Timer_IC_Filter_t;

typedef enum {
    TIMER_IC_POLARITY_RISING = 0,
    TIMER_IC_POLARITY_FALLING = 1,
    TIMER_IC_POLARITY_BOTH = 3
} Timer_IC_Polarity_t;

typedef void (*Timer_Callback_t)(void);

typedef struct {
    Timer_Instance_t instance;      // Timer instance (TIMER_2, TIMER_3, TIMER_4, or TIMER_5)
    Timer_Mode_t mode;              // Timer counting mode (up, down, or center-aligned)
    uint16_t prescaler;             // Prescaler value (1 to 65535)
    uint32_t autoReloadValue;       // Auto-reload value (1 to 0xFFFFFFFF)
    Timer_Channel_t channel;        // Timer channel for Capture/Compare(PWM) operations not used in basic timer mode
    Timer_Callback_t ovfCallback;   // Callback function for timer overflow interrupts
    GPIO_Port_t port;               // GPIO port for Capture/Compare (not used in basic timer mode)
    GPIO_Pin_t pin;                 // GPIO pin for Capture/Compare (not used in basic timer mode)
    GPIO_Pull_t pullType;           // GPIO pull-up/pull-down configuration for Capture/Compare pin (not used in basic timer mode)
    Timer_IC_Filter_t icFilter;     // Input capture filter configuration (used in input capture mode)
    Timer_IC_Polarity_t icPolarity; // Input capture polarity configuration (used in input capture mode)
    Timer_Callback_t ccCallback;    // Callback function for Capture/Compare interrupts (used in input capture modes)
} Timer_t;

// Doxygen documentation for the Timer module
/**
 * @brief Initializes the specified timer with the provided configuration.
 * @param timerConfig Pointer to a Timer_t structure containing the timer configuration parameters.
 * @return STD_ReturnType Returns STD_SUCCESS if initialization is successful, otherwise returns STD_ERROR.
 * @note This function configures the timer's counting mode, prescaler, auto-reload value, and sets up callbacks for interrupts. It does not start the timer; use Timer_Start or Timer_Start_IT to start the timer.
 */
STD_ReturnType Timer_Init(const Timer_t *timerConfig);
/**
 * @brief Starts the specified timer.
 * @param timerConfig Pointer to a Timer_t structure containing the timer configuration parameters.
 * @return STD_ReturnType Returns STD_SUCCESS if the timer is started successfully, otherwise returns STD_ERROR.
 */
STD_ReturnType Timer_Start(const Timer_t *timerConfig);
/**
 * @brief Starts the specified timer with interrupts enabled.
 * @param timerConfig Pointer to a Timer_t structure containing the timer configuration parameters.
 * @return STD_ReturnType Returns STD_SUCCESS if the timer is started successfully, otherwise returns STD_ERROR.
 */
STD_ReturnType Timer_Start_IT(const Timer_t *timerConfig);
/**
 * @brief Stops the specified timer.
 * @param timerConfig Pointer to a Timer_t structure containing the timer configuration parameters.
 * @return STD_ReturnType Returns STD_SUCCESS if the timer is stopped successfully, otherwise returns STD_ERROR.
 */
STD_ReturnType Timer_Start_PWM(const Timer_t *timerConfig);
/**
 * @brief Sets the duty cycle for the specified timer channel in PWM mode.
 * @param timerConfig Pointer to a Timer_t structure containing the timer configuration parameters.
 * @param dutyCycle Duty cycle percentage (0 to 100) to be set for the PWM signal.
 * @return STD_ReturnType Returns STD_SUCCESS if the duty cycle is set successfully, otherwise returns STD_ERROR.
 */
STD_ReturnType Timer_PWM_SetDutyCycle(const Timer_t *timerConfig, uint8_t dutyCycle);
/**
 * @brief Starts the specified timer in input capture mode.
 * @param timerConfig Pointer to a Timer_t structure containing the timer configuration parameters.
 * @return STD_ReturnType Returns STD_SUCCESS if the timer is started successfully, otherwise returns STD_ERROR.
 */
STD_ReturnType Timer_Start_IC(const Timer_t *timerConfig);
/**
 * @brief Stops the specified timer with interrupts enabled & Disable NVIC.
 * @param timerConfig Pointer to a Timer_t structure containing the timer configuration parameters.
 * @return STD_ReturnType Returns STD_SUCCESS if the timer is stopped successfully, otherwise returns STD_ERROR.
 */
STD_ReturnType Timer_Stop_IT(const Timer_t *timerConfig);
/**
 * @brief Stops the specified timer.
 * @param timerConfig Pointer to a Timer_t structure containing the timer configuration parameters.
 * @return STD_ReturnType Returns STD_SUCCESS if the timer is stopped successfully, otherwise returns STD_ERROR.
 */
STD_ReturnType Timer_Stop(const Timer_t *timerConfig);
/**
 * @brief Stops the specified timer channel in PWM mode.
 * @param timerConfig Pointer to a Timer_t structure containing the timer configuration parameters.
 * @return STD_ReturnType Returns STD_SUCCESS if the timer is stopped successfully, otherwise returns STD_ERROR.
 */
STD_ReturnType Timer_Stop_PWM(const Timer_t *timerConfig);
/**
 * @brief Stops the specified timer channel in PWM mode.
 * @param timerConfig Pointer to a Timer_t structure containing the timer configuration parameters.
 * @return STD_ReturnType Returns STD_SUCCESS if the timer is stopped successfully, otherwise returns STD_ERROR.
 */
STD_ReturnType Timer_Stop_IC(const Timer_t *timerConfig);
/**
 * @brief Gets the current counter value for the specified timer.
 * @param timerConfig Pointer to a Timer_t structure containing the timer configuration parameters.
 * @param counterValue Pointer to a variable where the counter value will be stored.
 * @return STD_ReturnType Returns STD_SUCCESS if the counter value is retrieved successfully, otherwise returns STD_ERROR.
 */
STD_ReturnType Timer_GetCounter(const Timer_t *timerConfig, uint32_t* counterValue);
/**
 * @brief Sets the auto-reload value for the specified timer.
 * @param timerConfig Pointer to a Timer_t structure containing the timer configuration parameters.
 * @param autoReloadValue Auto-reload value to be set for the timer.
 * @return STD_ReturnType Returns STD_SUCCESS if the auto-reload value is set successfully, otherwise returns STD_ERROR.
 */
STD_ReturnType Timer_SetAutoReload(const Timer_t *timerConfig);
/**
 * @brief Sets the prescaler for the specified timer.
 * @param timerConfig Pointer to a Timer_t structure containing the timer configuration parameters.
 * @param prescalerValue Prescaler value to be set for the timer.
 * @return STD_ReturnType Returns STD_SUCCESS if the prescaler value is set successfully, otherwise returns STD_ERROR.
 */
STD_ReturnType Timer_SetPrescaler(const Timer_t *timerConfig);
/**
 * @brief Sets the mode for the specified timer.
 * @param timerConfig Pointer to a Timer_t structure containing the timer configuration parameters.
 * @param mode Mode to be set for the timer.
 * @return STD_ReturnType Returns STD_SUCCESS if the mode is set successfully, otherwise returns STD_ERROR.
 */
STD_ReturnType Timer_SetMode(const Timer_t *timerConfig);

#endif // TIMER_H