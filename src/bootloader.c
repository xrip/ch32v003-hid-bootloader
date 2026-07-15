/* Copyright (c) 2026, Ilya Maslennikov aka xrip */

#include <stdint.h>
#include "bootloader.h"
#include "rv003usb.h"

#define COMMAND_WRITE_FLASH    3
#define COMMAND_BOOT_FIRMWARE  4

#define FLASH_ERASE_SIZE  1024u
#define FLASH_WRITE_SIZE  8u
#define FLASH_REPORT_SIZE (5u + FLASH_WRITE_SIZE)
#define FLASH_REPORT_PACKETS ((FLASH_REPORT_SIZE + 7u) / 8u)

#define FLASH_BSY  (1u << 0)
#define FLASH_PER  (1u << 1)
#define FLASH_STRT (1u << 6)
#define FLASH_LOCK (1u << 7)
#define FLASH_PG   (1u << 0)

struct rv003usb_internal rv003usb_internal_data;
static volatile uint32_t flash_report_buffer[(FLASH_REPORT_SIZE + 3u) / 4u];

static const uint8_t usb_descriptors[] = {
    /* Device descriptor, offset 0, length 18. */
    18, 1, 0x10, 0x01, 0, 0, 0, 8,
    0x09, 0x12, 0xef, 0xbe, 0x00, 0x01,
    0, 1, 1, 1,
    /* Configuration + interface + HID + endpoint descriptor, offset 18, length 34. */
    9, 2, 34, 0, 1, 1, 0, 0x80, 50,
    9, 4, 0, 0, 1, 3, 0, 0, 0,
    9, 0x21, 0x11, 0x01, 0, 1, 0x22, 16, 0,
    7, 5, 0x81, 0x03, 8, 0, 0xff,
    /* Report descriptor, offset 52, length 16. */
    0x06, 0x00, 0xff, 0x09, 0x01, 0xa1, 0x01,
    0x75, 0x08, 0x95, 0x0d, 0x09, 0x03, 0xb1, 0x02,
    0xc0,
    /* String descriptors: language at offset 68, product at offset 72. */
    4, 3, 0x09, 0x04,
    12, 3,
    'C', 0, 'H', 0, '3', 0, '2', 0, 'V', 0
};

struct descriptor_info {
    uint16_t request_value;
    uint8_t data_offset;
    uint8_t data_length;
};

static const struct descriptor_info usb_descriptor_map[] = {
    {0x0100, 0, 18},
    {0x0200, 18, 34},
    {0x0300, 68, 4},
    {0x0301, 72, 12},
    {0x2200, 52, 16},
};

static uint16_t read_u16_le(const uint8_t *bytes) {
    return (uint16_t)bytes[0] | (uint16_t)bytes[1] << 8;
}

static inline __attribute__((always_inline)) void wait_for_flash(void) {
    while (FLASH->STATR & FLASH_BSY) {}
}

#if defined(__GNUC__) && !defined(__clang__)
#define ATTR_NOIPA __attribute__((noipa))
#else
#define ATTR_NOIPA
#endif

static void __attribute__((noinline)) usb_detach(void) {
    GPIOC->CFGLR = GPIO_CFGLR_OUT_50Mhz_PP << (4 * USB_PIN_DM);
    GPIOC->BCR = 1u << USB_PIN_DM;
    __asm__ volatile(
        "lui a0, 0xf4\n"
        "1:\n"
        "c.addi a0, -1\n"
        "c.bnez a0, 1b\n"
        :
        :
        : "a0"
    );
}

void boot_firmware(void) __attribute__((used, noinline, noreturn)) ATTR_NOIPA;
void boot_firmware(void) {
    /* A fixed D- pull-up needs an active low pulse to leave the USB bus. */
    __asm__ volatile("csrci mstatus, 8" ::: "memory");
    EXTI->INTENR = 0;
    usb_detach();
    FLASH->BOOT_MODEKEYR = FLASH_KEY1;
    FLASH->BOOT_MODEKEYR = FLASH_KEY2;
    FLASH->STATR = 0;
    PFIC->CFGR = 0xBEEF0080;
    while (1) {}
}

static inline __attribute__((always_inline)) void process_flash_report(const uint8_t *report) {
    const uint8_t command = report[0];
    if (command == COMMAND_BOOT_FIRMWARE) {
        __asm__ volatile("c.j boot_firmware");
        __builtin_unreachable();
    }
    if (command != COMMAND_WRITE_FLASH)
        return;
    const uint32_t report_header = *(const uint32_t *)report;
    const uint32_t flash_address = (report_header >> 8) |
                                   *(const uint32_t *)(report + 4) << 24;

    FLASH->KEYR = FLASH_KEY1;
    FLASH->KEYR = FLASH_KEY2;
    wait_for_flash();
    if ((flash_address & (FLASH_ERASE_SIZE - 1)) == 0) {
        FLASH->CTLR = FLASH_PER;
        FLASH->ADDR = flash_address;
        FLASH->CTLR = FLASH_PER | FLASH_STRT;
        wait_for_flash();
    }
    uint32_t byte_offset = 0;
    FLASH->CTLR = FLASH_PG;
    do {
        *(volatile uint16_t *)(flash_address + byte_offset) =
            read_u16_le(report + 5 + byte_offset);
        wait_for_flash();
        byte_offset += 2;
    } while (byte_offset != FLASH_WRITE_SIZE);
    FLASH->CTLR = FLASH_LOCK;
}

