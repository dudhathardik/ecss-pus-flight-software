/**
 * @file    test_support.h
 * @brief   Shared helpers for the unit test cases (test code, not flight code).
 */
#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H

#include "pus/pus_config.h"
#include "pus/pus_tc.h"
#include "pus/pus_types.h"

/**
 * @brief Assemble a well formed telecommand, CRC included.
 * @return Total packet length in octets.
 */
size_t ts_build_tc(uint8_t *buf, size_t cap,
                   uint8_t service, uint8_t subtype, uint8_t ack_flags,
                   const uint8_t *app_data, size_t app_len);

/** @brief Recompute and rewrite the CRC of a packet edited in place. */
void ts_fix_crc(uint8_t *buf, size_t len);

/** @brief Flip one bit in the CRC so the packet fails its acceptance check. */
void ts_break_crc(uint8_t *buf, size_t len);

/** Decoded telemetry packet, as seen by the ground segment. */
typedef struct {
    uint8_t   service;
    uint8_t   subtype;
    uint16_t  seq_count;
    uint16_t  msg_counter;
    uint32_t  time_coarse;
    uint16_t  time_fine;
    const uint8_t *data;     /**< Points into the caller's buffer. */
    size_t    data_len;
    size_t    total_len;
} ts_tm_t;

/**
 * @brief Pop the next telemetry packet and decode its headers.
 * @return PUS_TRUE when a packet was available.
 */
pus_bool_t ts_tm_pop(uint8_t *buf, size_t cap, ts_tm_t *tm);

/**
 * @brief Drain the queue until a packet of the requested type is found.
 * @return PUS_TRUE when found; @p tm then describes it.
 */
pus_bool_t ts_tm_find(uint8_t service, uint8_t subtype,
                      uint8_t *buf, size_t cap, ts_tm_t *tm);

/** @brief Discard every queued telemetry packet. */
void ts_tm_flush(void);

/**
 * @brief Build a telecommand requesting both acknowledgements and run it
 *        through the full acceptance / routing / verification path.
 * @return The status obc_process_tc() reported to ground.
 */
pus_status_t ts_send_tc(uint8_t service, uint8_t subtype,
                        const uint8_t *app_data, size_t app_len);

#endif /* TEST_SUPPORT_H */
