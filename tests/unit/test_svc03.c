/**
 * @file    test_svc03.c
 * @brief   Unit tests for housekeeping reporting.
 */
#include "minunity.h"
#include "test_support.h"

#include "pus/hal.h"
#include "pus/obc_app.h"
#include "pus/pus_bytes.h"
#include "pus/svc03_housekeeping.h"
#include "pus/tm_queue.h"

static uint8_t s_buf[PUS_MAX_PACKET_SIZE];

void setUp(void)
{
    obc_init();
    ts_tm_flush();
}

void tearDown(void) { }

/**
 * @verifies SWREQ-SVC3-010
 * @verifies SWREQ-SVC3-070
 */
static void test_only_the_essential_report_is_enabled_at_power_on(void)
{
    TEST_ASSERT_TRUE(svc03_is_enabled((uint8_t)OBC_HK_SID_ESSENTIAL));
    TEST_ASSERT_FALSE(svc03_is_enabled((uint8_t)OBC_HK_SID_DIAGNOSTIC));
    TEST_ASSERT_FALSE(svc03_is_enabled(0u));
    TEST_ASSERT_FALSE(svc03_is_enabled(99u));
}

/** @verifies SWREQ-SVC3-020 */
static void test_essential_report_carries_the_documented_layout(void)
{
    const obc_state_t *st;
    ts_tm_t tm;

    hal_inject_temperature(123);
    obc_tick();
    ts_tm_flush();
    st = obc_get_state();

    TEST_ASSERT_EQUAL_INT(PUS_OK, svc03_generate((uint8_t)OBC_HK_SID_ESSENTIAL));
    TEST_ASSERT_TRUE(ts_tm_find(3u, (uint8_t)PUS_ST03_REPORT,
                                s_buf, sizeof(s_buf), &tm));

    TEST_ASSERT_EQUAL_size_t(9u, tm.data_len);
    TEST_ASSERT_EQUAL_UINT8(OBC_HK_SID_ESSENTIAL, pus_get_u8(&tm.data[0]));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)st->mode, pus_get_u8(&tm.data[1]));
    TEST_ASSERT_EQUAL_UINT32(st->uptime_ticks, pus_get_u32(&tm.data[2]));
    TEST_ASSERT_EQUAL_INT(123, pus_get_i16(&tm.data[6]));
    TEST_ASSERT_EQUAL_UINT8(st->heater_on, pus_get_u8(&tm.data[8]));
}

/** @verifies SWREQ-SVC3-020 */
static void test_essential_report_carries_a_negative_temperature(void)
{
    ts_tm_t tm;

    hal_inject_temperature(-155); /* -15.5 degC */
    obc_tick();
    ts_tm_flush();

    TEST_ASSERT_EQUAL_INT(PUS_OK, svc03_generate((uint8_t)OBC_HK_SID_ESSENTIAL));
    TEST_ASSERT_TRUE(ts_tm_find(3u, (uint8_t)PUS_ST03_REPORT,
                                s_buf, sizeof(s_buf), &tm));
    TEST_ASSERT_EQUAL_INT(-155, pus_get_i16(&tm.data[6]));
}

/** @verifies SWREQ-SVC3-020 */
static void test_diagnostic_report_counts_telecommands(void)
{
    ts_tm_t tm;

    (void)ts_send_tc(17u, 1u, NULL, 0u);          /* accepted */
    (void)obc_process_tc((const uint8_t *)"xx", 2u); /* rejected: truncated */
    ts_tm_flush();

    TEST_ASSERT_EQUAL_INT(PUS_OK, svc03_generate((uint8_t)OBC_HK_SID_DIAGNOSTIC));
    TEST_ASSERT_TRUE(ts_tm_find(3u, (uint8_t)PUS_ST03_REPORT,
                                s_buf, sizeof(s_buf), &tm));

    TEST_ASSERT_EQUAL_size_t(10u, tm.data_len);
    TEST_ASSERT_EQUAL_UINT8(OBC_HK_SID_DIAGNOSTIC, pus_get_u8(&tm.data[0]));
    TEST_ASSERT_EQUAL_UINT16(1u, pus_get_u16(&tm.data[1]));
    TEST_ASSERT_EQUAL_UINT16(1u, pus_get_u16(&tm.data[3]));
    TEST_ASSERT_EQUAL_UINT16((uint16_t)PUS_ERR_TRUNCATED,
                             pus_get_u16(&tm.data[5]));
}

/** @verifies SWREQ-SVC3-060 */
static void test_unknown_structure_identifier_is_rejected(void)
{
    const uint8_t list[2] = { 1u, 9u };

    TEST_ASSERT_EQUAL_INT(PUS_ERR_UNKNOWN_SID, svc03_generate(0u));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_UNKNOWN_SID, svc03_generate(9u));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_UNKNOWN_SID,
                          ts_send_tc(3u, (uint8_t)PUS_ST03_ENABLE,
                                     list, sizeof(list)));
}

/**
 * @verifies SWREQ-SVC3-040
 * A list containing one bad identifier must leave the configuration alone
 * rather than applying the valid entries first.
 */
