#include "NVIC_INTERFACE.h"
#include "NVIC_REGS.h"
#include "STD_TYPES.h"




NVIC_ERROR_t NVIC_EnableIRQ(IRQn_t IRQn)
{
    /* -------------------------------------------------------
     * 1) Extract encoded fields
     * ------------------------------------------------------- */
    uint32_t irq_num   = GET_IRQ_NUM(IRQn);
    uint32_t reg_index = GET_REG_INDEX(IRQn);
    uint32_t bit_pos   = GET_BIT_POS(IRQn);

    /* -------------------------------------------------------
     * 2) Validate IRQ number range for STM32F401
     * Valid external interrupts → 0 → 84
     * ------------------------------------------------------- */
    if (irq_num > NVIC_MAX_IRQ_NUM)    /* N-11: was 239U; use named constant for STM32F401CC max IRQ (SPI4 = 84) */
        return NVIC_ERROR_INVALID_IRQ_NUM;

    /* -------------------------------------------------------
     * 3) Validate register index (0 → 2 for F401)
     * ------------------------------------------------------- */
    if (reg_index > 7U)
        return NVIC_ERROR_INVALID_REG_INDEX;

    /* -------------------------------------------------------
     * 4) Validate bit position (0 → 31)
     * ------------------------------------------------------- */
    if (bit_pos > 31U)
        return NVIC_ERROR_INVALID_BIT_POS;

    /* -------------------------------------------------------
     * 5) Validate PACK_NVIC encoding consistency
     * irq_num must match (reg_index*32 + bit_pos)
     * ------------------------------------------------------- */
    if (irq_num != (reg_index * 32U + bit_pos))
        return NVIC_ERROR_INCONSISTENT_ENCODING;

    /* -------------------------------------------------------
     * 6) Compute the mask for ISER
     * ISER is W1S → writing 0 has no effect
     * ------------------------------------------------------- */
    uint32_t mask = (1UL << bit_pos);

    /* -------------------------------------------------------
     * 7) Enable interrupt
     * W1S register: writing 0 has no effect; |= unnecessary
     * ------------------------------------------------------- */
    NVIC->ISER.ISER_REG[reg_index] = mask;  /* N-12: was |=; W1S register — writing 0 has no effect, |= unnecessary */

    return NVIC_ERROR_OK;
}


NVIC_ERROR_t NVIC_DisableIRQ(IRQn_t IRQn)
{
    /* -------------------------------------------------------
     * 1) Extract encoded fields
     * ------------------------------------------------------- */
    uint32_t irq_num   = GET_IRQ_NUM(IRQn);
    uint32_t reg_index = GET_REG_INDEX(IRQn);
    uint32_t bit_pos   = GET_BIT_POS(IRQn);

    /* -------------------------------------------------------
     * 2) Validate IRQ number (0 → 84 for STM32F401)
     * ------------------------------------------------------- */
    if (irq_num > NVIC_MAX_IRQ_NUM)    /* N-11: was 239U; use named constant for STM32F401CC max IRQ (SPI4 = 84) */
        return NVIC_ERROR_INVALID_IRQ_NUM;

    /* -------------------------------------------------------
     * 3) Validate register index (0 → 2 for ISER/ICER/ISPR/ICPR/IABR)
     * ------------------------------------------------------- */
    if (reg_index > 7U)
        return NVIC_ERROR_INVALID_REG_INDEX;

    /* -------------------------------------------------------
     * 4) Validate bit position (0 → 31)
     * ------------------------------------------------------- */
    if (bit_pos > 31U)
        return NVIC_ERROR_INVALID_BIT_POS;

    /* -------------------------------------------------------
     * 5) Validate PACK_NVIC encoding consistency
     * irq_num MUST equal reg_index*32 + bit_pos
     * ------------------------------------------------------- */
    if (irq_num != (reg_index * 32U + bit_pos))
        return NVIC_ERROR_INCONSISTENT_ENCODING;

    /* -------------------------------------------------------
     * 6) Create write-1-to-clear mask for ICER
     * ------------------------------------------------------- */
    uint32_t mask = (1UL << bit_pos);

    /* -------------------------------------------------------
     * 7) Disable interrupt
     * ICER is W1C → writing 1 clears the enable bit
     * ------------------------------------------------------- */
    NVIC->ICER.ICER_REG[reg_index] = mask;

    return NVIC_ERROR_OK;
}



