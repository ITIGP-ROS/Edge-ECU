
//    Includes
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "app/bootloader.h"
#include "interface/HAL/hserial.h"
#include "interface/MCAL/gpio.h"
#include "interface/MCAL/flash.h"
#include "private/MCAL/flash_priv.h"
#include "interface/MCAL/rcc.h"
#include "interface/MCAL/crc.h"

//    Static Functions Decelerations
static STD_ReturnType Bootloader_GetChipID();
static STD_ReturnType Bootloader_ReadProtectionLevel();
static STD_ReturnType Bootloader_JumpToApp();
static STD_ReturnType Bootloader_EraseFlash();
static STD_ReturnType Bootloader_MemoryWrite();

// static uint8_t HostAddressVerification(uint32_t Jump_Address);
static uint8_t Bootloader_CRCVerify(uint8_t *data, uint8_t dataLen, uint32_t recievedCRC);
static void Bootloader_SendACK(void);
static void Bootloader_SendNACK(void);
static void Bootloader_SendDataToHost(uint8_t *Host_Buffer, uint32_t Data_Len);

//    Global Variables Definitions
static uint8_t appExists = 0;

UART_Config_t uart_config = {
    .UartInstance = BL_HOST_COMMUNICATION_UART,
    .BaudRate = UART_BAUDRATE_115200,
    .DataBits = UART_DATABITS_8,
    .Parity = UART_PARITY_NONE,
    .port = GPIO_PORTA,
    .txPin = GPIO_PIN_9
};

uint8_t txBuffer[3];
uint8_t rxBuffer[BL_HOST_BUFFER_RX_LENGTH]; 

HSerial_Buffer_t hserial_txBuffer = {
    .buffer.data = txBuffer,
    .buffer.index = 0,
    .buffer.length = 0
};

HSerial_Buffer_t hserial_rxBuffer = {
    .buffer.data = rxBuffer,
    .buffer.index = 0,
    .buffer.length = 0
};

HSerial_Config_t hserialConfig = {
    .uartConfig = &uart_config,
    .txBuffer = &hserial_txBuffer,
    .rxBuffer = &hserial_rxBuffer
};

//    Software Interfaces Implementations
STD_ReturnType BL_Init(SYSTICK_ClockSource_t clockSource){
    STD_ReturnType ret = STD_SUCCESS;
    ret = HSerial_Init(&hserialConfig, clockSource);
    ret = CRC_Init();
    return ret;
}

STD_ReturnType BL_FetchHostCommand(void){
	STD_ReturnType status = STD_SUCCESS;
	uint8_t Data_Length = 0;

    hserial_rxBuffer.buffer.data = rxBuffer;
	memset(rxBuffer, 0, BL_HOST_BUFFER_RX_LENGTH);

	// Read the length of the command packet received from the HOST
    hserialConfig.rxBuffer->buffer.length = 1;
	status = HSerial_ReceiveBuffer(&hserialConfig, BL_UART_DELAY);
    if(rxBuffer[0] == 0){
        return STD_ERROR;
    }

	if(status == STD_SUCCESS){
		if(ENTER_BOOTLOADER_CMD == rxBuffer[0]){
			uint8_t data = WE_ARE_IN_BOOTLOADER;
			Bootloader_SendDataToHost(&data, 1);
            return status;
		}
		Data_Length = rxBuffer[0];
		// Read the command packet received from the HOST
        hserial_rxBuffer.buffer.data = rxBuffer + 1;
		hserialConfig.rxBuffer->buffer.length = Data_Length;
		status = HSerial_ReceiveBuffer(&hserialConfig, BL_UART_DELAY*1000);

		if(status != STD_SUCCESS){
			status = STD_NACK;
		}
		else{
			switch(rxBuffer[1]){
				case BL_GET_CHIP_ID:
					status = Bootloader_GetChipID();
					status = STD_CMD_FINISHED;
					break;
				case BL_GET_PROTECTION_LEVEL:
					status = Bootloader_ReadProtectionLevel();
					status = STD_CMD_FINISHED;
					break;
				case BL_JUMP_TO_ADDR_CMD:
					Bootloader_JumpToApp();
					status = STD_CMD_FINISHED;
					break;
				case BL_FLASH_ERASE_CMD:
					status = Bootloader_EraseFlash();
					status = STD_CMD_FINISHED;
					break;
				case BL_MEM_WRITE_CMD:
					Bootloader_MemoryWrite();
					status = STD_CMD_FINISHED;
					break;
			}
		}
	}
    else {}

	return status;
}

