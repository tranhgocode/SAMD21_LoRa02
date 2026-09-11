/**
 * @file crc16.c
 * @brief CRC-16 calculation helpers.
 */

#include "crc16.h"

#define CRC16_CCITT_FALSE_INITIAL_VALUE  0xFFFFU
#define CRC16_CCITT_FALSE_POLYNOMIAL     0x1021U
#define CRC16_TOP_BIT                    0x8000U

bool CRC16_CalculateCcittFalse(const uint8_t *data,
                               size_t length,
                               uint16_t *crc)
{
    uint16_t value = CRC16_CCITT_FALSE_INITIAL_VALUE;
    size_t byteIndex;

    if ((crc == NULL) || ((data == NULL) && (length != 0U)))
    {
        return false;
    }

    for (byteIndex = 0U; byteIndex < length; byteIndex++)
    {
        uint8_t bitIndex;

        value ^= (uint16_t)((uint16_t)data[byteIndex] << 8U);

        for (bitIndex = 0U; bitIndex < 8U; bitIndex++)
        {
            if ((value & CRC16_TOP_BIT) != 0U)
            {
                value = (uint16_t)((value << 1U) ^
                                   CRC16_CCITT_FALSE_POLYNOMIAL);
            }
            else
            {
                value = (uint16_t)(value << 1U);
            }
        }
    }

    *crc = value;
    return true;
}
