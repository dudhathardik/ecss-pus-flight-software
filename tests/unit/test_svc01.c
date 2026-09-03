/**
 * @file    test_svc01.c
 * @brief   Unit tests for the request verification service.
 *
 * These cases check the contract the ground segment depends on: every
 * telecommand produces exactly the acknowledgements it asked for, and every
 * rejection is reported even when the packet was too damaged to carry usable
 * acknowledgement flags.
 */
#include "minunity.h"
#include "test_support.h"

#include "pus/obc_app.h"
#include "pus/pus_bytes.h"
#include "pus/svc01_verification.h"
#include "pus/svc17_test.h"
#include "pus/tm_queue.h"

static uint8_t s_buf[PUS_MAX_PACKET_SIZE];
static uint8_t s_tc[PUS_MAX_PACKET_SIZE];

void setUp(void)
{
    obc_init();
    ts_tm_flush();
}

void tearDown(void) { }

/**
 * @verifies SWREQ-SVC1-010
 * @verifies SWREQ-SVC1-030
 * @verifies SWREQ-SVC1-060
 */
static void test_reports_acceptance_then_completion_in_order(void)
{
    ts_tm_t tm;

    TEST_ASSERT_EQUAL_INT(PUS_OK,
        ts_send_tc((uint8_t)PUS_SERVICE_TEST,
                   (uint8_t)PUS_ST17_ARE_YOU_ALIVE, NULL, 0u));

    /* (1,1) acceptance, then the service response, then (1,7) completion. */
    TEST_ASSERT_TRUE(ts_tm_pop(s_buf, sizeof(s_buf), &tm));
    TEST_ASSERT_EQUAL_UINT8(1u, tm.service);
    TEST_ASSERT_EQUAL_UINT8(PUS_ST01_ACCEPTANCE_SUCCESS, tm.subtype);

    TEST_ASSERT_TRUE(ts_tm_pop(s_buf, sizeof(s_buf), &tm));
    TEST_ASSERT_EQUAL_UINT8(17u, tm.service);
    TEST_ASSERT_EQUAL_UINT8(PUS_ST17_I_AM_ALIVE, tm.subtype);

    TEST_ASSERT_TRUE(ts_tm_pop(s_buf, sizeof(s_buf), &tm));
    TEST_ASSERT_EQUAL_UINT8(1u, tm.service);
    TEST_ASSERT_EQUAL_UINT8(PUS_ST01_COMPLETION_SUCCESS, tm.subtype);

    TEST_ASSERT_EQUAL_size_t(0u, tm_queue_count());
}

/** @verifies SWREQ-SVC1-040 */
static void test_reports_echo_the_request_id(void)
{
    ts_tm_t  tm;
    size_t   len;
    uint32_t expected;

    len = ts_build_tc(s_tc, sizeof(s_tc), (uint8_t)PUS_SERVICE_TEST,
                      (uint8_t)PUS_ST17_ARE_YOU_ALIVE,
                      (uint8_t)PUS_ACK_ACCEPTANCE, NULL, 0u);
    expected = pus_get_u32(s_tc);

    TEST_ASSERT_EQUAL_INT(PUS_OK, obc_process_tc(s_tc, len));
    TEST_ASSERT_TRUE(ts_tm_find(1u, (uint8_t)PUS_ST01_ACCEPTANCE_SUCCESS,
                                s_buf, sizeof(s_buf), &tm));
    TEST_ASSERT_EQUAL_size_t(4u, tm.data_len);
    TEST_ASSERT_EQUAL_UINT32(expected, pus_get_u32(tm.data));
}

/** @verifies SWREQ-SVC1-010 */
static void test_no_acknowledgement_is_sent_when_none_was_requested(void)
{
    ts_tm_t tm;
    size_t  len;

    len = ts_build_tc(s_tc, sizeof(s_tc), (uint8_t)PUS_SERVICE_TEST,
                      (uint8_t)PUS_ST17_ARE_YOU_ALIVE, 0u, NULL, 0u);

    TEST_ASSERT_EQUAL_INT(PUS_OK, obc_process_tc(s_tc, len));

    /* Only the service response is downlinked. */
    TEST_ASSERT_EQUAL_size_t(1u, tm_queue_count());
    TEST_ASSERT_TRUE(ts_tm_pop(s_buf, sizeof(s_buf), &tm));
    TEST_ASSERT_EQUAL_UINT8(17u, tm.service);
}

