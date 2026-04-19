#include "interface/MCAL/dma.h"
#include "interface/MCAL/rcc.h"
#include "private/MCAL/dma_priv.h"
#include "interface/Core/nvic.h"

#define DMA_STREAM  DMA[dmaInstance->dmaNum]->STREAM[dmaInstance->stream]

#define DMA_CR_EN_BIT  0
#define DMA_CR_DMEIE_BIT 1
#define DMA_CR_TEIE_BIT  2
#define DMA_CR_HTIE_BIT  3
#define DMA_CR_TCIE_BIT  4

static STD_ReturnType DMA_NVIC_IRQ_Enable(uint8_t dmaNum, uint8_t streamNum);
static STD_ReturnType DMA_NVIC_IRQ_Disable(uint8_t dmaNum, uint8_t streamNum);
void static DMA_IRQ_Handler(uint8_t dmaNum, uint8_t streamNum, uint8_t bitOffset);

DMA_State_t dmaState[2][8] = {DMA_STATE_READY};

// To keep track of how many instances are using each DMA, for clock Enable/Disable
uint8_t dma1UsedCounter = 0;
uint8_t dma2UsedCounter = 0;

DMACBFunc_t dmaCombCallback[2][8] = {NULL};
DMACBFunc_t dmaHalfCallback[2][8] = {NULL};
DMACBFunc_t dmaErrCallback[2][8] = {NULL};
DMACBFunc_t dmaDirectErrCallback[2][8] = {NULL};

DMA_Registers_t* DMA[2] = {DMA1, DMA2};

STD_ReturnType DMA_Init(DMA_Instance_t* dmaInstance){
    STD_ReturnType ret = STD_SUCCESS;
    if(dmaInstance == NULL || dmaInstance->stream > 7 || dmaInstance->channel > 7){
        ret = STD_ERROR;
    }
    else{
        // 1. Enable Peripheral Clock
        if(dmaInstance->dmaNum == DMA_1 && dma1UsedCounter == 0){
            ret = RCC_ControlPeripheral(RCC_DMA1, RCC_PERIPHERAL_ENABLE);
            dma1UsedCounter++;
        }
        else if(dmaInstance->dmaNum == DMA_2 && dma2UsedCounter == 0){
            ret = RCC_ControlPeripheral(RCC_DMA2, RCC_PERIPHERAL_ENABLE);
            dma2UsedCounter++;
        }
        else{
            // Nothing
        }

        // Clear EN to able to write in CR
        DMA_STREAM.CR &= ~(1 << DMA_CR_EN_BIT);

        // 2. Configure Stream & Channel
        DMA_STREAM.CR |= (dmaInstance->channel << 25);
        
        // 3. Configure Direction
        DMA_STREAM.CR |= (dmaInstance->direction << 6);
        
        // 4. Memory increment
        DMA_STREAM.CR |= (dmaInstance->memInc << 10);

        // 5. Peripheral increment
        DMA_STREAM.CR |= (dmaInstance->periphInc << 9);

        // 6. Memory Burst
        DMA_STREAM.CR |= (dmaInstance->memBurst << 23);

        // 7. Peripheral Burst
        DMA_STREAM.CR |= (dmaInstance->periphBurst << 21);

        // 8. Memory Size
        DMA_STREAM.CR |= (dmaInstance->memSize << 13);

        // 9. Peripheral Size
        DMA_STREAM.CR |= (dmaInstance->periphSize << 11);

        // 10. Priority
        DMA_STREAM.CR |= (dmaInstance->priority << 16);

        // 11. Interrupt Comfigurations
        if(dmaInstance->interruptConf != DMA_IT_DISABLE){
            // Set needed Interrupt Flags
            DMA_STREAM.CR |= (dmaInstance->interruptConf << 1);
            // Enable NVIC
            ret = DMA_NVIC_IRQ_Enable(dmaInstance->dmaNum, dmaInstance->stream);
            // Set Callback
            dmaCombCallback[dmaInstance->dmaNum][dmaInstance->stream] = dmaInstance->combleteCallback;
            dmaHalfCallback[dmaInstance->dmaNum][dmaInstance->stream] = dmaInstance->halfCallback;
            dmaErrCallback[dmaInstance->dmaNum][dmaInstance->stream] = dmaInstance->errorCallback;
            dmaDirectErrCallback[dmaInstance->dmaNum][dmaInstance->stream] = dmaInstance->directErrorCallback;
        }
        else{
            ret = STD_ERROR;
        }
        
    }
    return ret;
}