static STD_ReturnType Bootloader_GetChipID(){
	STD_ReturnType status = STD_SUCCESS;
	uint16_t packetLen = rxBuffer[0] + 1;
	uint32_t receivedCRC = 0;
	uint16_t MCU_Identification_Number = 0;
	// Extract the CRC32 and packet length sent by the HOST
	receivedCRC = *((uint32_t *)((rxBuffer + packetLen) - CRC_TYPE_SIZE_BYTE));
	// CRC Verification
	if(CRC_VERIFICATION_PASSED == Bootloader_CRCVerify((uint8_t *)&rxBuffer[0] , packetLen - 4, receivedCRC)){
		Bootloader_SendACK();
		status = STD_ACK;
		// Get the MCU chip identification number
		MCU_Identification_Number = (uint16_t)(*(uint16_t*)0xE0042000 & 0xFFF); // DBGMCU_IDCODE => 0x423 for STM32F401xB/C
		// Report chip identification number to HOST
		Bootloader_SendDataToHost((uint8_t *)&MCU_Identification_Number, 2);
		status = STD_CMD_FINISHED;
	}
	else{
		Bootloader_SendNACK();
		status = STD_NACK;
	}
	return status;
}


static STD_ReturnType Bootloader_ReadProtectionLevel(){
	STD_ReturnType status = STD_SUCCESS;
	uint16_t packetLength = 0;
	uint32_t receivedCRC = 0;
	uint8_t RDP_Level = 0;

	// Extract the CRC32 and packet length sent by the HOST
	packetLength = rxBuffer[0] + 1;
	receivedCRC = *((uint32_t *)((rxBuffer + packetLength) - CRC_TYPE_SIZE_BYTE));
	// CRC Verification
	if(CRC_VERIFICATION_PASSED == Bootloader_CRCVerify((uint8_t *)&rxBuffer[0] , packetLength - 4, receivedCRC)){
		Bootloader_SendACK();
		status = STD_ACK;
		// Read Protection Level
		RDP_Level = (uint8_t)((FLASH->OPTCR >> 8) & 0xFF);
		if(0xAA == RDP_Level)
            RDP_Level = 0x00;
		else if(0x55 == RDP_Level)
            RDP_Level = 0x01;
		// Report Valid Protection Level
		Bootloader_SendDataToHost((uint8_t *)&RDP_Level, 1);
		status = STD_CMD_FINISHED;
	}
	else{
		Bootloader_SendNACK();
		status = STD_NACK;
	}
	return status;
}

static STD_ReturnType Bootloader_JumpToApp(){
	STD_ReturnType status = STD_SUCCESS;
	uint16_t packetLength = 0;
	uint32_t receivedCRC = 0;

	// Extract the CRC32 and packet length sent by the HOST
	packetLength = rxBuffer[0] + 1;
	receivedCRC = *((uint32_t *)((rxBuffer + packetLength) - CRC_TYPE_SIZE_BYTE));
	// CRC Verification
	if(CRC_VERIFICATION_PASSED == Bootloader_CRCVerify((uint8_t *)&rxBuffer[0] , packetLength - 4, receivedCRC)){
		Bootloader_SendACK();
		status = STD_ACK;
		uint32_t flashData = 0;
		status = FLASH_Read(FLASH_SECTOR_2, &flashData, 1);
		if(status == STD_SUCCESS && 0xFFFFFFFF != flashData){
			flashData = BOOTLOADER_APP_MAGIC;
			status = FLASH_Write(FLASH_SECTOR_1, &flashData, 1);
			if(status != STD_SUCCESS){
				return status;
			}
			appExists = 0xA0;
			Bootloader_SendDataToHost((uint8_t *)&appExists, 1);
			status = STD_CMD_FINISHED;

			// Software Reset
			*((volatile uint32_t *)0xE000ED0C) = (0x5FA << 16) | (1 << 2);
		}
		else{
			appExists = 0;
			Bootloader_SendDataToHost((uint8_t *)&appExists, 1);
			status = STD_CMD_FINISHED;
		}
	}
	else{
		Bootloader_SendNACK();
		status = STD_NACK;
	}
	return status;
}

static STD_ReturnType Bootloader_EraseFlash(){
	STD_ReturnType status = STD_SUCCESS;
	uint16_t packetLength = 0;
    uint32_t receivedCRC = 0;
	uint8_t eraseStatus = 0;

	// Extract the CRC32 and packet length sent by the HOST
	packetLength = rxBuffer[0] + 1;
	receivedCRC = *((uint32_t *)((rxBuffer + packetLength) - CRC_TYPE_SIZE_BYTE));
	// CRC Verification
	if(CRC_VERIFICATION_PASSED == Bootloader_CRCVerify((uint8_t *)&rxBuffer[0] , packetLength - 4, receivedCRC)){
		Bootloader_SendACK();
		status = STD_ACK;
		// Perform Mass erase or sector erase of the user flash
		status = FLASH_Erase(FLASH_SECTOR_NUMBER_1);
		status = FLASH_Erase(FLASH_SECTOR_NUMBER_2);
		status = FLASH_Erase(FLASH_SECTOR_NUMBER_3);
		if(STD_SUCCESS == status){
			// Report erase Passed
			eraseStatus = SUCCESSFUL_ERASE;
			Bootloader_SendDataToHost((uint8_t *)&eraseStatus, 1);
			status = STD_CMD_FINISHED;
		}
		else{
			// Report erase failed
			eraseStatus = UNSUCCESSFUL_ERASE;
			Bootloader_SendDataToHost((uint8_t *)&eraseStatus, 1);
			status = STD_ERROR;
		}
	}
	else{
		// Send Not acknowledge to the HOST
		Bootloader_SendNACK();
		status = STD_NACK;
	}
	return status;
}

