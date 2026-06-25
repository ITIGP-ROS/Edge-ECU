#include "STD_TYPES.h"
#include "GPIO_REGS.h"
#include "GPIO_INTERFACE.h"






GPIO_Error_t GPIO_INIT(GPIO_CONFIG_t *GPIO_Config)
{  
    GPIO_Error_t status = GPIO_OK;
    /* ========================================
       0) NULL PTR VALIDATION
       ======================================== */
    if (GPIO_Config == NULL){
        return GPIO_ERROR_NULL_PTR;
    }
    /* G-12: removed unreachable else { status = GPIO_OK; } — status is already GPIO_OK and no error was set */

    /* ========================================
       1) PIN VALIDATION
       ======================================= */
    if (GPIO_Config->Pin > GPIO_PIN_MAX_INDEX){  /* G-14: renamed MAX_GPIO_PINS -> GPIO_PIN_MAX_INDEX */
        return GPIO_ERROR_INVALID_PIN;
    }
    /* G-12: removed unreachable else { status = GPIO_OK; } */

    /* ========================================
       2) MODE VALIDATION
       ======================================== */
    if (GPIO_Config->Mode > MAX_GPIO_MODES){
        return GPIO_ERROR_INVALID_MODE;
    }
    /* G-12: removed unreachable else { status = GPIO_OK; } */

    /* ========================================
       3) PULL VALIDATION
       ======================================== */
    if (GPIO_Config->Pull > GPIO_PULL_DOWN){
        return GPIO_ERROR_INVALID_PUPD;
    }
    /* G-12: removed unreachable else { status = GPIO_OK; } */

    /* ========================================
       3a) ANALOG + PULL CONFLICT VALIDATION
       ======================================== */
    /* G-15: analog mode with pull resistors is contradictory — reject it */
    if (GPIO_Config->Mode == GPIO_MODE_ANALOG && GPIO_Config->Pull != GPIO_NO_PULL){
        return GPIO_ERROR_INVALID_PUPD;
    }

    /* ========================================
       4) SPEED VALIDATION (only for OUT/AF)
       ======================================== */
    if ((GPIO_Config->Mode == GPIO_MODE_OUTPUT ||
         GPIO_Config->Mode == GPIO_MODE_ALTERNATE) &&
         GPIO_Config->Speed > GPIO_SPEED_VERY_HIGH)
    {
        return GPIO_ERROR_INVALID_SPEED;
    }
    /* G-12: removed unreachable else { status = GPIO_OK; } */

    /* ========================================
       5) ALTERNATE FUNCTION VALIDATION
       ======================================== */
    if (GPIO_Config->Mode == GPIO_MODE_ALTERNATE &&
        GPIO_Config->Alternate > MAX_GPIO_AF)
    {
        return GPIO_ERROR_INVALID_AF;
    }
    /* G-12: removed unreachable else { status = GPIO_OK; } */

    /* ========================================
       6) PORT POINTER + CLOCK VALIDATION
       ======================================== */
   volatile GPIO_REGS_t *GPIOx = NULL;

    switch (GPIO_Config->Port)
    {
        case GPIO_PORTA:
            
            GPIOx = GPIOA;
            break;

        case GPIO_PORTB:
            GPIOx = GPIOB;
            break;
        case GPIO_PORTC:
            GPIOx = GPIOC;  
            break;
        case GPIO_PORTD:
            GPIOx = GPIOD;  
            break;
        case GPIO_PORTE:
            GPIOx = GPIOE;  
            break;
        case GPIO_PORTH:        
            GPIOx = GPIOH;  
            break;
        default:
            return GPIO_ERROR_INVALID_PIN;   // OR GPIO_ERROR_INVALID_PORT if you prefer
    }

    uint8_t  pin      = GPIO_Config->Pin;       /* G-13: removed volatile — local variable, no need for volatile */
    uint32_t pin_mask = (1U << pin);            /* G-13: removed volatile — same reason */

    /* ========================================
       7) CHECK IF PIN IS LOCKED (LCKR bit)
       ======================================== */
    if (GPIOx->LCKR.ALL & pin_mask){
       return GPIO_ERROR_PIN_LOCKED;
    }
    /* G-12: removed unreachable else { status = GPIO_OK; } */
    /* ========================================
       8) CHECK IF PIN IS JTAG/DEBUG PIN
          (PA13 = SWDIO, PA14 = SWCLK)
       ======================================== */
    if (GPIO_Config->Port == GPIO_PORTA &&
        (pin == 13U || pin == 14U))
    {
        return GPIO_ERROR_WRITE_PROTECTED;
    }
    /* G-12: removed unreachable else { status = GPIO_OK; } */

    /* ========================================
       9) CONFIGURE MODE (ALWAYS)
       ======================================== */
    GPIOx->MODER.ALL &= ~GPIO_MODE_MASK(pin);
    GPIOx->MODER.ALL |= ((uint32_t)GPIO_Config->Mode << (pin * 2));

    /* ========================================
       10) MODE-DEPENDENT CONFIGURATIONS
       ======================================== */

    /* --------------------------
       OUTPUT MODE
       -------------------------- */
    if (GPIO_Config->Mode == GPIO_MODE_OUTPUT)
    {
        // OTYPER
        GPIOx->OTYPER.ALL &= ~GPIO_OTYPE_MASK(pin);
        GPIOx->OTYPER.ALL |= ((uint32_t)GPIO_Config->Type  << pin);          /* G-18: cast to uint32_t before shift — MISRA Rule 10.1 */

        // OSPEEDR
        GPIOx->OSPEEDR.ALL &= ~GPIO_OSPEED_MASK(pin);
        GPIOx->OSPEEDR.ALL |= ((uint32_t)GPIO_Config->Speed << (pin * 2U));  /* G-18: cast to uint32_t before shift — MISRA Rule 10.1 */

        // PUPDR
        GPIOx->PUPDR.ALL &= ~GPIO_PUPDR_MASK(pin);
        GPIOx->PUPDR.ALL |= ((uint32_t)GPIO_Config->Pull  << (pin * 2U));    /* G-18: cast to uint32_t before shift — MISRA Rule 10.1 */

        // Init output LOW (safe default)
        GPIOx->BSRR.ALL = GPIO_BSRR_RESET_MASK(pin);
    }

    /* --------------------------
       INPUT MODE
       -------------------------- */
    else if (GPIO_Config->Mode == GPIO_MODE_INPUT)
    {
        // ONLY PUPDR is relevant
        GPIOx->PUPDR.ALL &= ~GPIO_PUPDR_MASK(pin);
        GPIOx->PUPDR.ALL |= ((uint32_t)GPIO_Config->Pull << (pin * 2U));  /* G-18: cast to uint32_t before shift — MISRA Rule 10.1 */

        // No OSPEEDR, no OTYPER, no AFR, no BSRR
    }

    /* --------------------------
       ANALOG MODE
       -------------------------- */
    else if (GPIO_Config->Mode == GPIO_MODE_ANALOG)
    {
        // Disable pull resistors
        GPIOx->PUPDR.ALL &= ~GPIO_PUPDR_MASK(pin);

        // Clear speed settings (optional)
        GPIOx->OSPEEDR.ALL &= ~GPIO_OSPEED_MASK(pin);

        // No AFR, no OTYPER, no BSRR
    }

    /* --------------------------
       ALTERNATE FUNCTION MODE
       -------------------------- */
    else if (GPIO_Config->Mode == GPIO_MODE_ALTERNATE)
    {
        // OTYPER
        GPIOx->OTYPER.ALL &= ~GPIO_OTYPE_MASK(pin);
        GPIOx->OTYPER.ALL |= ((uint32_t)GPIO_Config->Type  << pin);          /* G-18: cast to uint32_t before shift — MISRA Rule 10.1 */

        // OSPEEDR
        GPIOx->OSPEEDR.ALL &= ~GPIO_OSPEED_MASK(pin);
        GPIOx->OSPEEDR.ALL |= ((uint32_t)GPIO_Config->Speed << (pin * 2U));  /* G-18: cast to uint32_t before shift — MISRA Rule 10.1 */

        // PUPDR
        GPIOx->PUPDR.ALL &= ~GPIO_PUPDR_MASK(pin);
        GPIOx->PUPDR.ALL |= ((uint32_t)GPIO_Config->Pull  << (pin * 2U));    /* G-18: cast to uint32_t before shift — MISRA Rule 10.1 */

        // AFR
        if (pin < 8)
        {
            GPIOx->AFRL.ALL &= ~GPIO_AFRL_MASK(pin);
            GPIOx->AFRL.ALL |= ((uint32_t)GPIO_Config->Alternate << (pin * 4));
        }
        else
        {
            GPIOx->AFRH.ALL &= ~GPIO_AFRH_MASK(pin);
            GPIOx->AFRH.ALL |= ((uint32_t)GPIO_Config->Alternate << ((pin - 8) * 4));
        }
    }

    return status;
}

