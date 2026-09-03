/**
 * @file    test_pus_tm.c
 * @brief   Unit tests for telemetry packet construction.
 */
#include "minunity.h"
#include "test_support.h"

#include "pus/crc16.h"
#include "pus/hal.h"
#include "pus/pus_bytes.h"
#include "pus/pus_tm.h"
#include "pus/tm_queue.h"

#define TM_EMPTY_LEN  21u   /* 6 primary + 13 secondary + 2 CRC */

static uint8_t s_buf[PUS_MAX_PACKET_SIZE];
static size_t  s_len;

void setUp(void)
{
    hal_time_init();
    tm_queue_init();
    pus_tm_reset();
    s_len = 0u;
}

void tearDown(void) { }

/** @verifies SWREQ-TM-010 */
static void test_builds_the_expected_secondary_header(void)
{
    TEST_ASSERT_EQUAL_INT(PUS_OK,
        pus_tm_build(s_buf, sizeof(s_buf), 17u, 2u, NULL, 0u, &s_len));

    TEST_ASSERT_EQUAL_size_t(TM_EMPTY_LEN, s_len);
    TEST_ASSERT_EQUAL_HEX16(0x20u, s_buf[6]);   /* PUS version 2, time ref 0 */
    TEST_ASSERT_EQUAL_UINT8(17u, s_buf[7]);
    TEST_ASSERT_EQUAL_UINT8(2u, s_buf[8]);
    TEST_ASSERT_EQUAL_UINT16(0u, pus_get_u16(&s_buf[9]));
    TEST_ASSERT_EQUAL_UINT16(PUS_TM_DESTINATION_ID, pus_get_u16(&s_buf[11]));
    /* Packet type bit must be 0 for telemetry. */
    TEST_ASSERT_EQUAL_UINT8(0u, (uint8_t)((s_buf[0] >> 4) & 0x01u));
}

/** @verifies SWREQ-TM-040 */
static void test_appends_a_valid_error_control_field(void)
{
    const uint8_t payload[4] = { 0xDEu, 0xADu, 0xBEu, 0xEFu };

    TEST_ASSERT_EQUAL_INT(PUS_OK,
        pus_tm_build(s_buf, sizeof(s_buf), 5u, 1u, payload, sizeof(payload),
                     &s_len));

    TEST_ASSERT_EQUAL_size_t(TM_EMPTY_LEN + 4u, s_len);
    TEST_ASSERT_EQUAL_HEX16(crc16_ccitt(s_buf, s_len - 2u),
                            pus_get_u16(&s_buf[s_len - 2u]));
    TEST_ASSERT_EQUAL_MEMORY(payload, &s_buf[19], 4u);
}

/** @verifies SWREQ-TM-010 */
static void test_declared_length_matches_the_octets_produced(void)
{
    const uint8_t payload[4] = { 1u, 2u, 3u, 4u };

    TEST_ASSERT_EQUAL_INT(PUS_OK,
        pus_tm_build(s_buf, sizeof(s_buf), 3u, 25u, payload, sizeof(payload),
                     &s_len));

    TEST_ASSERT_EQUAL_UINT16((uint16_t)(s_len - 7u), pus_get_u16(&s_buf[4]));
}

/** @verifies SWREQ-TM-020 */
static void test_sequence_count_increments_and_wraps(void)
{
    uint16_t first;
    uint16_t second;

    (void)pus_tm_build(s_buf, sizeof(s_buf), 17u, 2u, NULL, 0u, &s_len);
    first = (uint16_t)(pus_get_u16(&s_buf[2]) & 0x3FFFu);
    (void)pus_tm_build(s_buf, sizeof(s_buf), 17u, 2u, NULL, 0u, &s_len);
    second = (uint16_t)(pus_get_u16(&s_buf[2]) & 0x3FFFu);

    TEST_ASSERT_EQUAL_UINT16(0u, first);
    TEST_ASSERT_EQUAL_UINT16(1u, second);
    TEST_ASSERT_TRUE(second <= 0x3FFFu);
}