STD_ReturnType DMA_DeInit(DMA_Instance_t* dmaInstance){
    STD_ReturnType ret = STD_SUCCESS;

    if(dmaInstance == NULL || dmaInstance->stream > 7 || dmaInstance->channel > 7){
        ret = STD_ERROR;
    }
    else{
        DMA_STREAM.CR &= ~(1 << DMA_CR_EN_BIT);
 
        DMA_STREAM.CR = 0U;
        DMA_STREAM.NDTR = 0U;
        DMA_STREAM.PAR = 0U;
        DMA_STREAM.M0AR = 0U;
        DMA_STREAM.M1AR = 0U;
        DMA_STREAM.FCR = 0U;

        // Disable Peripheral Clock
        if(dmaInstance->dmaNum == DMA_1){
            if (dma1UsedCounter > 0) {
                dma1UsedCounter--;
            }
            if(dma1UsedCounter == 0){
                ret = RCC_ControlPeripheral(RCC_DMA1, RCC_PERIPHERAL_DISABLE);
            }
        }
        else if(dmaInstance->dmaNum == DMA_2){
            if (dma2UsedCounter > 0) {
                dma2UsedCounter--;
            }
            if(dma2UsedCounter == 0){
                ret = RCC_ControlPeripheral(RCC_DMA2, RCC_PERIPHERAL_DISABLE);
            }
        }
        else{
            return STD_ERROR;
        }

        // NVIC Disable
        ret = DMA_NVIC_IRQ_Disable(dmaInstance->dmaNum, dmaInstance->stream);

        dmaState[dmaInstance->dmaNum][dmaInstance->stream] = DMA_STATE_READY;
    }

    return ret;
}

STD_ReturnType DMA_Start(DMA_Instance_t* dmaInstance, uint32_t src, uint32_t dest, uint32_t length){
    STD_ReturnType ret = STD_SUCCESS;

    if(dmaInstance == NULL || dmaInstance->stream > 7 || dmaInstance->channel > 7){
        ret = STD_ERROR;
    }
    else{
        DMA_STREAM.CR &= ~(1 << DMA_CR_EN_BIT);

        DMA_STREAM.NDTR = length;
        if(dmaInstance->direction == DMA_MEMORY_TO_PERIPHERAL){
            DMA_STREAM.PAR = dest;
            DMA_STREAM.M0AR = src;
        }
        else{
            DMA_STREAM.PAR = src;
            DMA_STREAM.M0AR = dest;
        }

        DMA_STREAM.CR |= (1 << DMA_CR_EN_BIT);
        dmaState[dmaInstance->dmaNum][dmaInstance->stream] = DMA_STATE_BUSY;
    }

    return ret;
}

STD_ReturnType DMA_Abort(DMA_Instance_t* dmaInstance){
    STD_ReturnType ret = STD_SUCCESS;
    if(dmaInstance == NULL){
        ret = STD_ERROR;
    }
    else{
        if(dmaState[dmaInstance->dmaNum][dmaInstance->stream] != DMA_STATE_BUSY){
            ret = STD_ERROR;
        }
        else{
            dmaState[dmaInstance->dmaNum][dmaInstance->stream] = DMA_STATE_ABORT;
            DMA_STREAM.CR &= ~(1 << DMA_CR_EN_BIT);
        }
    }
    return ret;
}

STD_ReturnType DMA_GetState(DMA_Instance_t* dmaInstance, DMA_State_t* state){
    STD_ReturnType ret = STD_SUCCESS;
    if(dmaInstance == NULL || state == NULL){
        ret = STD_ERROR;
    }
    else{
        *state = dmaState[dmaInstance->dmaNum][dmaInstance->stream];
    }
    return ret;
}

static STD_ReturnType DMA_NVIC_IRQ_Enable(uint8_t dmaNum, uint8_t streamNum){
    STD_ReturnType ret = STD_SUCCESS;

    if(dmaNum == DMA_1){
        switch(streamNum){
            case DMA_STREAM_0:
                ret = NVIC_EnableIRQ(DMA1_Stream0_IRQn);
                break;
            case DMA_STREAM_1:
                ret = NVIC_EnableIRQ(DMA1_Stream1_IRQn);
                break;
            case DMA_STREAM_2:
                ret = NVIC_EnableIRQ(DMA1_Stream2_IRQn);
                break;
            case DMA_STREAM_3:
                ret = NVIC_EnableIRQ(DMA1_Stream3_IRQn);
                break;
            case DMA_STREAM_4:
                ret = NVIC_EnableIRQ(DMA1_Stream4_IRQn);
                break;
            case DMA_STREAM_5:
                ret = NVIC_EnableIRQ(DMA1_Stream5_IRQn);
                break;
            case DMA_STREAM_6:
                ret = NVIC_EnableIRQ(DMA1_Stream6_IRQn);
                break;
            case DMA_STREAM_7:
                ret = NVIC_EnableIRQ(DMA1_Stream7_IRQn);
                break;
        }
    }
    else if(dmaNum == DMA_2){
        switch(streamNum){
            case DMA_STREAM_0:
                ret = NVIC_EnableIRQ(DMA2_Stream0_IRQn);
                break;
            case DMA_STREAM_1:
                ret = NVIC_EnableIRQ(DMA2_Stream1_IRQn);
                break;
            case DMA_STREAM_2:
                ret = NVIC_EnableIRQ(DMA2_Stream2_IRQn);
                break;
            case DMA_STREAM_3:
                ret = NVIC_EnableIRQ(DMA2_Stream3_IRQn);
                break;
            case DMA_STREAM_4:
                ret = NVIC_EnableIRQ(DMA2_Stream4_IRQn);
                break;
            case DMA_STREAM_5:
                ret = NVIC_EnableIRQ(DMA2_Stream5_IRQn);
                break;
            case DMA_STREAM_6:
                ret = NVIC_EnableIRQ(DMA2_Stream6_IRQn);
                break;
            case DMA_STREAM_7:
                ret = NVIC_EnableIRQ(DMA2_Stream7_IRQn);
                break;
        }
    }
    else{
        ret = STD_ERROR;
    }

    return ret;
}

