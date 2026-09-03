# Software Requirements Specification

**Project:** OBSW-PUS demonstrator — PUS service handler for a spacecraft
on-board computer
**Issue:** 1.0
**Applicable standards:** ECSS-E-ST-40C (software engineering),
ECSS-Q-ST-80C (software product assurance), ECSS-E-ST-70-41C (telemetry and
telecommand packet utilisation), CCSDS 133.0-B (space packet protocol)

---

## 1. Scope

This document specifies the software requirements for the on-board software
(OBSW) of a representative spacecraft on-board computer. The OBSW terminates
the telecommand uplink, produces the telemetry downlink, and runs one
autonomous control function — survival heater control — so that the
verification approach can be demonstrated on something with real behaviour
rather than on packet plumbing alone.

The demonstrator is a portfolio piece, not flight software: it implements a
representative subset of a real unit's requirement baseline (six services out
of the twenty defined by ECSS-E-ST-70-41C) with the same process artefacts.

## 2. How to read this document

Each requirement is written as

> `- **SWREQ-<area>-<number>** (<verification method>) The OBSW shall ...`

The verification method is one of:

| Method | Meaning |
|---|---|
| Test | Verified by executing a test case that carries a matching `@verifies` tag |
| Analysis | Verified by an automated check over the sources |
| Inspection | Verified by reading the code during the peer review |
| Review | Verified by reviewing a document |

Every requirement is traced to the code that implements it by an
`@implements` tag, and every requirement whose method is *Test* is traced to
at least one test case. `tools/trace_matrix.py` derives
[traceability.md](traceability.md) from those tags and fails the build if a
requirement loses either link.

## 3. Definitions

| Term | Meaning |
|---|---|
| APID | Application process identifier, 11 bits of the CCSDS primary header |
| CUC | CCSDS unsegmented time code |
| Minor cycle | One period of the OBSW scheduler, 100 ms |
| PUS | Packet utilisation standard, ECSS-E-ST-70-41C |
| SID | Structure identifier of a housekeeping report |
| TC / TM | Telecommand / telemetry |

---

## 4. Space packet protocol

- **SWREQ-CCSDS-010** (Test) The OBSW shall encode and decode the six-octet CCSDS space packet primary header comprising packet version number, packet type, secondary header flag, APID, sequence flags, packet sequence count and packet data length, with every field transmitted most significant octet first.
- **SWREQ-CCSDS-020** (Test) The OBSW shall reject any telecommand whose packet version number is not zero.
- **SWREQ-CCSDS-030** (Test) The OBSW shall compute the total length of a space packet as six octets of primary header plus the packet data length field plus one.

## 5. Packet error control

- **SWREQ-CRC-010** (Test) The OBSW shall compute the packet error control field as a CRC-16-CCITT over the whole packet excluding the field itself, using polynomial 0x1021, seed 0xFFFF, no input or output reflection and no final exclusive-or.
- **SWREQ-CRC-020** (Test) The OBSW shall reject any telecommand whose packet error control field does not match the computed checksum.

## 6. Telecommand acceptance

- **SWREQ-TC-010** (Test) The OBSW shall decode the five-octet telecommand secondary header comprising PUS version number, acknowledgement flags, service type, message subtype and source identifier, and shall expose the remaining octets as the application data field.
- **SWREQ-TC-015** (Test) The OBSW shall perform the acceptance checks in the following order: buffer size, declared length consistency, packet error control field, packet version number, packet type, secondary header flag, APID, PUS version number. No field shall be interpreted before the packet error control field has verified.
- **SWREQ-TC-020** (Test) The OBSW shall reject any telecommand shorter than thirteen octets.
- **SWREQ-TC-030** (Test) The OBSW shall reject any telecommand whose declared packet data length does not match the number of octets delivered by the transport layer.
- **SWREQ-TC-040** (Test) The OBSW shall reject any packet received on the uplink whose packet type field indicates telemetry.
- **SWREQ-TC-050** (Test) The OBSW shall reject any telecommand whose secondary header flag is not set.
- **SWREQ-TC-060** (Test) The OBSW shall reject any telecommand whose APID differs from the configured telecommand APID.
- **SWREQ-TC-070** (Test) The OBSW shall reject any telecommand whose PUS version number is not two.
- **SWREQ-TC-080** (Test) The OBSW shall make the acknowledgement flags of an accepted telecommand available to the services that generate the verification reports.

## 7. Telemetry generation

