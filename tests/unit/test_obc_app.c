/**
 * @file    test_obc_app.c
 * @brief   Unit tests for mode management and autonomous thermal control.
 *
 * The thermal law is specified with two switching thresholds and an alarm
 * threshold, so the cases below are chosen by boundary value analysis: for
 * every threshold T the values T-1, T and T+1 are exercised, in both
 * directions of travel, plus the hysteresis band in between.
 */
#include "minunity.h"
#include "test_support.h"

#include "pus/hal.h"
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

/** Apply a temperature and run one minor cycle. */
static void step_at(int16_t temp_dc)
{
    hal_inject_temperature(temp_dc);
    obc_tick();
}

/** @verifies SWREQ-APP-010 */
static void test_power_on_state(void)
{
    const obc_state_t *st = obc_get_state();

    TEST_ASSERT_EQUAL_INT(OBC_MODE_SAFE, st->mode);
    TEST_ASSERT_EQUAL_UINT32(0u, st->uptime_ticks);
    TEST_ASSERT_EQUAL_UINT16(0u, st->tc_accepted);
    TEST_ASSERT_EQUAL_UINT16(0u, st->tc_rejected);
    TEST_ASSERT_FALSE(st->heater_on);
}

/** @verifies SWREQ-APP-020 */
static void test_boot_event_is_reported(void)
{
    ts_tm_t tm;

    obc_init(); /* setUp flushed the queue, so start again */
    TEST_ASSERT_TRUE(ts_tm_find(5u, (uint8_t)PUS_ST05_INFORMATIVE,
                                s_buf, sizeof(s_buf), &tm));
    TEST_ASSERT_EQUAL_UINT16((uint16_t)OBC_EVT_BOOT_COMPLETED,
                             pus_get_u16(tm.data));
}

/** @verifies SWREQ-APP-110 */
static void test_uptime_counts_minor_cycles(void)
{
    int i;

    for (i = 0; i < 5; ++i) {
        obc_tick();
    }
    TEST_ASSERT_EQUAL_UINT32(5u, obc_get_state()->uptime_ticks);
}

/** @verifies SWREQ-APP-030 */
static void test_mode_change_is_reported_once(void)
{
    ts_tm_t tm;

    TEST_ASSERT_EQUAL_INT(PUS_OK, obc_set_mode(OBC_MODE_NOMINAL));
    TEST_ASSERT_TRUE(ts_tm_find(5u, (uint8_t)PUS_ST05_INFORMATIVE,
                                s_buf, sizeof(s_buf), &tm));
    TEST_ASSERT_EQUAL_UINT16((uint16_t)OBC_EVT_MODE_CHANGED, pus_get_u16(tm.data));
    TEST_ASSERT_EQUAL_UINT16((uint16_t)OBC_MODE_NOMINAL, pus_get_u16(&tm.data[2]));

    /* Commanding the mode that is already active is a no-operation. */
    ts_tm_flush();
    TEST_ASSERT_EQUAL_INT(PUS_OK, obc_set_mode(OBC_MODE_NOMINAL));
    TEST_ASSERT_EQUAL_size_t(0u, tm_queue_count());
}

/** @verifies SWREQ-APP-030 */
static void test_undefined_mode_is_refused(void)
{
    TEST_ASSERT_EQUAL_INT(PUS_ERR_BAD_PARAMETER, obc_set_mode((obc_mode_t)7));
    TEST_ASSERT_EQUAL_INT(OBC_MODE_SAFE, obc_get_state()->mode);
}

/** @verifies SWREQ-APP-080 */
static void test_heater_stays_off_in_safe_mode_however_cold_it_gets(void)
{
    step_at(-400);
    TEST_ASSERT_FALSE(obc_get_state()->heater_on);
    step_at(OBC_TEMP_HEATER_ON_DC - 1);
    TEST_ASSERT_FALSE(obc_get_state()->heater_on);
}

/**
 * @verifies SWREQ-APP-040
 * Boundary values around the switch-on threshold, approached from above.
 */
static void test_heater_switches_on_at_the_lower_threshold(void)
{
    (void)obc_set_mode(OBC_MODE_NOMINAL);

    step_at(OBC_TEMP_HEATER_ON_DC + 1);   /* just above: no action yet */
    TEST_ASSERT_FALSE(obc_get_state()->heater_on);

    step_at(OBC_TEMP_HEATER_ON_DC);       /* exactly on the threshold */
    TEST_ASSERT_TRUE(obc_get_state()->heater_on);

    obc_init();
    (void)obc_set_mode(OBC_MODE_NOMINAL);
    step_at(OBC_TEMP_HEATER_ON_DC - 1);   /* just below */
    TEST_ASSERT_TRUE(obc_get_state()->heater_on);
}

