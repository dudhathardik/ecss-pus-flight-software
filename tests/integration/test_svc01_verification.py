"""Functional tests: request verification reports seen from the ground."""

from gsw import AckFlags

ERR_BAD_CRC = 8
ERR_UNKNOWN_SERVICE = 9


def test_ITC_001_010_acknowledgements_arrive_in_the_specified_order(obc_quiet):
    """@verifies SWREQ-SVC1-010
    @verifies SWREQ-SVC1-030
    @verifies SWREQ-SVC1-060
    """
    obc_quiet.send_tc(17, 1, ack=AckFlags.ACCEPTANCE | AckFlags.COMPLETION)
    ordered = obc_quiet.drain(window=0.4)

    assert [(p.service, p.subtype) for p in ordered] == [(1, 1), (17, 2), (1, 7)]


def test_ITC_001_020_reports_echo_the_request_id(obc):
    """@verifies SWREQ-SVC1-040"""
    sent = obc.send_tc(17, 1, seq_count=0x0123, ack=AckFlags.ALL)
    expected = int.from_bytes(sent[0:4], "big")

    assert obc.expect(1, 1).request_id == expected
    assert obc.expect(1, 7).request_id == expected


def test_ITC_001_030_nothing_is_acknowledged_when_nothing_is_requested(obc_quiet):
    """@verifies SWREQ-SVC1-010"""
    obc_quiet.send_tc(17, 1, ack=AckFlags.NONE)
    ordered = obc_quiet.drain(window=0.4)

    assert [(p.service, p.subtype) for p in ordered] == [(17, 2)]


def test_ITC_001_040_only_the_completion_is_acknowledged(obc_quiet):
    """@verifies SWREQ-SVC1-010"""
    obc_quiet.send_tc(17, 1, ack=AckFlags.COMPLETION)
    ordered = obc_quiet.drain(window=0.4)

    assert [(p.service, p.subtype) for p in ordered] == [(17, 2), (1, 7)]


def test_ITC_001_050_a_corrupted_telecommand_is_always_reported(obc):
    """@verifies SWREQ-SVC1-020
    @verifies SWREQ-SVC1-050
    @verifies SWREQ-CRC-020
    """
    obc.send_tc(17, 1, ack=AckFlags.NONE, valid_crc=False)

    report = obc.expect(1, 2)
    assert report.failure_code == ERR_BAD_CRC
    # ... and the anomaly is also visible as an event report.
    assert obc.expect(5, 2).event_id == 3  # OBC_EVT_TC_REJECTED


def test_ITC_001_060_an_unknown_service_fails_after_acceptance(obc):
    """@verifies SWREQ-APP-100"""
    obc.send_tc(99, 1, ack=AckFlags.ALL)

    obc.expect(1, 1)
    assert obc.expect(1, 8).failure_code == ERR_UNKNOWN_SERVICE
