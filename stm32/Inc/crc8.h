#ifndef CRC8_H
#define CRC8_H

#include <stdint.h>

/* CRC-8/ATM: poly=0x07, init=0x00, 不反转 */
uint8_t crc8_calc(const uint8_t *data, uint16_t len);

#endif /* CRC8_H */
