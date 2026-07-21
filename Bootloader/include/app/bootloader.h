#ifndef BOOTLOADER_H_
#define BOOTLOADER_H_

#include "../../lib/STD_Types.h"
#include "interface/Core/systick.h"

#define BL_HOST_COMMUNICATION_UART       UART1
#define BL_REPLAY_START_BYTE             0xEE
#define BL_UART_DELAY					 1000   // 1000 ms
#define BL_HOST_BUFFER_RX_LENGTH         140

#define BL_GET_VERSION                   0x10
#define BL_GET_PROTECTION_LEVEL          0x11
#define BL_JUMP_TO_ADDR_CMD              0x12
#define BL_FLASH_ERASE_CMD               0x13
#define BL_MEM_WRITE_CMD                 0x14

#define ENTER_BOOTLOADER_CMD             0xAA
#define WE_ARE_IN_BOOTLOADER             0xFB
#define BOOTLOADER_FLAG_ADDR  		 ((volatile uint32_t *)0x2000FFF8)
#define BOOTLOADER_APP_MAGIC  		 0xDEADBEEF
#define BOOT_FLAG_CLEAR              0x00000000

/* App vector table validation */
#define SRAM_BASE       0x20000000UL
#define SRAM_END        0x20010000UL   /* 64 KB */
#define APP_FLASH_BASE  0x08008000UL   /* Sector 2 start */
#define APP_FLASH_END   0x08040000UL   /* Sector 5 end   */

uint8_t App_IsValid(void);

// CRC_VERIFICATION
#define CRC_TYPE_SIZE_BYTE               4

#define CRC_VERIFICATION_FAILED          0x00
#define CRC_VERIFICATION_PASSED          0x01

#define BL_SEND_ACK                      0xAA
#define BL_SEND_NACK                     0x00

#define ADDRESS_IS_INVALID               0x00
#define ADDRESS_IS_VALID                 0x01

#define STM32F401_SRAM1_SIZE             (64 * 1024)
#define STM32F401_FLASH_SIZE             (1024 * 1024)
#define STM32F401_SRAM1_END              (SRAM1_BASE + STM32F401_SRAM1_SIZE)
#define STM32F401_FLASH_END              (FLASH_BASE + STM32F401_FLASH_SIZE)

#define UNSUCCESSFUL_ERASE               0x00
#define SUCCESSFUL_ERASE                 0xE1
#define INVALID_SECTOR_NUMBER            0x02
#define VALID_SECTOR_NUMBER              0x03

// BL_MEM_WRITE_CMD
#define FLASH_PAYLOAD_WRITE_FAILED       0x00
#define FLASH_PAYLOAD_WRITE_PASSED       0xE2

typedef void (*mainAppPtr)(void);
typedef void (*jumpPtr)(void);

STD_ReturnType BL_FetchHostCommand(void);
STD_ReturnType BL_Init(SYSTICK_ClockSource_t clockSource);

#endif /* BOOTLOADER_H_ */
