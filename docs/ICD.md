# Interface Control Document — TC/TM packets

**Issue:** 1.0 — applicable to OBSW-PUS 1.0
**References:** CCSDS 133.0-B space packet protocol, ECSS-E-ST-70-41C

All fields are transmitted most significant octet first. Bit 0 is the most
significant bit of the first octet. Sizes are in octets unless stated.

---

## 1. Identification

| Item | Value |
|---|---|
| Telecommand APID | 0x00C (12) |
| Telemetry APID | 0x00C (12) |
| Telemetry destination ID | 0x0001 |
| PUS version number | 2 (issue C) |
| Maximum packet size | 256 octets |
| Sequence flags | 0b11, unsegmented, on every packet |

## 2. Space packet primary header (6 octets)

| Offset | Bits | Field | Value |
|---|---|---|---|
| 0 | 3 | Packet version number | 0 |
| 0 | 1 | Packet type | 1 = TC, 0 = TM |
| 0 | 1 | Secondary header flag | 1 |
| 0-1 | 11 | APID | 0x00C |
| 2 | 2 | Sequence flags | 0b11 |
| 2-3 | 14 | Packet sequence count | Incremented per packet, wraps at 16383 |
| 4-5 | 16 | Packet data length | Total length minus 7 |

## 3. Telecommand secondary header (5 octets)

| Offset | Size | Field | Value |
|---|---|---|---|
| 6 | 4 bits | TC PUS version number | 2 |
| 6 | 4 bits | Acknowledgement flags | bit 0 acceptance, bit 1 start, bit 2 progress, bit 3 completion |
| 7 | 1 | Service type | |
| 8 | 1 | Message subtype | |
| 9-10 | 2 | Source ID | Ground application identifier |

Application data follows, then the 2-octet packet error control field.
The smallest valid telecommand is therefore 13 octets.

## 4. Telemetry secondary header (13 octets)

| Offset | Size | Field | Value |
|---|---|---|---|
| 6 | 4 bits | TM PUS version number | 2 |
| 6 | 4 bits | Spacecraft time reference status | 0 (not synchronised) |
| 7 | 1 | Service type | |
| 8 | 1 | Message subtype | |
| 9-10 | 2 | Message type counter | Per service type, see deviation D-01 |
| 11-12 | 2 | Destination ID | 0x0001 |
| 13-16 | 4 | CUC coarse time | Seconds since the on-board epoch |
| 17-18 | 2 | CUC fine time | Units of 1/65536 s |

Source data follows, then the 2-octet packet error control field.
An empty telemetry packet is therefore 21 octets.

## 5. Packet error control field

CRC-16-CCITT over every octet of the packet except the field itself:
polynomial 0x1021, seed 0xFFFF, no input or output reflection, no final
exclusive-or. Check value over the ASCII string `123456789` is `0x29B1`.

---

## 6. Telecommands

### (3,5) enable housekeeping generation / (3,6) disable / (3,27) generate once

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | N, number of structure identifiers, N >= 1 |
| 1..N | 1 each | Structure identifiers |

Rejected as a whole if any identifier is undefined.

### (5,5) disable event reporting / (5,6) enable event reporting

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | N, number of event identifiers, N >= 1 |
| 1.. | 2 each | Event identifiers |

Rejected as a whole if any identifier is undefined.

### (8,1) perform function

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | Function identifier |
| 1.. | variable | Arguments, per the table below |

| ID | Function | Arguments | Notes |
|---|---|---|---|
| 1 | Set mode | 1 octet: 0 = SAFE, 1 = NOMINAL | |
| 2 | Set heater | 1 octet: 0 = off, 1 = on | Refused in SAFE mode |
| 3 | Inject temperature | 2 octets, signed, 0.1 degC | Test hook |
| 4 | Reset counters | none | |
| 5 | Set on-board time | 4 octets coarse + 2 octets fine | |

### (17,1) are you alive

No application data. Any application data is rejected.

---

## 7. Telemetry

### (1,1) / (1,7) successful acceptance / completion

| Offset | Size | Field |
|---|---|---|
| 0-3 | 4 | Request ID: first four octets of the telecommand primary header |

