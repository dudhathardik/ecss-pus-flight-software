"""Fixtures for the functional / hardware-software integration test suite.

The software under test runs as a separate process behind a TC/TM socket, so
these cases exercise the same interface the ground segment will use: nothing
here can reach into the on-board state directly, which is the point.
"""

import pathlib
import socket
import subprocess

import pytest

from gsw import ObcLink

ROOT = pathlib.Path(__file__).resolve().parents[2]
BINARY = ROOT / "build" / "obc_sim"

# The runner is driven faster than flight so that a housekeeping period is
# 200 ms rather than 1 s. Nothing in the software under test knows the
# difference: the time base is the tick counter.
TICK_MS = 20
HK_PERIOD_S = TICK_MS * 10 / 1000.0


def _free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.bind(("127.0.0.1", 0))
        return probe.getsockname()[1]


@pytest.fixture(scope="session")
def sim_port():
    """Start obc_sim once for the whole session and return its port."""
    if not BINARY.exists():
        pytest.fail("{} not built - run 'make' first".format(BINARY))

    port = _free_port()
    proc = subprocess.Popen(
        [str(BINARY), "--port", str(port), "--tick-ms", str(TICK_MS), "--quiet"],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        universal_newlines=True,
    )
    banner = proc.stdout.readline()
    if "listening" not in banner:
        proc.kill()
        pytest.fail("obc_sim did not start: {!r}".format(banner))

    yield port

    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()


@pytest.fixture
def link(sim_port):
    """A raw TC/TM link. The on-board software reboots on every connection."""
    connection = ObcLink(port=sim_port)
    yield connection
    connection.close()


@pytest.fixture
def obc(link):
    """A link on which the power-on event report has already been collected."""
    link.expect(5, 1, timeout=2.0)
    return link


@pytest.fixture
def obc_quiet(obc):
    """A link with periodic housekeeping inhibited.

    Several cases assert on the exact sequence of packets a telecommand
    produces. Periodic reports would interleave with that sequence, so they
    are switched off first - the same thing an operator does before running a
    packet-level procedure.
    """
    obc.send_tc(3, 6, bytes([1, 1]))   # (3,6) disable, 1 structure, SID 1
    obc.expect(1, 7)
    obc.drain(window=HK_PERIOD_S * 1.5)  # flush anything already generated
    return obc


@pytest.fixture
def hk_period():
    """Housekeeping generation period of the runner, in seconds."""
    return HK_PERIOD_S
