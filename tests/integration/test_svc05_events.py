"""Functional tests: event reporting and event filtering."""

from gsw.obc_db import (
    ERR_BAD_CRC,
    ERR_UNKNOWN_EVENT,
    EVT_BOOT_COMPLETED,
    EVT_TC_REJECTED,
    ST_EVT_DISABLE,
    ST_EVT_ENABLE,
    ST_EVT_LOW,
    SVC_EVENT,
    event_list,
)


def test_ITC_005_010_a_rejected_telecommand_raises_an_event(obc):
    """@verifies SWREQ-SVC5-010"""
    obc.send_tc(17, 1, valid_crc=False)

    report = obc.expect_event(EVT_TC_REJECTED, subtype=ST_EVT_LOW)
    assert report.event_aux == ERR_BAD_CRC


def test_ITC_005_020_a_disabled_event_is_not_downlinked(obc):
    """@verifies SWREQ-SVC5-020
    @verifies SWREQ-SVC5-030
    """
    obc.send_tc(SVC_EVENT, ST_EVT_DISABLE, event_list(EVT_TC_REJECTED))
    obc.expect(1, 7)
    obc.drain(window=0.3)

    obc.send_tc(17, 1, valid_crc=False)

    # The verification report still arrives; only the event is suppressed.
    assert obc.expect(1, 2).failure_code == ERR_BAD_CRC
    obc.expect_none(5, ST_EVT_LOW, window=0.4)


def test_ITC_005_030_events_can_be_re_enabled(obc):
    """@verifies SWREQ-SVC5-020"""
    obc.send_tc(SVC_EVENT, ST_EVT_DISABLE, event_list(EVT_TC_REJECTED))
    obc.expect(1, 7)
    obc.send_tc(SVC_EVENT, ST_EVT_ENABLE, event_list(EVT_TC_REJECTED))
    obc.expect(1, 7)
    obc.drain(window=0.3)

    obc.send_tc(17, 1, valid_crc=False)
    assert obc.expect_event(EVT_TC_REJECTED).event_aux == ERR_BAD_CRC


def test_ITC_005_040_unknown_event_identifier_is_refused(obc):
    """@verifies SWREQ-SVC5-040"""
    obc.send_tc(SVC_EVENT, ST_EVT_DISABLE, event_list(EVT_BOOT_COMPLETED, 0xFF))
    obc.expect(1, 1)
    assert obc.expect(1, 8).failure_code == ERR_UNKNOWN_EVENT

    # The valid identifier in the same list must not have been touched.
    obc.send_tc(SVC_EVENT, ST_EVT_ENABLE, event_list(EVT_BOOT_COMPLETED))
    obc.expect(1, 7)
