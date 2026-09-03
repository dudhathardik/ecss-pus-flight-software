/**
 * @file    crc16.h
 * @brief   CRC-16-CCITT used as the CCSDS packet error control field.
 *
 * Polynomial 0x1021, seed 0xFFFF, no input/output reflection, no final XOR
 * (ECSS-E-ST-70-41C Annex B).
 */
#ifndef PUS_CRC16_H
#define PUS_CRC16_H

#include "pus/pus_types.h"

/** Seed value mandated by ECSS-E-ST-70-41C. */
#define CRC16_SEED  0xFFFFu

/**
 * @brief Compute the CRC over a buffer.
 * @param data  Buffer to protect. May be NULL only if @p len is zero.
 * @param len   Number of octets to include.
 * @return      The 16-bit checksum, or CRC16_SEED for an empty buffer.
 */
uint16_t crc16_ccitt(const uint8_t *data, size_t len);

#endif /* PUS_CRC16_H */
