"""Mission database: the ground-side view of the on-board interface.

Everything a test needs to name a value instead of writing a magic number
lives here, transcribed from ICD.md. Keeping it in one module means an ICD
change is a one-file change on the ground side too.
"""

import struct
from collections import namedtuple

# -- services ------------------------------------------------------------

SVC_VERIFICATION = 1
SVC_HOUSEKEEPING = 3
SVC_EVENT = 5
SVC_FUNCTION = 8
SVC_TEST = 17

ST_ACCEPT_OK = 1
ST_ACCEPT_FAIL = 2
ST_COMPLETE_OK = 7
ST_COMPLETE_FAIL = 8

ST_HK_ENABLE = 5
ST_HK_DISABLE = 6
ST_HK_REPORT = 25
ST_HK_ONE_SHOT = 27

ST_EVT_INFO = 1
ST_EVT_LOW = 2
ST_EVT_MEDIUM = 3
ST_EVT_HIGH = 4
ST_EVT_DISABLE = 5
ST_EVT_ENABLE = 6

ST_FN_PERFORM = 1

# -- identifiers ---------------------------------------------------------

SID_ESSENTIAL = 1
SID_DIAGNOSTIC = 2

EVT_BOOT_COMPLETED = 1
EVT_MODE_CHANGED = 2
EVT_TC_REJECTED = 3
EVT_TM_QUEUE_OVERFLOW = 4
EVT_HEATER_SWITCHED = 5
EVT_TEMP_ALARM = 6

FID_SET_MODE = 1
FID_SET_HEATER = 2
FID_INJECT_TEMP = 3
FID_RESET_COUNTERS = 4
FID_SET_TIME = 5

MODE_SAFE = 0
MODE_NOMINAL = 1

# -- failure codes (pus_status_t) ---------------------------------------

ERR_NULL = 1
ERR_BUFFER_TOO_SMALL = 2
ERR_TRUNCATED = 3
ERR_BAD_VERSION = 4
ERR_BAD_PACKET_TYPE = 5
ERR_NO_SEC_HEADER = 6
ERR_BAD_APID = 7
ERR_BAD_CRC = 8
ERR_UNKNOWN_SERVICE = 9
ERR_UNKNOWN_SUBTYPE = 10
ERR_BAD_LENGTH = 11
ERR_BAD_PARAMETER = 12
ERR_UNKNOWN_SID = 13
ERR_UNKNOWN_EVENT = 14
ERR_UNKNOWN_FUNCTION = 15
ERR_QUEUE_FULL = 16
ERR_NOT_PERMITTED = 17

# -- thermal set points (units of 0.1 degC, from pus_config.h) -----------

TEMP_HEATER_ON = 50
TEMP_HEATER_OFF = 250
TEMP_ALARM = 400

# -- housekeeping layouts ------------------------------------------------

Essential = namedtuple("Essential",
                       "sid mode uptime_ticks temperature_dc heater_on")
Diagnostic = namedtuple("Diagnostic",
                        "sid tc_accepted tc_rejected last_failure_code "
                        "tm_queued tm_overflows")


def decode_hk(data):
    """Decode a (3,25) source data field into the matching named tuple."""
    sid = data[0]
    if sid == SID_ESSENTIAL:
        if len(data) != 9:
            raise ValueError("essential report is {} octets, expected 9"
                             .format(len(data)))
        mode, uptime, temp, heater = struct.unpack(">BIhB", data[1:])
        return Essential(sid, mode, uptime, temp, heater)
    if sid == SID_DIAGNOSTIC:
        if len(data) != 10:
            raise ValueError("diagnostic report is {} octets, expected 10"
                             .format(len(data)))
        accepted, rejected, failure, queued, overflows = \
            struct.unpack(">HHHBH", data[1:])
        return Diagnostic(sid, accepted, rejected, failure, queued, overflows)
    raise ValueError("unknown structure identifier {}".format(sid))


# -- telecommand argument builders --------------------------------------

def hk_list(*sids):
    """Application data of a (3,5) / (3,6) / (3,27) telecommand."""
    return bytes([len(sids)]) + bytes(sids)


def event_list(*event_ids):
    """Application data of a (5,5) / (5,6) telecommand."""
    out = bytearray([len(event_ids)])
    for identifier in event_ids:
        out += int(identifier).to_bytes(2, "big")
    return bytes(out)


def perform(function_id, args=b""):
    """Application data of an (8,1) telecommand."""
    return bytes([function_id]) + bytes(args)


def set_mode(mode):
    return perform(FID_SET_MODE, bytes([mode]))


def set_heater(on):
    return perform(FID_SET_HEATER, bytes([1 if on else 0]))


def inject_temperature(deci_celsius):
    return perform(FID_INJECT_TEMP, struct.pack(">h", deci_celsius))


def set_time(coarse, fine=0):
    """CCSDS unsegmented time code: 4 octets of seconds, 2 of sub-seconds."""
    return perform(FID_SET_TIME, struct.pack(">IH", coarse, fine))