static STD_ReturnType Bootloader_MemoryWrite(){
	STD_ReturnType status = STD_SUCCESS;
	uint16_t packetLength = 0;
    uint32_t receivedCRC = 0;
	uint32_t flashAddress = 0;
	uint8_t Payload_Len = 0;
	uint8_t Flash_Payload_Write_Status = FLASH_PAYLOAD_WRITE_FAILED;

	// Extract the CRC32 and packet length sent by the HOST
	packetLength = rxBuffer[0] + 1;
	receivedCRC = *((uint32_t *)((rxBuffer + packetLength) - CRC_TYPE_SIZE_BYTE));
	// CRC Verification
	if(CRC_VERIFICATION_PASSED == Bootloader_CRCVerify((uint8_t *)&rxBuffer[0] , packetLength - 4, receivedCRC)){
		Bootloader_SendACK();
		status = STD_ACK;
		// Extract the start address from the Host packet
		flashAddress = *((uint32_t *)(&rxBuffer[2]));
		// Extract the payload length from the Host packet
		Payload_Len = rxBuffer[6];
		// Write the payload to the Flash memory => check if its last packet - padd the remaining bytes with 0x00 if its not 4 bytes aligned
		if(Payload_Len % 4 != 0){
			uint8_t paddingBytes = 4 - (Payload_Len % 4);
			memset(&rxBuffer[7 + Payload_Len], 0, paddingBytes);
			Payload_Len += paddingBytes;
		}
		status = FLASH_Write(flashAddress, (uint32_t *)&rxBuffer[7], Payload_Len / 4);
		if(STD_SUCCESS == status){
			// Report payload write passed
			Flash_Payload_Write_Status = FLASH_PAYLOAD_WRITE_PASSED;
			Bootloader_SendDataToHost((uint8_t *)&Flash_Payload_Write_Status, 1);
			status = STD_CMD_FINISHED;
		}
		else{
			// Report payload write failed
			Bootloader_SendDataToHost((uint8_t *)&Flash_Payload_Write_Status, 1);
			status = STD_ERROR;
		}
	}
	else{
		// Send Not acknowledge to the HOST
		Bootloader_SendNACK();
		status = STD_NACK;
	}
	return status;
}


static uint8_t Bootloader_CRCVerify(uint8_t *data, uint8_t dataLen, uint32_t receivedCRC){
	uint8_t CRC_Status = CRC_VERIFICATION_FAILED;
	uint32_t calculatedCRC = 0;
	// Calculate CRC32
	for(uint8_t i = 0; i < dataLen; i++){
        uint32_t crcBuf = (uint32_t)data[i];
        CRC_Accumulate(&crcBuf, 1, &calculatedCRC);
    }
    CRC_Reset();

	// Compare the Host CRC and Calculated CRC
	if(calculatedCRC == receivedCRC){
		CRC_Status = CRC_VERIFICATION_PASSED;
	}
	else{
		CRC_Status = CRC_VERIFICATION_FAILED;
	}

	return CRC_Status;
}

static void Bootloader_SendACK(void){
    txBuffer[0] = BL_SEND_ACK;
    hserialConfig.txBuffer->buffer.length = 1;
	HSerial_SendBuffer(&hserialConfig, BL_UART_DELAY);
}

static void Bootloader_SendNACK(void){
    txBuffer[0] = BL_SEND_NACK;
    hserialConfig.txBuffer->buffer.length = 1;
	HSerial_SendBuffer(&hserialConfig, BL_UART_DELAY);
}

static void Bootloader_SendDataToHost(uint8_t *Host_Buffer, uint32_t Data_Len){
    // Send start byte first
    txBuffer[0] = BL_REPLAY_START_BYTE;
    memcpy(&txBuffer[1], Host_Buffer, Data_Len);
    hserialConfig.txBuffer->buffer.length = Data_Len + 1;
    HSerial_SendBuffer(&hserialConfig, BL_UART_DELAY);
}