NVIC_ERROR_t NVIC_SetPendingIRQ(IRQn_t IRQn)
{
    /* -------------------------------------------------------
     * 1) Extract encoded fields from your packed IRQ number
     * ------------------------------------------------------- */
    uint32_t irq_num   = GET_IRQ_NUM(IRQn);
    uint32_t reg_index = GET_REG_INDEX(IRQn);
    uint32_t bit_pos   = GET_BIT_POS(IRQn);

    /* -------------------------------------------------------
     * 2) Validate raw IRQ number range
     * STM32F401 supports only external IRQs 0 → 84
     * ------------------------------------------------------- */
    if (irq_num > NVIC_MAX_IRQ_NUM)    /* N-11: was 239U; use named constant for STM32F401CC max IRQ (SPI4 = 84) */
        return NVIC_ERROR_INVALID_IRQ_NUM;

    /* -------------------------------------------------------
     * 3) Validate register index (must be: 0 → 7 for F401)
     * IRQ 0–31 → ISPR0
     * IRQ 32–63 → ISPR1
     * IRQ 64–84 → ISPR2
     * ------------------------------------------------------- */
    if (reg_index > 7U)
        return NVIC_ERROR_INVALID_REG_INDEX;

    /* -------------------------------------------------------
     * 4) Validate bit position (0 → 31 only)
     * ------------------------------------------------------- */
    if (bit_pos > 31U)
        return NVIC_ERROR_INVALID_BIT_POS;

    /* -------------------------------------------------------
     * 5) Validate encoding consistency
     * irq_num MUST equal reg_index*32 + bit_pos
     * If not — your encoded IRQ value is wrong/corrupted
     * ------------------------------------------------------- */
    if (irq_num != (reg_index * 32U + bit_pos))
        return NVIC_ERROR_INCONSISTENT_ENCODING;

    /* -------------------------------------------------------
     * 6) Form the bitmask for NVIC_ISPR
     * ------------------------------------------------------- */
    uint32_t mask = (1UL << bit_pos);

    /* -------------------------------------------------------
     * 7) Set the pending bit
     * W1S register: writing 0 has no effect; |= unnecessary
     * ------------------------------------------------------- */
    NVIC->ISPR.ISPR_REG[reg_index] = mask;  /* N-12: was |=; W1S register — writing 0 has no effect, |= unnecessary */

    return NVIC_ERROR_OK;
}

NVIC_ERROR_t NVIC_GetPendingIRQ(IRQn_t IRQn, uint8_t *pending)  /* N-14: signature changed — uint32_t return replaced with NVIC_ERROR_t + output pointer */
{
    /* -------------------------------------------------------
     * 1) Extract encoded fields
     * ------------------------------------------------------- */
    uint32_t irq_num   = GET_IRQ_NUM(IRQn);
    uint32_t reg_index = GET_REG_INDEX(IRQn);
    uint32_t bit_pos   = GET_BIT_POS(IRQn);

    /* -------------------------------------------------------
     * 2) Validate IRQ number range for STM32F401
     * External IRQs range: 0 → 84
     * ------------------------------------------------------- */
    if (irq_num > NVIC_MAX_IRQ_NUM)    /* N-11: was 239U; use named constant for STM32F401CC max IRQ (SPI4 = 84) */
        return NVIC_ERROR_INVALID_IRQ_NUM;

    /* -------------------------------------------------------
     * 3) Validate register index (0 → 7)
     * ------------------------------------------------------- */
    if (reg_index > 7U)
        return NVIC_ERROR_INVALID_REG_INDEX;

    /* -------------------------------------------------------
     * 4) Validate bit position (0 → 31)
     * ------------------------------------------------------- */
    if (bit_pos > 31U)
        return NVIC_ERROR_INVALID_BIT_POS;

    /* -------------------------------------------------------
     * 5) Validate encoding consistency
     * irq_num MUST equal reg_index*32 + bit_pos
     * ------------------------------------------------------- */
    if (irq_num != (reg_index * 32U + bit_pos))
        return NVIC_ERROR_INCONSISTENT_ENCODING;

    /* -------------------------------------------------------
     * 6) Validate output pointer before dereferencing
     * ------------------------------------------------------- */
    if (pending == (void *)0U)         /* N-14: NULL check — return INVALID_PARAM instead of dereferencing null */
        return NVIC_ERROR_INVALID_PARAM;

    /* -------------------------------------------------------
     * 7) Read the pending bit state from NVIC_ISPR and write
     *    result through output pointer
     * Returns via pointer:
     *   1 → interrupt is pending
     *   0 → interrupt is not pending
     * ------------------------------------------------------- */
    uint32_t reg_value = NVIC->ISPR.ISPR_REG[reg_index];
    *pending = (uint8_t)((reg_value >> bit_pos) & 1U); /* N-14: result written through pointer; cast to uint8_t */

    return NVIC_ERROR_OK;
}

