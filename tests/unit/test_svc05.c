/**
 * @file    test_svc05.c
 * @brief   Unit tests for event reporting.
 */
#include "minunity.h"
#include "test_support.h"

#include "pus/obc_app.h"
#include "pus/pus_bytes.h"
#include "pus/svc05_event.h"
#include "pus/tm_queue.h"

static uint8_t s_buf[PUS_MAX_PACKET_SIZE];

void setUp(void)
{
    obc_init();
    ts_tm_flush();
}

void tearDown(void) { }

/** Build the application data of a (5,5) / (5,6) telecommand. */
static size_t event_list(uint8_t *out, const uint16_t *ids, uint8_t count)
{
    uint8_t i;

    out[0] = count;
    for (i = 0u; i < count; ++i) {
        pus_put_u16(&out[1u + ((size_t)i * 2u)], ids[i]);
    }
    return (size_t)(1u + ((size_t)count * 2u));
}

/** @verifies SWREQ-SVC5-010 */
static void test_report_carries_the_identifier_and_auxiliary_word(void)
{
    ts_tm_t tm;

    TEST_ASSERT_EQUAL_INT(PUS_OK,
        svc05_report(OBC_EVT_TEMP_ALARM, (uint8_t)PUS_ST05_HIGH_SEVERITY, 0x1234u));

    TEST_ASSERT_TRUE(ts_tm_pop(s_buf, sizeof(s_buf), &tm));
    TEST_ASSERT_EQUAL_UINT8(5u, tm.service);
    TEST_ASSERT_EQUAL_UINT8(PUS_ST05_HIGH_SEVERITY, tm.subtype);
    TEST_ASSERT_EQUAL_size_t(4u, tm.data_len);
    TEST_ASSERT_EQUAL_UINT16((uint16_t)OBC_EVT_TEMP_ALARM, pus_get_u16(&tm.data[0]));
    TEST_ASSERT_EQUAL_UINT16(0x1234u, pus_get_u16(&tm.data[2]));
}

/** @verifies SWREQ-SVC5-010 */
static void test_severity_selects_the_message_subtype(void)
{
    ts_tm_t tm;

    (void)svc05_report(OBC_EVT_BOOT_COMPLETED, (uint8_t)PUS_ST05_INFORMATIVE, 0u);
    TEST_ASSERT_TRUE(ts_tm_pop(s_buf, sizeof(s_buf), &tm));
    TEST_ASSERT_EQUAL_UINT8(1u, tm.subtype);

    (void)svc05_report(OBC_EVT_TC_REJECTED, (uint8_t)PUS_ST05_LOW_SEVERITY, 0u);
    TEST_ASSERT_TRUE(ts_tm_pop(s_buf, sizeof(s_buf), &tm));
    TEST_ASSERT_EQUAL_UINT8(2u, tm.subtype);
}

/** @verifies SWREQ-SVC5-040 */
static void test_unknown_identifier_and_severity_are_rejected(void)
{
    TEST_ASSERT_EQUAL_INT(PUS_ERR_UNKNOWN_EVENT,
        svc05_report((obc_event_id_t)0, (uint8_t)PUS_ST05_INFORMATIVE, 0u));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_UNKNOWN_EVENT,
        svc05_report((obc_event_id_t)(OBC_EVENT_MAX_IDS + 1), 1u, 0u));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_BAD_PARAMETER,
        svc05_report(OBC_EVT_BOOT_COMPLETED, 0u, 0u));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_BAD_PARAMETER,
        svc05_report(OBC_EVT_BOOT_COMPLETED, 5u, 0u));
    TEST_ASSERT_EQUAL_size_t(0u, tm_queue_count());
}

