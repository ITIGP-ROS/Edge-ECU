
#include "private/Core/nvic_priv.h"
#include "configuration/Core/nvic_conf.h"
#include "interface/Core/nvic.h"

STD_ReturnType NVIC_EnableIRQ(NVIC_IRQ_Type IRQn){
	STD_ReturnType ret = STD_SUCCESS;

	if(NVIC_IRQ_NUMS_START <= (uint32_t)IRQn && NVIC_IRQ_NUMS_END >= (uint32_t)IRQn){
		NVIC->ISER[((uint32_t)IRQn / 32)] = (1 << ((uint32_t)IRQn % 32));
	}
	else{
		ret = STD_ERROR;
	}
	return ret;
}

STD_ReturnType NVIC_DisableIRQ(NVIC_IRQ_Type IRQn){
	STD_ReturnType ret = STD_SUCCESS;

	if(NVIC_IRQ_NUMS_START <= (uint32_t)IRQn && NVIC_IRQ_NUMS_END >= (uint32_t)IRQn){
		NVIC->ICER[((uint32_t)IRQn / 32)] = (1 << ((uint32_t)IRQn % 32));
	}
	else{
		ret = STD_ERROR;
	}
	return ret;
}

STD_ReturnType NVIC_GetEnableIRQ(NVIC_IRQ_Type IRQn, uint8_t* status){
	STD_ReturnType ret = STD_SUCCESS;

	if((NVIC_IRQ_NUMS_START <= (uint32_t)IRQn && NVIC_IRQ_NUMS_END >= (uint32_t)IRQn) && status != NULL){
		*status = (NVIC->ICER[((uint32_t)IRQn / 32)] >> ((uint32_t)IRQn % 32)) &  1;
	}
	else{
		ret = STD_ERROR;
	}
	return ret;
}

STD_ReturnType NVIC_SetPendingIRQ(NVIC_IRQ_Type IRQn){
	STD_ReturnType ret = STD_SUCCESS;

	if(NVIC_IRQ_NUMS_START <= (uint32_t)IRQn && NVIC_IRQ_NUMS_END >= (uint32_t)IRQn){
		NVIC->ISPR[((uint32_t)IRQn / 32)] = (1 << ((uint32_t)IRQn % 32));
	}
	else{
		ret = STD_ERROR;
	}
	return ret;
}

STD_ReturnType NVIC_ClearPendingIRQ(NVIC_IRQ_Type IRQn){
	STD_ReturnType ret = STD_SUCCESS;

	if(NVIC_IRQ_NUMS_START <= (uint32_t)IRQn && NVIC_IRQ_NUMS_END >= (uint32_t)IRQn){
		NVIC->ICPR[((uint32_t)IRQn / 32)] = (1 << ((uint32_t)IRQn % 32));
	}
	else{
		ret = STD_ERROR;
	}
	return ret;
}

STD_ReturnType NVIC_GetPendingIRQ(NVIC_IRQ_Type IRQn, uint8_t* status){
	STD_ReturnType ret = STD_SUCCESS;

	if((NVIC_IRQ_NUMS_START <= (uint32_t)IRQn && NVIC_IRQ_NUMS_END >= (uint32_t)IRQn) && status != NULL){
		*status = (NVIC->ICPR[((uint32_t)IRQn / 32)] >> ((uint32_t)IRQn % 32)) & 1;
	}
	else{
		ret = STD_ERROR;
	}

	return ret;
}

STD_ReturnType NVIC_GetActiveIRQ(NVIC_IRQ_Type IRQn, uint8_t* status){
	STD_ReturnType ret = STD_SUCCESS;

	if(NVIC_IRQ_NUMS_START <= (uint32_t)IRQn && NVIC_IRQ_NUMS_END >= (uint32_t)IRQn){
		*status = (NVIC->IABR[((uint32_t)IRQn / 32)] >> ((uint32_t)IRQn % 32)) & 1;
	}
	else{
		ret = STD_ERROR;
	}

	return ret;
}

STD_ReturnType NVIC_SetPriorityIRQ(NVIC_IRQ_Type IRQn, uint8_t priority){
	STD_ReturnType ret = STD_SUCCESS;

	if(NVIC_IRQ_NUMS_START <= (uint32_t)IRQn && NVIC_IRQ_NUMS_END >= (uint32_t)IRQn){
		NVIC->IPR[IRQn] = (priority << NVIC_PRIORITY_BITS); // p p p p x x x x (bits[3:0] are ignored)
	}
	else{
		ret = STD_ERROR;
	}
	return ret;
}

STD_ReturnType NVIC_GetPriorityIRQ(NVIC_IRQ_Type IRQn, uint8_t* priority){
	STD_ReturnType ret = STD_SUCCESS;

	if(NVIC_IRQ_NUMS_START <= (uint32_t)IRQn && NVIC_IRQ_NUMS_END >= (uint32_t)IRQn){
		*priority = NVIC->IPR[IRQn] >> NVIC_PRIORITY_BITS;
	}
	else{
		ret = STD_ERROR;
	}

	return ret;
}

STD_ReturnType NVIC_SetSoftwareTrigger(NVIC_IRQ_Type IRQn){
    STD_ReturnType ret = STD_SUCCESS;

    if(NVIC_IRQ_NUMS_START <= (uint32_t)IRQn && NVIC_IRQ_NUMS_END >= (uint32_t)IRQn){
        NVIC->STIR = (uint32_t)IRQn;
    }
    else{
        ret = STD_ERROR;
    }

    return ret;
}