GPIO_Error_t GPIO_UnlockPin(GPIO_PORT_t Port, GPIO_PIN_t Pin)
{
    /* G-16: GPIO lock is hardware-irreversible until system reset on STM32F4.
     *       Once a pin is locked via the LCKR sequence, the LCKK bit stays set
     *       and can only be cleared by a system reset — there is no software unlock.
     *       The previous implementation looped waiting for LCKK to go to 0, which
     *       never happens, causing an infinite timeout. Return write-protected instead.
     *       G-14: removed reference to old MAX_GPIO_PINS (now GPIO_PIN_MAX_INDEX). */
    (void)Port;  /* suppress unused-parameter warning; signature kept for caller compatibility */
    (void)Pin;
    return GPIO_ERROR_WRITE_PROTECTED;
}
GPIO_Error_t GPIO_DeInit(GPIO_PORT_t Port, GPIO_PIN_t Pin)
{
   volatile GPIO_REGS_t *GPIOx = NULL;

    /* -----------------------------
       1) Validate pin number
       ----------------------------- */
    if (Pin > 15U)
    {
        return GPIO_ERROR_INVALID_PIN;
    }

    /* -----------------------------
       2) Select port base pointer
       ----------------------------- */
    switch (Port)
    {
        case GPIO_PORTA: GPIOx = GPIOA; break;
        case GPIO_PORTB: GPIOx = GPIOB; break;
        case GPIO_PORTC: GPIOx = GPIOC; break;
        case GPIO_PORTD: GPIOx = GPIOD; break;
        case GPIO_PORTE: GPIOx = GPIOE; break;
        case GPIO_PORTH: GPIOx = GPIOH; break;
        default:
            return GPIO_ERROR_INVALID_PIN;
    }

    /* -----------------------------
       3) Do NOT allow deinit of SWD pins
       ----------------------------- */
    if (Port == GPIO_PORTA && (Pin == 13U || Pin == 14U))
    {
        return GPIO_ERROR_WRITE_PROTECTED;
    }

    /* -----------------------------
       4) Clear MODE (set to input)
       ----------------------------- */
    GPIOx->MODER.ALL &= ~GPIO_MODE_MASK(Pin);

    /* -----------------------------
       5) Clear OTYPER (default PP)
       ----------------------------- */
    GPIOx->OTYPER.ALL &= ~GPIO_OTYPE_MASK(Pin);

    /* -----------------------------
       6) Clear OSPEEDR (low speed)
       ----------------------------- */
    GPIOx->OSPEEDR.ALL &= ~GPIO_OSPEED_MASK(Pin);

    /* -----------------------------
       7) Clear PUPDR (no pull)
       ----------------------------- */
    GPIOx->PUPDR.ALL &= ~GPIO_PUPDR_MASK(Pin);

    /* -----------------------------
       8) Clear AFR (set AF0)
       ----------------------------- */
    if (Pin < 8U)
    {
        GPIOx->AFRL.ALL &= ~GPIO_AFRL_MASK(Pin);
    }
    else
    {
        GPIOx->AFRH.ALL &= ~GPIO_AFRH_MASK(Pin);
    }

    /* -----------------------------
       9) Clear output data (safe)
       ----------------------------- */
    GPIOx->BSRR.ALL = GPIO_BSRR_RESET_MASK(Pin);

    return GPIO_OK;
}
GPIO_Error_t GPIO_ReadPin(GPIO_PORT_t Port, GPIO_PIN_t Pin, uint8_t *State)
{
    volatile GPIO_REGS_t *GPIOx = NULL;

    /* ------------------------------
       1) Validate arguments
       ------------------------------ */
    if (State == NULL)
    {
        return GPIO_ERROR_NULL_PTR;
    }

    if (Pin > 15U)
    {
        return GPIO_ERROR_INVALID_PIN;
    }

    /* ------------------------------
       2) Select GPIO port pointer
       ------------------------------ */
    switch (Port)
    {
        case GPIO_PORTA: GPIOx = GPIOA; break;
        case GPIO_PORTB: GPIOx = GPIOB; break;
        case GPIO_PORTC: GPIOx = GPIOC; break;
        case GPIO_PORTD: GPIOx = GPIOD; break;
        case GPIO_PORTE: GPIOx = GPIOE; break;
        case GPIO_PORTH: GPIOx = GPIOH; break;
        default:
            return GPIO_ERROR_INVALID_PIN;
    }

    /* ------------------------------
       3) Read the pin state
       ------------------------------ */
    uint32_t pinMask = (1U << Pin);

    if ((GPIOx->IDR.ALL & pinMask) != 0U)
    {
        *State = 1U;     // HIGH
    }
    else
    {
        *State = 0U;     // LOW
    }

    return GPIO_OK;
}
GPIO_Error_t GPIO_ReadPinFast(GPIO_PORT_t Port, GPIO_PIN_t Pin, uint8_t *state)  /* changed: out-param replaces raw return */
{
   volatile GPIO_REGS_t *GPIOx = NULL;
    if (state == NULL) return GPIO_ERROR_NULL_PTR;  /* changed: NULL guard on output pointer */
    if (Pin > 15U)
    {
        return GPIO_ERROR_INVALID_PIN;  /* changed: proper error code replaces 0xFF sentinel */
    }
    /* ------------------------------
       1) Select GPIO port pointer
       ------------------------------ */
    switch (Port)
    {
        case GPIO_PORTA: GPIOx = GPIOA; break;
        case GPIO_PORTB: GPIOx = GPIOB; break;
        case GPIO_PORTC: GPIOx = GPIOC; break;
        case GPIO_PORTD: GPIOx = GPIOD; break;
        case GPIO_PORTE: GPIOx = GPIOE; break;
        case GPIO_PORTH: GPIOx = GPIOH; break;
        default:
          return GPIO_ERROR_INVALID_PIN;  /* changed: proper error code replaces 0xFF sentinel */
    }

    *state = (uint8_t)((GPIOx->IDR.ALL >> Pin) & 1U);  /* changed: write pin state to caller's variable */
    return GPIO_OK;
}