/**
 * @verifies SWREQ-SVC1-020
 * @verifies SWREQ-SVC1-050
 * A corrupted packet never had trustworthy acknowledgement flags, so the
 * rejection is downlinked regardless of what those flags said.
 */
static void test_rejection_is_reported_even_without_acknowledgement_flags(void)
{
    ts_tm_t tm;
    size_t  len;

    len = ts_build_tc(s_tc, sizeof(s_tc), (uint8_t)PUS_SERVICE_TEST,
                      (uint8_t)PUS_ST17_ARE_YOU_ALIVE, 0u, NULL, 0u);
    ts_break_crc(s_tc, len);

    TEST_ASSERT_EQUAL_INT(PUS_ERR_BAD_CRC, obc_process_tc(s_tc, len));

    TEST_ASSERT_TRUE(ts_tm_find(1u, (uint8_t)PUS_ST01_ACCEPTANCE_FAILURE,
                                s_buf, sizeof(s_buf), &tm));
    TEST_ASSERT_EQUAL_size_t(6u, tm.data_len);
    TEST_ASSERT_EQUAL_UINT16((uint16_t)PUS_ERR_BAD_CRC, pus_get_u16(&tm.data[4]));
}

/**
 * @verifies SWREQ-SVC1-030
 * @verifies SWREQ-APP-100
 */
static void test_execution_failure_yields_a_completion_failure_report(void)
{
    ts_tm_t tm;

    /* Service 99 has no handler. */
    TEST_ASSERT_EQUAL_INT(PUS_ERR_UNKNOWN_SERVICE, ts_send_tc(99u, 1u, NULL, 0u));

    TEST_ASSERT_TRUE(ts_tm_find(1u, (uint8_t)PUS_ST01_ACCEPTANCE_SUCCESS,
                                s_buf, sizeof(s_buf), &tm));
    TEST_ASSERT_TRUE(ts_tm_find(1u, (uint8_t)PUS_ST01_COMPLETION_FAILURE,
                                s_buf, sizeof(s_buf), &tm));
    TEST_ASSERT_EQUAL_UINT16((uint16_t)PUS_ERR_UNKNOWN_SERVICE,
                             pus_get_u16(&tm.data[4]));
}

/**
 * @verifies SWREQ-SVC1-050
 * A runt packet carries no request ID, and the report must still go out.
 */
static void test_runt_packet_is_reported_with_a_zero_request_id(void)
{
    const uint8_t runt[2] = { 0x18u, 0x0Cu };
    ts_tm_t tm;

    TEST_ASSERT_EQUAL_INT(PUS_ERR_TRUNCATED, obc_process_tc(runt, sizeof(runt)));
    TEST_ASSERT_TRUE(ts_tm_find(1u, (uint8_t)PUS_ST01_ACCEPTANCE_FAILURE,
                                s_buf, sizeof(s_buf), &tm));
    TEST_ASSERT_EQUAL_UINT32(0u, pus_get_u32(tm.data));
    TEST_ASSERT_EQUAL_UINT16((uint16_t)PUS_ERR_TRUNCATED,
                             pus_get_u16(&tm.data[4]));
}

/** @verifies SWREQ-ROB-010 */
static void test_null_telecommand_is_rejected(void)
{
    TEST_ASSERT_EQUAL_INT(PUS_ERR_NULL, obc_process_tc(NULL, 13u));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_NULL, svc01_acceptance_success(NULL));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_NULL, svc01_completion_success(NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_reports_acceptance_then_completion_in_order);
    RUN_TEST(test_reports_echo_the_request_id);
    RUN_TEST(test_no_acknowledgement_is_sent_when_none_was_requested);
    RUN_TEST(test_rejection_is_reported_even_without_acknowledgement_flags);
    RUN_TEST(test_execution_failure_yields_a_completion_failure_report);
    RUN_TEST(test_runt_packet_is_reported_with_a_zero_request_id);
    RUN_TEST(test_null_telecommand_is_rejected);
    return UNITY_END();
}
