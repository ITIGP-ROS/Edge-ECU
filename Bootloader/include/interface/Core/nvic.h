#ifndef NVIC_H
#define NVIC_H      

#include "../../../lib/STD_Types.h"

#define NVIC_IRQ_Disabled			0
#define NVIC_IRQ_Enabled			1

#define NVIC_IRQ_NOT_PENDING		0
#define NVIC_IRQ_PENDING			1

#define NVIC_IRQ_NOT_ACTIVE			0
#define NVIC_IRQ_ACTIVE				1

#define NVIC_PRIORITY_BITS			4U			/* high 4bits of byte 				*/

typedef enum
{
	WWDG_IRQn 				= 0,		 /* Window WatchDog                             */
	PVD_IRQn				= 1,         /* PVD through EXTI Line detection             */
	TAMP_STAMP_IRQn 		= 2,         /* Tamper and TimeStamps through the EXTI line */
	RTC_WKUP_IRQn			= 3,         /* RTC Wakeup through the EXTI line            */
	FLASH_IRQn 				= 4,         /* FLASH                                       */
	RCC_IRQn				= 5,         /* RCC                                         */
	EXTI0_IRQn 				= 6,         /* EXTI Line0                                  */
	EXTI1_IRQn				= 7,         /* EXTI Line1                                  */
	EXTI2_IRQn 				= 8,         /* EXTI Line2                                  */
	EXTI3_IRQn				= 9,         /* EXTI Line3                                  */
	EXTI4_IRQn 				= 10,        /* EXTI Line4                                  */
	DMA1_Stream0_IRQn		= 11,        /* DMA1 Stream 0                               */
	DMA1_Stream1_IRQn		= 12,        /* DMA1 Stream 1                               */
	DMA1_Stream2_IRQn		= 13,        /* DMA1 Stream 2                               */
	DMA1_Stream3_IRQn		= 14,        /* DMA1 Stream 3                               */
	DMA1_Stream4_IRQn		= 15,        /* DMA1 Stream 4                               */
	DMA1_Stream5_IRQn		= 16,        /* DMA1 Stream 5                               */
	DMA1_Stream6_IRQn		= 17,        /* DMA1 Stream 6                               */
	ADC_IRQn				= 18,        /* ADC1, ADC2 and ADC3s                        */
	EXTI9_5_IRQn			= 23,        /* External Line[9:5]s                         */
	TIM1_BRK_TIM9_IRQn		= 24,        /* TIM1 Break and TIM9                         */
	TIM1_UP_TIM10_IRQn		= 25,        /* TIM1 Update and TIM10                       */
	TIM1_TRG_COM_TIM11_IRQn	= 26,        /* TIM1 Trigger and Commutation and TIM11      */
	TIM1_CC_IRQn			= 27,        /* TIM1 Capture Compare                        */
	TIM2_IRQn				= 28,        /* TIM2                                        */
	TIM3_IRQn				= 29,        /* TIM3                                        */
	TIM4_IRQn				= 30,        /* TIM4                                        */
	I2C1_EV_IRQn			= 31,        /* I2C1 Event                                  */
	I2C1_ER_IRQn			= 32,        /* I2C1 Error                                  */
	I2C2_EV_IRQn			= 33,        /* I2C2 Event                                  */
	I2C2_ER_IRQn			= 34,        /* I2C2 Error                                  */
	SPI1_IRQn				= 35,        /* SPI1                                        */
	SPI2_IRQn				= 36,        /* SPI2                                        */
	USART1_IRQn				= 37,        /* USART1                                      */
	USART2_IRQn				= 38,        /* USART2                                      */
	EXTI15_10_IRQn			= 40,        /* External Line[15:10]s                       */
	RTC_Alarm_IRQn			= 41,        /* RTC Alarm (A and B) through EXTI Line       */
	OTG_FS_WKUP_IRQn		= 42,        /* USB OTG FS Wakeup through EXTI line         */
	DMA1_Stream7_IRQn		= 47,        /* DMA1 Stream7                                */
	SDIO_IRQn				= 49,        /* SDIO                                        */
	TIM5_IRQn				= 50,        /* TIM5                                        */
	SPI3_IRQn				= 51,        /* SPI3                                        */
	DMA2_Stream0_IRQn		= 56,        /* DMA2 Stream 0                               */
	DMA2_Stream1_IRQn		= 57,        /* DMA2 Stream 1                               */
	DMA2_Stream2_IRQn		= 58,        /* DMA2 Stream 2                               */
	DMA2_Stream3_IRQn		= 59,        /* DMA2 Stream 3                               */
	DMA2_Stream4_IRQn		= 60,        /* DMA2 Stream 4                               */
	OTG_FS_IRQn				= 67,        /* USB OTG FS                                  */
	DMA2_Stream5_IRQn		= 68,        /* DMA2 Stream 5                               */
	DMA2_Stream6_IRQn		= 69,        /* DMA2 Stream 6                               */
	DMA2_Stream7_IRQn		= 70,        /* DMA2 Stream 7                               */
	USART6_IRQn				= 71,        /* USART6                                      */
	I2C3_EV_IRQn		    = 72,        /* I2C3 event                                  */
	I2C3_ER_IRQn			= 73,        /* I2C3 error                                  */
	FPU_IRQn				= 81,        /* FPU                                         */
	SPI4_IRQn				= 84,        /* SPI4                                        */

} NVIC_IRQ_Type;

