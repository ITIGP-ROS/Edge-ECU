#include "STD_TYPES.h"
#include "RCC_INTERFACE.h"
#include "RCC_REGS.h"

/* Issue 2: FLASH ACR register address and control bit masks — no HAL, direct MMIO access */
#define FLASH_ACR_REG     (*((volatile uint32_t *)0x40023C00U))
#define FLASH_ACR_LATENCY  (0x07U)         /* bits[2:0]: wait state count mask */
#define FLASH_ACR_PRFTEN   (1U << 8U)      /* prefetch enable */
#define FLASH_ACR_ICEN     (1U << 9U)      /* instruction cache enable */
#define FLASH_ACR_DCEN     (1U << 10U)     /* data cache enable */

static const uint16_t AHB_PRESC_TABLE[16] =
{
    1,1,1,1,1,1,1,1,   /* 0xxx → /1 */
    2,                  /* 1000      */
    4,                  /* 1001      */
    8,                  /* 1010      */
    16,                 /* 1011      */
    64,                 /* 1100      */
    128,                /* 1101      */
    256,                /* 1110      */
    512                 /* 1111      */
};

static const uint8_t APB_PRESC_TABLE[8] =
{
    1,1,1,1,   /* 0xx → /1 */
    2,         /* 100       */
    4,         /* 101       */
    8,         /* 110       */
    16         /* 111       */
};

/* Issue 2: Configure FLASH wait states + instruction/data caches before a clock change.
   Per RM0368 Table 6 (VDD 2.7–3.6 V): ≤30 MHz→0 WS, ≤60 MHz→1 WS, ≤84 MHz→2 WS. */
static void RCC_ConfigureFlashLatency(uint32_t hclk_freq)
{
    uint32_t latency;
    if (hclk_freq <= 30000000UL)
        latency = 0U;
    else if (hclk_freq <= 60000000UL)
        latency = 1U;
    else
        latency = 2U;  /* 84 MHz requires 2 wait states per RM0368 Table 6 */

    FLASH_ACR_REG = (FLASH_ACR_REG & ~FLASH_ACR_LATENCY) |
                    (latency | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN);
}

RCC_Error_t RCC_SET_SYSCLK(RCC_SYSCLK_TYPE_t clk_type)
{
    uint32_t timeout = 10000U;
    uint32_t sw_target;
    uint32_t sw_timeout;
    uint32_t target_freq = 0U;

    switch (clk_type)
    {
        case CLK_HSI:
            RCC->CR.BITS.HSION = 1;
            while (RCC->CR.BITS.HSIRDY == CLK_NOT_READY)
            {
                if (--timeout == 0U)
                    return RCC_ERROR_TIMEOUT;
            }
            target_freq = PLL_HSI_FREQ;   /* 16 MHz */
            sw_target   = RCC_CFGR_SW_HSI;
            break;

        case CLK_HSE:
            RCC->CR.BITS.HSEON = 1;
            while (RCC->CR.BITS.HSERDY == CLK_NOT_READY)
            {
                if (--timeout == 0U)
                    return RCC_ERROR_TIMEOUT;
            }
            target_freq = PLL_HSE_FREQ;   /* user-configured crystal frequency */
            sw_target   = RCC_CFGR_SW_HSE;
            break;

        case CLK_PLL:
            RCC->CR.BITS.PLLON = 1;
            while (RCC->CR.BITS.PLLRDY == CLK_NOT_READY)
            {
                if (--timeout == 0U)
                    return RCC_ERROR_TIMEOUT;
            }
            {
                /* Issue 2: query actual PLL output to determine correct flash latency */
                RCC_Error_t pll_ret = RCC_GET_PLL_FREQ(&target_freq);
                if (pll_ret != RCC_OK)
                    return pll_ret;
            }
            sw_target = RCC_CFGR_SW_PLL;
            break;

        default:
            return RCC_ERROR_INVALID_CLK_SRC;
    }

    /* Issue 2: set FLASH wait states before raising the clock frequency */
    RCC_ConfigureFlashLatency(target_freq);

    /* Write SW bits to select the new system clock source */
    RCC->CFGR.BITS.SW = sw_target;

    /* Issue 5: poll SWS until the hardware confirms the clock switch */
    sw_timeout = 10000U;
    while (((RCC->CFGR.ALL & RCC_CFGR_SWS_MASK) >> RCC_CFGR_SWS_POS) != sw_target)
    {
        if (--sw_timeout == 0U)
            return RCC_ERROR_TIMEOUT;
    }

    return RCC_OK;
}

