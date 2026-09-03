/**
 * @file    test_defensive.c
 * @brief   Coverage of the defensive paths of every module.
 *
 * The null-pointer guards required by SWREQ-ROB-010 are exactly the code a
 * suite driven only by nominal scenarios never reaches, and exactly the code
 * that has to work the one time it is needed. They get their own test file so
 * that the statement coverage figure reported to product assurance is not
 * quietly propped up by unreachable branches.
 */
#include "minunity.h"
#include "test_support.h"

#include "pus/ccsds.h"
#include "pus/crc16.h"
#include "pus/hal.h"
#include "pus/obc_app.h"
#include "pus/pus_tm.h"
#include "pus/svc01_verification.h"
#include "pus/svc03_housekeeping.h"
#include "pus/svc05_event.h"
#include "pus/svc08_function.h"
#include "pus/svc17_test.h"
#include "pus/tm_queue.h"

void setUp(void)
{
    obc_init();
    ts_tm_flush();
}

void tearDown(void) { }

/** @verifies SWREQ-ROB-010 */
static void test_every_service_handler_rejects_a_null_telecommand(void)
{
    TEST_ASSERT_EQUAL_INT(PUS_ERR_NULL, svc03_handle(NULL));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_NULL, svc05_handle(NULL));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_NULL, svc08_handle(NULL));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_NULL, svc17_handle(NULL));
    TEST_ASSERT_EQUAL_size_t(0u, tm_queue_count());
}

/** @verifies SWREQ-ROB-010 */
static void test_verification_service_rejects_a_null_telecommand(void)
{
    TEST_ASSERT_EQUAL_INT(PUS_ERR_NULL, svc01_acceptance_success(NULL));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_NULL, svc01_completion_success(NULL));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_NULL, svc01_completion_failure(NULL, PUS_OK));
    TEST_ASSERT_EQUAL_size_t(0u, tm_queue_count());
}

/**
 * @verifies SWREQ-SVC1-030
 * A telecommand that asked for no completion acknowledgement must stay silent
 * on failure too, not only on success.
 */
static void test_completion_failure_honours_the_acknowledgement_flags(void)
{
    uint8_t  tc[PUS_MAX_PACKET_SIZE];
    size_t   len;
    pus_tc_t parsed;

    len = ts_build_tc(tc, sizeof(tc), 17u, 1u, 0u, NULL, 0u);
    TEST_ASSERT_EQUAL_INT(PUS_OK, pus_tc_parse(tc, len, &parsed));

    TEST_ASSERT_EQUAL_INT(PUS_OK,
        svc01_completion_failure(&parsed, PUS_ERR_BAD_PARAMETER));
    TEST_ASSERT_EQUAL_size_t(0u, tm_queue_count());
}

/** @verifies SWREQ-SVC5-040 */
static void test_event_status_of_an_undefined_identifier_is_disabled(void)
{
    TEST_ASSERT_FALSE(svc05_is_enabled((obc_event_id_t)0));
    TEST_ASSERT_FALSE(svc05_is_enabled((obc_event_id_t)(OBC_EVENT_MAX_IDS + 1)));
    TEST_ASSERT_TRUE(svc05_is_enabled(OBC_EVT_BOOT_COMPLETED));
}

/** @verifies SWREQ-ROB-010 */
static void test_time_accessor_tolerates_null_arguments(void)
{
    uint32_t coarse = 0xDEADBEEFu;
    uint16_t fine   = 0xBEEFu;

    hal_get_cuc_time(NULL, &fine);
    hal_get_cuc_time(&coarse, NULL);

    /* Nothing was written and nothing crashed. */
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEFu, coarse);
    TEST_ASSERT_EQUAL_UINT16(0xBEEFu, fine);
}

/** @verifies SWREQ-TM-050 */
static void test_send_reports_a_packet_that_cannot_be_built(void)
{
    uint8_t oversized[PUS_MAX_PACKET_SIZE];

    TEST_ASSERT_EQUAL_INT(PUS_ERR_BUFFER_TOO_SMALL,
                          pus_tm_send(17u, 2u, oversized, sizeof(oversized)));
    TEST_ASSERT_EQUAL_size_t(0u, tm_queue_count());
}

/** @verifies SWREQ-TMQ-020 */
static void test_send_reports_a_full_downlink_queue(void)
{
    unsigned i;

    for (i = 0u; i < (unsigned)PUS_TM_QUEUE_DEPTH; ++i) {
        TEST_ASSERT_EQUAL_INT(PUS_OK, pus_tm_send(17u, 2u, NULL, 0u));
    }
    TEST_ASSERT_EQUAL_INT(PUS_ERR_QUEUE_FULL, pus_tm_send(17u, 2u, NULL, 0u));
    TEST_ASSERT_EQUAL_UINT16(1u, tm_queue_overflow_count());
}

/** @verifies SWREQ-SVC5-030 */
static void test_a_suppressed_event_reports_no_error_to_its_caller(void)
{
    const uint16_t ids[1] = { (uint16_t)OBC_EVT_HEATER_SWITCHED };
    uint8_t app[4];

    app[0] = 1u;
    app[1] = (uint8_t)(ids[0] >> 8);
    app[2] = (uint8_t)(ids[0] & 0xFFu);

    TEST_ASSERT_EQUAL_INT(PUS_OK, ts_send_tc(5u, 5u, app, 3u));
    ts_tm_flush();

    /* The control law does not care whether the event went out. */
    (void)obc_set_mode(OBC_MODE_NOMINAL);
    hal_inject_temperature(0);
    obc_tick();

    TEST_ASSERT_TRUE(obc_get_state()->heater_on);
    TEST_ASSERT_FALSE(svc05_is_enabled(OBC_EVT_HEATER_SWITCHED));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_every_service_handler_rejects_a_null_telecommand);
    RUN_TEST(test_verification_service_rejects_a_null_telecommand);
    RUN_TEST(test_completion_failure_honours_the_acknowledgement_flags);
    RUN_TEST(test_event_status_of_an_undefined_identifier_is_disabled);
    RUN_TEST(test_time_accessor_tolerates_null_arguments);
    RUN_TEST(test_send_reports_a_packet_that_cannot_be_built);
    RUN_TEST(test_send_reports_a_full_downlink_queue);
    RUN_TEST(test_a_suppressed_event_reports_no_error_to_its_caller);
    return UNITY_END();
}
