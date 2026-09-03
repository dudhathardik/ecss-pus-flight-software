"""Functional tests: connection test service and power-on behaviour.

Test case identifiers follow SVP.md: ITC-<service>-<number>.
"""

from gsw import AckFlags

EVT_BOOT_COMPLETED = 1


def test_ITC_APP_010_boot_event_is_the_first_packet_downlinked(link):
    """@verifies SWREQ-APP-020"""
    packet = link.read_tm(timeout=2.0)
    assert (packet.service, packet.subtype) == (5, 1)
    assert packet.event_id == EVT_BOOT_COMPLETED


def test_ITC_017_010_are_you_alive_is_answered(obc):
    """@verifies SWREQ-SVC17-010"""
    obc.send_tc(17, 1, ack=AckFlags.NONE)
    packet = obc.expect(17, 2)
    assert packet.data == b""
    assert packet.apid == 0x00C


def test_ITC_017_020_response_carries_an_increasing_sequence_count(obc):
    """@verifies SWREQ-TM-020"""
    obc.send_tc(17, 1, ack=AckFlags.NONE)
    first = obc.expect(17, 2)
    obc.send_tc(17, 1, ack=AckFlags.NONE)
    second = obc.expect(17, 2)

    assert second.seq_count == first.seq_count + 1
    assert second.msg_counter == first.msg_counter + 1


def test_ITC_017_030_time_stamp_advances_between_responses(obc, hk_period):
    """@verifies SWREQ-HAL-010"""
    obc.send_tc(17, 1, ack=AckFlags.NONE)
    first = obc.expect(17, 2)

    # Let a housekeeping period elapse, which is several minor cycles.
    obc.drain(window=hk_period * 1.5)

    obc.send_tc(17, 1, ack=AckFlags.NONE)
    second = obc.expect(17, 2)

    first_t = first.time_coarse * 65536 + first.time_fine
    second_t = second.time_coarse * 65536 + second.time_fine
    assert second_t > first_t


def test_ITC_017_040_non_empty_request_is_rejected(obc):
    """@verifies SWREQ-SVC17-020"""
    obc.send_tc(17, 1, data=b"\x01\x02", ack=AckFlags.ALL)
    obc.expect(1, 1)
    report = obc.expect(1, 8)
    assert report.failure_code == 11  # PUS_ERR_BAD_LENGTH
    obc.expect_none(17, 2, window=0.3)


def test_ITC_017_050_unknown_subtype_is_rejected(obc):
    """@verifies SWREQ-SVC17-020"""
    obc.send_tc(17, 99)
    obc.expect(1, 1)
    assert obc.expect(1, 8).failure_code == 10  # PUS_ERR_UNKNOWN_SUBTYPE