RCC_Error_t get_sys_clk(RCC_SYSCLK_TYPE_t* clk_src)
{
    if (clk_src == NULL)
        return RCC_ERROR_INVALID_PARAM;

    uint32_t clk_value = (RCC->CFGR.ALL & RCC_CFGR_SWS_MASK) >> RCC_CFGR_SWS_POS;
    *clk_src = (RCC_SYSCLK_TYPE_t)clk_value;

    if (*clk_src >= CLK_MAX)
        return RCC_ERROR_INVALID_CLK_SRC;

    return RCC_OK;
}

RCC_Error_t RCC_CTL_CLK(RCC_SYSCLK_TYPE_t clk_type, RCC_CLK_CTRL_t state)
{
    RCC_Error_t error_state = RCC_OK;

    if (clk_type >= CLK_MAX || (state != CLK_ENABLE && state != CLK_DISABLE))
    {
        error_state = RCC_ERROR_INVALID_PARAM;
        return error_state;
    }

    uint32_t curret_clk = (RCC->CFGR.ALL & RCC_CFGR_SWS_MASK) >> RCC_CFGR_SWS_POS;
    if (curret_clk == clk_type && state == CLK_DISABLE)
    {
        error_state = RCC_ERROR_CLK_ACTIVE;
        return error_state;
    }

    uint32_t timeout = 10000U;
    switch (clk_type)
    {
        case CLK_HSI:
            if (state == CLK_ENABLE)
            {
                RCC->CR.BITS.HSION = state;
                while (RCC->CR.BITS.HSIRDY == CLK_NOT_READY)
                {
                    if (--timeout == 0U)
                        return RCC_ERROR_TIMEOUT;
                }
            }
            else
            {
                RCC->CR.BITS.HSION = state;
            }
            break;

        case CLK_HSE:
            if (state == CLK_ENABLE)
            {
                RCC->CR.BITS.HSEON = state;
                while (RCC->CR.BITS.HSERDY == CLK_NOT_READY)
                {
                    if (--timeout == 0U)
                        return RCC_ERROR_TIMEOUT;
                }
            }
            else
            {
                RCC->CR.BITS.HSEON = state;
            }
            break;

        case CLK_PLL:
            if (state == CLK_ENABLE)
            {
                RCC->CR.BITS.PLLON = state;
                while (RCC->CR.BITS.PLLRDY == CLK_NOT_READY)
                {
                    if (--timeout == 0U)
                        return RCC_ERROR_TIMEOUT;
                }
            }
            else
            {
                RCC->CR.BITS.PLLON = state;
            }
            break;

        default:
            error_state = RCC_ERROR_INVALID_PARAM;
            return error_state;
    }

    return RCC_OK;
}

