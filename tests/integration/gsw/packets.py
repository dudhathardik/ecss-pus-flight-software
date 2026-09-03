"""CCSDS / PUS-C packet encoding and decoding for the test ground segment.

This module is written directly from ICD.md rather than from the flight
source: encoding the packets a second time, independently, is what makes the
functional tests capable of catching an error that exists in both the flight
encoder and the flight decoder.
"""

from dataclasses import dataclass

APID = 0x00C
PUS_VERSION_C = 2

PRIMARY_HEADER_LEN = 6
TC_SEC_HEADER_LEN = 5
TM_SEC_HEADER_LEN = 13
CRC_LEN = 2

SEQ_FLAG_UNSEGMENTED = 0b11


class AckFlags:
    """Acknowledgement flags of the telecommand secondary header."""

    NONE = 0x0
    ACCEPTANCE = 0x1
    START = 0x2
    PROGRESS = 0x4
    COMPLETION = 0x8
    ALL = 0xF


def crc16_ccitt(data):
    """CRC-16-CCITT-FALSE: poly 0x1021, seed 0xFFFF, no reflection, no XOR."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def build_tc(service, subtype, data=b"", ack=AckFlags.ACCEPTANCE | AckFlags.COMPLETION,
             seq_count=0, apid=APID, source_id=0x0101, pus_version=PUS_VERSION_C,
             ccsds_version=0, packet_type=1, sec_hdr_flag=1,
             declared_length=None, valid_crc=True):
    """Assemble a telecommand.

    Every header field can be overridden so that the robustness cases can
    inject exactly one defect at a time.
    """
    body = bytearray()
    body.append(((pus_version & 0x0F) << 4) | (ack & 0x0F))
    body.append(service & 0xFF)
    body.append(subtype & 0xFF)
    body += source_id.to_bytes(2, "big")
    body += bytes(data)

    total = PRIMARY_HEADER_LEN + len(body) + CRC_LEN
    length_field = (total - PRIMARY_HEADER_LEN - 1) if declared_length is None \
        else declared_length

    word0 = ((ccsds_version & 0x07) << 13) | ((packet_type & 0x01) << 12) | \
            ((sec_hdr_flag & 0x01) << 11) | (apid & 0x07FF)
    word1 = (SEQ_FLAG_UNSEGMENTED << 14) | (seq_count & 0x3FFF)

    packet = bytearray()
    packet += word0.to_bytes(2, "big")
    packet += word1.to_bytes(2, "big")
    packet += (length_field & 0xFFFF).to_bytes(2, "big")
    packet += body

    crc = crc16_ccitt(packet)
    if not valid_crc:
        crc ^= 0x0001
    packet += crc.to_bytes(2, "big")
    return bytes(packet)


@dataclass
class TmPacket:
    """A decoded telemetry packet."""

    apid: int
    seq_count: int
    service: int
    subtype: int
    msg_counter: int
    destination_id: int
    time_coarse: int
    time_fine: int
    data: bytes
    raw: bytes

    def __repr__(self):
        return "TM({},{}) seq={} t={}.{:05d} data={}".format(
            self.service, self.subtype, self.seq_count,
            self.time_coarse, self.time_fine, self.data.hex())

    @property
    def request_id(self):
        """Request ID echoed by a service 1 report."""
        return int.from_bytes(self.data[0:4], "big")

    @property
    def failure_code(self):
        """Failure code carried by a (1,2) or (1,8) report."""
        return int.from_bytes(self.data[4:6], "big")

    @property
    def event_id(self):
        """Event identifier carried by a service 5 report."""
        return int.from_bytes(self.data[0:2], "big")

    @property
    def event_aux(self):
        """Auxiliary word carried by a service 5 report."""
        return int.from_bytes(self.data[2:4], "big")


def parse_tm(raw):
    """Decode one telemetry packet, verifying its packet error control field."""
    if len(raw) < PRIMARY_HEADER_LEN + TM_SEC_HEADER_LEN + CRC_LEN:
        raise ValueError("telemetry packet too short: {} octets".format(len(raw)))

    word0 = int.from_bytes(raw[0:2], "big")
    word1 = int.from_bytes(raw[2:4], "big")
    length_field = int.from_bytes(raw[4:6], "big")

    if (word0 >> 12) & 0x01 != 0:
        raise ValueError("packet type bit set on a telemetry packet")
    if PRIMARY_HEADER_LEN + length_field + 1 != len(raw):
        raise ValueError("declared length {} does not match {} octets received"
                         .format(length_field, len(raw)))
    if crc16_ccitt(raw[:-CRC_LEN]) != int.from_bytes(raw[-CRC_LEN:], "big"):
        raise ValueError("telemetry CRC mismatch")

    off = PRIMARY_HEADER_LEN
    if (raw[off] >> 4) & 0x0F != PUS_VERSION_C:
        raise ValueError("unexpected PUS version in telemetry")

    return TmPacket(
        apid=word0 & 0x07FF,
        seq_count=word1 & 0x3FFF,
        service=raw[off + 1],
        subtype=raw[off + 2],
        msg_counter=int.from_bytes(raw[off + 3:off + 5], "big"),
        destination_id=int.from_bytes(raw[off + 5:off + 7], "big"),
        time_coarse=int.from_bytes(raw[off + 7:off + 11], "big"),
        time_fine=int.from_bytes(raw[off + 11:off + 13], "big"),
        data=bytes(raw[off + TM_SEC_HEADER_LEN:-CRC_LEN]),
        raw=bytes(raw),
    )
