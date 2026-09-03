/**
 * @file    obc_app.c
 * @brief   Application layer: mode management, thermal control, TC routing.
 */
#include "pus/obc_app.h"
#include "pus/pus_config.h"
#include "pus/pus_tm.h"
#include "pus/tm_queue.h"
#include "pus/hal.h"
#include "pus/svc01_verification.h"
#include "pus/svc03_housekeeping.h"
#include "pus/svc05_event.h"
#include "pus/svc08_function.h"
#include "pus/svc17_test.h"

static obc_state_t s_state;
/** Set while the over-temperature event has been reported, to report once. */
static pus_bool_t  s_alarm_latched;
/** Last observed value of the downlink queue overflow counter. */
static uint16_t    s_last_overflow;

/**
 * @implements SWREQ-APP-010
 * @implements SWREQ-APP-020
 */
void obc_init(void)
{
    hal_time_init();
    tm_queue_init();
    pus_tm_reset();
    svc03_init();
    svc05_init();

    /* Power-on mode is SAFE: autonomous heating stays inhibited until the
       ground segment has assessed the unit and commanded NOMINAL. */
    s_state.mode              = OBC_MODE_SAFE;
    s_state.uptime_ticks      = 0u;
    s_state.temperature_dc    = hal_read_temperature();
    s_state.heater_on         = PUS_FALSE;
    s_state.tc_accepted       = 0u;
    s_state.tc_rejected       = 0u;
    s_state.last_failure_code = 0u;

    hal_set_heater(PUS_FALSE);
    s_alarm_latched = PUS_FALSE;
    s_last_overflow = 0u;

    (void)svc05_report(OBC_EVT_BOOT_COMPLETED,
                       (uint8_t)PUS_ST05_INFORMATIVE, 0u);
}

const obc_state_t *obc_get_state(void)
{
    return &s_state;
}

/** @implements SWREQ-APP-090 */
void obc_reset_counters(void)
{
    s_state.tc_accepted       = 0u;
    s_state.tc_rejected       = 0u;
    s_state.last_failure_code = 0u;
}

/**
 * @implements SWREQ-APP-130
 *
 * Single point of control for the heater line: the state mirror published in
 * housekeeping, the hardware line and the event report can never disagree,
 * whether the change came from ground or from the control law.
 */
static void obc_apply_heater(pus_bool_t on)
{
    if (on == s_state.heater_on) {
        return;
    }

    hal_set_heater(on);
    s_state.heater_on = on;
    (void)svc05_report(OBC_EVT_HEATER_SWITCHED,
                       (uint8_t)PUS_ST05_INFORMATIVE, (uint16_t)on);
}

/** @implements SWREQ-SVC8-030 */
pus_status_t obc_set_heater(pus_bool_t on)
{
    /* In SAFE mode the survival law owns the line; manual commanding would
       fight it on the very next minor cycle. */
    if (s_state.mode == OBC_MODE_SAFE) {
        return PUS_ERR_NOT_PERMITTED;
    }

    obc_apply_heater((on != PUS_FALSE) ? PUS_TRUE : PUS_FALSE);
    return PUS_OK;
}

/**
 * @implements SWREQ-APP-030
 * @implements SWREQ-APP-080
 */
pus_status_t obc_set_mode(obc_mode_t mode)
{
    if ((mode != OBC_MODE_SAFE) && (mode != OBC_MODE_NOMINAL)) {
        return PUS_ERR_BAD_PARAMETER;
    }
    if (mode == s_state.mode) {
        return PUS_OK; /* idempotent: no transition, no event */
    }

    s_state.mode = mode;
    if (mode == OBC_MODE_SAFE) {
        obc_apply_heater(PUS_FALSE);
    }

    (void)svc05_report(OBC_EVT_MODE_CHANGED,
                       (uint8_t)PUS_ST05_INFORMATIVE, (uint16_t)mode);
    return PUS_OK;
}

/** @implements SWREQ-APP-100 */
static pus_status_t obc_route(const pus_tc_t *tc)
{
    pus_status_t st;

    switch (tc->service_type) {
    case PUS_SERVICE_HOUSEKEEPING:
        st = svc03_handle(tc);
        break;
    case PUS_SERVICE_EVENT:
        st = svc05_handle(tc);
        break;
    case PUS_SERVICE_FUNCTION:
        st = svc08_handle(tc);
        break;
    case PUS_SERVICE_TEST:
        st = svc17_handle(tc);
        break;
    default:
        st = PUS_ERR_UNKNOWN_SERVICE;
        break;
    }

    return st;
}