RCC_Error_t RCC_PLL_CFG(PLL_CFG_t *PLL_CFG)
{
    if (PLL_CFG == NULL)
        return RCC_ERROR_INVALID_PARAM;

    if (RCC->CR.BITS.PLLON == CLK_ENABLE)
        return RCC_ERROR_CLK_ACTIVE;

    if (PLL_CFG->PLL_M < PLL_M_MIN || PLL_CFG->PLL_M > PLL_M_MAX)
        return RCC_ERROR_INVALID_PARAM;

    if (PLL_CFG->PLL_N < PLL_N_MIN || PLL_CFG->PLL_N > PLL_N_MAX)
        return RCC_ERROR_INVALID_PARAM;

    if (PLL_CFG->PLL_P != PLLP_2 &&
        PLL_CFG->PLL_P != PLLP_4 &&
        PLL_CFG->PLL_P != PLLP_6 &&
        PLL_CFG->PLL_P != PLLP_8)
        return RCC_ERROR_INVALID_PARAM;

    uint32_t PLL_INPUT_FREQ = 0U;

    switch (PLL_CFG->PLL_Source)
    {
        case PLL_SRC_HSI:
            if (RCC->CR.BITS.HSIRDY == CLK_NOT_READY)
                return RCC_ERROR_HSI_FAILED;
            PLL_INPUT_FREQ = PLL_HSI_FREQ;
            break;

        case PLL_SRC_HSE:
            if (RCC->CR.BITS.HSEON == 0)
                return RCC_ERROR_HSE_FAILED;
            if (RCC->CR.BITS.HSERDY == CLK_NOT_READY)
                return RCC_ERROR_HSE_FAILED;
            PLL_INPUT_FREQ = PLL_HSE_FREQ;
            break;

        default:
            return RCC_ERROR_INVALID_PARAM;
    }

    uint32_t VCO_INPUT = PLL_INPUT_FREQ / PLL_CFG->PLL_M;
    if (VCO_INPUT < VCO_INPUT_MIN || VCO_INPUT > VCO_INPUT_MAX)
        return RCC_ERROR_PLL_FAILED;

    uint32_t VCO_OUTPUT = VCO_INPUT * PLL_CFG->PLL_N;
    if (VCO_OUTPUT < VCO_OUTPUT_MIN || VCO_OUTPUT > VCO_OUTPUT_MAX)
        return RCC_ERROR_PLL_FAILED;

    uint32_t PLL_OUTPUT = VCO_OUTPUT / PLL_CFG->PLL_P;
    if (PLL_OUTPUT > PLL_MAX_FREQ)
        return RCC_ERROR_PLL_FAILED;

    /* Issue 4: USB frequency check removed — project does not use USB.
       PLLQ must still be written to a valid value per hardware requirement. */

    /* Reset PLLCFGR to default reset state (per RM0368) */
    RCC->PLLCFGR.ALL = 0x24003010;

    RCC->PLLCFGR.BITS.PLLSRC = (PLL_CFG->PLL_Source == PLL_SRC_HSE) ? 1U : 0U;
    RCC->PLLCFGR.BITS.PLLM   = PLL_CFG->PLL_M;
    RCC->PLLCFGR.BITS.PLLN   = PLL_CFG->PLL_N;

    /* PLLP encoding: actual=2→00, 4→01, 6→10, 8→11 */
    RCC->PLLCFGR.BITS.PLLP = ((PLL_CFG->PLL_P >> 1) - 1U);

    /* Write PLLQ — must be set to a valid value even without USB enforcement */
    RCC->PLLCFGR.BITS.PLLQ = PLL_CFG->PLL_Q;

    return RCC_OK;
}

RCC_Error_t RCC_PLL_Enable(void)
{
    uint32_t timeout = 10000U;  /* Issue 3: added timeout — prevents infinite loop on PLL failure */
    RCC->CR.BITS.PLLON = 1;

    while (RCC->CR.BITS.PLLRDY == 0)
    {
        if (--timeout == 0U)     /* Issue 3: return timeout error instead of hanging */
            return RCC_ERROR_TIMEOUT;
    }

    return RCC_OK;
}

RCC_Error_t RCC_GET_PLL_FREQ(uint32_t* freq)
{
    if (freq == NULL)
        return RCC_ERROR_INVALID_PARAM;

    if (RCC->CR.BITS.PLLON == CLK_DISABLE)
        return RCC_ERROR_PLL_FAILED;

    if (RCC->CR.BITS.PLLRDY == CLK_NOT_READY)
        return RCC_ERROR_PLL_FAILED;

    uint32_t pll_src_freq;
    uint32_t pllm, plln, pllp;
    uint32_t vco_input, vco_output, pll_output;

    if (RCC->PLLCFGR.BITS.PLLSRC == PLL_SRC_HSE)
        pll_src_freq = PLL_HSE_FREQ;
    else
        pll_src_freq = PLL_HSI_FREQ;

    pllm = RCC->PLLCFGR.BITS.PLLM;
    plln = RCC->PLLCFGR.BITS.PLLN;
    /* PLLP encoding: bits→actual: 00→2, 01→4, 10→6, 11→8 */
    pllp = (RCC->PLLCFGR.BITS.PLLP + 1U) * 2U;

    vco_input  = pll_src_freq / pllm;
    vco_output = vco_input * plln;
    pll_output = vco_output / pllp;

    if (pll_output > PLL_MAX_FREQ)
        return RCC_ERROR_PLL_FAILED;

    *freq = pll_output;
    return RCC_OK;
}