NVIC_ERROR_t NVIC_ClearPendingIRQ(IRQn_t IRQn)
{
    /* -------------------------------------------------------
     * 1) Extract fields from your packed IRQ format
     * ------------------------------------------------------- */
    uint32_t irq_num   = GET_IRQ_NUM(IRQn);
    uint32_t reg_index = GET_REG_INDEX(IRQn);
    uint32_t bit_pos   = GET_BIT_POS(IRQn);

    /* -------------------------------------------------------
     * 2) Validate IRQ number range for STM32F401
     * Valid external interrupts: 0 → 84
     * ------------------------------------------------------- */
    if (irq_num > NVIC_MAX_IRQ_NUM)    /* N-11: was 239U; use named constant for STM32F401CC max IRQ (SPI4 = 84) */
        return NVIC_ERROR_INVALID_IRQ_NUM;

    /* -------------------------------------------------------
     * 3) Validate register index
     * F401 uses ICPR0, ICPR1, ICPR2 only
     * ------------------------------------------------------- */
    if (reg_index > 7U)
        return NVIC_ERROR_INVALID_REG_INDEX;

    /* -------------------------------------------------------
     * 4) Validate bit position (0–31)
     * ------------------------------------------------------- */
    if (bit_pos > 31U)
        return NVIC_ERROR_INVALID_BIT_POS;

    /* -------------------------------------------------------
     * 5) Validate encoded consistency
     * irq_num MUST equal (reg_index*32 + bit_pos)
     * ------------------------------------------------------- */
    if (irq_num != (reg_index * 32U + bit_pos))
        return NVIC_ERROR_INCONSISTENT_ENCODING;

    /* -------------------------------------------------------
     * 6) Compute bit mask
     * ------------------------------------------------------- */
    uint32_t mask = (1UL << bit_pos);

    /* -------------------------------------------------------
     * 7) Clear interrupt pending bit
     * NVIC ICPR registers are write-1-to-clear
     * ------------------------------------------------------- */
    NVIC->ICPR.ICPR_REG[reg_index] = mask;

    return NVIC_ERROR_OK;
}

NVIC_ERROR_t NVIC_GetActive(IRQn_t IRQn, uint8_t *active)  /* N-14: signature changed — uint32_t return replaced with NVIC_ERROR_t + output pointer */
{
    /* -------------------------------------------------------
     * 1) Extract encoded fields
     * ------------------------------------------------------- */
    uint32_t irq_num   = GET_IRQ_NUM(IRQn);
    uint32_t reg_index = GET_REG_INDEX(IRQn);
    uint32_t bit_pos   = GET_BIT_POS(IRQn);

    /* -------------------------------------------------------
     * 2) Validate IRQ number (STM32F401 supports 0 → 84)
     * ------------------------------------------------------- */
    if (irq_num > NVIC_MAX_IRQ_NUM)    /* N-11: was 239U; use named constant for STM32F401CC max IRQ (SPI4 = 84) */
        return NVIC_ERROR_INVALID_IRQ_NUM;

    /* -------------------------------------------------------
     * 3) Validate register index (0 → 7)
     * ------------------------------------------------------- */
    if (reg_index > 7U)
        return NVIC_ERROR_INVALID_REG_INDEX;

    /* -------------------------------------------------------
     * 4) Validate bit position (0 → 31)
     * ------------------------------------------------------- */
    if (bit_pos > 31U)
        return NVIC_ERROR_INVALID_BIT_POS;

    /* -------------------------------------------------------
     * 5) Validate packed encoding consistency
     * irq_num must equal (reg_index*32 + bit_pos)
     * ------------------------------------------------------- */
    if (irq_num != (reg_index * 32U + bit_pos))
        return NVIC_ERROR_INCONSISTENT_ENCODING;

    /* -------------------------------------------------------
     * 6) Validate output pointer before dereferencing
     * ------------------------------------------------------- */
    if (active == (void *)0U)          /* N-14: NULL check — return INVALID_PARAM instead of dereferencing null */
        return NVIC_ERROR_INVALID_PARAM;

    /* -------------------------------------------------------
     * 7) Read IABR register (Interrupt Active Bit Register)
     *    and write result through output pointer
     * Returns via pointer:
     *   1 → interrupt is active
     *   0 → interrupt is inactive
     * ------------------------------------------------------- */
    uint32_t reg_value = NVIC->IABR.IABR_REG[reg_index];
    *active = (uint8_t)((reg_value >> bit_pos) & 1U);  /* N-14: result written through pointer; cast to uint8_t */

    return NVIC_ERROR_OK;
}