/**
 * @brief Enables a specific external interrupt (IRQ) line in the NVIC.
 * @param IRQn The Interrupt Request Number (0-84) defined in the NVIC_IRQ_Type enum.
 * @return STD_ReturnType
 * - STD_SUCCESS: Interrupt successfully enabled.
 * - STD_ERROR: Invalid IRQn provided.
 */
STD_ReturnType NVIC_EnableIRQ(NVIC_IRQ_Type IRQn);

/**
 * @brief Disables a specific external interrupt (IRQ) line in the NVIC.
 * @param IRQn The Interrupt Request Number (0-84) defined in the NVIC_IRQ_Type enum.
 * @return STD_ReturnType
 * - STD_SUCCESS: Interrupt successfully disabled.
 * - STD_ERROR: Invalid IRQn provided.
 */
STD_ReturnType NVIC_DisableIRQ(NVIC_IRQ_Type IRQn);

/**
 * @brief Reads the enable status of a specific external interrupt (IRQ) line.
 * @param IRQn The Interrupt Request Number (0-84) defined in the NVIC_IRQ_Type enum.
 * @param status Pointer to a uint8_t variable to store the status (0=Disabled, 1=Enabled).
 * @return STD_ReturnType
 * - STD_SUCCESS: Status retrieved successfully.
 * - STD_ERROR: Invalid IRQn or NULL status pointer provided.
 */
STD_ReturnType NVIC_GetEnableIRQ(NVIC_IRQ_Type IRQn, uint8_t* status);

/**
 * @brief Sets the pending status of a specific external interrupt (IRQ) line.
 * @param IRQn The Interrupt Request Number (0-84) defined in the NVIC_IRQ_Type enum.
 * @return STD_ReturnType
 * - STD_SUCCESS: Pending status set successfully.
 * - STD_ERROR: Invalid IRQn provided.
 */
STD_ReturnType NVIC_SetPendingIRQ(NVIC_IRQ_Type IRQn);

/**
 * @brief Clears the pending status of a specific external interrupt (IRQ) line.
 * @param IRQn The Interrupt Request Number (0-84) defined in the NVIC_IRQ_Type enum.
 * @return STD_ReturnType
 * - STD_SUCCESS: Pending status cleared successfully.
 * - STD_ERROR: Invalid IRQn provided.
 */
STD_ReturnType NVIC_ClearPendingIRQ(NVIC_IRQ_Type IRQn);

/**
 * @brief Reads the pending status of a specific external interrupt (IRQ) line.
 * @param IRQn The Interrupt Request Number (0-84) defined in the NVIC_IRQ_Type enum.
 * @param status Pointer to a uint8_t variable to store the status (0=Not Pending, 1=Pending).
 * @return STD_ReturnType
 * - STD_SUCCESS: Status retrieved successfully.
 * - STD_ERROR: Invalid IRQn or NULL status pointer provided.
 */
STD_ReturnType NVIC_GetPendingIRQ(NVIC_IRQ_Type IRQn, uint8_t* status);

/**
 * @brief Reads the active status of a specific external interrupt (IRQ) line.
 * @param IRQn The Interrupt Request Number (0-84) defined in the NVIC_IRQ_Type enum.
 * @param status Pointer to a uint8_t variable to store the status (0=Not Active, 1=Active).
 * @return STD_ReturnType
 * - STD_SUCCESS: Status retrieved successfully.
 * - STD_ERROR: Invalid IRQn or NULL status pointer provided.
 */
STD_ReturnType NVIC_GetActiveIRQ(NVIC_IRQ_Type IRQn, uint8_t* status);

/**
 * @brief Sets the priority for a specific external interrupt (IRQ) line.
 * @param IRQn The Interrupt Request Number (0-84) defined in the NVIC_IRQ_Type enum.
 * @param priority The 4-bit priority value (0-15).
 * @return STD_ReturnType
 * - STD_SUCCESS: Priority set successfully.
 * - STD_ERROR: Invalid IRQn provided.
 */
STD_ReturnType NVIC_SetPriorityIRQ(NVIC_IRQ_Type IRQn, uint8_t priority);

/**
 * @brief Retrieves the priority configured for a specific external interrupt (IRQ) line.
 * @param IRQn The Interrupt Request Number (0-84) defined in the NVIC_IRQ_Type enum.
 * @param priority Pointer to a uint8_t variable to store the 4-bit priority value (0-15).
 * @return STD_ReturnType
 * - STD_SUCCESS: Priority retrieved successfully.
 * - STD_ERROR: Invalid IRQn or NULL priority pointer provided.
 */
STD_ReturnType NVIC_GetPriorityIRQ(NVIC_IRQ_Type IRQn, uint8_t* priority);

/**
 * @brief Triggers an interrupt using the Software Trigger Interrupt Register (STIR).
 *
 * @param IRQn The Interrupt Request Number (0-84) to be triggered.
 * @return STD_ReturnType
 * - STD_SUCCESS: Interrupt successfully software-triggered.
 * - STD_ERROR: Invalid IRQn provided.
 */
STD_ReturnType NVIC_SetSoftwareTrigger(NVIC_IRQ_Type IRQn);

#endif  // NVIC_H