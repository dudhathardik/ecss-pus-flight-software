"""Functional tests: mode management and autonomous thermal control.

This is the end-to-end chain the unit tests cannot cover on their own:
a telecommand arrives on the link, changes the on-board state, the control
law reacts on the next minor cycle, and the ground sees the consequence in
both an event report and the next housekeeping report.
"""

import pytest

from gsw.obc_db import (
    ERR_BAD_PARAMETER,
    ERR_NOT_PERMITTED,
    EVT_HEATER_SWITCHED,
    EVT_MODE_CHANGED,
    EVT_TEMP_ALARM,
    MODE_NOMINAL,
    MODE_SAFE,
    SID_ESSENTIAL,
    ST_EVT_HIGH,
    ST_EVT_INFO,
    ST_FN_PERFORM,
    ST_HK_ONE_SHOT,
    SVC_FUNCTION,
    SVC_HOUSEKEEPING,
    TEMP_ALARM,
    TEMP_HEATER_OFF,
    TEMP_HEATER_ON,
    decode_hk,
    hk_list,
    inject_temperature,
    set_heater,
    set_mode,
)


def current_hk(link):
    """Force and decode a fresh essential housekeeping report.

    Telemetry already buffered is discarded first: a periodic report produced
    before the command under test would answer the question about the state
    that existed beforehand.
    """
    link.flush()
    link.send_tc(SVC_HOUSEKEEPING, ST_HK_ONE_SHOT, hk_list(SID_ESSENTIAL))
    return decode_hk(link.expect_hk(SID_ESSENTIAL, timeout=2.0).data)


def apply_temperature(link, deci_celsius):
    """Inject a thermistor reading and wait for the control law to act on it."""
    link.send_tc(SVC_FUNCTION, ST_FN_PERFORM, inject_temperature(deci_celsius))
    link.expect(1, 7)


@pytest.fixture
def nominal(obc):
    """An on-board computer already switched to NOMINAL mode."""
    obc.send_tc(SVC_FUNCTION, ST_FN_PERFORM, set_mode(MODE_NOMINAL))
    obc.expect(1, 7)
    obc.expect_event(EVT_MODE_CHANGED, subtype=ST_EVT_INFO)
    return obc


def test_ITC_008_010_mode_transition_is_commanded_and_observed(obc):
    """@verifies SWREQ-SVC8-020
    @verifies SWREQ-APP-030
    """
    assert current_hk(obc).mode == MODE_SAFE

    obc.send_tc(SVC_FUNCTION, ST_FN_PERFORM, set_mode(MODE_NOMINAL))
    obc.expect(1, 1)
    obc.expect(1, 7)
    assert obc.expect_event(EVT_MODE_CHANGED).event_aux == MODE_NOMINAL

    assert current_hk(obc).mode == MODE_NOMINAL


def test_ITC_008_020_undefined_mode_is_refused(obc):
    """@verifies SWREQ-SVC8-020"""
    obc.send_tc(SVC_FUNCTION, ST_FN_PERFORM, set_mode(7))
    obc.expect(1, 1)
    assert obc.expect(1, 8).failure_code == ERR_BAD_PARAMETER
    assert current_hk(obc).mode == MODE_SAFE


def test_ITC_008_030_heater_switches_on_when_cold(nominal):
    """@verifies SWREQ-APP-040"""
    apply_temperature(nominal, TEMP_HEATER_ON)

    assert nominal.expect_event(EVT_HEATER_SWITCHED).event_aux == 1
    report = current_hk(nominal)
    assert report.heater_on == 1
    assert report.temperature_dc == TEMP_HEATER_ON


def test_ITC_008_040_heater_switches_off_when_warm(nominal):
    """@verifies SWREQ-APP-050"""
    apply_temperature(nominal, TEMP_HEATER_ON)
    nominal.expect_event(EVT_HEATER_SWITCHED)

    apply_temperature(nominal, TEMP_HEATER_OFF)
    assert nominal.expect_event(EVT_HEATER_SWITCHED).event_aux == 0
    assert current_hk(nominal).heater_on == 0


def test_ITC_008_050_the_hysteresis_band_produces_no_switching(nominal):
    """@verifies SWREQ-APP-060"""
    midpoint = (TEMP_HEATER_ON + TEMP_HEATER_OFF) // 2

    apply_temperature(nominal, TEMP_HEATER_ON)
    nominal.expect_event(EVT_HEATER_SWITCHED)

    apply_temperature(nominal, midpoint)
    nominal.expect_none(5, ST_EVT_INFO, window=0.4)
    assert current_hk(nominal).heater_on == 1  # unchanged, as specified


def test_ITC_008_060_over_temperature_drops_the_unit_into_safe_mode(nominal):
    """@verifies SWREQ-APP-070
    @verifies SWREQ-APP-080
    """
    apply_temperature(nominal, TEMP_ALARM - 1)
    nominal.expect_none(5, ST_EVT_HIGH, window=0.3)
    assert current_hk(nominal).mode == MODE_NOMINAL

    apply_temperature(nominal, TEMP_ALARM)
    alarm = nominal.expect_event(EVT_TEMP_ALARM, subtype=ST_EVT_HIGH)
    assert alarm.event_aux == TEMP_ALARM

    report = current_hk(nominal)
    assert report.mode == MODE_SAFE
    assert report.heater_on == 0


def test_ITC_008_070_manual_heater_command_is_refused_in_safe_mode(obc):
    """@verifies SWREQ-SVC8-030"""
    obc.send_tc(SVC_FUNCTION, ST_FN_PERFORM, set_heater(True))
    obc.expect(1, 1)
    assert obc.expect(1, 8).failure_code == ERR_NOT_PERMITTED
    assert current_hk(obc).heater_on == 0


def test_ITC_008_080_manual_heater_command_works_in_nominal_mode(nominal):
    """@verifies SWREQ-SVC8-030"""
    apply_temperature(nominal, (TEMP_HEATER_ON + TEMP_HEATER_OFF) // 2)

    nominal.send_tc(SVC_FUNCTION, ST_FN_PERFORM, set_heater(True))
    nominal.expect(1, 7)
    assert current_hk(nominal).heater_on == 1

    nominal.send_tc(SVC_FUNCTION, ST_FN_PERFORM, set_heater(False))
    nominal.expect(1, 7)
    assert current_hk(nominal).heater_on == 0


def test_ITC_008_090_on_board_time_can_be_set_from_ground(obc):
    """@verifies SWREQ-SVC8-070
    @verifies SWREQ-HAL-020
    """
    from gsw.obc_db import set_time

    before = obc.expect_hk(SID_ESSENTIAL, timeout=2.0).time_coarse

    obc.send_tc(SVC_FUNCTION, ST_FN_PERFORM, set_time(1_000_000, 0))
    obc.expect(1, 7)

    obc.flush()
    obc.send_tc(SVC_HOUSEKEEPING, ST_HK_ONE_SHOT, hk_list(SID_ESSENTIAL))
    after = obc.expect_hk(SID_ESSENTIAL, timeout=2.0)

    assert before < 10           # the unit boots at time zero
    assert after.time_coarse >= 1_000_000