NVIC_ERROR_t NVIC_SetPriority(IRQn_t IRQn, uint32_t priority)
{
    /* -------------------------------------------------------
     * 1) Extract packed fields
     * ------------------------------------------------------- */
    uint32_t irq_num   = GET_IRQ_NUM(IRQn);
    uint32_t reg_index = GET_REG_INDEX(IRQn);
    uint32_t bit_pos   = GET_BIT_POS(IRQn);

    /* -------------------------------------------------------
     * 2) Validate IRQ number for STM32F401 (0 → 84)
     * ------------------------------------------------------- */
    if (irq_num > NVIC_MAX_IRQ_NUM)    /* N-11: was 239U; use named constant for STM32F401CC max IRQ (SPI4 = 84) */
        return NVIC_ERROR_INVALID_IRQ_NUM;

    /* -------------------------------------------------------
     * 3) Validate register index (0 → 7)
     * ------------------------------------------------------- */
    if (reg_index > 7U)                /* N-13: missing validation step added — matches pattern in NVIC_EnableIRQ */
        return NVIC_ERROR_INVALID_REG_INDEX;

    /* -------------------------------------------------------
     * 4) Validate bit position (0 → 31)
     * ------------------------------------------------------- */
    if (bit_pos > 31U)                 /* N-13: missing validation step added — matches pattern in NVIC_EnableIRQ */
        return NVIC_ERROR_INVALID_BIT_POS;

    /* -------------------------------------------------------
     * 5) Validate PACK_NVIC encoding consistency
     * irq_num MUST equal reg_index*32 + bit_pos
     * ------------------------------------------------------- */
    if (irq_num != (reg_index * 32U + bit_pos))  /* N-16: was duplicate "step 2"; renumbered step 5 */
        return NVIC_ERROR_INCONSISTENT_ENCODING;

    /* -------------------------------------------------------
     * 6) Validate priority value (0 → 15)
     * Only upper 4 bits are used on Cortex-M4
     * ------------------------------------------------------- */
    if (priority > 15U)                /* N-16: was "step 3"; renumbered step 6 */
        return NVIC_ERROR_INVALID_PRIORITY;

    /* -------------------------------------------------------
     * 7) Build the 8-bit priority byte
     *
     * Bits[7:4] = priority
     * Bits[3:0] = 0 (unused)
     * ------------------------------------------------------- */
    uint8_t priority_byte = (uint8_t)(priority << 4U); /* N-16: was "step 4"; renumbered step 7 */

    /* -------------------------------------------------------
     * 8) Set priority:
     * IPR_BYTE[irq_num] maps 1:1 to each interrupt priority
     * ------------------------------------------------------- */
    NVIC->IPR.IPR_BYTE[irq_num] = priority_byte; /* N-16: was "step 5"; renumbered step 8 */

    return NVIC_ERROR_OK;
}

