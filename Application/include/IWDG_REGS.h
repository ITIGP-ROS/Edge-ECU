/**
 ******************************************************************************
 * @file    iwdg_regs.h
 * @brief   Independent Watchdog (IWDG) Register Definitions for STM32F401CC
 * @author  Abdulrahman
 * @date    January 2026
 ******************************************************************************
 */

#ifndef IWDG_REGS_H
#define IWDG_REGS_H

#include "STD_TYPES.h"

/*===========================================================================*/
/*                          REGISTER STRUCTURES                               */
/*===========================================================================*/

typedef union {
    volatile uint32_t ALL;
    struct {
        volatile uint32_t KEY        : 16;
        volatile uint32_t RESERVED0  : 16;
    } BITS;
} IWDG_KR_t;

typedef union {
    volatile uint32_t ALL;
    struct {
        volatile uint32_t PR         : 3;
        volatile uint32_t RESERVED0  : 29;
    } BITS;
} IWDG_PR_t;

typedef union {
    volatile uint32_t ALL;
    struct {
        volatile uint32_t RL         : 12;
        volatile uint32_t RESERVED0  : 20;
    } BITS;
} IWDG_RLR_t;

typedef union {
    volatile uint32_t ALL;
    struct {
        volatile uint32_t PVU        : 1;
        volatile uint32_t RVU        : 1;
        volatile uint32_t RESERVED0  : 30;
    } BITS;
} IWDG_SR_t;

typedef struct {
    IWDG_KR_t   KR;
    IWDG_PR_t   PR;
    IWDG_RLR_t  RLR;
    IWDG_SR_t   SR;
} IWDG_REGS_t;

#define IWDG_BASEADDR           0x40003000UL
#define IWDG                    ((IWDG_REGS_t*)IWDG_BASEADDR)


/*===========================================================================*/
/*                          KEY VALUES                                        */
/*===========================================================================*/

#define IWDG_KEY_RELOAD         ((uint32_t)0xAAAAU)
#define IWDG_KEY_ENABLE         ((uint32_t)0xCCCCU)
#define IWDG_KEY_WR_ACCESS      ((uint32_t)0x5555U)


/*===========================================================================*/
/*                          PRESCALER VALUES                                  */
/*===========================================================================*/

/* Raw values for PR register (only for direct register writes, not for
   comparison with enum IWDG_Prescaler_t). These are NOT the same as the
   divider value. */
#define IWDG_PRESCALER_4        0x0U
#define IWDG_PRESCALER_8        0x1U
#define IWDG_PRESCALER_16       0x2U
#define IWDG_PRESCALER_32       0x3U
#define IWDG_PRESCALER_64       0x4U
#define IWDG_PRESCALER_128      0x5U
#define IWDG_PRESCALER_256      0x6U


/*===========================================================================*/
/*                          RELOAD VALUES                                     */
/*===========================================================================*/

#define IWDG_RLR_MIN            ((uint32_t)0x001U)
#define IWDG_RLR_MAX            ((uint32_t)0xFFFU)


/*===========================================================================*/
/*                          STATUS FLAGS                                      */
/*===========================================================================*/

#define IWDG_SR_PVU_POS         0U
#define IWDG_SR_RVU_POS         1U

#define IWDG_SR_PVU_MASK        ((uint32_t)0x0001U)
#define IWDG_SR_RVU_MASK        ((uint32_t)0x0002U)
#define IWDG_SR_UPDATE_MASK     (IWDG_SR_PVU_MASK | IWDG_SR_RVU_MASK)


/*===========================================================================*/
/*                          CLOCK CONSTANTS                                   */
/*===========================================================================*/

#define IWDG_LSI_FREQ_TYP       32000U
#define IWDG_LSI_FREQ_MIN       17000U
#define IWDG_LSI_FREQ_MAX       47000U


/*===========================================================================*/
/*                          TIMEOUT POLL CONSTANT                             */
/*===========================================================================*/

#define IWDG_SR_POLL_TIMEOUT    50000U


/*===========================================================================*/
/*                          HELPER MACROS                                     */
/*===========================================================================*/

#define IWDG_IS_PVU_ONGOING()    ((IWDG->SR.ALL & IWDG_SR_PVU_MASK) != 0U)
#define IWDG_IS_RVU_ONGOING()    ((IWDG->SR.ALL & IWDG_SR_RVU_MASK) != 0U)
#define IWDG_IS_UPDATE_ONGOING() ((IWDG->SR.ALL & IWDG_SR_UPDATE_MASK) != 0U)

#define IWDG_WRITE_KEY(key)      (IWDG->KR.ALL = (key))

#define IWDG_ENABLE_WRITE()      IWDG_WRITE_KEY(IWDG_KEY_WR_ACCESS)
#define IWDG_RELOAD()            IWDG_WRITE_KEY(IWDG_KEY_RELOAD)
#define IWDG_START()             IWDG_WRITE_KEY(IWDG_KEY_ENABLE)

#define IWDG_SET_PRESCALER(prescaler) \
    do { \
        IWDG->PR.ALL = ((uint32_t)(prescaler) & 0x7U); \
    } while(0)

#define IWDG_SET_RELOAD(reload) \
    do { \
        IWDG->RLR.ALL = ((uint32_t)(reload) & (uint32_t)0xFFFU); \
    } while(0)

#define IWDG_GET_PRESCALER()    (IWDG->PR.ALL  & 0x7U)
#define IWDG_GET_RELOAD()       (IWDG->RLR.ALL & (uint32_t)0xFFFU)

#endif /* IWDG_REGS_H */