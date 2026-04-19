#ifndef NVIC_PRIV_H
#define NVIC_PRIV_H

#include "../../../lib/STD_Types.h"

#define NVIC_BASE_ADDRESS		0xE000E100UL	
#define NVIC					((NVIC_Type*)NVIC_BASE_ADDRESS)

typedef struct
{
	volatile uint32_t ISER[8U];	// Offset = 0x000 - RW - Interrupt Set-enable Registers
	uint32_t Reserved0[24U];
	volatile uint32_t ICER[8U];	// Offset = 0x080 - RW - Interrupt Clear-enable Registers
	uint32_t Reserved1[24U];
	volatile uint32_t ISPR[8U];	// Offset = 0x100 - RW - Interrupt Set-pending Registers
	uint32_t Reserved2[24U];
	volatile uint32_t ICPR[8U];	// Offset = 0x180 - RW - Interrupt Clear-pending Registers
	uint32_t Reserved3[24U];
	volatile uint32_t IABR[8U];	// Offset = 0x200 - RW - Interrupt Active Bit Registers
	uint32_t Reserved4[56U];
	volatile uint8_t IPR[240U];	// Offset = 0x300 - RW - Interrupt Priority Registers
	uint32_t Reserved5[696U];
	volatile uint32_t STIR;		// Offset = 0xE00 - WO - Software Trigger Interrupt Register
} NVIC_Type;


#endif  // NVIC_PRIV_H