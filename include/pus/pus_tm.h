/**
 * @file    pus_tm.h
 * @brief   Construction of PUS-C telemetry packets.
 */
#ifndef PUS_TM_H
#define PUS_TM_H

#include "pus/ccsds.h"

/**
 * Length of the TM packet secondary header in octets:
 * version/time-status (1) + type (1) + subtype (1) + message counter (2)
 * + destination ID (2) + CUC time (6).
 */
#define PUS_TM_SEC_HEADER_LEN   13u

/** Length of the CCSDS unsegmented time code field (4 coarse + 2 fine). */
#define PUS_CUC_LEN             6u

/** Reset the packet sequence count and the per-service message counters. */
void pus_tm_reset(void);

/**
 * @brief Build a complete telemetry packet, CRC included.
 *
 * @param buf       Destination buffer.
 * @param cap       Capacity of @p buf in octets.
 * @param service   PUS service type.
 * @param subtype   PUS message subtype.
 * @param src_data  Source data field (may be NULL when @p src_len is 0).
 * @param src_len   Length of the source data field.
 * @param out_len   Receives the total packet length on success.
 * @return PUS_OK, PUS_ERR_NULL or PUS_ERR_BUFFER_TOO_SMALL.
 */
pus_status_t pus_tm_build(uint8_t *buf, size_t cap,
                          uint8_t service, uint8_t subtype,
                          const uint8_t *src_data, size_t src_len,
                          size_t *out_len);

/**
 * @brief Build a telemetry packet and push it onto the downlink queue.
 * @return PUS_OK, a build error, or PUS_ERR_QUEUE_FULL.
 */
pus_status_t pus_tm_send(uint8_t service, uint8_t subtype,
                         const uint8_t *src_data, size_t src_len);

#endif /* PUS_TM_H */
