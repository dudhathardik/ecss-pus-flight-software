/**
 * @file    tm_queue.c
 * @brief   Statically allocated downlink FIFO.
 */
#include "pus/tm_queue.h"

/** @implements SWREQ-TMQ-030 : storage is static, nothing is ever malloc'ed. */
static uint8_t  s_slots[PUS_TM_QUEUE_DEPTH][PUS_MAX_PACKET_SIZE];
static size_t   s_lengths[PUS_TM_QUEUE_DEPTH];
static size_t   s_head;      /* index of the oldest packet          */
static size_t   s_count;     /* number of packets currently stored  */
static uint16_t s_overflows; /* packets dropped since the last init */

void tm_queue_init(void)
{
    s_head      = 0u;
    s_count     = 0u;
    s_overflows = 0u;
}

/**
 * @implements SWREQ-TMQ-010
 * @implements SWREQ-TMQ-020
 * @implements SWREQ-ROB-010
 *
 * On overflow the newest packet is dropped rather than the oldest: the
 * housekeeping and event reports already queued describe the onset of the
 * anomaly and are the ones the ground segment needs for diagnosis.
 */
pus_status_t tm_queue_push(const uint8_t *packet, size_t len)
{
    size_t tail;
    size_t i;

    if (packet == NULL) {
        return PUS_ERR_NULL;
    }
    if ((len == 0u) || (len > (size_t)PUS_MAX_PACKET_SIZE)) {
        return PUS_ERR_BAD_LENGTH;
    }
    if (s_count >= (size_t)PUS_TM_QUEUE_DEPTH) {
        if (s_overflows < 0xFFFFu) {
            s_overflows = (uint16_t)(s_overflows + 1u);
        }
        return PUS_ERR_QUEUE_FULL;
    }

    tail = (s_head + s_count) % (size_t)PUS_TM_QUEUE_DEPTH;
    for (i = 0u; i < len; ++i) {
        s_slots[tail][i] = packet[i];
    }
    s_lengths[tail] = len;
    s_count += 1u;

    return PUS_OK;
}

/** @implements SWREQ-TMQ-010 */
pus_status_t tm_queue_pop(uint8_t *buf, size_t cap, size_t *out_len)
{
    size_t len;
    size_t i;

    if ((buf == NULL) || (out_len == NULL)) {
        return PUS_ERR_NULL;
    }
    if (s_count == 0u) {
        return PUS_ERR_QUEUE_FULL; /* empty: nothing to hand over */
    }

    len = s_lengths[s_head];
    if (cap < len) {
        return PUS_ERR_BUFFER_TOO_SMALL;
    }

    for (i = 0u; i < len; ++i) {
        buf[i] = s_slots[s_head][i];
    }
    *out_len = len;

    s_head   = (s_head + 1u) % (size_t)PUS_TM_QUEUE_DEPTH;
    s_count -= 1u;

    return PUS_OK;
}

size_t tm_queue_count(void)
{
    return s_count;
}

uint16_t tm_queue_overflow_count(void)
{
    return s_overflows;
}
