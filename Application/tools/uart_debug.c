#include "uart_debug.h"

/* USART1 direct register access — no HAL */
#define RCC_APB2ENR   (*((volatile uint32_t *)0x40023844U))
#define RCC_AHB1ENR   (*((volatile uint32_t *)0x40023830U))
#define GPIOA_MODER   (*((volatile uint32_t *)0x40020000U))
#define GPIOA_AFRH    (*((volatile uint32_t *)0x40020024U))
#define USART1_SR     (*((volatile uint32_t *)0x40011000U))
#define USART1_DR     (*((volatile uint32_t *)0x40011004U))
#define USART1_BRR    (*((volatile uint32_t *)0x40011008U))
#define USART1_CR1    (*((volatile uint32_t *)0x4001100CU))

void UART_Debug_Init(void)
{
    /* Enable GPIOA clock */
    RCC_AHB1ENR |= (1U << 0);

    /* Enable USART1 clock (APB2 bit 4) */
    RCC_APB2ENR |= (1U << 4);

    /* PA9 → Alternate Function mode (MODER bits [19:18] = 10) */
    GPIOA_MODER &= ~(3U << 18);
    GPIOA_MODER |=  (2U << 18);

    /* PA9 → AF7 (USART1_TX) in AFRH bits [7:4] */
    GPIOA_AFRH  &= ~(0xFU << 4);
    GPIOA_AFRH  |=  (7U   << 4);

    /* BRR for 115200 baud @ 84 MHz PCLK2 (OVER8=0, 16x oversampling):
       USARTDIV = fPCLK / (16 × BaudRate)
               = 84,000,000 / (16 × 115,200) = 45.572
       mantissa = 45  = 0x2D
       fraction = round(0.572 × 16) = 9
       BRR = (45 << 4) | 9 = 0x2D9 */
    USART1_BRR = 0x2D9U;

    /* Enable USART1, TX enable */
    USART1_CR1 = (1U << 13) | (1U << 3);
}

void UART_Debug_SendChar(char c)
{
    /* Wait until TXE (bit 7) is set */
    while (!(USART1_SR & (1U << 7)));
    USART1_DR = (uint32_t)c;
}

void UART_Debug_SendStr(const char *str)
{
    while (*str != '\0')
    {
        UART_Debug_SendChar(*str);
        str++;
    }
}

void UART_Debug_SendUInt(uint32_t val)
{
    if (val == 0U)
    {
        UART_Debug_SendChar('0');
        return;
    }

    char buf[10];
    uint8_t i = 0U;

    while (val > 0U)
    {
        buf[i] = (char)('0' + (val % 10U));
        i++;
        val /= 10U;
    }

    /* Print digits in reverse */
    while (i > 0U)
    {
        i--;
        UART_Debug_SendChar(buf[i]);
    }
}