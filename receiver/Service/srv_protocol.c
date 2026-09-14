#include "srv_protocol.h"
#include <stddef.h>

uint8_t srv_protocol_calc_checksum(const uint8_t *p_data, uint16_t len)
{
    uint8_t sum = 0;
    if (p_data == NULL || len == 0) {
        return 0;
    }
    for (uint16_t i = 0; i < len; i++) {
        sum += p_data[i];
    }
    return sum;
}
