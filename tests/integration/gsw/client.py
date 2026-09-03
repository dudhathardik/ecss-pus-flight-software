"""Telecommand / telemetry link to the on-board software under test."""

import socket
import time

from .packets import PRIMARY_HEADER_LEN, build_tc, parse_tm


class TmTimeout(AssertionError):
    """Raised when the expected telemetry does not arrive in time."""


class ObcLink:
    """A TC/TM port, plus the small amount of book-keeping tests need.

    Telemetry that arrives while waiting for a specific packet is kept in
    ``self.received`` instead of being discarded, so a test can assert on the
    ordering of everything the unit sent.
    """

    def __init__(self, host="127.0.0.1", port=12345, timeout=5.0):
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.sock.settimeout(timeout)
        self.received = []
        self._pending = []

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    # -- uplink ----------------------------------------------------------

    def send_raw(self, packet):
        """Put raw octets on the uplink, defects included."""
        self.sock.sendall(packet)

    def send_tc(self, service, subtype, data=b"", **kwargs):
        """Build and send a telecommand; returns the octets that were sent."""
        packet = build_tc(service, subtype, data, **kwargs)
        self.send_raw(packet)
        return packet

    # -- downlink --------------------------------------------------------

    def _read_exact(self, count, deadline):
        chunks = bytearray()
        while len(chunks) < count:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TmTimeout("timed out after {} of {} octets"
                                .format(len(chunks), count))
            self.sock.settimeout(remaining)
            try:
                chunk = self.sock.recv(count - len(chunks))
            except socket.timeout:
                raise TmTimeout("timed out after {} of {} octets"
                                .format(len(chunks), count))
            if not chunk:
                raise TmTimeout("link closed by the on-board software")
            chunks += chunk
        return bytes(chunks)

    def read_tm(self, timeout=2.0):
        """Read exactly one telemetry packet off the link."""
        if self._pending:
            return self._pending.pop(0)

        deadline = time.monotonic() + timeout
        header = self._read_exact(PRIMARY_HEADER_LEN, deadline)
        length_field = int.from_bytes(header[4:6], "big")
        body = self._read_exact(length_field + 1, deadline)
        packet = parse_tm(header + body)
        self.received.append(packet)
        return packet

    def expect_where(self, predicate, description, timeout=2.0):
        """Wait for the first packet satisfying ``predicate``.

        Packets that do not match are put back, so a later expectation can
        still consume them: an assertion must not silently swallow the
        telemetry another assertion is about to look for.
        """
        for index, packet in enumerate(self._pending):
            if predicate(packet):
                return self._pending.pop(index)

        deadline = time.monotonic() + timeout
        skipped = []
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                self._pending.extend(skipped)
                raise TmTimeout("no {} within {} s; saw {}".format(
                    description, timeout,
                    [(p.service, p.subtype) for p in skipped]))
            try:
                packet = self.read_tm(timeout=remaining)
            except TmTimeout:
                self._pending.extend(skipped)
                raise TmTimeout("no {} within {} s; saw {}".format(
                    description, timeout,
                    [(p.service, p.subtype) for p in skipped]))
            if predicate(packet):
                self._pending.extend(skipped)
                return packet
            skipped.append(packet)

    def expect(self, service, subtype, timeout=2.0):
        """Wait for a specific (service, subtype)."""
        return self.expect_where(
            lambda p: (p.service, p.subtype) == (service, subtype),
            "({},{})".format(service, subtype), timeout)

    def expect_event(self, event_id, subtype=None, timeout=2.0):
        """Wait for a service 5 report carrying ``event_id``."""
        def matches(packet):
            if packet.service != 5 or packet.subtype > 4:
                return False
            if subtype is not None and packet.subtype != subtype:
                return False
            return packet.event_id == event_id

        return self.expect_where(matches, "event {}".format(event_id), timeout)

    def expect_hk(self, sid, timeout=2.0):
        """Wait for a (3,25) report of the given structure identifier."""
        return self.expect_where(
            lambda p: (p.service, p.subtype) == (3, 25) and p.data[:1] == bytes([sid]),
            "housekeeping SID {}".format(sid), timeout)

    def expect_none(self, service, subtype, window=0.5):
        """Assert that no such packet arrives within ``window`` seconds."""
        deadline = time.monotonic() + window
        seen = []
        try:
            while True:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return
                try:
                    packet = self.read_tm(timeout=remaining)
                except TmTimeout:
                    return
                if (packet.service, packet.subtype) == (service, subtype):
                    raise AssertionError(
                        "unexpected ({},{}) received: {}".format(
                            service, subtype, packet))
                seen.append(packet)
        finally:
            self._pending.extend(seen)

    def flush(self, window=0.1):
        """Discard the telemetry received so far, buffer and link alike.

        Used before a state query so that the answer cannot be satisfied by a
        periodic report generated before the command under test.
        """
        while True:
            try:
                self.read_tm(timeout=window)
            except TmTimeout:
                return

    def drain(self, window=0.4):
        """Collect everything the unit sends over a short quiet window."""
        collected = list(self._pending)
        self._pending = []
        deadline = time.monotonic() + window
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return collected
            try:
                collected.append(self.read_tm(timeout=remaining))
            except TmTimeout:
                return collected