static STD_ReturnType DMA_NVIC_IRQ_Disable(uint8_t dmaNum, uint8_t streamNum){
    STD_ReturnType ret = STD_SUCCESS;

    if(dmaNum == DMA_1){
        switch(streamNum){
            case DMA_STREAM_0:
                ret = NVIC_DisableIRQ(DMA1_Stream0_IRQn);
                break;
            case DMA_STREAM_1:
                ret = NVIC_DisableIRQ(DMA1_Stream1_IRQn);
                break;
            case DMA_STREAM_2:
                ret = NVIC_DisableIRQ(DMA1_Stream2_IRQn);
                break;
            case DMA_STREAM_3:
                ret = NVIC_DisableIRQ(DMA1_Stream3_IRQn);
                break;
            case DMA_STREAM_4:
                ret = NVIC_DisableIRQ(DMA1_Stream4_IRQn);
                break;
            case DMA_STREAM_5:
                ret = NVIC_DisableIRQ(DMA1_Stream5_IRQn);
                break;
            case DMA_STREAM_6:
                ret = NVIC_DisableIRQ(DMA1_Stream6_IRQn);
                break;
            case DMA_STREAM_7:
                ret = NVIC_DisableIRQ(DMA1_Stream7_IRQn);
                break;
        }
    }
    else if(dmaNum == DMA_2){
        switch(streamNum){
            case DMA_STREAM_0:
                ret = NVIC_DisableIRQ(DMA2_Stream0_IRQn);
                break;
            case DMA_STREAM_1:
                ret = NVIC_DisableIRQ(DMA2_Stream1_IRQn);
                break;
            case DMA_STREAM_2:
                ret = NVIC_DisableIRQ(DMA2_Stream2_IRQn);
                break;
            case DMA_STREAM_3:
                ret = NVIC_DisableIRQ(DMA2_Stream3_IRQn);
                break;
            case DMA_STREAM_4:
                ret = NVIC_DisableIRQ(DMA2_Stream4_IRQn);
                break;
            case DMA_STREAM_5:
                ret = NVIC_DisableIRQ(DMA2_Stream5_IRQn);
                break;
            case DMA_STREAM_6:
                ret = NVIC_DisableIRQ(DMA2_Stream6_IRQn);
                break;
            case DMA_STREAM_7:
                ret = NVIC_DisableIRQ(DMA2_Stream7_IRQn);
                break;
        }
    }
    else{
        ret = STD_ERROR;
    }

    return ret;
}