/**
 * @implements SWREQ-APP-090
 * @implements SWREQ-SVC1-060
 * @implements SWREQ-ROB-030
 * @implements SWREQ-ROB-040
 *
 * The three reports leave in the order the ground segment expects:
 * acceptance, then whatever the service produced, then completion. Rejecting
 * a telecommand touches no state beyond the counters, so the next one is
 * accepted normally however damaged its predecessor was.
 */
pus_status_t obc_process_tc(const uint8_t *buf, size_t len)
{
    pus_tc_t     tc;
    pus_status_t st;

    st = pus_tc_parse(buf, len, &tc);
    if (st != PUS_OK) {
        if (s_state.tc_rejected < 0xFFFFu) {
            s_state.tc_rejected = (uint16_t)(s_state.tc_rejected + 1u);
        }
        s_state.last_failure_code = (uint16_t)st;
        (void)svc01_reject_raw(buf, len, st);
        (void)svc05_report(OBC_EVT_TC_REJECTED,
                           (uint8_t)PUS_ST05_LOW_SEVERITY, (uint16_t)st);
        return st;
    }

    if (s_state.tc_accepted < 0xFFFFu) {
        s_state.tc_accepted = (uint16_t)(s_state.tc_accepted + 1u);
    }
    (void)svc01_acceptance_success(&tc);

    st = obc_route(&tc);
    if (st == PUS_OK) {
        (void)svc01_completion_success(&tc);
    } else {
        s_state.last_failure_code = (uint16_t)st;
        (void)svc01_completion_failure(&tc, st);
        (void)svc05_report(OBC_EVT_TC_REJECTED,
                           (uint8_t)PUS_ST05_LOW_SEVERITY, (uint16_t)st);
    }

    return st;
}

/**
 * @implements SWREQ-APP-040
 * @implements SWREQ-APP-050
 * @implements SWREQ-APP-060
 * @implements SWREQ-APP-070
 * @implements SWREQ-APP-080
 *
 * Between the two switching thresholds the heater line is left as it is; the
 * resulting hysteresis band is what keeps the relay from chattering when the
 * thermistor reading dithers around a single set point.
 */
static void obc_thermal_control(void)
{
    const int16_t t = s_state.temperature_dc;

    if (s_state.mode == OBC_MODE_NOMINAL) {
        if (t >= (int16_t)OBC_TEMP_ALARM_DC) {
            if (s_alarm_latched == PUS_FALSE) {
                s_alarm_latched = PUS_TRUE;
                (void)svc05_report(OBC_EVT_TEMP_ALARM,
                                   (uint8_t)PUS_ST05_HIGH_SEVERITY,
                                   (uint16_t)t);
            }
            (void)obc_set_mode(OBC_MODE_SAFE);
        } else if (t >= (int16_t)OBC_TEMP_HEATER_OFF_DC) {
            obc_apply_heater(PUS_FALSE);
        } else if (t <= (int16_t)OBC_TEMP_HEATER_ON_DC) {
            obc_apply_heater(PUS_TRUE);
        } else {
            /* Inside the hysteresis band: no action. */
        }
    } else {
        obc_apply_heater(PUS_FALSE);
    }

    if (t < (int16_t)OBC_TEMP_HEATER_OFF_DC) {
        s_alarm_latched = PUS_FALSE;
    }
}

/**
 * @implements SWREQ-APP-110
 * @implements SWREQ-APP-120
 */
void obc_tick(void)
{
    uint16_t overflows;

    hal_time_advance();
    s_state.uptime_ticks += 1u;
    s_state.temperature_dc = hal_read_temperature();

    obc_thermal_control();

    /* The queue cannot report its own overflow (the report would itself be
       dropped), so the application samples the counter once per cycle. */
    overflows = tm_queue_overflow_count();
    if (overflows != s_last_overflow) {
        s_last_overflow = overflows;
        (void)svc05_report(OBC_EVT_TM_QUEUE_OVERFLOW,
                           (uint8_t)PUS_ST05_LOW_SEVERITY, overflows);
    }

    svc03_tick();
}