NVIC_ERROR_t NVIC_GetPriority(IRQn_t IRQn, uint8_t *priority)  /* N-14: signature changed — uint32_t return replaced with NVIC_ERROR_t + output pointer */
{
    /* -------------------------------------------------------
     * 1) Extract PACK_NVIC fields
     * ------------------------------------------------------- */
    uint32_t irq_num   = GET_IRQ_NUM(IRQn);
    uint32_t reg_index = GET_REG_INDEX(IRQn);
    uint32_t bit_pos   = GET_BIT_POS(IRQn);

    /* -------------------------------------------------------
     * 2) Validate IRQ number (STM32F401 supports 0 → 84)
     * ------------------------------------------------------- */
    if (irq_num > NVIC_MAX_IRQ_NUM)    /* N-11: was 239U; use named constant for STM32F401CC max IRQ (SPI4 = 84) */
        return NVIC_ERROR_INVALID_IRQ_NUM;

    /* -------------------------------------------------------
     * 3) Validate register index (0 → 7)
     * ------------------------------------------------------- */
    if (reg_index > 7U)
        return NVIC_ERROR_INVALID_REG_INDEX;

    /* -------------------------------------------------------
     * 4) Validate bit position (0 → 31)
     * ------------------------------------------------------- */
    if (bit_pos > 31U)
        return NVIC_ERROR_INVALID_BIT_POS;

    /* -------------------------------------------------------
     * 5) Validate packed encoding consistency
     * irq_num MUST equal (reg_index*32 + bit_pos)
     * ------------------------------------------------------- */
    if (irq_num != (reg_index * 32U + bit_pos))
        return NVIC_ERROR_INCONSISTENT_ENCODING;

    /* -------------------------------------------------------
     * 6) Validate output pointer before dereferencing
     * ------------------------------------------------------- */
    if (priority == (void *)0U)        /* N-14: NULL check — return INVALID_PARAM instead of dereferencing null */
        return NVIC_ERROR_INVALID_PARAM;

    /* -------------------------------------------------------
     * 7) Read priority byte and write result through pointer
     * Cortex-M4 stores 8 bits per IRQ in IPR_BYTE[]
     * Only upper 4 bits [7:4] are implemented
     * priority = priority_byte >> 4  (0 → 15)
     * ------------------------------------------------------- */
    uint8_t priority_byte = NVIC->IPR.IPR_BYTE[irq_num];
    *priority = (uint8_t)(priority_byte >> 4U);  /* N-14: result written through pointer; cast to uint8_t */

    return NVIC_ERROR_OK;
}


NVIC_ERROR_t NVIC_SetPriorityGrouping(uint32_t priority_grouping)
{
    /* ----------------------------------------------------------
     * 1) Validate PRIGROUP (only bits [2:0])
     * ---------------------------------------------------------- */
    if (priority_grouping > 7U)
        return NVIC_ERROR_INVALID_PRIORITY_GROUP;

    /* ----------------------------------------------------------
     * 2) Read current AIRCR but mask out PRIGROUP
     * Keep SYSRESETREQ, ENDIANNESS, and reserved bits untouched
     * ---------------------------------------------------------- */
    uint32_t current = SCB->AIRCR.ALL;

    /* Remove old PRIGROUP */
    current &= ~(AIRCR_PRIGROUP_MASK | AIRCR_VECTKEY_MASK);

    /* ----------------------------------------------------------
     * 3) Write back AIRCR with:
     *    - VECTKEY = 0x5FA
     *    - New PRIGROUP value
     * ---------------------------------------------------------- */
    SCB->AIRCR.ALL = current |
                     AIRCR_VECTKEY |
                     (priority_grouping << 8U);

    /* ----------------------------------------------------------
     * 4) Readback verify — hardware ignores write if VECTKEY
     *    was wrong; confirm PRIGROUP was accepted
     * ---------------------------------------------------------- */
    uint32_t readback = (SCB->AIRCR.ALL & AIRCR_PRIGROUP_MASK) >> 8U;  /* N-17: readback added — detects VECTKEY failure */
    if (readback != priority_grouping)                                   /* N-17: write not accepted → return error */
        return NVIC_ERROR_INVALID_PRIORITY_GROUP;

    return NVIC_ERROR_OK;
}



void __attribute__((noreturn)) NVIC_SystemReset(void)  /* N-15: return type changed to void (noreturn returning error is meaningless); noreturn attribute added */
{
    uint32_t reg = SCB->AIRCR.ALL;
    reg &= ~AIRCR_VECTKEY_MASK;
    reg |= AIRCR_VECTKEY | AIRCR_SYSRESETREQ;

    __DSB();              /* N-15: data sync barrier added — ensures all memory writes complete before reset asserts */
    SCB->AIRCR.ALL = reg; /* triggers system reset */
    while (1U);           /* N-15: unreachable safety net — reset in progress; prevents compiler warning on noreturn */
}