/** @verifies SWREQ-TM-030 */
static void test_message_counter_is_kept_per_service(void)
{
    (void)pus_tm_build(s_buf, sizeof(s_buf), 3u, 25u, NULL, 0u, &s_len);
    TEST_ASSERT_EQUAL_UINT16(0u, pus_get_u16(&s_buf[9]));

    (void)pus_tm_build(s_buf, sizeof(s_buf), 3u, 25u, NULL, 0u, &s_len);
    TEST_ASSERT_EQUAL_UINT16(1u, pus_get_u16(&s_buf[9]));

    /* A different service keeps its own counter. */
    (void)pus_tm_build(s_buf, sizeof(s_buf), 5u, 1u, NULL, 0u, &s_len);
    TEST_ASSERT_EQUAL_UINT16(0u, pus_get_u16(&s_buf[9]));
}

/** @verifies SWREQ-HAL-010 */
static void test_time_stamp_follows_the_tick_counter(void)
{
    uint32_t t0;
    uint32_t t1;
    int      i;

    (void)pus_tm_build(s_buf, sizeof(s_buf), 17u, 2u, NULL, 0u, &s_len);
    t0 = pus_get_u32(&s_buf[13]);

    for (i = 0; i < 10; ++i) {
        hal_time_advance();   /* 10 x 100 ms = 1 s */
    }

    (void)pus_tm_build(s_buf, sizeof(s_buf), 17u, 2u, NULL, 0u, &s_len);
    t1 = pus_get_u32(&s_buf[13]);

    TEST_ASSERT_EQUAL_UINT32(0u, t0);
    TEST_ASSERT_EQUAL_UINT32(1u, t1);
}

/** @verifies SWREQ-TM-050 */
static void test_refuses_to_overrun_the_destination_buffer(void)
{
    uint8_t payload[PUS_MAX_PACKET_SIZE];

    TEST_ASSERT_EQUAL_INT(PUS_ERR_BUFFER_TOO_SMALL,
        pus_tm_build(s_buf, TM_EMPTY_LEN - 1u, 17u, 2u, NULL, 0u, &s_len));

    TEST_ASSERT_EQUAL_INT(PUS_ERR_BUFFER_TOO_SMALL,
        pus_tm_build(s_buf, sizeof(s_buf), 3u, 25u, payload, sizeof(payload),
                     &s_len));
}

/** @verifies SWREQ-ROB-010 */
static void test_rejects_null_arguments(void)
{
    const uint8_t payload[1] = { 0u };

    TEST_ASSERT_EQUAL_INT(PUS_ERR_NULL,
        pus_tm_build(NULL, sizeof(s_buf), 17u, 2u, NULL, 0u, &s_len));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_NULL,
        pus_tm_build(s_buf, sizeof(s_buf), 17u, 2u, payload, 1u, NULL));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_NULL,
        pus_tm_build(s_buf, sizeof(s_buf), 17u, 2u, NULL, 1u, &s_len));
}

/** @verifies SWREQ-TM-060 */
static void test_send_places_the_packet_on_the_downlink_queue(void)
{
    ts_tm_t tm;

    TEST_ASSERT_EQUAL_size_t(0u, tm_queue_count());
    TEST_ASSERT_EQUAL_INT(PUS_OK, pus_tm_send(17u, 2u, NULL, 0u));
    TEST_ASSERT_EQUAL_size_t(1u, tm_queue_count());

    TEST_ASSERT_TRUE(ts_tm_pop(s_buf, sizeof(s_buf), &tm));
    TEST_ASSERT_EQUAL_UINT8(17u, tm.service);
    TEST_ASSERT_EQUAL_UINT8(2u, tm.subtype);
    TEST_ASSERT_EQUAL_size_t(0u, tm.data_len);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_builds_the_expected_secondary_header);
    RUN_TEST(test_appends_a_valid_error_control_field);
    RUN_TEST(test_declared_length_matches_the_octets_produced);
    RUN_TEST(test_sequence_count_increments_and_wraps);
    RUN_TEST(test_message_counter_is_kept_per_service);
    RUN_TEST(test_time_stamp_follows_the_tick_counter);
    RUN_TEST(test_refuses_to_overrun_the_destination_buffer);
    RUN_TEST(test_rejects_null_arguments);
    RUN_TEST(test_send_places_the_packet_on_the_downlink_queue);
    return UNITY_END();
}
