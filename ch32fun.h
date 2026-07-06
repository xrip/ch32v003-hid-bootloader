#ifndef CH32FUN_H
#define CH32FUN_H

#define RCC_BASE        0x40021000
#define FLASH_R_BASE    0x40022000
#define EXTI_BASE       0x40010400
#define TIM1_BASE       0x40012C00

#define GPIOD_BASE      0x40011400
#define GPIOD           ((GPIO_TypeDef *)GPIOD_BASE)
#define AFIO            ((AFIO_TypeDef *)0x40010000)
#define EXTI            ((EXTI_TypeDef *)EXTI_BASE)
#define RCC             ((RCC_TypeDef *)RCC_BASE)
#define FLASH           ((FLASH_TypeDef *)FLASH_R_BASE)
#define PFIC            ((PFIC_TypeDef *)0xE000E000)

#define RCC_APB2Periph_GPIOD 0x00000020
#define RCC_APB2Periph_AFIO  0x00000001
#define GPIO_PortSourceGPIOD 3

#define GPIO_Speed_In             0x0
#define GPIO_CNF_IN_FLOATING      0x4
#define GPIO_CFGLR_OUT_50Mhz_PP   0x3

#define EXTI7_0_IRQn 20

#define FLASH_KEY1 0x45670123
#define FLASH_KEY2 0xCDEF89AB
#define CR_LOCK_Set (1 << 7)

#define XW_C_LBU(rd, rs, imm) lbu rd, imm(rs)
#define XW_C_LHU(rd, rs, imm) lhu rd, imm(rs)
#define XW_C_SB(rs, rd, imm) sb rs, imm(rd)

#ifndef __ASSEMBLER__
#include <stdint.h>

typedef struct {
    volatile uint32_t CFGLR;
    volatile uint32_t CFGHR;
    volatile uint32_t INDR;
    volatile uint32_t OUTDR;
    volatile uint32_t BSHR;
    volatile uint32_t BCR;
    volatile uint32_t LCKR;
} GPIO_TypeDef;

typedef struct {
    volatile uint32_t CTLR;
    volatile uint32_t CFGR0;
    volatile uint32_t INTR;
    volatile uint32_t APB2PRSTR;
    volatile uint32_t APB1PRSTR;
    volatile uint32_t AHBPCENR;
    volatile uint32_t APB2PCENR;
    volatile uint32_t APB1PCENR;
    volatile uint32_t RSTSCKR;
} RCC_TypeDef;

typedef struct {
    volatile uint32_t ECR;
    volatile uint32_t PCFR1;
    volatile uint32_t EXTICR;
} AFIO_TypeDef;

typedef struct {
    volatile uint32_t INTENR;
    volatile uint32_t EVENR;
    volatile uint32_t RTENR;
    volatile uint32_t FTENR;
    volatile uint32_t SWIEVR;
    volatile uint32_t INTFR;
} EXTI_TypeDef;

typedef struct {
    volatile uint32_t ACTLR;
    volatile uint32_t KEYR;
    volatile uint32_t OBKEYR;
    volatile uint32_t STATR;
    volatile uint32_t CTLR;
    volatile uint32_t ADDR;
    volatile uint32_t RESERVED;
    volatile uint32_t OBR;
    volatile uint32_t WPR;
    volatile uint32_t MODEKEYR;
} FLASH_TypeDef;

typedef struct {
    volatile uint32_t ISR[8];
    volatile uint32_t IPR[8];
    volatile uint32_t ITHRESDR;
    volatile uint32_t RESERVED;
    volatile uint32_t CFGR;
    volatile uint32_t GISR;
    volatile uint8_t VTFIDR[4];
    uint8_t RESERVED0[0x90];
    volatile uint32_t IENR[8];
    uint8_t RESERVED1[0x60];
    volatile uint32_t IRER[8];
    uint8_t RESERVED2[0x60];
    volatile uint32_t IPSR[8];
    uint8_t RESERVED3[0x60];
    volatile uint32_t IPRR[8];
    uint8_t RESERVED4[0x60];
    volatile uint32_t IACTR[8];
    uint8_t RESERVED5[0xE0];
    volatile uint32_t SCTLR;
} PFIC_TypeDef;

static inline void NVIC_EnableIRQ(int irq) {
    PFIC->IENR[((uint32_t)irq) >> 5] = 1u << (((uint32_t)irq) & 31u);
}

#endif

#endif