GPIO_Error_t GPIO_TogglePin(GPIO_PORT_t Port, GPIO_PIN_t Pin)
{
   volatile GPIO_REGS_t *GPIOx = NULL;

    /* Validate pin number */
    if (Pin > 15U)
        return GPIO_ERROR_INVALID_PIN;

    /* Select port */
    switch (Port)
    {
        case GPIO_PORTA: GPIOx = GPIOA; break;
        case GPIO_PORTB: GPIOx = GPIOB; break;
        case GPIO_PORTC: GPIOx = GPIOC; break;
        case GPIO_PORTD: GPIOx = GPIOD; break;
        case GPIO_PORTE: GPIOx = GPIOE; break;
        case GPIO_PORTH: GPIOx = GPIOH; break;
        default:
            return GPIO_ERROR_INVALID_PIN;
    }

    /* Read current pin state */
    uint32_t mask = GPIO_ODR_MASK(Pin);

    if ((GPIOx->ODR.ALL & mask) != 0U)
    {
        /* Pin is HIGH → reset it */
        GPIOx->BSRR.ALL = GPIO_BSRR_RESET_MASK(Pin);
    }
    else
    {
        /* Pin is LOW → set it */
        GPIOx->BSRR.ALL = GPIO_BSRR_SET_MASK(Pin);
    }

    return GPIO_OK;
}
GPIO_Error_t GPIO_WritePin(GPIO_PORT_t Port, GPIO_PIN_t Pin, GPIO_PinState_t State)
{
   volatile GPIO_REGS_t *GPIOx = NULL;

    /* Validate pin number */
    if (Pin > 15U)
        return GPIO_ERROR_INVALID_PIN;

    /* Select port */
    switch (Port)
    {
        case GPIO_PORTA: GPIOx = GPIOA; break;
        case GPIO_PORTB: GPIOx = GPIOB; break;
        case GPIO_PORTC: GPIOx = GPIOC; break;
        case GPIO_PORTD: GPIOx = GPIOD; break;
        case GPIO_PORTE: GPIOx = GPIOE; break;
        case GPIO_PORTH: GPIOx = GPIOH; break;
        default:
            return GPIO_ERROR_INVALID_PIN;
    }

    /* Write pin state */
    if (State != GPIO_PIN_RESET)
    {
        /* Set pin HIGH */
        GPIOx->BSRR.ALL = GPIO_BSRR_SET_MASK(Pin);
    }
    else
    {
        /* Set pin LOW */
        GPIOx->BSRR.ALL = GPIO_BSRR_RESET_MASK(Pin);
    }

    return GPIO_OK;
}

