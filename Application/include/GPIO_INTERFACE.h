#ifndef GPIO_INTERFACE_H
#define GPIO_INTERFACE_H    

#include  "STD_TYPES.h"


typedef enum {
    GPIO_OK                       = 0x00U,   // Operation successful
    GPIO_ERROR_INVALID_PIN        = 0x01U,   // Invalid pin number or port
    GPIO_ERROR_INVALID_MODE       = 0x02U,   // Invalid mode (input/output/AF/analog)
    GPIO_ERROR_INVALID_SPEED      = 0x03U,   // Invalid speed configuration
    GPIO_ERROR_INVALID_PUPD       = 0x04U,   // Invalid pull-up/pull-down configuration
    GPIO_ERROR_INVALID_AF         = 0x05U,   // Invalid alternate function selection
    GPIO_ERROR_NULL_PTR           = 0x06U,   // Null pointer passed to function
    GPIO_ERROR_PIN_LOCKED         = 0x07U,   // Pin is locked, cannot modify configuration
    /* G-11: Reserved — clock enablement check not implemented in GPIO driver; caller must enable port clock via RCC before calling GPIO_INIT */
    GPIO_ERROR_PORT_NOT_ENABLED   = 0x08U,   // Corresponding port clock not enabled
    GPIO_ERROR_WRITE_PROTECTED    = 0x09U,   // Attempt to write to a protected pin (e.g. JTAG)
    GPIO_ERROR_TIMEOUT            = 0x0AU,   // Operation timeout (e.g. during lock/unlock)
    GPIO_ERROR_UNKNOWN            = 0xFFU    // Unknown or unexpected error
} GPIO_Error_t;


/* G-07: explicit values assigned to all enumerators — MISRA Rule 8.12 */
typedef enum {
    GPIO_PIN0  = 0U,
    GPIO_PIN1  = 1U,
    GPIO_PIN2  = 2U,
    GPIO_PIN3  = 3U,
    GPIO_PIN4  = 4U,
    GPIO_PIN5  = 5U,
    GPIO_PIN6  = 6U,
    GPIO_PIN7  = 7U,
    GPIO_PIN8  = 8U,
    GPIO_PIN9  = 9U,
    GPIO_PIN10 = 10U,
    GPIO_PIN11 = 11U,
    GPIO_PIN12 = 12U,
    GPIO_PIN13 = 13U,
    GPIO_PIN14 = 14U,
    GPIO_PIN15 = 15U
} GPIO_PIN_t;

typedef enum{
    GPIO_PIN_RESET = 0U,
    GPIO_PIN_SET   = 1U   /* G-10: explicit value assigned — MISRA Rule 8.12 */
} GPIO_PinState_t;

/* G-08: explicit values assigned to all enumerators — MISRA Rule 8.12 */
typedef enum {
    GPIO_PORTA = 0U,
    GPIO_PORTB = 1U,
    GPIO_PORTC = 2U,
    GPIO_PORTD = 3U,
    GPIO_PORTE = 4U,
    GPIO_PORTH = 5U
} GPIO_PORT_t;

typedef enum
{
    GPIO_MODE_INPUT ,
    GPIO_MODE_OUTPUT ,
    GPIO_MODE_ALTERNATE ,
    GPIO_MODE_ANALOG    
} GPIO_Mode_t;

typedef enum
{
    GPIO_OUTPUT_PUSH_PULL ,
    GPIO_OUTPUT_OPEN_DRAIN    
} GPIO_OutputType_t;

typedef enum
{
    GPIO_NO_PULL ,
    GPIO_PULL_UP ,
    GPIO_PULL_DOWN     
} GPIO_PullType_t;

typedef enum
{
    GPIO_SPEED_LOW ,
    GPIO_SPEED_MEDIUM ,
    GPIO_SPEED_HIGH ,
    GPIO_SPEED_VERY_HIGH     
} GPIO_Speed_t;

/* G-09: explicit values matching STM32F401 AF table — AF_SDIO was resolving to 11 (wrong, is AF12),
         AF_EVENTOUT was resolving to 12 (wrong, is AF15) */
typedef enum
{
    AF_SYSTEM    = 0U,
    AF_TIM_1_2   = 1U,
    AF_TIM_3_5   = 2U,
    AF_TIM_9_11  = 3U,
    AF_I2C_1_3   = 4U,
    AF_SPI_1_4   = 5U,
    AF_SPI_3     = 6U,
    AF_USART_1_2 = 7U,
    AF_USART_6   = 8U,
    AF_I2C_2_3   = 9U,
    AF_OTG_FS    = 10U,
    AF_SDIO      = 12U,  /* G-09: SDIO is AF12 on STM32F401, not sequential 11 */
    AF_EVENTOUT  = 15U   /* G-09: EVENTOUT is AF15 on STM32F401, not sequential 12 */
} GPIO_AlternateFunction_t;

typedef struct
{
    GPIO_PIN_t        Pin;        // Pin number: e.g. GPIO_PIN_0, GPIO_PIN_5
    GPIO_PORT_t      Port;       // Port: e.g. GPIO_PORTA, GPIO_PORTB
    GPIO_Mode_t       Mode;       // Input, Output, AF, Analog
    GPIO_OutputType_t Type;       // Push-pull or Open-drain
    GPIO_Speed_t      Speed;      // Low, Medium, High, Very High
    GPIO_PullType_t       Pull;       // No pull, Pull-up, Pull-down
    GPIO_AlternateFunction_t           Alternate;  // AF number (if AF mode)
} GPIO_CONFIG_t;






GPIO_Error_t GPIO_INIT(GPIO_CONFIG_t *GPIO_Config);
GPIO_Error_t GPIO_DeInit(GPIO_PORT_t Port, GPIO_PIN_t Pin);
GPIO_Error_t GPIO_ReadPin(GPIO_PORT_t Port, GPIO_PIN_t Pin, uint8_t *State);
GPIO_Error_t GPIO_WritePin(GPIO_PORT_t Port, GPIO_PIN_t Pin, GPIO_PinState_t State);
GPIO_Error_t GPIO_TogglePin(GPIO_PORT_t Port, GPIO_PIN_t Pin);
GPIO_Error_t GPIO_LockPin(GPIO_PORT_t Port, GPIO_PIN_t Pin);
GPIO_Error_t GPIO_UnlockPin(GPIO_PORT_t Port, GPIO_PIN_t Pin);
GPIO_Error_t GPIO_ReadPinFast(GPIO_PORT_t Port, GPIO_PIN_t Pin, uint8_t *state);  /* changed: out-param avoids error/value ambiguity */














#endif // GPIO_INTERFACE_H