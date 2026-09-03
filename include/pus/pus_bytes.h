/**
 * @file    pus_bytes.h
 * @brief   Big-endian (network order) serialisation helpers.
 *
 * CCSDS and PUS fields are transmitted most significant octet first
 * regardless of the endianness of the processor, so all field access goes
 * through these helpers instead of struct overlays or casts.
 */
#ifndef PUS_BYTES_H
#define PUS_BYTES_H

#include "pus/pus_types.h"

static inline void pus_put_u8(uint8_t *p, uint8_t v)
{
    p[0] = v;
}

static inline void pus_put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)((v >> 8) & 0xFFu);
    p[1] = (uint8_t)(v & 0xFFu);
}

static inline void pus_put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)((v >> 24) & 0xFFu);
    p[1] = (uint8_t)((v >> 16) & 0xFFu);
    p[2] = (uint8_t)((v >> 8) & 0xFFu);
    p[3] = (uint8_t)(v & 0xFFu);
}

static inline uint8_t pus_get_u8(const uint8_t *p)
{
    return p[0];
}

static inline uint16_t pus_get_u16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static inline uint32_t pus_get_u32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}

static inline int16_t pus_get_i16(const uint8_t *p)
{
    return (int16_t)pus_get_u16(p);
}

static inline void pus_put_i16(uint8_t *p, int16_t v)
{
    pus_put_u16(p, (uint16_t)v);
}

#endif /* PUS_BYTES_H */
