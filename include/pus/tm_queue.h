/**
 * @file    tm_queue.h
 * @brief   Statically allocated FIFO holding telemetry awaiting downlink.
 */
#ifndef PUS_TM_QUEUE_H
#define PUS_TM_QUEUE_H

#include "pus/pus_config.h"
#include "pus/pus_types.h"

/** Empty the queue and clear the overflow counter. */
void tm_queue_init(void);

/**
 * @brief Append a packet to the queue.
 * @return PUS_OK, PUS_ERR_NULL, PUS_ERR_BAD_LENGTH or PUS_ERR_QUEUE_FULL.
 *         On overflow the packet is dropped and the overflow counter grows.
 */
pus_status_t tm_queue_push(const uint8_t *packet, size_t len);

/**
 * @brief Remove the oldest packet from the queue.
 * @param buf      Destination buffer.
 * @param cap      Capacity of @p buf.
 * @param out_len  Receives the length of the packet copied out.
 * @return PUS_OK, PUS_ERR_NULL, PUS_ERR_BUFFER_TOO_SMALL or PUS_ERR_QUEUE_FULL
 *         when the queue is empty (reported as count 0 by tm_queue_count()).
 */
pus_status_t tm_queue_pop(uint8_t *buf, size_t cap, size_t *out_len);

/** @return Number of packets currently queued. */
size_t tm_queue_count(void);

/** @return Number of packets dropped since the last tm_queue_init(). */
uint16_t tm_queue_overflow_count(void);

#endif /* PUS_TM_QUEUE_H */