### (1,2) / (1,8) failed acceptance / completion

| Offset | Size | Field |
|---|---|---|
| 0-3 | 4 | Request ID (zero if fewer than four octets were received) |
| 4-5 | 2 | Failure code, see section 8 |

### (3,25) housekeeping parameter report, SID 1 (essential)

| Offset | Size | Type | Field |
|---|---|---|---|
| 0 | 1 | u8 | Structure ID = 1 |
| 1 | 1 | u8 | Mode: 0 = SAFE, 1 = NOMINAL |
| 2-5 | 4 | u32 | Uptime in minor cycles |
| 6-7 | 2 | i16 | Temperature, 0.1 degC |
| 8 | 1 | u8 | Heater state |

### (3,25) housekeeping parameter report, SID 2 (diagnostic)

| Offset | Size | Type | Field |
|---|---|---|---|
| 0 | 1 | u8 | Structure ID = 2 |
| 1-2 | 2 | u16 | Telecommands accepted |
| 3-4 | 2 | u16 | Telecommands rejected |
| 5-6 | 2 | u16 | Failure code of the most recent rejection |
| 7 | 1 | u8 | Packets currently on the downlink queue |
| 8-9 | 2 | u16 | Downlink queue overflow count |

### (5,1) / (5,2) / (5,3) / (5,4) event report

Subtype carries the severity: 1 informative, 2 low, 3 medium, 4 high.

| Offset | Size | Field |
|---|---|---|
| 0-1 | 2 | Event identifier |
| 2-3 | 2 | Auxiliary data |

| ID | Event | Severity | Auxiliary data |
|---|---|---|---|
| 1 | Boot completed | 1 informative | 0 |
| 2 | Mode changed | 1 informative | New mode |
| 3 | Telecommand rejected | 2 low | Failure code |
| 4 | Downlink queue overflow | 2 low | Total packets dropped |
| 5 | Heater switched | 1 informative | New heater state |
| 6 | Over-temperature | 4 high | Temperature, 0.1 degC |

### (17,2) connection test report

No source data.

---

## 8. Failure codes

| Code | Name | Meaning |
|---|---|---|
| 1 | NULL | Mandatory pointer argument was null |
| 2 | BUFFER_TOO_SMALL | Output buffer cannot hold the result |
| 3 | TRUNCATED | Packet shorter than its header claims |
| 4 | BAD_VERSION | CCSDS or PUS version number invalid |
| 5 | BAD_PACKET_TYPE | Telemetry packet received on the uplink |
| 6 | NO_SEC_HEADER | Secondary header flag not set |
| 7 | BAD_APID | APID not routed to this application |
| 8 | BAD_CRC | Packet error control field mismatch |
| 9 | UNKNOWN_SERVICE | No handler for this service type |
| 10 | UNKNOWN_SUBTYPE | No handler for this message subtype |
| 11 | BAD_LENGTH | Application data length not as expected |
| 12 | BAD_PARAMETER | Parameter value outside its range |
| 13 | UNKNOWN_SID | Housekeeping structure ID not defined |
| 14 | UNKNOWN_EVENT | Event identifier not defined |
| 15 | UNKNOWN_FUNCTION | Function identifier not defined |
| 16 | QUEUE_FULL | Downlink queue overflow |
| 17 | NOT_PERMITTED | Rejected in the current operating mode |

## 9. Thermal set points

| Parameter | Value |
|---|---|
| Heater switch-on threshold | 5.0 degC (50) |
| Heater switch-off threshold | 25.0 degC (250) |
| Over-temperature alarm | 40.0 degC (400) |
| Minor cycle | 100 ms |
| Housekeeping period | 10 minor cycles |

## 10. Deviations from ECSS-E-ST-70-41C

| ID | Deviation | Rationale |
|---|---|---|
| D-01 | One message type counter per service type instead of one per (APID, type, subtype) | A full table costs 128 KiB. The application emits a single APID and the ground segment uses the counter only for per-service gap detection. |
| D-02 | Service 1 start and progress reports (subtypes 3 to 6) are not implemented | No service in this subset executes across more than one minor cycle, so start and completion would always coincide. |