/** @verifies SWREQ-SVC5-020 */
/** @verifies SWREQ-SVC5-030 */
static void test_a_disabled_event_produces_no_telemetry(void)
{
    const uint16_t ids[1] = { (uint16_t)OBC_EVT_TEMP_ALARM };
    uint8_t app[8];
    size_t  n;

    n = event_list(app, ids, 1u);
    TEST_ASSERT_EQUAL_INT(PUS_OK,
        ts_send_tc(5u, (uint8_t)PUS_ST05_DISABLE, app, n));
    ts_tm_flush();

    TEST_ASSERT_FALSE(svc05_is_enabled(OBC_EVT_TEMP_ALARM));
    /* Suppression is nominal behaviour, not an error. */
    TEST_ASSERT_EQUAL_INT(PUS_OK,
        svc05_report(OBC_EVT_TEMP_ALARM, (uint8_t)PUS_ST05_HIGH_SEVERITY, 0u));
    TEST_ASSERT_EQUAL_size_t(0u, tm_queue_count());

    /* Other identifiers are unaffected. */
    TEST_ASSERT_TRUE(svc05_is_enabled(OBC_EVT_BOOT_COMPLETED));
}

/** @verifies SWREQ-SVC5-020 */
static void test_reporting_can_be_re_enabled(void)
{
    const uint16_t ids[1] = { (uint16_t)OBC_EVT_TEMP_ALARM };
    uint8_t app[8];
    size_t  n;

    n = event_list(app, ids, 1u);
    (void)ts_send_tc(5u, (uint8_t)PUS_ST05_DISABLE, app, n);
    (void)ts_send_tc(5u, (uint8_t)PUS_ST05_ENABLE, app, n);
    ts_tm_flush();

    TEST_ASSERT_TRUE(svc05_is_enabled(OBC_EVT_TEMP_ALARM));
    TEST_ASSERT_EQUAL_INT(PUS_OK,
        svc05_report(OBC_EVT_TEMP_ALARM, (uint8_t)PUS_ST05_HIGH_SEVERITY, 0u));
    TEST_ASSERT_EQUAL_size_t(1u, tm_queue_count());
}

/** @verifies SWREQ-SVC5-040 */
static void test_a_list_with_one_bad_identifier_changes_nothing(void)
{
    const uint16_t ids[2] = { (uint16_t)OBC_EVT_BOOT_COMPLETED, 0x00FFu };
    uint8_t app[8];
    size_t  n;

    n = event_list(app, ids, 2u);
    TEST_ASSERT_EQUAL_INT(PUS_ERR_UNKNOWN_EVENT,
        ts_send_tc(5u, (uint8_t)PUS_ST05_DISABLE, app, n));
    TEST_ASSERT_TRUE(svc05_is_enabled(OBC_EVT_BOOT_COMPLETED));
}

/** @verifies SWREQ-ROB-020 */
static void test_malformed_service_5_telecommands_are_rejected(void)
{
    const uint8_t empty[1] = { 0u };
    const uint8_t short_list[2] = { 1u, 0u }; /* one identifier promised, one octet given */

    TEST_ASSERT_EQUAL_INT(PUS_ERR_BAD_LENGTH,
        ts_send_tc(5u, (uint8_t)PUS_ST05_DISABLE, NULL, 0u));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_BAD_PARAMETER,
        ts_send_tc(5u, (uint8_t)PUS_ST05_DISABLE, empty, 1u));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_BAD_LENGTH,
        ts_send_tc(5u, (uint8_t)PUS_ST05_DISABLE, short_list, 2u));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_UNKNOWN_SUBTYPE, ts_send_tc(5u, 99u, NULL, 0u));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_NULL, svc05_handle(NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_report_carries_the_identifier_and_auxiliary_word);
    RUN_TEST(test_severity_selects_the_message_subtype);
    RUN_TEST(test_unknown_identifier_and_severity_are_rejected);
    RUN_TEST(test_a_disabled_event_produces_no_telemetry);
    RUN_TEST(test_reporting_can_be_re_enabled);
    RUN_TEST(test_a_list_with_one_bad_identifier_changes_nothing);
    RUN_TEST(test_malformed_service_5_telecommands_are_rejected);
    return UNITY_END();
}