/**
 * @verifies SWREQ-APP-050
 * Boundary values around the switch-off threshold, approached from below.
 */
static void test_heater_switches_off_at_the_upper_threshold(void)
{
    (void)obc_set_mode(OBC_MODE_NOMINAL);

    step_at(OBC_TEMP_HEATER_ON_DC);        /* heater on */
    TEST_ASSERT_TRUE(obc_get_state()->heater_on);

    step_at(OBC_TEMP_HEATER_OFF_DC - 1);   /* still inside the band */
    TEST_ASSERT_TRUE(obc_get_state()->heater_on);

    step_at(OBC_TEMP_HEATER_OFF_DC);       /* exactly on the threshold */
    TEST_ASSERT_FALSE(obc_get_state()->heater_on);
}

/** @verifies SWREQ-APP-060 */
static void test_hysteresis_band_leaves_the_heater_untouched(void)
{
    const int16_t mid = (int16_t)((OBC_TEMP_HEATER_ON_DC +
                                   OBC_TEMP_HEATER_OFF_DC) / 2);

    (void)obc_set_mode(OBC_MODE_NOMINAL);

    step_at(OBC_TEMP_HEATER_ON_DC);
    step_at(mid);
    TEST_ASSERT_TRUE(obc_get_state()->heater_on);   /* stays on   */

    step_at(OBC_TEMP_HEATER_OFF_DC);
    step_at(mid);
    TEST_ASSERT_FALSE(obc_get_state()->heater_on);  /* stays off  */
}

/** @verifies SWREQ-APP-070 */
static void test_over_temperature_forces_safe_mode_and_a_high_severity_event(void)
{
    ts_tm_t tm;

    (void)obc_set_mode(OBC_MODE_NOMINAL);
    ts_tm_flush();

    step_at(OBC_TEMP_ALARM_DC - 1);
    TEST_ASSERT_EQUAL_INT(OBC_MODE_NOMINAL, obc_get_state()->mode);
    TEST_ASSERT_FALSE(ts_tm_find(5u, (uint8_t)PUS_ST05_HIGH_SEVERITY,
                                 s_buf, sizeof(s_buf), &tm));

    step_at(OBC_TEMP_ALARM_DC);
    TEST_ASSERT_EQUAL_INT(OBC_MODE_SAFE, obc_get_state()->mode);
    TEST_ASSERT_FALSE(obc_get_state()->heater_on);
    TEST_ASSERT_TRUE(ts_tm_find(5u, (uint8_t)PUS_ST05_HIGH_SEVERITY,
                                s_buf, sizeof(s_buf), &tm));
    TEST_ASSERT_EQUAL_UINT16((uint16_t)OBC_EVT_TEMP_ALARM, pus_get_u16(tm.data));
    TEST_ASSERT_EQUAL_INT(OBC_TEMP_ALARM_DC, (int16_t)pus_get_u16(&tm.data[2]));
}

/**
 * @verifies SWREQ-APP-070
 * The alarm must not repeat every minor cycle while the unit stays hot: an
 * event storm is what fills the downlink queue and hides the real anomaly.
 */
static void test_over_temperature_event_is_not_repeated(void)
{
    ts_tm_t tm;
    int     alarms = 0;

    (void)obc_set_mode(OBC_MODE_NOMINAL);
    step_at(OBC_TEMP_ALARM_DC);
    (void)obc_set_mode(OBC_MODE_NOMINAL); /* ground tries to recover too soon */
    ts_tm_flush();

    step_at(OBC_TEMP_ALARM_DC);
    step_at(OBC_TEMP_ALARM_DC + 100);

    while (ts_tm_pop(s_buf, sizeof(s_buf), &tm) == PUS_TRUE) {
        if ((tm.service == 5u) && (tm.subtype == (uint8_t)PUS_ST05_HIGH_SEVERITY)) {
            alarms++;
        }
    }
    TEST_ASSERT_EQUAL_INT(0, alarms);
}

