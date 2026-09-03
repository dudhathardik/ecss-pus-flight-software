/**
 * @file    test_tm_queue.c
 * @brief   Unit tests for the downlink FIFO, including its overflow behaviour.
 */
#include "minunity.h"

#include "pus/tm_queue.h"

static uint8_t s_out[PUS_MAX_PACKET_SIZE];

void setUp(void)
{
    tm_queue_init();
}

void tearDown(void) { }

static void push_marked(uint8_t marker)
{
    uint8_t packet[4];

    packet[0] = marker;
    packet[1] = 0u;
    packet[2] = 0u;
    packet[3] = 0u;
    (void)tm_queue_push(packet, sizeof(packet));
}

/** @verifies SWREQ-TMQ-010 */
static void test_packets_leave_in_the_order_they_arrived(void)
{
    size_t len = 0u;
    uint8_t i;

    for (i = 1u; i <= 5u; ++i) {
        push_marked(i);
    }
    TEST_ASSERT_EQUAL_size_t(5u, tm_queue_count());

    for (i = 1u; i <= 5u; ++i) {
        TEST_ASSERT_EQUAL_INT(PUS_OK, tm_queue_pop(s_out, sizeof(s_out), &len));
        TEST_ASSERT_EQUAL_size_t(4u, len);
        TEST_ASSERT_EQUAL_UINT8(i, s_out[0]);
    }
    TEST_ASSERT_EQUAL_size_t(0u, tm_queue_count());
}

/** @verifies SWREQ-TMQ-010 */
static void test_indices_wrap_around_the_ring(void)
{
    size_t len = 0u;
    uint8_t i;

    /* Two full laps of the ring, popping as we go. */
    for (i = 0u; i < (uint8_t)(2u * PUS_TM_QUEUE_DEPTH); ++i) {
        push_marked(i);
        TEST_ASSERT_EQUAL_INT(PUS_OK, tm_queue_pop(s_out, sizeof(s_out), &len));
        TEST_ASSERT_EQUAL_UINT8(i, s_out[0]);
    }
    TEST_ASSERT_EQUAL_UINT16(0u, tm_queue_overflow_count());
}

/** @verifies SWREQ-TMQ-020 */
static void test_drops_the_newest_packet_when_full_and_counts_it(void)
{
    size_t len = 0u;
    uint8_t i;

    for (i = 0u; i < (uint8_t)PUS_TM_QUEUE_DEPTH; ++i) {
        push_marked(i);
    }
    TEST_ASSERT_EQUAL_size_t(PUS_TM_QUEUE_DEPTH, tm_queue_count());
    TEST_ASSERT_EQUAL_UINT16(0u, tm_queue_overflow_count());

    push_marked(0xFFu);
    TEST_ASSERT_EQUAL_size_t(PUS_TM_QUEUE_DEPTH, tm_queue_count());
    TEST_ASSERT_EQUAL_UINT16(1u, tm_queue_overflow_count());

    push_marked(0xFEu);
    TEST_ASSERT_EQUAL_UINT16(2u, tm_queue_overflow_count());

    /* The oldest packet survived: history is preserved, not the newest data. */
    TEST_ASSERT_EQUAL_INT(PUS_OK, tm_queue_pop(s_out, sizeof(s_out), &len));
    TEST_ASSERT_EQUAL_UINT8(0u, s_out[0]);
}

/** @verifies SWREQ-TMQ-020 */
static void test_init_clears_the_overflow_counter(void)
{
    uint8_t i;

    for (i = 0u; i < (uint8_t)(PUS_TM_QUEUE_DEPTH + 2u); ++i) {
        push_marked(i);
    }
    TEST_ASSERT_EQUAL_UINT16(2u, tm_queue_overflow_count());

    tm_queue_init();
    TEST_ASSERT_EQUAL_UINT16(0u, tm_queue_overflow_count());
    TEST_ASSERT_EQUAL_size_t(0u, tm_queue_count());
}

/** @verifies SWREQ-ROB-010 */
static void test_rejects_invalid_pushes_and_pops(void)
{
    uint8_t oversized[PUS_MAX_PACKET_SIZE + 1u];
    uint8_t packet[4] = { 1u, 2u, 3u, 4u };
    size_t  len = 0u;

    TEST_ASSERT_EQUAL_INT(PUS_ERR_NULL, tm_queue_push(NULL, 4u));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_BAD_LENGTH, tm_queue_push(packet, 0u));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_BAD_LENGTH,
                          tm_queue_push(oversized, sizeof(oversized)));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_NULL, tm_queue_pop(NULL, 4u, &len));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_NULL, tm_queue_pop(s_out, 4u, NULL));

    /* Popping an empty queue is not an error the caller has to handle
       differently from "nothing to send". */
    TEST_ASSERT_EQUAL_INT(PUS_ERR_QUEUE_FULL,
                          tm_queue_pop(s_out, sizeof(s_out), &len));

    (void)tm_queue_push(packet, sizeof(packet));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_BUFFER_TOO_SMALL,
                          tm_queue_pop(s_out, 2u, &len));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_packets_leave_in_the_order_they_arrived);
    RUN_TEST(test_indices_wrap_around_the_ring);
    RUN_TEST(test_drops_the_newest_packet_when_full_and_counts_it);
    RUN_TEST(test_init_clears_the_overflow_counter);
    RUN_TEST(test_rejects_invalid_pushes_and_pops);
    return UNITY_END();
}