- **SWREQ-TM-010** (Test) The OBSW shall emit telemetry packets carrying a thirteen-octet secondary header comprising PUS version number, spacecraft time reference status, service type, message subtype, message type counter, destination identifier and a six-octet CUC time field.
- **SWREQ-TM-020** (Test) The OBSW shall increment the packet sequence count by one for every telemetry packet emitted, wrapping at 16383.
- **SWREQ-TM-030** (Test) The OBSW shall maintain an independent message type counter per service type. *Deviation D-01 from ECSS-E-ST-70-41C, which requires one counter per (APID, service type, message subtype); see rationale in `src/pus_tm.c`.*
- **SWREQ-TM-040** (Test) The OBSW shall append a valid packet error control field to every telemetry packet.
- **SWREQ-TM-050** (Test) The OBSW shall report a buffer error rather than write beyond the end of the destination buffer when the requested source data does not fit.
- **SWREQ-TM-060** (Test) The OBSW shall place every generated telemetry packet on the downlink queue.

## 8. Downlink queue

- **SWREQ-TMQ-010** (Test) The OBSW shall hold telemetry awaiting downlink in a first-in first-out queue of at least sixteen packets.
- **SWREQ-TMQ-020** (Test) On queue overflow the OBSW shall discard the newest packet, preserve the packets already queued, and increment a saturating overflow counter.
- **SWREQ-TMQ-030** (Analysis) The OBSW shall allocate all of its memory statically; no dynamic allocation function shall appear in the flight sources. *Verified by the `make static` check, which fails on any use of malloc, calloc, realloc, free or alloca.*

## 9. Service 1 — request verification

- **SWREQ-SVC1-010** (Test) The OBSW shall emit a (1,1) successful acceptance report for every telecommand that passes the acceptance checks and whose acknowledgement flags request it.
- **SWREQ-SVC1-020** (Test) The OBSW shall emit a (1,2) failed acceptance report, carrying the request identifier and the failure code, for every telecommand that fails the acceptance checks.
- **SWREQ-SVC1-030** (Test) The OBSW shall emit a (1,7) or (1,8) completion report for every accepted telecommand whose acknowledgement flags request it, according to whether execution succeeded.
- **SWREQ-SVC1-040** (Test) The OBSW shall set the request identifier of every verification report to the first four octets of the primary header of the telecommand being reported on.
- **SWREQ-SVC1-050** (Test) The OBSW shall emit the failed acceptance report irrespective of the acknowledgement flags, and shall use a request identifier of zero when fewer than four octets were received.
- **SWREQ-SVC1-060** (Test) The OBSW shall emit the reports of one telecommand in the order acceptance report, service response, completion report.

## 10. Service 3 — housekeeping reporting

- **SWREQ-SVC3-010** (Test) The OBSW shall define two housekeeping structures: an essential structure (SID 1) and a diagnostic structure (SID 2).
- **SWREQ-SVC3-020** (Test) The OBSW shall report each housekeeping structure as a (3,25) packet whose source data follows the layout given in [ICD.md](ICD.md).
- **SWREQ-SVC3-030** (Test) The OBSW shall generate a report for every enabled structure once every ten minor cycles.
- **SWREQ-SVC3-040** (Test) The OBSW shall enable and disable periodic generation per structure on receipt of a (3,5) or (3,6) telecommand.
- **SWREQ-SVC3-050** (Test) The OBSW shall generate a single report for each structure listed in a (3,27) telecommand, whether or not periodic generation is enabled for it, and without changing the periodic configuration.
- **SWREQ-SVC3-060** (Test) The OBSW shall reject a service 3 telecommand that names an undefined structure identifier, and shall leave the configuration of every structure unchanged.
- **SWREQ-SVC3-070** (Test) The OBSW shall enable periodic generation of the essential structure, and only that structure, at power-on.

## 11. Service 5 — event reporting

- **SWREQ-SVC5-010** (Test) The OBSW shall report an event as a service 5 packet whose message subtype is the event severity and whose source data is the two-octet event identifier followed by a two-octet auxiliary word.
- **SWREQ-SVC5-020** (Test) The OBSW shall enable and disable reporting per event identifier on receipt of a (5,6) or (5,5) telecommand.
- **SWREQ-SVC5-030** (Test) The OBSW shall not emit a report for a disabled event identifier, and shall not treat the suppression as an error.
- **SWREQ-SVC5-040** (Test) The OBSW shall reject a service 5 telecommand that names an undefined event identifier, and shall leave the configuration of every identifier unchanged.

## 12. Service 8 — function management