/** @verifies SWREQ-APP-070 */
static void test_alarm_rearms_after_cooling_down(void)
{
    ts_tm_t tm;

    (void)obc_set_mode(OBC_MODE_NOMINAL);
    step_at(OBC_TEMP_ALARM_DC);           /* alarm, back to SAFE */
    step_at(OBC_TEMP_HEATER_OFF_DC - 1);  /* cooled below the re-arm point */
    (void)obc_set_mode(OBC_MODE_NOMINAL);
    ts_tm_flush();

    step_at(OBC_TEMP_ALARM_DC);
    TEST_ASSERT_TRUE(ts_tm_find(5u, (uint8_t)PUS_ST05_HIGH_SEVERITY,
                                s_buf, sizeof(s_buf), &tm));
}

/** @verifies SWREQ-APP-130 */
static void test_heater_transitions_are_reported(void)
{
    ts_tm_t tm;

    (void)obc_set_mode(OBC_MODE_NOMINAL);
    ts_tm_flush();

    step_at(OBC_TEMP_HEATER_ON_DC);
    TEST_ASSERT_TRUE(ts_tm_find(5u, (uint8_t)PUS_ST05_INFORMATIVE,
                                s_buf, sizeof(s_buf), &tm));
    TEST_ASSERT_EQUAL_UINT16((uint16_t)OBC_EVT_HEATER_SWITCHED,
                             pus_get_u16(tm.data));
    TEST_ASSERT_EQUAL_UINT16(1u, pus_get_u16(&tm.data[2]));

    /* No further event while the state is unchanged. */
    ts_tm_flush();
    step_at(OBC_TEMP_HEATER_ON_DC);
    TEST_ASSERT_EQUAL_size_t(0u, tm_queue_count());
}

/** @verifies SWREQ-APP-090 */
static void test_telecommand_counters_track_acceptance(void)
{
    const obc_state_t *st = obc_get_state();
    uint8_t tc[PUS_MAX_PACKET_SIZE];
    size_t  len;

    (void)ts_send_tc(17u, 1u, NULL, 0u);
    TEST_ASSERT_EQUAL_UINT16(1u, st->tc_accepted);
    TEST_ASSERT_EQUAL_UINT16(0u, st->tc_rejected);

    len = ts_build_tc(tc, sizeof(tc), 17u, 1u, 0u, NULL, 0u);
    ts_break_crc(tc, len);
    (void)obc_process_tc(tc, len);
    TEST_ASSERT_EQUAL_UINT16(1u, st->tc_accepted);
    TEST_ASSERT_EQUAL_UINT16(1u, st->tc_rejected);
    TEST_ASSERT_EQUAL_UINT16((uint16_t)PUS_ERR_BAD_CRC, st->last_failure_code);
}

/**
 * @verifies SWREQ-TMQ-020
 * @verifies SWREQ-APP-120
 * Saturating the downlink must not lose the anomaly itself: once the queue
 * drains, an overflow event is reported to ground.
 */
static void test_downlink_saturation_is_reported_once_drained(void)
{
    ts_tm_t  tm;
    unsigned i;

    for (i = 0u; i < (unsigned)(PUS_TM_QUEUE_DEPTH + 4u); ++i) {
        (void)ts_send_tc(17u, 1u, NULL, 0u);
    }
    TEST_ASSERT_TRUE(tm_queue_overflow_count() > 0u);

    ts_tm_flush();
    obc_tick();

    TEST_ASSERT_TRUE(ts_tm_find(5u, (uint8_t)PUS_ST05_LOW_SEVERITY,
                                s_buf, sizeof(s_buf), &tm));
    TEST_ASSERT_EQUAL_UINT16((uint16_t)OBC_EVT_TM_QUEUE_OVERFLOW,
                             pus_get_u16(tm.data));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_power_on_state);
    RUN_TEST(test_boot_event_is_reported);
    RUN_TEST(test_uptime_counts_minor_cycles);
    RUN_TEST(test_mode_change_is_reported_once);
    RUN_TEST(test_undefined_mode_is_refused);
    RUN_TEST(test_heater_stays_off_in_safe_mode_however_cold_it_gets);
    RUN_TEST(test_heater_switches_on_at_the_lower_threshold);
    RUN_TEST(test_heater_switches_off_at_the_upper_threshold);
    RUN_TEST(test_hysteresis_band_leaves_the_heater_untouched);
    RUN_TEST(test_over_temperature_forces_safe_mode_and_a_high_severity_event);
    RUN_TEST(test_over_temperature_event_is_not_repeated);
    RUN_TEST(test_alarm_rearms_after_cooling_down);
    RUN_TEST(test_heater_transitions_are_reported);
    RUN_TEST(test_telecommand_counters_track_acceptance);
    RUN_TEST(test_downlink_saturation_is_reported_once_drained);
    return UNITY_END();
}
