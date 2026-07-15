#ifndef BOOTLOADER_H
#define BOOTLOADER_H

/* ============================================================
 * rv003usb configuration (vendor feature flags)
 * ============================================================ */

#define FUNCONF_SYSTICK_USE_HCLK 1

#define RV003USB_HID_FEATURES 1
#define RV003USB_SUPPORT_CONTROL_OUT 0
#define RV003USB_USE_REBOOT_FEATURE_REPORT 0
#define RV003USB_HANDLE_IN_REQUEST 0
#define RV003USB_HANDLE_USER_DATA 0
#define RV003USB_OTHER_CONTROL 0
#define RV003USB_EVENT_DEBUGGING 0
#define RV003USB_DEBUG_TIMING 0
#define RV003USB_CUSTOM_C 0
#define RV003USB_USB_TERMINAL 0
#define RV003USB_BOOTLOADER 0
#define RV003USB_OPTIMIZE_FLASH 1

/* ============================================================
 * USB pin and endpoint configuration
 * ============================================================ */

#define ENDPOINTS 2

#define USB_PORT C
#define USB_PIN_DP 1
#define USB_PIN_DM 2

/* ============================================================
 * Register base addresses
 * ============================================================ */

#define RCC_BASE        0x40021000
#define FLASH_R_BASE    0x40022000
#define EXTI_BASE       0x40010400
#define TIM1_BASE       0x40012C00
#define SYSTICK_BASE    0xE000F000

#define GPIOC_BASE      0x40011000

/* ============================================================
 * Peripheral pointers (usable in C; inert in asm)
 * ============================================================ */

#define GPIOC           ((GPIO_TypeDef *)GPIOC_BASE)
#define AFIO            ((AFIO_TypeDef *)0x40010000)
#define EXTI            ((EXTI_TypeDef *)EXTI_BASE)
#define RCC             ((RCC_TypeDef *)RCC_BASE)
#define FLASH           ((FLASH_TypeDef *)FLASH_R_BASE)
#define PFIC            ((PFIC_TypeDef *)0xE000E000)

/* ============================================================
 * Peripheral bit / mode defines
 * ============================================================ */

#define RCC_APB2Periph_GPIOC 0x00000010
#define RCC_APB2Periph_AFIO  0x00000001
#define GPIO_PortSourceGPIOC 2

#define GPIO_Speed_In             0x0
#define GPIO_CNF_IN_FLOATING      0x4
#define GPIO_CFGLR_OUT_50Mhz_PP   0x3

#define EXTI7_0_IRQn 20

#define FLASH_KEY1 0x45670123
#define FLASH_KEY2 0xCDEF89AB
#define CR_LOCK_Set (1 << 7)

/* RV32E compressed-load/store helpers used by rv003usb.S */
#define XW_C_LBU(rd, rs, imm) lbu rd, imm(rs)
#define XW_C_LHU(rd, rs, imm) lhu rd, imm(rs)
#define XW_C_SB(rs, rd, imm) sb rs, imm(rd)

#ifndef __ASSEMBLER__
#include <stdint.h>

/* ============================================================
 * Peripheral register layouts
 * ============================================================ */

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
    volatile uint32_t BOOT_MODEKEYR;
} FLASH_TypeDef;

_Static_assert(__builtin_offsetof(FLASH_TypeDef, BOOT_MODEKEYR) == 0x28,
               "FLASH BOOT_MODEKEYR offset must match CH32V003");

typedef struct {
    volatile uint32_t ISR[8];
    volatile uint32_t IPR[8];
    volatile uint32_t ITHRESDR;
    volatile uint32_t RESERVED;
    volatile uint32_t CFGR;
    volatile uint32_t GISR;
    volatile uint8_t VTFIDR[4];
    uint8_t RESERVED0[12];
    volatile uint32_t VTFADDR[4];
    uint8_t RESERVED1[0x90];
    volatile uint32_t IENR[8];
    uint8_t RESERVED2[0x60];
    volatile uint32_t IRER[8];
    uint8_t RESERVED3[0x60];
    volatile uint32_t IPSR[8];
    uint8_t RESERVED4[0x60];
    volatile uint32_t IPRR[8];
    uint8_t RESERVED5[0x60];
    volatile uint32_t IACTR[8];
    uint8_t RESERVED6[0xE0];
    volatile uint8_t IPRIOR[256];
    uint8_t RESERVED7[0x810];
    volatile uint32_t SCTLR;
} PFIC_TypeDef;

_Static_assert(__builtin_offsetof(PFIC_TypeDef, IENR) == 0x100,
               "PFIC IENR offset must match CH32V003");

/* ============================================================
 * USB descriptor list (rv003usb optional API)
 * ============================================================ */

struct descriptor_list_struct {
    uint32_t lIndexValue;
    const uint8_t *addr;
    uint16_t length;
};

extern const struct descriptor_list_struct descriptor_list[];

/* ============================================================
 * Inline helpers
 * ============================================================ */

static inline void NVIC_EnableIRQ(int irq) {
    PFIC->IENR[((uint32_t)irq) >> 5] = 1u << (((uint32_t)irq) & 31u);
}

#endif /* __ASSEMBLER__ */

#define DESCRIPTOR_LIST_ENTRIES 0

#endif /* BOOTLOADER_H */
