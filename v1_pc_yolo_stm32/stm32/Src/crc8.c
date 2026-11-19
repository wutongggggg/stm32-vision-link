#include "crc8.h"

uint8_t crc8_calc(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0x00u;
    while (len--) {
        crc ^= *data++;
        for (uint8_t i = 0; i < 8u; i++) {
            crc = (crc & 0x80u) ? (uint8_t)((crc << 1) ^ 0x07u)
                                : (uint8_t)(crc << 1);
        }
    }
    return crc;
}
