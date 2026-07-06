#ifndef USB_CONFIG_H
#define USB_CONFIG_H

#define ENDPOINTS 2

#define USB_PORT D
#define USB_PIN_DP 3
#define USB_PIN_DM 4
#define USB_PIN_DPU 5

#ifndef __ASSEMBLER__
#include <stdint.h>

struct descriptor_list_struct {
    uint32_t lIndexValue;
    const uint8_t *addr;
    uint16_t length;
};

extern const struct descriptor_list_struct descriptor_list[];
#endif

#define DESCRIPTOR_LIST_ENTRIES 0

#endif