static void test_a_rejected_list_changes_nothing(void)
{
    const uint8_t list[3] = { 2u, 2u, 9u };

    TEST_ASSERT_FALSE(svc03_is_enabled((uint8_t)OBC_HK_SID_DIAGNOSTIC));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_UNKNOWN_SID,
                          ts_send_tc(3u, (uint8_t)PUS_ST03_ENABLE,
                                     list, sizeof(list)));
    TEST_ASSERT_FALSE(svc03_is_enabled((uint8_t)OBC_HK_SID_DIAGNOSTIC));
}

/** @verifies SWREQ-SVC3-040 */
static void test_enable_and_disable_periodic_generation(void)
{
    const uint8_t both[3] = { 2u, 1u, 2u };
    const uint8_t one[2]  = { 1u, 1u };

    TEST_ASSERT_EQUAL_INT(PUS_OK,
        ts_send_tc(3u, (uint8_t)PUS_ST03_ENABLE, both, sizeof(both)));
    TEST_ASSERT_TRUE(svc03_is_enabled(1u));
    TEST_ASSERT_TRUE(svc03_is_enabled(2u));

    TEST_ASSERT_EQUAL_INT(PUS_OK,
        ts_send_tc(3u, (uint8_t)PUS_ST03_DISABLE, one, sizeof(one)));
    TEST_ASSERT_FALSE(svc03_is_enabled(1u));
    TEST_ASSERT_TRUE(svc03_is_enabled(2u));
}

/** @verifies SWREQ-SVC3-050 */
static void test_one_shot_generation_ignores_the_enable_flag(void)
{
    const uint8_t list[2] = { 1u, 2u };
    ts_tm_t tm;
    int     reports = 0;

    TEST_ASSERT_FALSE(svc03_is_enabled(2u));
    TEST_ASSERT_EQUAL_INT(PUS_OK,
        ts_send_tc(3u, (uint8_t)PUS_ST03_ONE_SHOT, list, sizeof(list)));

    while (ts_tm_pop(s_buf, sizeof(s_buf), &tm) == PUS_TRUE) {
        if ((tm.service == 3u) && (tm.subtype == (uint8_t)PUS_ST03_REPORT)) {
            reports++;
        }
    }
    TEST_ASSERT_EQUAL_INT(1, reports);

    /* A one-shot request must not change the periodic configuration. */
    TEST_ASSERT_FALSE(svc03_is_enabled(2u));
}

/** @verifies SWREQ-SVC3-030 */
static void test_periodic_report_appears_once_per_period(void)
{
    ts_tm_t tm;
    unsigned i;
    int      reports = 0;

    for (i = 0u; i < (unsigned)OBC_HK_PERIOD_TICKS - 1u; ++i) {
        obc_tick();
    }
    TEST_ASSERT_EQUAL_size_t(0u, tm_queue_count());

    obc_tick();
    while (ts_tm_pop(s_buf, sizeof(s_buf), &tm) == PUS_TRUE) {
        if ((tm.service == 3u) && (tm.subtype == (uint8_t)PUS_ST03_REPORT)) {
            reports++;
            TEST_ASSERT_EQUAL_UINT8(OBC_HK_SID_ESSENTIAL, tm.data[0]);
        }
    }
    TEST_ASSERT_EQUAL_INT(1, reports);
}

/** @verifies SWREQ-ROB-020 */
static void test_malformed_service_3_telecommands_are_rejected(void)
{
    const uint8_t empty_list[1] = { 0u };
    const uint8_t short_list[2] = { 2u, 1u }; /* claims 2 identifiers, sends 1 */

    TEST_ASSERT_EQUAL_INT(PUS_ERR_BAD_LENGTH,
        ts_send_tc(3u, (uint8_t)PUS_ST03_ENABLE, NULL, 0u));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_BAD_PARAMETER,
        ts_send_tc(3u, (uint8_t)PUS_ST03_ENABLE, empty_list, 1u));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_BAD_LENGTH,
        ts_send_tc(3u, (uint8_t)PUS_ST03_ENABLE, short_list, 2u));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_UNKNOWN_SUBTYPE,
        ts_send_tc(3u, 99u, NULL, 0u));
    TEST_ASSERT_EQUAL_INT(PUS_ERR_NULL, svc03_handle(NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_only_the_essential_report_is_enabled_at_power_on);
    RUN_TEST(test_essential_report_carries_the_documented_layout);
    RUN_TEST(test_essential_report_carries_a_negative_temperature);
    RUN_TEST(test_diagnostic_report_counts_telecommands);
    RUN_TEST(test_unknown_structure_identifier_is_rejected);
    RUN_TEST(test_a_rejected_list_changes_nothing);
    RUN_TEST(test_enable_and_disable_periodic_generation);
    RUN_TEST(test_one_shot_generation_ignores_the_enable_flag);
    RUN_TEST(test_periodic_report_appears_once_per_period);
    RUN_TEST(test_malformed_service_3_telecommands_are_rejected);
    return UNITY_END();
}
