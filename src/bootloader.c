/* Copyright (c) 2026, Ilya Maslennikov aka xrip */

#include <stdint.h>
#include "bootloader.h"
#include "rv003usb.h"

#define RID_WRITE  0x41

#define CMD_WRITE  3
#define CMD_RESET  4

#define PAGE_SIZE  64u

#define FLASH_BSY  (1u << 0)
#define FLASH_PG   (1u << 0)
#define FLASH_PER  (1u << 1)
#define FLASH_STRT (1u << 6)
#define FLASH_LOCK (1u << 7)

struct rv003usb_internal rv003usb_internal_data;
static volatile uint32_t report_out[4];

static const uint8_t descriptors[] = {
    /* Device descriptor, offset 0, length 18. */
    18, 1, 0x10, 0x01, 0, 0, 0, 8,
    0x09, 0x12, 0x03, 0xb0, 0x00, 0x01,
    0, 0, 0, 1,
    /* Configuration + interface + HID descriptor, offset 18, length 27. */
    9, 2, 27, 0, 1, 1, 0, 0x80, 50,
    9, 4, 0, 0, 0, 3, 0, 0, 0,
    9, 0x21, 0x11, 0x01, 0, 1, 0x22, 18, 0,
    /* Report descriptor, offset 45, length 18. */
    0x06, 0x00, 0xff, 0x09, 0x01, 0xa1, 0x01,
    0x85, RID_WRITE,
    0x75, 0x08, 0x95, 0x0d, 0x09, 0x03, 0xb1, 0x02,
    0xc0
};

static uint16_t get16(const uint8_t *p) {
    return (uint16_t)p[0] | (uint16_t)p[1] << 8;
}

static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static inline __attribute__((always_inline)) void flash_wait(void) {
    while (FLASH->STATR & FLASH_BSY) {}
}

#if defined(__GNUC__) && !defined(__clang__)
#define ATTR_NOIPA __attribute__((noipa))
#else
#define ATTR_NOIPA
#endif

void boot_user(void) __attribute__((used, noinline, noreturn)) ATTR_NOIPA;
void boot_user(void) {
    FLASH->MODEKEYR = FLASH_KEY1;
    FLASH->MODEKEYR = FLASH_KEY2;
    FLASH->STATR = 0;
    PFIC->CFGR = 0xBEEF0080;
    while (1) {}
}

static __attribute__((noinline)) void process_payload(const uint8_t *r) {
    const uint32_t addr = get32(r + 1);
    if (r[0] == CMD_RESET)
        return boot_user();
    if (r[0] != CMD_WRITE)
        return;

    FLASH->KEYR = FLASH_KEY1;
    FLASH->KEYR = FLASH_KEY2;
    flash_wait();
    if ((addr & (PAGE_SIZE - 1)) == 0) {
        FLASH->CTLR = FLASH_PER;
        FLASH->ADDR = addr;
        FLASH->CTLR = FLASH_PER | FLASH_STRT;
        flash_wait();
    }
    FLASH->CTLR = FLASH_PG;
    uint32_t i = 0;
    do {
        *(volatile uint16_t *)(addr + i) = get16(r + 5 + i);
        flash_wait();
        i += 2;
    } while (i != 8);
    FLASH->CTLR = FLASH_LOCK;
}

static inline void usb_setup(void) {
    /* Direct write: only GPIOD+AFIO are ever needed (USB is bit-banged; FLASH/PFIC always-on). */
    RCC->APB2PCENR = RCC_APB2Periph_GPIOD | RCC_APB2Periph_AFIO;
    /* Reset value is 0x44444444 (all floating-in); set PD5/DPU to 50MHz PP out. */
    GPIOD->CFGLR = 0x44344444;
    AFIO->EXTICR = GPIO_PortSourceGPIOD << 8;
    EXTI->INTENR = 1 << USB_PIN_DM;
    EXTI->FTENR = 1 << USB_PIN_DM;
    GPIOD->BSHR = 1 << USB_PIN_DPU;
    NVIC_EnableIRQ(EXTI7_0_IRQn);
}

void usb_pid_handle_in(const uint32_t addr, const uint8_t *data, const uint32_t endp, const uint32_t unused, const struct rv003usb_internal *ist) {
    (void)addr; (void)data; (void)unused;
    const struct usb_endpoint *e = &ist->eps[0];
    uint32_t left = e->max_len - (e->count << 3);
    if (left > 8) left = 8;
    usb_send_data(e->opaque + (e->count << 3), left, 0, e->toggle_in ? 0b01001011 : 0b11000011);
}

void usb_pid_handle_data(const uint32_t this_token, uint8_t *data, const uint32_t which_data, const uint32_t length, struct rv003usb_internal *ist) {
    (void)this_token; (void)which_data; (void)length;
    struct usb_endpoint *e = &ist->eps[0];
    uint8_t *p = data;
    if (ist->setup_request == 2) {
        const uint32_t off = e->count << 3;
        volatile uint32_t *dst = (volatile uint32_t *)((uint8_t *)report_out + off);
        const uint32_t *src = (const uint32_t *)p;
        dst[0] = src[0];
        dst[1] = src[1];
        e->count++;
        if (e->count == 2) {
            const uint8_t *r = (const uint8_t *)report_out;
            process_payload(r + (r[0] == RID_WRITE));
        }
    } else if (ist->setup_request) {
        const uint32_t setup0 = ((const uint32_t *)p)[0];
        const uint32_t setup1 = ((const uint32_t *)p)[1];
        const uint32_t req = (uint16_t)setup0 >> 1;
        const uint32_t wvi = (setup0 >> 16) | (setup1 << 16);
        e->max_len = 0;
        e->count = 0;
        ist->setup_request = 0;
        if (req == (0x0680 >> 1)) {
            uint32_t off = 0;
            uint32_t len = 18;
            if (wvi & 0xff) goto send_ack;
            const uint32_t type = wvi >> 8;
            if (type == 2) {
                off = 18;
                len = 27;
            } else if (type == 0x22) {
                off = 45;
            } else if (type != 1) {
                goto send_ack;
            }
            e->opaque = (uint8_t *)descriptors + off;
            e->max_len = len;
        } else if (req == (0x0500 >> 1)) {
            ist->my_address = wvi;
        } else if (req == (0x0921 >> 1) && (wvi & 0xffff) == 0x0341) {
            ist->setup_request = 2;
        }
    }
send_ack:
    usb_send_data(nullptr, 0, 2, 0xD2);
}

int __attribute__((noreturn)) main(void) {
    usb_setup();
    __asm__ goto(
        "lui a2, 0x300\n"
        "la a0, report_out\n"
        "1:\n"
        "c.lw a1, 0(a0)\n"
        "c.bnez a1, %l[stay]\n"
        "c.addi a2, -1\n"
        "c.bnez a2, 1b\n"
        "c.j boot_user\n"
        :
        :
        : "a0", "a1", "a2"
        : stay
    );
    __builtin_unreachable();
stay:
    while (1) {}
}
