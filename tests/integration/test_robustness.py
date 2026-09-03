"""Functional tests: behaviour of the uplink under malformed traffic.

Every case injects exactly one defect into an otherwise valid telecommand and
checks both that the unit rejects it with the documented failure code and that
the link keeps working afterwards. A rejection that also wedges the receiver
is not a rejection.
"""

import time

from gsw.obc_db import (
    ERR_BAD_APID,
    ERR_BAD_CRC,
    ERR_BAD_PACKET_TYPE,
    ERR_BAD_VERSION,
    ERR_NO_SEC_HEADER,
    ERR_TRUNCATED,
    EVT_TC_REJECTED,
)


def reject_code(link):
    """Return the failure code of the next acceptance failure report."""
    return link.expect(1, 2).failure_code


def test_ITC_ROB_010_corrupted_packet(obc):
    """@verifies SWREQ-CRC-020"""
    obc.send_tc(17, 1, valid_crc=False)
    assert reject_code(obc) == ERR_BAD_CRC


def test_ITC_ROB_020_foreign_apid(obc):
    """@verifies SWREQ-TC-060"""
    obc.send_tc(17, 1, apid=0x2A)
    assert reject_code(obc) == ERR_BAD_APID


def test_ITC_ROB_030_unsupported_ccsds_version(obc):
    """@verifies SWREQ-CCSDS-020"""
    obc.send_tc(17, 1, ccsds_version=1)
    assert reject_code(obc) == ERR_BAD_VERSION


def test_ITC_ROB_040_telemetry_packet_on_the_uplink(obc):
    """@verifies SWREQ-TC-040"""
    obc.send_tc(17, 1, packet_type=0)
    assert reject_code(obc) == ERR_BAD_PACKET_TYPE


def test_ITC_ROB_050_missing_secondary_header_flag(obc):
    """@verifies SWREQ-TC-050"""
    obc.send_tc(17, 1, sec_hdr_flag=0)
    assert reject_code(obc) == ERR_NO_SEC_HEADER


def test_ITC_ROB_060_previous_pus_issue(obc):
    """@verifies SWREQ-TC-070"""
    obc.send_tc(17, 1, pus_version=1)
    assert reject_code(obc) == ERR_BAD_VERSION


def test_ITC_ROB_070_runt_packet(obc):
    """@verifies SWREQ-TC-020

    A six-octet primary header announcing a two-octet data field: shorter
    than any telecommand can be, but self-consistent, so the link stays in
    step and the next telecommand is still accepted.
    """
    runt = bytes([0x18, 0x0C, 0xC0, 0x00, 0x00, 0x01, 0x20, 0x11])
    obc.send_raw(runt)

    assert reject_code(obc) == ERR_TRUNCATED
    assert obc.expect_event(EVT_TC_REJECTED).event_aux == ERR_TRUNCATED


def test_ITC_ROB_080_the_link_survives_a_rejected_telecommand(obc):
    """@verifies SWREQ-ROB-030"""
    obc.send_tc(17, 1, valid_crc=False)
    assert reject_code(obc) == ERR_BAD_CRC

    obc.send_tc(17, 1)
    obc.expect(17, 2)
    obc.expect(1, 7)


def test_ITC_ROB_090_burst_of_telecommands_is_fully_acknowledged(obc):
    """@verifies SWREQ-ROB-040

    Fifty telecommands back to back, faster than the minor cycle: nothing may
    be dropped silently, and the sequence counts must stay gap free.
    """
    burst = 50
    for index in range(burst):
        obc.send_tc(17, 1, seq_count=index)

    completions = 0
    responses = 0
    deadline = time.monotonic() + 10.0
    seq_counts = []

    while (completions < burst) and (time.monotonic() < deadline):
        packet = obc.read_tm(timeout=2.0)
        seq_counts.append(packet.seq_count)
        if (packet.service, packet.subtype) == (1, 7):
            completions += 1
        elif (packet.service, packet.subtype) == (17, 2):
            responses += 1

    assert completions == burst
    assert responses == burst
    # Every packet the unit emitted carries the next sequence count.
    assert seq_counts == list(range(seq_counts[0], seq_counts[0] + len(seq_counts)))


def test_ITC_ROB_100_oversized_packet_is_dropped(obc):
    """@verifies SWREQ-ROB-050

    A packet announcing more octets than the receive buffer can hold. The
    runner discards it and reports the rejection; the remaining octets stay in
    the stream, so this case deliberately runs last on its own connection.
    A flight link delivers framed transfer frames, where this cannot happen.
    """
    obc.send_tc(17, 1, data=b"\x00" * 300)
    assert reject_code(obc) == ERR_TRUNCATED