RCC_Error_t RCC_EN_CLK_PERIPHERAL(RCC_Peripheral_t peri_clk)
{
    RCC_Error_t error_state = RCC_OK;
    uint32_t bus     = (peri_clk & BUS_MASK) >> 5;  /* Issue 9: removed volatile — pure local computation value */
    uint32_t bit_pos =  peri_clk & BIT_MASK;         /* Issue 9: removed volatile — pure local computation value */

    if (bus > 3)
        return RCC_ERROR_INVALID_PARAM;

    if (bit_pos > 31)
        return RCC_ERROR_INVALID_PARAM;

    switch (bus)
    {
        case RCC_AHB1_BUS:
            RCC->AHB1ENR.ALL |= (1U << bit_pos);
            break;

        case RCC_AHB2_BUS:
            RCC->AHB2ENR.ALL |= (1U << bit_pos);
            break;

        case RCC_APB1_BUS:
            RCC->APB1ENR.ALL |= (1U << bit_pos);
            break;

        case RCC_APB2_BUS:
            RCC->APB2ENR.ALL |= (1U << bit_pos);
            break;

        default:
            error_state = RCC_ERROR_INVALID_PARAM;
            break;
    }

    return error_state;
}

RCC_Error_t RCC_DisablePeripheralClock(RCC_Peripheral_t peri_clk)
{
    uint32_t bus     = (peri_clk & BUS_MASK) >> 5;  /* Issue 10: changed from uint8_t to uint32_t, unified with RCC_EN_CLK_PERIPHERAL */
    uint32_t bit_pos =  peri_clk & BIT_MASK;          /* Issue 10: changed from uint8_t to uint32_t, unified with RCC_EN_CLK_PERIPHERAL */

    if (bus > 3)
        return RCC_ERROR_INVALID_PARAM;

    if (bit_pos > 31)
        return RCC_ERROR_INVALID_PARAM;

    switch (bus)
    {
        case RCC_AHB1_BUS:
            RCC->AHB1ENR.ALL &= ~(1U << bit_pos);
            break;

        case RCC_AHB2_BUS:
            RCC->AHB2ENR.ALL &= ~(1U << bit_pos);
            break;

        case RCC_APB1_BUS:
            RCC->APB1ENR.ALL &= ~(1U << bit_pos);
            break;

        case RCC_APB2_BUS:
            RCC->APB2ENR.ALL &= ~(1U << bit_pos);
            break;

        default:
            return RCC_ERROR_INVALID_PARAM;
    }

    return RCC_OK;
}

static RCC_Error_t GET_SYSCLK_FREQ(uint32_t *freq)
{
    if (freq == NULL)
        return RCC_ERROR_INVALID_PARAM;

    switch (RCC->CFGR.BITS.SWS)
    {
        case CLK_HSI:
            *freq = PLL_HSI_FREQ;
            break;

        case CLK_HSE:
            *freq = PLL_HSE_FREQ;
            break;

        case CLK_PLL:
            return RCC_GET_PLL_FREQ(freq);

        default:
            return RCC_ERROR_INVALID_CLK_SRC;
    }

    return RCC_OK;
}

/* Issue 15: Public wrapper — exposes the static GET_SYSCLK_FREQ to external callers */
RCC_Error_t RCC_GET_SYSCLK_FREQ(uint32_t *freq)
{
    return GET_SYSCLK_FREQ(freq);  /* delegates to the static implementation */
}

RCC_Error_t GET_AHB_FREQ(uint32_t* freq)
{
    if (freq == NULL)
        return RCC_ERROR_INVALID_PARAM;

    uint32_t sysclk;
    RCC_Error_t ret = GET_SYSCLK_FREQ(&sysclk);
    if (ret != RCC_OK)
        return ret;

    uint32_t hpre = (RCC->CFGR.ALL >> 4) & 0xFU;

    *freq = sysclk / AHB_PRESC_TABLE[hpre];
    return RCC_OK;
}

