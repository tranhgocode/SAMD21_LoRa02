/**
 * @file crc16.h
 * @brief CRC-16 calculation helpers.
 */

#ifndef CRC16_H
#define CRC16_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Calculate CRC-16/CCITT-FALSE for a byte buffer
 *
 * A NULL data pointer is valid only when length is zero. In that case the
 * result is the algorithm's initial value, 0xFFFF
 */
bool CRC16_CalculateCcittFalse(const uint8_t *data,
                               size_t length,
                               uint16_t *crc);

#ifdef __cplusplus
}
#endif

#endif /* CRC16_H */
