#include <stdint.h>
#include "ch32fun.h"
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
static volatile uint32_t report_out[5];

static const uint8_t device_desc[] = {
    18, 1, 0x10, 0x01, 0, 0, 0, 8,
    0x09, 0x12, 0x03, 0xb0, 0x00, 0x01,
    0, 0, 0, 1
};

static const uint8_t report_desc[] = {
    0x06, 0x00, 0xff, 0x09, 0x01, 0xa1, 0x01,
    0x85, RID_WRITE,
    0x75, 0x08, 0x95, 0x0d, 0x09, 0x03, 0xb1, 0x02,
    0xc0
};

static const uint8_t config_desc[] = {
    9, 2, 27, 0, 1, 1, 0, 0x80, 50,
    9, 4, 0, 0, 0, 3, 0, 0, 0,
    9, 0x21, 0x11, 0x01, 0, 1, 0x22, sizeof(report_desc), 0,
};

static uint16_t get16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void flash_wait(void) {
    while (FLASH->STATR & FLASH_BSY) {}
}

static void __attribute__((noinline, noreturn)) boot_user(void) {
    FLASH->MODEKEYR = FLASH_KEY1;
    FLASH->MODEKEYR = FLASH_KEY2;
    FLASH->STATR = 0;
    PFIC->CFGR = 0xBEEF0080;
    while (1) {}
}

static void process_report(const uint8_t *r, uint32_t n) {
    if (n && r[0] == RID_WRITE) {
        r++;
        n--;
    }
    if (n < 13)
        return;

    uint32_t addr = get32(r + 1);
    if (r[0] == CMD_RESET)
        boot_user();
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
    for (uint32_t i = 0; i < 8; i += 2) {
        *(volatile uint16_t *)(addr + i) = get16(r + 5 + i);
        flash_wait();
    }
    FLASH->CTLR = FLASH_LOCK;
}

void usb_setup(void) {
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOD | RCC_APB2Periph_AFIO;
    GPIOD->CFGLR = (GPIOD->CFGLR & ~((0xf << 12) | (0xf << 16) | (0xf << 20))) |
        ((GPIO_Speed_In | GPIO_CNF_IN_FLOATING) << 12) |
        ((GPIO_Speed_In | GPIO_CNF_IN_FLOATING) << 16) |
        (GPIO_CFGLR_OUT_50Mhz_PP << 20);
    AFIO->EXTICR = GPIO_PortSourceGPIOD << 8;
    EXTI->INTENR = 1 << USB_PIN_DM;
    EXTI->FTENR = 1 << USB_PIN_DM;
    GPIOD->BSHR = 1 << USB_PIN_DPU;
    NVIC_EnableIRQ(EXTI7_0_IRQn);
}

void usb_pid_handle_in(uint32_t addr, uint8_t *data, uint32_t endp, uint32_t unused, struct rv003usb_internal *ist) {
    (void)addr; (void)data; (void)unused;
    struct usb_endpoint *e = &ist->eps[endp];
    uint32_t left = e->max_len - (e->count << 3);
    if (left > 8) left = 8;
    usb_send_data(e->opaque + (e->count << 3), left, 0, e->toggle_in ? 0b01001011 : 0b11000011);
}

void usb_pid_handle_data(uint32_t token, uint8_t *data, uint32_t which_data, uint32_t length, struct rv003usb_internal *ist) {
    (void)token; (void)which_data; (void)length;
    struct usb_endpoint *e = &ist->eps[ist->current_endpoint];
    uint8_t *p = data;
    if (ist->setup_request == 2) {
        uint32_t off = e->count << 3;
        uint8_t *dst = (uint8_t *)&report_out[1] + off;
        for (uint32_t i = 0; i < 8; i++)
            dst[i] = p[i];
        e->count++;
        if (e->count == 2) {
            report_out[0] = 1;
            process_report((const uint8_t *)&report_out[1], 14);
        }
    } else if (ist->setup_request) {
        struct usb_urb *s = (struct usb_urb *)p;
        uint32_t req = s->wRequestTypeLSBRequestMSB >> 1;
        uint32_t wvi = s->lValueLSBIndexMSB;
        e->opaque = 0;
        e->max_len = 0;
        e->count = 0;
        ist->setup_request = 0;
        if (req == (0x0680 >> 1)) {
            if (wvi == 0x00000100) {
                e->opaque = (uint8_t *)device_desc;
                e->max_len = sizeof(device_desc);
            } else if (wvi == 0x00000200) {
                e->opaque = (uint8_t *)config_desc;
                e->max_len = sizeof(config_desc);
            } else if (wvi == 0x00002200) {
                e->opaque = (uint8_t *)report_desc;
                e->max_len = sizeof(report_desc);
            }
        } else if (req == (0x0500 >> 1)) {
            ist->my_address = wvi;
        } else if (req == (0x0921 >> 1) && (wvi & 0xffff) == 0x0341) {
            ist->setup_request = 2;
        }
    }
    usb_send_data(0, 0, 2, 0xD2);
}

int main(void) {
    usb_setup();
    __asm__ goto(
        "lui t0, 0x300\n"
        "1:\n"
        "la t1, report_out\n"
        "lw t2, 0(t1)\n"
        "bnez t2, %l[stay]\n"
        "addi t0, t0, -1\n"
        "bnez t0, 1b\n"
        :
        :
        : "t0", "t1", "t2"
        : stay
    );
    boot_user();
stay:
    while (1) {}
}