void static DMA_IRQ_Handler(uint8_t dmaNum, uint8_t streamNum, uint8_t bitOffset){
    volatile uint32_t *Flag;
    volatile uint32_t *ClrFlag;
    // Determine which ISR and IFCR to use based on dma & stream number
    if(streamNum <= 3){
        Flag  = &DMA[dmaNum]->LISR;
        ClrFlag = &DMA[dmaNum]->LIFCR;
    }
    else{
        Flag  = &DMA[dmaNum]->HISR;
        ClrFlag = &DMA[dmaNum]->HIFCR;
    }

    if(dmaState[dmaNum][streamNum] == DMA_STATE_ABORT){
        if((DMA[dmaNum]->STREAM[streamNum].CR & (1 << DMA_CR_EN_BIT)) == 0){
            // Clear all interrupt flags
            *ClrFlag |= (1 << (bitOffset + DMA_CR_TCIE_BIT + 1));
            *ClrFlag |= (1 << (bitOffset + DMA_CR_HTIE_BIT + 1));
            *ClrFlag |= (1 << (bitOffset + DMA_CR_TEIE_BIT + 1));
            *ClrFlag |= (1 << (bitOffset + DMA_CR_DMEIE_BIT + 1));
            dmaState[dmaNum][streamNum] = DMA_STATE_READY;
            return;
        }
    }
    // Transmission Complete
    if(((*Flag) & (1 << (bitOffset + DMA_CR_TCIE_BIT + 1))) && (DMA[dmaNum]->STREAM[streamNum].CR & (1 << DMA_CR_TCIE_BIT))){
        *ClrFlag |= (1 << (bitOffset + DMA_CR_TCIE_BIT + 1));
        dmaState[dmaNum][streamNum] = DMA_STATE_READY;
        if(dmaCombCallback[dmaNum][streamNum] != NULL){
            dmaCombCallback[dmaNum][streamNum]();
        }
    }
    // Half Transmission Complete
    if(((*Flag) & (1 << (bitOffset + DMA_CR_HTIE_BIT + 1))) && (DMA[dmaNum]->STREAM[streamNum].CR & (1 << DMA_CR_HTIE_BIT))){
        *ClrFlag |= (1 << (bitOffset + DMA_CR_HTIE_BIT + 1));
        if(dmaHalfCallback[dmaNum][streamNum] != NULL){
            dmaHalfCallback[dmaNum][streamNum]();
        }
    }
    // Transmission Error
    if(((*Flag) & (1 << (bitOffset + DMA_CR_TEIE_BIT + 1))) && (DMA[dmaNum]->STREAM[streamNum].CR & (1 << DMA_CR_TEIE_BIT))){
        *ClrFlag |= (1 << (bitOffset + DMA_CR_TEIE_BIT + 1));
        if(dmaErrCallback[dmaNum][streamNum] != NULL){
            dmaErrCallback[dmaNum][streamNum]();
        }
    }
    // Direct Mode Error
    if(((*Flag) & (1 << (bitOffset + DMA_CR_DMEIE_BIT + 1))) && (DMA[dmaNum]->STREAM[streamNum].CR & (1 << DMA_CR_DMEIE_BIT))){
        *ClrFlag |= (1 << (bitOffset + DMA_CR_DMEIE_BIT + 1));
        if(dmaDirectErrCallback[dmaNum][streamNum] != NULL){
            dmaDirectErrCallback[dmaNum][streamNum]();
        }
    }
}

// DMA Interrupt Handlers
void DMA1_Stream0_IRQHandler(void){
    DMA_IRQ_Handler(DMA_1, DMA_STREAM_0, 0);
}

void DMA1_Stream1_IRQHandler(void){
    DMA_IRQ_Handler(DMA_1, DMA_STREAM_1, 6);
}

void DMA1_Stream2_IRQHandler(void){
    DMA_IRQ_Handler(DMA_1, DMA_STREAM_2, 16);
}

void DMA1_Stream3_IRQHandler(void){
    DMA_IRQ_Handler(DMA_1, DMA_STREAM_3, 22);
}

void DMA1_Stream4_IRQHandler(void){
    DMA_IRQ_Handler(DMA_1, DMA_STREAM_4, 0);
}

void DMA1_Stream5_IRQHandler(void){
    DMA_IRQ_Handler(DMA_1, DMA_STREAM_5, 6);
}

void DMA1_Stream6_IRQHandler(void){
    DMA_IRQ_Handler(DMA_1, DMA_STREAM_6, 16);
}

void DMA1_Stream7_IRQHandler(void){
    DMA_IRQ_Handler(DMA_1, DMA_STREAM_7, 22);
}


void DMA2_Stream0_IRQHandler(void){
    DMA_IRQ_Handler(DMA_2, DMA_STREAM_0, 0);
}

void DMA2_Stream1_IRQHandler(void){
    DMA_IRQ_Handler(DMA_2, DMA_STREAM_1, 6);
}

void DMA2_Stream2_IRQHandler(void){
    DMA_IRQ_Handler(DMA_2, DMA_STREAM_2, 16);
}

void DMA2_Stream3_IRQHandler(void){
    DMA_IRQ_Handler(DMA_2, DMA_STREAM_3, 22);
}

void DMA2_Stream4_IRQHandler(void){
    DMA_IRQ_Handler(DMA_2, DMA_STREAM_4, 0);
}

void DMA2_Stream5_IRQHandler(void){
    DMA_IRQ_Handler(DMA_2, DMA_STREAM_5, 6);
}

void DMA2_Stream6_IRQHandler(void){
    DMA_IRQ_Handler(DMA_2, DMA_STREAM_6, 16);
}

void DMA2_Stream7_IRQHandler(void){
    DMA_IRQ_Handler(DMA_2, DMA_STREAM_7, 22);
}