- **SWREQ-SVC8-010** (Test) The OBSW shall interpret the application data of an (8,1) telecommand as a one-octet function identifier followed by the arguments of that function.
- **SWREQ-SVC8-020** (Test) The OBSW shall change the operating mode on function 1, rejecting any value other than SAFE or NOMINAL.
- **SWREQ-SVC8-030** (Test) The OBSW shall command the survival heater on function 2, and shall refuse the command while the unit is in SAFE mode.
- **SWREQ-SVC8-040** (Test) The OBSW shall override the acquired temperature on function 3, for test purposes.
- **SWREQ-SVC8-050** (Test) The OBSW shall reject an (8,1) telecommand that names an undefined function identifier.
- **SWREQ-SVC8-060** (Test) The OBSW shall reject an (8,1) telecommand whose argument length does not match the selected function.
- **SWREQ-SVC8-070** (Test) The OBSW shall set the on-board time on function 5, from a six-octet CUC value, such that the time reported afterwards continues to advance from the commanded value.

## 13. Service 17 — connection test

- **SWREQ-SVC17-010** (Test) The OBSW shall answer a (17,1) telecommand with a (17,2) report carrying no source data.
- **SWREQ-SVC17-020** (Test) The OBSW shall reject a (17,1) telecommand that carries application data, and any other service 17 subtype.

## 14. Application

- **SWREQ-APP-010** (Test) The OBSW shall enter SAFE mode at power-on, with the survival heater off and all counters at zero.
- **SWREQ-APP-020** (Test) The OBSW shall report an informative event on completion of initialisation.
- **SWREQ-APP-030** (Test) The OBSW shall report an informative event carrying the new mode on every mode transition, and shall treat a command to the mode already active as a no-operation.
- **SWREQ-APP-040** (Test) In NOMINAL mode the OBSW shall switch the survival heater on when the acquired temperature is at or below 5.0 degrees Celsius.
- **SWREQ-APP-050** (Test) In NOMINAL mode the OBSW shall switch the survival heater off when the acquired temperature is at or above 25.0 degrees Celsius.
- **SWREQ-APP-060** (Test) Between the two switching thresholds the OBSW shall leave the survival heater in its current state.
- **SWREQ-APP-070** (Test) The OBSW shall report a high severity event and enter SAFE mode when the acquired temperature reaches 40.0 degrees Celsius, shall report that event once per excursion, and shall re-arm it once the temperature has fallen below 25.0 degrees Celsius.
- **SWREQ-APP-080** (Test) In SAFE mode the OBSW shall hold the survival heater off regardless of temperature and regardless of ground commanding.
- **SWREQ-APP-090** (Test) The OBSW shall count accepted and rejected telecommands, retain the failure code of the most recent rejection, and reset all three on function 4.
- **SWREQ-APP-100** (Test) The OBSW shall reject a telecommand addressed to a service it does not implement, after having accepted it.
- **SWREQ-APP-110** (Test) The OBSW shall count elapsed minor cycles in an uptime counter published in housekeeping.
- **SWREQ-APP-120** (Test) The OBSW shall report a low severity event when the downlink queue overflow counter changes, once the queue has room again.
- **SWREQ-APP-130** (Test) The OBSW shall report an informative event on every change of the survival heater line, whatever commanded the change.

## 15. Platform abstraction

- **SWREQ-HAL-010** (Test) The OBSW shall derive the CUC time stamp of every telemetry packet from the minor cycle counter, so that a given command sequence produces identical time stamps on every run of the host build.
- **SWREQ-HAL-020** (Test) The OBSW shall provide an entry point to set the on-board time, and shall continue to advance time from the value commanded.

## 16. Robustness

- **SWREQ-ROB-010** (Test) Every externally callable function of the OBSW shall validate its pointer arguments and return a status code rather than dereference a null pointer.
- **SWREQ-ROB-020** (Test) The OBSW shall reject a telecommand whose application data is malformed, and shall apply no part of a rejected telecommand.
- **SWREQ-ROB-030** (Test) A rejected telecommand shall not affect the ability of the OBSW to accept the telecommands that follow it.
- **SWREQ-ROB-040** (Test) The OBSW shall process telecommands arriving faster than one per minor cycle without dropping any of them.
- **SWREQ-ROB-050** (Test) The telecommand reception path shall discard a packet whose declared length exceeds the receive buffer, and shall report the rejection to ground. *Allocated to the platform driver, `sim/obc_sim.c` in the host build.*

---

## 17. Requirements deliberately not implemented

The following are part of a real unit's baseline and are consciously out of
scope here. They are listed so that the boundary of the demonstrator is
explicit rather than accidental.

| Area | Why it is out of scope |
|---|---|
| Service 6 memory management | Needs a real memory map and a boot loader |
| Service 11 time-based scheduling | Needs a persistent schedule store |
| Service 15 on-board storage and retrieval | Needs a mass memory model |
| Service 23 file management | Needs a file system |
| Redundancy management, watchdog servicing | Needs the target hardware |
| SpaceWire / CAN drivers | Replaced by the host TCP transport |
