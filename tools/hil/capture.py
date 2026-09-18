"""Shared timeout-bounded serial capture/assert helper for tools/hil/*.py
(T128) -- extracted from the duplicated pattern in boot_restore_check.py,
quality_gate_check.py, and friends: open the port, read line-by-line with a
hard overall deadline, parse the `[TAG] token key=value ...` grammar
(contracts/serial-log.md), and always release the port (even on an
assertion failure or KeyboardInterrupt) before returning -- so a script using
this helper never accidentally holds the port open across a subsequent
`pio run --target upload`.
"""

import time

try:
    import serial
except ImportError:  # pragma: no cover - surfaced by the caller's own import
    serial = None


def parse_line(line):
    """Parses one `[TAG] token key=value ...` line into (tag, tokens).

    Returns (None, []) for a line that doesn't start with a recognizable
    `[TAG]` prefix (e.g. boot-time noise before logging is initialized).
    """
    line = line.strip()
    if not line.startswith("["):
        return None, []
    try:
        tag_end = line.index("]")
    except ValueError:
        return None, []
    tag = line[1:tag_end]
    tokens = line[tag_end + 1:].strip().split()
    return tag, tokens


class SerialCapture:
    """Context-managed serial port that guarantees release on exit.

    Usage:
        with SerialCapture(port, baud) as cap:
            match = cap.wait_for(lambda tag, tokens: tag == "IMU" and "valid=1" in tokens,
                                  timeout_s=10.0)
    """

    def __init__(self, port, baud=115200, read_timeout_s=1.0):
        if serial is None:
            raise RuntimeError("pyserial is required (pip install -r tools/hil/requirements.txt)")
        self._ser = serial.Serial(port, baud, timeout=read_timeout_s)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self._ser.close()
        return False  # never swallow an exception

    def read_line(self):
        """Reads one line (blocking up to the port's own read timeout),
        returning "" if nothing arrived. Never raises on a decode error."""
        raw = self._ser.readline()
        if not raw:
            return ""
        try:
            return raw.decode("utf-8", errors="replace")
        except Exception:
            return ""

    def wait_for(self, predicate, timeout_s, on_line=None):
        """Reads lines until `predicate(tag, tokens)` returns true or
        `timeout_s` elapses. `on_line`, if given, is called with the raw
        line for every line read (e.g. for progress printing). Returns the
        raw line that matched, or None on timeout."""
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            line = self.read_line()
            if not line:
                continue
            if on_line is not None:
                on_line(line)
            tag, tokens = parse_line(line)
            if predicate(tag, tokens):
                return line
        return None