static inline void setup_usb(void) {
    /* Direct write: only GPIOC+AFIO are ever needed (USB is bit-banged; FLASH/PFIC always-on). */
    RCC->APB2PCENR = RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO;
    usb_detach();
    GPIOC->CFGLR = (GPIO_CNF_IN_FLOATING << (4 * USB_PIN_DP)) |
                   (GPIO_CNF_IN_FLOATING << (4 * USB_PIN_DM));
    AFIO->EXTICR = GPIO_PortSourceGPIOC << 4;
    EXTI->INTENR = 1 << USB_PIN_DM;
    EXTI->FTENR = 1 << USB_PIN_DM;
    *(volatile uint32_t *)SYSTICK_BASE = 5; /* Enable SysTick from the 48 MHz HCLK. */
    NVIC_EnableIRQ(EXTI7_0_IRQn);
}

void usb_pid_handle_in(const uint32_t device_address, const uint8_t *packet_data,
                       const uint32_t endpoint_number, const uint32_t unused,
                       struct rv003usb_internal *usb_state) {
    (void)device_address; (void)packet_data; (void)unused;
    if (endpoint_number) {
        /* HID requires Interrupt IN; all useful reports still use control EP0. */
        usb_send_data(nullptr, 0, 2, 0x5A); /* NAK */
        return;
    }
    const struct usb_endpoint *endpoint_state = &usb_state->eps[0];
    uint32_t bytes_left = endpoint_state->max_len - (endpoint_state->count << 3);
    if (bytes_left > 8) bytes_left = 8;
    const uint32_t data_token = endpoint_state->toggle_in ? 0b01001011 : 0b11000011;
    if (!bytes_left || !endpoint_state->opaque) {
        usb_send_empty(data_token);
        if (usb_state->setup_request == 2 && endpoint_state->count == FLASH_REPORT_PACKETS) {
            usb_state->setup_request = 0;
            process_flash_report((const uint8_t *)flash_report_buffer);
        }
        return;
    }
    usb_send_data(endpoint_state->opaque + (endpoint_state->count << 3),
                  bytes_left, 0, data_token);
}

void usb_pid_handle_data(const uint32_t received_token, uint8_t *packet_data,
                         const uint32_t data_pid, const uint32_t packet_length,
                         struct rv003usb_internal *usb_state) {
    (void)received_token; (void)data_pid; (void)packet_length;
    struct usb_endpoint *endpoint_state = &usb_state->eps[0];
    if (usb_state->setup_request == 2) {
        const uint32_t report_offset = endpoint_state->count << 3;
        volatile uint32_t *report_destination =
            (volatile uint32_t *)((uint8_t *)flash_report_buffer + report_offset);
        const uint32_t *packet_source = (const uint32_t *)packet_data;
        report_destination[0] = packet_source[0];
        report_destination[1] = packet_source[1];
        endpoint_state->count++;
    } else if (usb_state->setup_request) {
        const uint32_t request_header = ((const uint32_t *)packet_data)[0];
        const uint32_t request_tail = ((const uint32_t *)packet_data)[1];
        const uint32_t request_code = (uint16_t)request_header >> 1;
        const uint32_t request_value = request_header >> 16;
        endpoint_state->opaque = nullptr;
        endpoint_state->max_len = 0;
        endpoint_state->count = 0;
        usb_state->setup_request = 0;
        if (request_code == (0x0680 >> 1)) {
            for (const struct descriptor_info *descriptor = usb_descriptor_map;
                 descriptor != usb_descriptor_map + sizeof(usb_descriptor_map) / sizeof(usb_descriptor_map[0]);
                 descriptor++) {
                if (descriptor->request_value != request_value)
                    continue;
                endpoint_state->opaque = (uint8_t *)usb_descriptors + descriptor->data_offset;
                endpoint_state->max_len = descriptor->data_length;
                const uint32_t requested_length = request_tail >> 16;
                if (endpoint_state->max_len > requested_length)
                    endpoint_state->max_len = requested_length;
                goto send_status_ack;
            }
        } else if (request_code == (0x0500 >> 1)) {
            usb_state->my_address = request_value;
        } else if (request_code == (0x0921 >> 1) && request_value == 0x0300) {
            usb_state->setup_request = 2;
        }
    }
send_status_ack:
    usb_send_data(nullptr, 0, 2, 0xD2);
}

int __attribute__((noreturn)) main(void) {
    flash_report_buffer[0] = 0;
    setup_usb();
    __asm__ goto(
        "lui a1, 0x8000\n"
        "c.lw a1, 0(a1)\n"
        "c.addi a1, 1\n"
        "c.beqz a1, %l[stay]\n"
        "lui a2, 0x1ba8\n"
        "lui a0, 0x20000\n"
        "1:\n"
        "c.lw a1, 0(a0)\n"
        "c.bnez a1, %l[stay]\n"
        "c.addi a2, -1\n"
        "c.bnez a2, 1b\n"
        "c.j boot_firmware\n"
        :
        :
        : "a0", "a1", "a2"
        : stay
    );
    __builtin_unreachable();
stay:
    while (1) {}
}