GPIO_Error_t GPIO_LockPin(GPIO_PORT_t Port, GPIO_PIN_t Pin)
{
   volatile GPIO_REGS_t *GPIOx = NULL;

    /* -----------------------------
       1) Validate pin number
       ----------------------------- */
    if (Pin > 15U)
    {
        return GPIO_ERROR_INVALID_PIN;
    }

    /* -----------------------------
       2) Select GPIO port
       ----------------------------- */
    switch (Port)
    {
        case GPIO_PORTA: GPIOx = GPIOA; break;
        case GPIO_PORTB: GPIOx = GPIOB; break;
        case GPIO_PORTC: GPIOx = GPIOC; break;
        case GPIO_PORTD: GPIOx = GPIOD; break;
        case GPIO_PORTE: GPIOx = GPIOE; break;
        case GPIO_PORTH: GPIOx = GPIOH; break;
        default:
            return GPIO_ERROR_INVALID_PIN;
    }

    /* -----------------------------
       3) SWD pins MUST NOT be locked
          PA13 = SWDIO, PA14 = SWCLK
       ----------------------------- */
    if (Port == GPIO_PORTA && (Pin == 13U || Pin == 14U))
    {
        return GPIO_ERROR_WRITE_PROTECTED;
    }

    /* -----------------------------
       4) Prepare mask & lock bit
       ----------------------------- */
    const uint32_t LCKK = (1U << 16);
    uint32_t mask = GPIO_LCKR_MASK(Pin);

    /* -----------------------------
       5) LOCK SEQUENCE (RM0368)
          Step1: LCKx=1, LCKK=1
          Step2: LCKx=1, LCKK=0
          Step3: LCKx=1, LCKK=1
          Step4: Read LCKR
       ----------------------------- */

    /* Step 1 */
    GPIOx->LCKR.ALL = (mask | LCKK);

    /* Step 2 */
    GPIOx->LCKR.ALL = mask;

    /* Step 3 */
    GPIOx->LCKR.ALL = (mask | LCKK);

    /* Step 4 (dummy read) */
    volatile uint32_t tmp = GPIOx->LCKR.ALL;
    (void)tmp;   /* suppress unused warning */

    /* -----------------------------
       6) Verify pin is locked
          LCKK must now be 1
       ----------------------------- */
    if ((GPIOx->LCKR.ALL & LCKK) == 0U)
    {
        return GPIO_ERROR_UNKNOWN;  // Should not happen
    }

    return GPIO_OK;
}
