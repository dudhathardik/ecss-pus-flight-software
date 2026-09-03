/**
 * @file    crc16.c
 * @brief   CRC-16-CCITT used as the CCSDS packet error control field.
 */
#include "pus/crc16.h"

/**
 * @implements SWREQ-CRC-010
 *
 * Bitwise implementation: 8 shifts per octet. A 256-entry lookup table would
 * be roughly four times faster but costs 512 bytes of read-only memory; on the
 * target the CRC is computed on packets of at most PUS_MAX_PACKET_SIZE octets
 * once per minor cycle, so the bitwise form is comfortably within budget and
 * keeps the code auditable (see SVP.md section 5, WCET rationale).
 */
uint16_t crc16_ccitt(const uint8_t *data, size_t len)
{
    uint16_t crc = (uint16_t)CRC16_SEED;
    size_t   i;
    uint8_t  bit;

    if (data == NULL) {
        return crc;
    }

    for (i = 0u; i < len; ++i) {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (bit = 0u; bit < 8u; ++bit) {
            if ((crc & 0x8000u) != 0u) {
                crc = (uint16_t)((uint16_t)(crc << 1) ^ 0x1021u);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }

    return crc;
}
