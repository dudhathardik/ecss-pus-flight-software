"""Functional tests: housekeeping reporting over the TC/TM link."""

from gsw.obc_db import (
    ERR_BAD_LENGTH,
    ERR_UNKNOWN_SID,
    SID_DIAGNOSTIC,
    SID_ESSENTIAL,
    ST_HK_DISABLE,
    ST_HK_ENABLE,
    ST_HK_ONE_SHOT,
    SVC_HOUSEKEEPING,
    decode_hk,
    hk_list,
)


def test_ITC_003_010_essential_report_is_generated_from_power_on(obc):
    """@verifies SWREQ-SVC3-030
    @verifies SWREQ-SVC3-070
    """
    report = decode_hk(obc.expect_hk(SID_ESSENTIAL).data)

    assert report.sid == SID_ESSENTIAL
    assert report.mode in (0, 1)
    assert report.uptime_ticks > 0
    assert report.heater_on in (0, 1)
    assert -2000 < report.temperature_dc < 2000


def test_ITC_003_020_reports_repeat_once_per_period(obc, hk_period):
    """@verifies SWREQ-SVC3-030"""
    first = decode_hk(obc.expect_hk(SID_ESSENTIAL).data)
    second = decode_hk(obc.expect_hk(SID_ESSENTIAL, timeout=hk_period * 5).data)

    # Ten minor cycles per period, and the uptime counter is in minor cycles.
    assert second.uptime_ticks - first.uptime_ticks == 10


def test_ITC_003_030_generation_can_be_switched_off(obc, hk_period):
    """@verifies SWREQ-SVC3-040"""
    obc.expect_hk(SID_ESSENTIAL)

    obc.send_tc(SVC_HOUSEKEEPING, ST_HK_DISABLE, hk_list(SID_ESSENTIAL))
    obc.expect(1, 7)
    obc.drain(window=hk_period * 1.5)

    obc.expect_none(3, 25, window=hk_period * 3)


def test_ITC_003_040_the_diagnostic_structure_can_be_switched_on(obc, hk_period):
    """@verifies SWREQ-SVC3-040"""
    obc.send_tc(SVC_HOUSEKEEPING, ST_HK_ENABLE, hk_list(SID_DIAGNOSTIC))
    obc.expect(1, 7)

    report = decode_hk(obc.expect_hk(SID_DIAGNOSTIC, timeout=hk_period * 5).data)
    assert report.sid == SID_DIAGNOSTIC
    # Both structures now report in the same period.
    obc.expect_hk(SID_ESSENTIAL, timeout=hk_period * 5)


def test_ITC_003_050_one_shot_works_on_a_disabled_structure(obc):
    """@verifies SWREQ-SVC3-050"""
    obc.send_tc(SVC_HOUSEKEEPING, ST_HK_ONE_SHOT, hk_list(SID_DIAGNOSTIC))
    obc.expect(1, 7)

    report = decode_hk(obc.expect_hk(SID_DIAGNOSTIC, timeout=1.0).data)
    assert report.sid == SID_DIAGNOSTIC


def test_ITC_003_060_diagnostic_counters_follow_the_uplink(obc):
    """@verifies SWREQ-APP-090"""
    obc.send_tc(17, 1)                       # accepted
    obc.expect(1, 7)
    obc.send_tc(17, 1, valid_crc=False)      # rejected
    obc.expect(1, 2)

    obc.send_tc(SVC_HOUSEKEEPING, ST_HK_ONE_SHOT, hk_list(SID_DIAGNOSTIC))
    report = decode_hk(obc.expect_hk(SID_DIAGNOSTIC, timeout=1.0).data)

    assert report.tc_accepted >= 2          # the connection test and this one
    assert report.tc_rejected == 1
    assert report.tm_overflows == 0


def test_ITC_003_070_unknown_structure_identifier_is_refused(obc):
    """@verifies SWREQ-SVC3-060"""
    obc.send_tc(SVC_HOUSEKEEPING, ST_HK_ENABLE, hk_list(9))
    obc.expect(1, 1)
    assert obc.expect(1, 8).failure_code == ERR_UNKNOWN_SID


def test_ITC_003_080_malformed_structure_list_is_refused(obc):
    """@verifies SWREQ-ROB-020"""
    # The list claims two identifiers but only one is present.
    obc.send_tc(SVC_HOUSEKEEPING, ST_HK_ENABLE, bytes([2, SID_ESSENTIAL]))
    obc.expect(1, 1)
    assert obc.expect(1, 8).failure_code == ERR_BAD_LENGTH