RCC_Error_t GET_PCLK_FREQ(RCC_Peripheral_t peri, uint32_t* freq)
{
    if (freq == NULL)
        return RCC_ERROR_INVALID_PARAM;

    uint32_t hclk;
    RCC_Error_t ret = GET_AHB_FREQ(&hclk);
    if (ret != RCC_OK)
        return ret;

    uint32_t presc;
    uint32_t div;

    /* Issue 8: replaced magic numbers 0x40/0x60 with named constants from RCC_REGS.h */
    if ((peri & 0xE0U) == PERIPH_BUS_APB1_MARKER)
    {
        presc = (RCC->CFGR.ALL >> 10) & 0x7U;   /* PPRE1 */
        div   = APB_PRESC_TABLE[presc];
    }
    else if ((peri & 0xE0U) == PERIPH_BUS_APB2_MARKER)
    {
        presc = (RCC->CFGR.ALL >> 13) & 0x7U;   /* PPRE2 */
        div   = APB_PRESC_TABLE[presc];
    }
    else
    {
        return RCC_ERROR_INVALID_PARAM;
    }

    *freq = hclk / div;
    return RCC_OK;
}

/* Convenience function: brings STM32F401CC to 84 MHz from reset using HSI.
 * Full sequence: flash latency → PLL config → PLL enable → switch sysclk → verify SWS.
 *
 * PLL parameters (HSI = 16 MHz):
 *   M=16 → VCO input  = 16 / 16 = 1 MHz        (within 1–2 MHz range)
 *   N=336 → VCO output = 1 × 336 = 336 MHz      (within 192–432 MHz range)
 *   P=4  → SYSCLK     = 336 / 4 = 84 MHz        (at STM32F401 max)
 *   Q=7  → USB clock  = 336 / 7 = 48 MHz        (valid, not enforced here)
 */
RCC_Error_t RCC_INIT_84MHz_HSI(void)
{
    RCC_Error_t ret;
    PLL_CFG_t pll_cfg;

    pll_cfg.PLL_Source = PLL_SRC_HSI;
    pll_cfg.PLL_M      = PLLM_16;
    pll_cfg.PLL_N      = PLLN_336;
    pll_cfg.PLL_P      = PLLP_4;
    pll_cfg.PLL_Q      = PLLQ_7;

    /* Step 1: Configure PLL */
    ret = RCC_PLL_CFG(&pll_cfg);
    if (ret != RCC_OK)
        return ret;

    /* Step 2: Enable PLL and wait for lock */
    ret = RCC_PLL_Enable();
    if (ret != RCC_OK)
        return ret;

    /* Step 3: Set APB1 prescaler to /2 → PCLK1 = 42 MHz (max for APB1 on F401)
       PPRE1 bits [12:10] in RCC_CFGR = 0b100 = divide by 2
       Must be set BEFORE switching sysclk to avoid momentary overspeed on APB1 */
    RCC->CFGR.ALL = (RCC->CFGR.ALL & ~(0x7U << 10U)) | (0x4U << 10U);

    /* Step 4: APB2 prescaler stays at /1 → PCLK2 = 84 MHz (within APB2 max)
       PPRE2 bits [15:13] = 0b000 = no division — this is reset default, no action needed */

    /* Step 5: Switch SYSCLK to PLL (includes flash latency config + SWS verification) */
    ret = RCC_SET_SYSCLK(CLK_PLL);
    return ret;
}


RCC_Error_t RCC_LSI_Enable(void)
{
    uint32_t timeout = 100000U;

    /* Set LSION bit in RCC_CSR */
    RCC->CSR.ALL |= (1U << 0U);   /* LSION = 1 */

    /* Wait for LSIRDY */
    while ((RCC->CSR.ALL & (1U << 1U)) == 0U)
    {
        if (--timeout == 0U)
        {
            return RCC_ERROR_TIMEOUT;
        }
    }

    return RCC_OK;
}