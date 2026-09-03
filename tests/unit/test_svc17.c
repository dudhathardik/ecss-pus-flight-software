/**
 * @file    test_svc17.c
 * @brief   Unit tests for the connection test service.
 */
#include "minunity.h"
#include "test_support.h"

#include "pus/obc_app.h"
#include "pus/svc17_test.h"
#include "pus/tm_queue.h"

static uint8_t s_buf[PUS_MAX_PACKET_SIZE];

void setUp(void)
{
    obc_init();
    ts_tm_flush();
}

void tearDown(void) { }

/** @verifies SWREQ-SVC17-010 */
static void test_are_you_alive_is_answered_with_an_empty_report(void)
{
    ts_tm_t tm;

    TEST_ASSERT_EQUAL_INT(PUS_OK,
        ts_send_tc((uint8_t)PUS_SERVICE_TEST,
                   (uint8_t)PUS_ST17_ARE_YOU_ALIVE, NULL, 0u));

    TEST_ASSERT_TRUE(ts_tm_find((uint8_t)PUS_SERVICE_TEST,
                                (uint8_t)PUS_ST17_I_AM_ALIVE,
                                s_buf, sizeof(s_buf), &tm));
    TEST_ASSERT_EQUAL_size_t(0u, tm.data_len);
    TEST_ASSERT_EQUAL_size_t(21u, tm.total_len);
}

/** @verifies SWREQ-SVC17-020 */
static void test_application_data_is_not_accepted(void)
{
    const uint8_t payload[1] = { 0x00u };

    TEST_ASSERT_EQUAL_INT(PUS_ERR_BAD_LENGTH,
        ts_send_tc((uint8_t)PUS_SERVICE_TEST,
                   (uint8_t)PUS_ST17_ARE_YOU_ALIVE, payload, 1u));
}

/** @verifies SWREQ-SVC17-020 */
static void test_unknown_subtype_is_rejected(void)
{
    TEST_ASSERT_EQUAL_INT(PUS_ERR_UNKNOWN_SUBTYPE,
        ts_send_tc((uint8_t)PUS_SERVICE_TEST, 99u, NULL, 0u));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_UNKNOWN_SUBTYPE,
        ts_send_tc((uint8_t)PUS_SERVICE_TEST,
                   (uint8_t)PUS_ST17_I_AM_ALIVE, NULL, 0u));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_are_you_alive_is_answered_with_an_empty_report);
    RUN_TEST(test_application_data_is_not_accepted);
    RUN_TEST(test_unknown_subtype_is_rejected);
    return UNITY_END();
}
