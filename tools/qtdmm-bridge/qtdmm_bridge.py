#!/usr/bin/env python3
# Copyright (c) 2026 The QtDMM developers
# SPDX-License-Identifier: GPL-3.0-or-later
"""qtdmm-bridge: serve meters on a small box (Raspberry Pi, ...) to QtDMM
over the network, RFC 2217 style.

One TCP port per meter. QtDMM connects with its port type "RFC2217" and sends
the meter's line settings (baud rate, data bits, parity, stop bits, DTR/RTS)
itself, so the bridge needs to know nothing about the meter - it is a dumb
pipe that understands the telnet COM-PORT-OPTION commands.

    qtdmm_bridge.py --port 4000=/dev/ttyUSB0 --port 4001=/dev/serial/by-id/usb-...
    qtdmm_bridge.py --port 4002=hid:1a86:e008          # a meter on a USB-HID cable (Linux)
    qtdmm_bridge.py -c /etc/qtdmm-bridge.toml
    qtdmm_bridge.py --list

Requires Python 3.11 (tomllib) and pyserial. No other dependencies.

The bridge accepts one client per port; a new connection replaces the old
one, so a QtDMM that reconnects after a network hiccup gets in at once. When
the device disappears (USB unplugged) the TCP connection stays up and the
bridge waits for the device to come back.

NOTE: there is no authentication or encryption. Bind to 127.0.0.1 and use an
SSH tunnel outside a trusted network.
"""

from __future__ import annotations

import argparse
import asyncio
import dataclasses
import logging
import os
import signal
import socket
import sys
import threading
from typing import Callable, Optional

__version__ = "0.2.0"

log = logging.getLogger("qtdmm-bridge")

# ---------------------------------------------------------------------------
# telnet / RFC 2217 constants

IAC = 0xFF
DONT = 0xFE
DO = 0xFD
WONT = 0xFC
WILL = 0xFB
SB = 0xFA
SE = 0xF0
NOP = 0xF1

OPT_BINARY = 0
OPT_ECHO = 1
OPT_SGA = 3
OPT_COM_PORT = 44  # RFC 2217 COM-PORT-OPTION

# client -> server suboption commands; the server answers with cmd + 100
SIGNATURE = 0
SET_BAUDRATE = 1
SET_DATASIZE = 2
SET_PARITY = 3
SET_STOPSIZE = 4
SET_CONTROL = 5
NOTIFY_LINESTATE = 6
NOTIFY_MODEMSTATE = 7
FLOWCONTROL_SUSPEND = 8
FLOWCONTROL_RESUME = 9
SET_LINESTATE_MASK = 10
SET_MODEMSTATE_MASK = 11
PURGE_DATA = 12
SERVER_OFFSET = 100

# SET-PARITY values (RFC 2217 section 2.6: 1 none, 2 odd, 3 even, 4 mark, 5 space)
PARITY_NAMES = {1: "N", 2: "O", 3: "E", 4: "M", 5: "S"}
PARITY_CODES = {v: k for k, v in PARITY_NAMES.items()}
# SET-STOPSIZE values
STOP_NAMES = {1: 1, 2: 2, 3: 1.5}
STOP_CODES = {v: k for k, v in STOP_NAMES.items()}

# SET-CONTROL values (RFC 2217 section 2.7)
CTRL_REQ_FLOW = 0
CTRL_FLOW_NONE = 1
CTRL_FLOW_XONXOFF = 2
CTRL_FLOW_HARDWARE = 3
CTRL_REQ_BREAK = 4
CTRL_BREAK_ON = 5
CTRL_BREAK_OFF = 6
CTRL_REQ_DTR = 7
CTRL_DTR_ON = 8
CTRL_DTR_OFF = 9
CTRL_REQ_RTS = 10
CTRL_RTS_ON = 11
CTRL_RTS_OFF = 12

SIGNATURE_TEXT = f"qtdmm-bridge {__version__}"


# ---------------------------------------------------------------------------
# backends


@dataclasses.dataclass
class LineSettings:
    """What QtDMM asks for; the backend applies what it can."""

    baudrate: int = 9600
    bytesize: int = 8
    parity: str = "N"
    stopbits: float = 1
    dtr: bool = True
    rts: bool = True


class Backend:
    """A meter-side device. Subclasses are serial ports (here) and, later,
    the USB-HID cables. All methods may be called from the asyncio thread;
    ``on_data`` is called from any thread."""

    name = "?"

    def __init__(self, on_data: Callable[[bytes], None], on_error: Callable[[str], None]):
        self.on_data = on_data
        self.on_error = on_error
        self.settings = LineSettings()

    def open(self) -> None:  # raises on failure
        raise NotImplementedError

    def close(self) -> None:
        raise NotImplementedError

    @property
    def is_open(self) -> bool:
        raise NotImplementedError

    def write(self, data: bytes) -> None:
        raise NotImplementedError

    def apply(self, settings: LineSettings) -> LineSettings:
        """Applies line settings; returns what the device actually runs with."""
        self.settings = settings
        return settings

    def purge(self, what: int) -> None:
        pass


class SerialBackend(Backend):
    """A serial port through pyserial, with a reader thread."""

    def __init__(self, device: str, on_data, on_error):
        super().__init__(on_data, on_error)
        self.name = device
        self._device = device
        self._serial = None
        self._thread: Optional[threading.Thread] = None
        self._alive = False
        self._no_control_lines = False
        self._warned: set = set()

    def open(self) -> None:
        import serial  # lazy: --list and the tests do without it

        ser = serial.serial_for_url(self._device, do_not_open=True)
        ser.timeout = 0.2  # lets the reader thread notice close()
        ser.open()
        self._serial = ser
        self.apply(self.settings)
        self._alive = True
        self._thread = threading.Thread(target=self._reader, name=f"read {self._device}", daemon=True)
        self._thread.start()
        log.info("%s: opened", self._device)

    def close(self) -> None:
        self._alive = False
        ser, self._serial = self._serial, None
        if ser is not None:
            try:
                ser.close()
            except Exception:  # noqa: BLE001 - closing a vanished device
                pass
        if self._thread is not None and self._thread is not threading.current_thread():
            self._thread.join(timeout=2)
        self._thread = None

    @property
    def is_open(self) -> bool:
        return self._serial is not None and self._alive

    def write(self, data: bytes) -> None:
        ser = self._serial
        if ser is None:
            return
        try:
            ser.write(data)
        except Exception as exc:  # noqa: BLE001
            self._fail(f"write failed: {exc}")

    def apply(self, settings: LineSettings) -> LineSettings:
        self.settings = settings
        ser = self._serial
        if ser is not None:
            try:
                ser.baudrate = settings.baudrate
                ser.bytesize = settings.bytesize
                ser.parity = settings.parity
                ser.stopbits = settings.stopbits
            except Exception as exc:  # noqa: BLE001
                # not fatal: the port stays open with what it accepted (a pty,
                # for instance, takes no 7-bit frames)
                key = (settings.baudrate, settings.bytesize, settings.parity, settings.stopbits)
                if key not in self._warned:
                    self._warned.add(key)
                    log.warning("%s: cannot apply %d %d%s%g: %s", self._device, settings.baudrate,
                                settings.bytesize, settings.parity, settings.stopbits, exc)
            self._set_control_lines(settings)
        return settings

    def _set_control_lines(self, settings: LineSettings) -> None:
        """DTR/RTS power many meter cables; a pty or a bare UART has no such
        lines, which is no reason to give up on the device."""
        ser = self._serial
        if ser is None:
            return
        try:
            ser.dtr = settings.dtr
            ser.rts = settings.rts
        except Exception as exc:  # noqa: BLE001
            if not self._no_control_lines:
                log.info("%s: no DTR/RTS control (%s)", self._device, exc)
                self._no_control_lines = True

    def purge(self, what: int) -> None:
        ser = self._serial
        if ser is None:
            return
        try:
            if what in (1, 3):
                ser.reset_input_buffer()
            if what in (2, 3):
                ser.reset_output_buffer()
        except Exception:  # noqa: BLE001
            pass

    def _reader(self) -> None:
        while self._alive:
            ser = self._serial
            if ser is None:
                break
            try:
                data = ser.read(ser.in_waiting or 1)
            except Exception as exc:  # noqa: BLE001 - device unplugged
                if self._alive:
                    self._fail(f"read failed: {exc}")
                break
            if data:
                self.on_data(data)

    def _fail(self, msg: str) -> None:
        if not self._alive:
            return
        self._alive = False
        log.warning("%s: %s", self._device, msg)
        self.on_error(msg)


# ---------------------------------------------------------------------------
# USB-HID meter cables (Linux hidraw, no extra library)
#
# The chips and report layouts mirror QtDMM's HIDSerialDevice; keep both in step.

HID_CABLES = {
    (0x04FA, 0x2490): ("CH9325", "Hoitek HE2325U (UT-D04 type)"),
    (0x1A86, 0xE008): ("CH9325", "WCH CH9325 (UT-D04, UT803, ...)"),
    (0x10C4, 0xEA80): ("CP2110", "SiLabs CP2110 (UT-D09, first revision)"),
    (0x1A86, 0xE429): ("CH9329", "WCH CH9329 (UT-D09, second revision, fixed 9600 8N1)"),
    (0x0820, 0x0001): ("BU86X", "Brymen BU-86X"),
}


def hidraw_devices() -> list[tuple[str, int, int]]:
    """(/dev/hidrawN, vid, pid) for every hidraw node (Linux)."""
    out = []
    root = "/sys/class/hidraw"
    if not os.path.isdir(root):
        return out
    for node in sorted(os.listdir(root)):
        try:
            with open(f"{root}/{node}/device/uevent") as f:
                uevent = f.read()
        except OSError:
            continue
        for line in uevent.splitlines():
            if line.startswith("HID_ID="):
                parts = line.split(":")
                if len(parts) == 3:
                    out.append((f"/dev/{node}", int(parts[1], 16), int(parts[2], 16)))
    return out


def unpack_hid_report(chip: str, report: bytes) -> Optional[bytes]:
    """UART bytes carried in one input report; None for a malformed report."""
    if not report:
        return None
    if chip == "BU86X":
        return report  # the whole report is UART data
    if chip in ("CH9329", "CP2110"):
        # @0 count (0..63; on the CP2110 this is the report id), @1.. raw bytes
        count = report[0]
        if count > 63 or count > len(report) - 1:
            return None
        return report[1:1 + count]
    # CH9325: @0 = 0xF0 | count (low 3 bits), @1.. bytes with the top bit set
    count = report[0] & 0x07
    if count > len(report) - 1:
        return None
    return bytes(b & 0x7F for b in report[1:1 + count])


def pack_hid_write(chip: str, data: bytes) -> bytes:
    """Output report for UART bytes (first byte = report id, hidraw style)."""
    data = data[:63]
    if chip == "BU86X":
        return b"\x00" + data
    if chip == "CH9329":
        r = b"\x00" + bytes([len(data)]) + data
        return r + b"\x00" * (65 - len(r))
    if chip == "CP2110":
        return bytes([len(data)]) + data
    # CH9325: count without the 0xF0 of the input reports, up to seven
    # bytes, an 8-byte report (libsigrok serial_hid_ch9325.c); the UT325
    # needs its start byte, most of these cables only receive
    data = data[:7]
    r = b"\x00" + bytes([len(data)]) + data
    return r + b"\x00" * (9 - len(r))


def cp2110_config_report(s: LineSettings) -> bytes:
    """Feature report 0x50: baud big endian, parity (0 none, 1 even, 2 odd),
    flow control, data bits - 5, stop bits (0 = 1, 1 = 2)."""
    baud = min(max(s.baudrate or 9600, 300), 1000000)
    parity = {"N": 0, "E": 1, "O": 2}.get(s.parity, 0)
    bits = s.bytesize if 5 <= s.bytesize <= 8 else 8
    return bytes([0x50]) + baud.to_bytes(4, "big") + bytes([parity, 0, bits - 5, 1 if s.stopbits >= 2 else 0])


def ch9325_config_report(s: LineSettings) -> bytes:
    """Feature report (id 0): baud little endian, then data bits - 5."""
    baud = s.baudrate or 19200
    bits = s.bytesize if 5 <= s.bytesize <= 8 else 8
    return b"\x00" + baud.to_bytes(4, "little") + bytes([bits - 5])


class HidRaw:
    """The few hidraw operations the backend needs; replaced by a fake in tests."""

    HIDIOCSFEATURE = lambda length: (3 << 30) | (length << 16) | (ord("H") << 8) | 0x06  # noqa: E731

    def __init__(self, path: str):
        self.path = path
        self.fd = os.open(path, os.O_RDWR)

    def read(self, timeout: float) -> Optional[bytes]:
        """One report, or None after the timeout."""
        import select

        r, _, _ = select.select([self.fd], [], [], timeout)
        if not r:
            return None
        return os.read(self.fd, 64)

    def write(self, report: bytes) -> None:
        os.write(self.fd, report)

    def send_feature(self, report: bytes) -> None:
        import fcntl

        buf = bytearray(report)
        fcntl.ioctl(self.fd, HidRaw.HIDIOCSFEATURE(len(buf)), buf, True)

    def close(self) -> None:
        if self.fd >= 0:
            os.close(self.fd)
            self.fd = -1


class HidBackend(Backend):
    """A meter behind a USB-HID cable, served like a serial port. The line
    settings QtDMM sends become the cable's feature report where the chip
    takes one (CH9325, CP2110); the others run at their fixed speed."""

    raw_factory = HidRaw  # tests swap in a fake

    def __init__(self, spec: str, on_data, on_error):
        super().__init__(on_data, on_error)
        # spec: "1a86:e008" (first matching cable) or "/dev/hidraw2"
        self.name = f"hid:{spec}"
        self._spec = spec
        self._raw: Optional[HidRaw] = None
        self.chip = "CH9325"
        self._thread: Optional[threading.Thread] = None
        self._alive = False

    def resolve(self) -> tuple[str, str]:
        """(path, chip) for the spec; raises when the cable is not there."""
        devices = hidraw_devices()
        if self._spec.startswith("/"):
            for path, vid, pid in devices:
                if path == self._spec:
                    chip = HID_CABLES.get((vid, pid), ("CH9325", ""))[0]
                    return path, chip
            raise FileNotFoundError(f"{self._spec}: no such HID device")
        try:
            vid_s, pid_s = self._spec.split(":", 1)
            vid, pid = int(vid_s, 16), int(pid_s, 16)
        except ValueError as exc:
            raise ValueError(f"{self._spec}: expected VID:PID (hex) or /dev/hidrawN") from exc
        for path, v, p in devices:
            if (v, p) == (vid, pid):
                return path, HID_CABLES.get((vid, pid), ("CH9325", ""))[0]
        raise FileNotFoundError(f"no HID cable {vid:04x}:{pid:04x} attached")

    def open(self) -> None:
        path, self.chip = self.resolve()
        self._raw = self.raw_factory(path)
        self._alive = True
        try:
            self._configure(self.settings)
        except OSError as exc:
            self._raw.close()
            self._raw = None
            self._alive = False
            raise OSError(f"{path}: feature report failed: {exc}") from exc
        self._thread = threading.Thread(target=self._reader, name=f"read {path}", daemon=True)
        self._thread.start()
        log.info("%s: opened %s (%s)", self.name, path, self.chip)

    def close(self) -> None:
        self._alive = False
        raw, self._raw = self._raw, None
        if raw is not None:
            raw.close()
        if self._thread is not None and self._thread is not threading.current_thread():
            self._thread.join(timeout=2)
        self._thread = None

    @property
    def is_open(self) -> bool:
        return self._raw is not None and self._alive

    def write(self, data: bytes) -> None:
        raw = self._raw
        if raw is None:
            return
        step = 7 if self.chip == "CH9325" else 63   # bytes one report carries
        for i in range(0, len(data), step):
            report = pack_hid_write(self.chip, data[i:i + step])
            if not report:
                return
            try:
                raw.write(report)
            except OSError as exc:
                self._fail(f"write failed: {exc}")
                return

    def apply(self, settings: LineSettings) -> LineSettings:
        self.settings = settings
        if self._raw is not None:
            try:
                self._configure(settings)
            except OSError as exc:
                log.warning("%s: cannot apply line settings: %s", self.name, exc)
        return settings

    def _configure(self, s: LineSettings) -> None:
        raw = self._raw
        if raw is None:
            return
        if self.chip == "CH9325":
            raw.send_feature(ch9325_config_report(s))
        elif self.chip == "CP2110":
            raw.send_feature(b"\x41\x01")   # UART enable
            raw.send_feature(cp2110_config_report(s))
        elif self.chip == "CH9329" and s.baudrate not in (0, 9600):
            log.warning("%s: this cable runs at 9600 baud, the meter wants %d", self.name, s.baudrate)
        # BU-86X: fixed speed, nothing to configure

    def _reader(self) -> None:
        while self._alive:
            raw = self._raw
            if raw is None:
                break
            try:
                report = raw.read(0.2)
            except OSError as exc:  # cable unplugged
                if self._alive:
                    self._fail(f"read failed: {exc}")
                break
            if report:
                data = unpack_hid_report(self.chip, report)
                if data is None:
                    log.debug("%s: malformed report %s", self.name, report.hex(" "))
                elif data:
                    self.on_data(data)

    def _fail(self, msg: str) -> None:
        if not self._alive:
            return
        self._alive = False
        log.warning("%s: %s", self.name, msg)
        self.on_error(msg)


BackendFactory = Callable[[Callable[[bytes], None], Callable[[str], None]], Backend]


def make_backend_factory(device: str) -> BackendFactory:
    """'/dev/ttyUSB0', 'serial:/dev/ttyUSB0', 'loop://' -> SerialBackend;
    'hid:1a86:e008' or 'hid:/dev/hidraw2' -> HidBackend."""
    if device.startswith("hid:"):
        spec = device[len("hid:"):]
        if not sys.platform.startswith("linux"):
            raise SystemExit(f"{device}: HID cables are served through hidraw, Linux only")
        return lambda on_data, on_error: HidBackend(spec, on_data, on_error)
    if device.startswith("serial:"):
        device = device[len("serial:"):]
    return lambda on_data, on_error: SerialBackend(device, on_data, on_error)


# ---------------------------------------------------------------------------
# RFC 2217 session


class Rfc2217Protocol:
    """Telnet/COM-PORT-OPTION state machine for one client.

    Bytes from the client go through ``feed()``: plain data is passed to
    ``on_data`` (for the backend), telnet commands are answered through
    ``send`` and option changes reported through ``on_option``. Data from the
    backend is escaped with ``escape()`` before it goes to the client.
    """

    def __init__(self, send: Callable[[bytes], None], on_data: Callable[[bytes], None],
                 on_option: Callable[[int, bytes], bytes]):
        self._send = send
        self._on_data = on_data
        self._on_option = on_option
        self._state = "data"
        self._sb = bytearray()
        self._pending = bytearray()

    # a COM-PORT-OPTION subnegotiation is a handful of bytes; anything longer
    # is garbage (or an attack) and must not grow the buffer without bound
    MAX_SUBNEGOTIATION = 256

    # --- outgoing -----------------------------------------------------------

    @staticmethod
    def escape(data: bytes) -> bytes:
        return data.replace(bytes([IAC]), bytes([IAC, IAC]))

    def send_suboption(self, cmd: int, payload: bytes = b"") -> None:
        self._send(bytes([IAC, SB, OPT_COM_PORT, cmd]) + self.escape(payload) + bytes([IAC, SE]))

    def send_command(self, cmd: int, option: int) -> None:
        self._send(bytes([IAC, cmd, option]))

    # --- incoming -----------------------------------------------------------

    def feed(self, data: bytes) -> None:
        for byte in data:
            self._feed_byte(byte)
        if self._pending:
            self._on_data(bytes(self._pending))
            self._pending.clear()

    def _feed_byte(self, byte: int) -> None:
        state = self._state
        if state == "data":
            if byte == IAC:
                self._state = "iac"
            else:
                self._pending.append(byte)
        elif state == "iac":
            if byte == IAC:
                self._pending.append(IAC)
                self._state = "data"
            elif byte == SB:
                self._sb.clear()
                self._state = "sb"
            elif byte in (WILL, WONT, DO, DONT):
                self._state = byte
            else:  # NOP, DM, BRK, ... - nothing to do
                self._state = "data"
        elif state in (WILL, WONT, DO, DONT):
            self._negotiate(state, byte)
            self._state = "data"
        elif state == "sb":
            if byte == IAC:
                self._state = "sb_iac"
            elif len(self._sb) >= self.MAX_SUBNEGOTIATION:
                self._sb.clear()
                self._state = "data"
            else:
                self._sb.append(byte)
        elif state == "sb_iac":
            if byte == IAC:
                self._sb.append(IAC)
                self._state = "sb"
            elif byte == SE:
                self._suboption(bytes(self._sb))
                self._state = "data"
            else:  # malformed; drop the subnegotiation
                self._state = "data"

    def _negotiate(self, cmd: int, option: int) -> None:
        # We are happy with binary transmission and the COM-PORT-OPTION in both
        # directions; everything else is declined. (QtDMM does not negotiate at
        # all, it sends the suboptions right away - which we accept as well.)
        wanted = option in (OPT_BINARY, OPT_COM_PORT, OPT_SGA)
        if cmd == WILL:
            self.send_command(DO if wanted else DONT, option)
        elif cmd == DO:
            self.send_command(WILL if wanted else WONT, option)
        # WONT/DONT need no answer

    def _suboption(self, payload: bytes) -> None:
        if len(payload) < 2 or payload[0] != OPT_COM_PORT:
            return
        cmd, value = payload[1], payload[2:]
        if cmd == SIGNATURE:
            if value:
                log.info("client signature: %s", value.decode("ascii", "replace"))
            self.send_suboption(SIGNATURE + SERVER_OFFSET, SIGNATURE_TEXT.encode("ascii"))
            return
        reply = self._on_option(cmd, value)
        if reply is not None:
            self.send_suboption(cmd + SERVER_OFFSET, reply)


class PortSession:
    """Glue between one client connection, its protocol and the backend."""

    def __init__(self, name: str, backend: Backend, loop: asyncio.AbstractEventLoop,
                 writer: asyncio.StreamWriter):
        self.name = name
        self.backend = backend
        self._loop = loop
        self._writer = writer
        self._closed = False
        self._overflow = False
        self.protocol = Rfc2217Protocol(self._send, self._to_backend, self._option)

    # client side
    # a client that stopped reading (half-open TCP, laptop asleep) must not make
    # us buffer meter data without bound; beyond this the data is dropped
    MAX_WRITE_BUFFER = 64 * 1024

    def _send(self, data: bytes) -> None:
        if self._closed:
            return
        try:
            transport = self._writer.transport
            if transport.get_write_buffer_size() > self.MAX_WRITE_BUFFER:
                if not self._overflow:
                    self._overflow = True
                    log.warning("%s: client is not reading, dropping data", self.name)
                return
            self._overflow = False
            self._writer.write(data)
        except Exception:  # noqa: BLE001
            self._closed = True

    def feed(self, data: bytes) -> None:
        self.protocol.feed(data)

    # backend side (may run on the reader thread)
    def from_backend(self, data: bytes) -> None:
        try:
            self._loop.call_soon_threadsafe(self._send, self.protocol.escape(data))
        except RuntimeError:  # loop already closed, reader thread still winding down
            pass

    def _to_backend(self, data: bytes) -> None:
        self.backend.write(data)

    def _option(self, cmd: int, value: bytes) -> Optional[bytes]:
        s = dataclasses.replace(self.backend.settings)
        if cmd == SET_BAUDRATE:
            if len(value) == 4:
                baud = int.from_bytes(value, "big")
                if baud:
                    s.baudrate = baud
            s = self.backend.apply(s)
            log.info("%s: %d baud", self.name, s.baudrate)
            return s.baudrate.to_bytes(4, "big")
        if cmd == SET_DATASIZE:
            if value and value[0] in (5, 6, 7, 8):
                s.bytesize = value[0]
            s = self.backend.apply(s)
            return bytes([s.bytesize])
        if cmd == SET_PARITY:
            if value and value[0] in PARITY_NAMES:
                s.parity = PARITY_NAMES[value[0]]
            s = self.backend.apply(s)
            return bytes([PARITY_CODES[s.parity]])
        if cmd == SET_STOPSIZE:
            if value and value[0] in STOP_NAMES:
                s.stopbits = STOP_NAMES[value[0]]
            s = self.backend.apply(s)
            return bytes([STOP_CODES[s.stopbits]])
        if cmd == SET_CONTROL:
            if not value:
                return None
            v = value[0]
            if v == CTRL_DTR_ON:
                s.dtr = True
            elif v == CTRL_DTR_OFF:
                s.dtr = False
            elif v == CTRL_RTS_ON:
                s.rts = True
            elif v == CTRL_RTS_OFF:
                s.rts = False
            elif v == CTRL_REQ_FLOW:
                return bytes([CTRL_FLOW_NONE])
            elif v == CTRL_REQ_DTR:
                return bytes([CTRL_DTR_ON if s.dtr else CTRL_DTR_OFF])
            elif v == CTRL_REQ_RTS:
                return bytes([CTRL_RTS_ON if s.rts else CTRL_RTS_OFF])
            else:  # flow control / break: not supported, echo the request
                return bytes([v])
            s = self.backend.apply(s)
            log.info("%s: DTR %s, RTS %s", self.name, "on" if s.dtr else "off", "on" if s.rts else "off")
            if v in (CTRL_DTR_ON, CTRL_DTR_OFF):
                return bytes([CTRL_DTR_ON if s.dtr else CTRL_DTR_OFF])
            return bytes([CTRL_RTS_ON if s.rts else CTRL_RTS_OFF])
        if cmd == PURGE_DATA:
            if value:
                self.backend.purge(value[0])
            return value[:1]
        if cmd in (SET_LINESTATE_MASK, SET_MODEMSTATE_MASK):
            return value[:1]  # accepted, but we never notify
        if cmd in (FLOWCONTROL_SUSPEND, FLOWCONTROL_RESUME):
            return b""
        return None

    def close(self) -> None:
        self._closed = True


# ---------------------------------------------------------------------------
# server


@dataclasses.dataclass
class PortConfig:
    tcp_port: int
    device: str
    name: str = ""
    # optional defaults, used until the client sends its own
    baudrate: Optional[int] = None
    bytesize: Optional[int] = None
    parity: Optional[str] = None
    stopbits: Optional[float] = None
    dtr: Optional[bool] = None
    rts: Optional[bool] = None

    def __post_init__(self):
        if not self.name:
            self.name = self.device

    def initial_settings(self) -> LineSettings:
        s = LineSettings()
        for f in ("baudrate", "bytesize", "parity", "stopbits", "dtr", "rts"):
            v = getattr(self, f)
            if v is not None:
                setattr(s, f, v)
        return s


class PortServer:
    """One TCP listener, one backend, at most one client."""

    RETRY_SECONDS = 2.0

    def __init__(self, cfg: PortConfig, factory: BackendFactory, bind: str, strict: bool = False):
        self.cfg = cfg
        self._factory = factory
        self._bind = bind
        self._strict = strict
        self._server: Optional[asyncio.AbstractServer] = None
        self._session: Optional[PortSession] = None
        self._client_writer: Optional[asyncio.StreamWriter] = None
        self._client_done: Optional[asyncio.Event] = None
        self._backend: Optional[Backend] = None
        self._device_lost = asyncio.Event()
        self._loop: Optional[asyncio.AbstractEventLoop] = None

    async def start(self) -> None:
        self._loop = asyncio.get_running_loop()
        self._server = await asyncio.start_server(self._client, self._bind, self.cfg.tcp_port)
        log.info("%s: listening on %s:%d for %s", self.cfg.name, self._bind, self.cfg.tcp_port, self.cfg.device)

    async def stop(self) -> None:
        # Python 3.12's wait_closed() waits for the handlers, so drop the client first
        await self._drop_client()
        if self._server:
            self._server.close()
            await self._server.wait_closed()

    @property
    def port(self) -> int:
        assert self._server is not None
        return self._server.sockets[0].getsockname()[1]

    async def _drop_client(self) -> None:
        """Closes the current client's socket; its handler then winds down on
        its own (no task cancellation - that is fragile across Python versions)."""
        writer, self._client_writer = self._client_writer, None
        done, self._client_done = self._client_done, None
        if writer is not None:
            writer.close()
        if done is not None:
            try:
                await asyncio.wait_for(done.wait(), 2)
            except asyncio.TimeoutError:
                log.warning("%s: old client handler did not finish", self.cfg.name)

    async def _client(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        peer = writer.get_extra_info("peername")
        if self._client_writer is not None:
            log.info("%s: new client %s replaces the old one", self.cfg.name, peer)
            await self._drop_client()
        done = asyncio.Event()
        self._client_writer = writer
        self._client_done = done
        log.info("%s: client %s connected", self.cfg.name, peer)

        backend = self._factory(self._on_backend_data, self._on_backend_error)
        backend.settings = self.cfg.initial_settings()
        self._backend = backend
        session = PortSession(self.cfg.name, backend, self._loop, writer)
        self._session = session
        self._device_lost.clear()

        try:
            sock = writer.get_extra_info("socket")
            if sock is not None:
                sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            keeper = asyncio.ensure_future(self._keep_device_open(backend, writer))
            try:
                while True:
                    data = await reader.read(4096)
                    if not data:
                        break
                    session.feed(data)
            finally:
                keeper.cancel()
        except (ConnectionError, asyncio.IncompleteReadError):
            pass
        finally:
            log.info("%s: client %s disconnected", self.cfg.name, peer)
            session.close()
            backend.close()
            if self._session is session:
                self._session = None
                self._backend = None
            if self._client_writer is writer:
                self._client_writer = None
                self._client_done = None
            writer.close()
            done.set()

    async def _keep_device_open(self, backend: Backend, writer: asyncio.StreamWriter) -> None:
        """Opens the device, and re-opens it after it vanished (USB unplug).
        With --strict the client is dropped instead."""
        warned = False
        while True:
            if not backend.is_open:
                try:
                    backend.open()
                    warned = False
                except Exception as exc:  # noqa: BLE001
                    if self._strict:
                        log.error("%s: %s - dropping the client", self.cfg.name, exc)
                        writer.close()
                        return
                    if not warned:
                        log.warning("%s: %s - waiting for the device", self.cfg.name, exc)
                        warned = True
                    await asyncio.sleep(self.RETRY_SECONDS)
                    continue
            self._device_lost.clear()
            await self._device_lost.wait()
            backend.close()
            if self._strict:
                log.error("%s: device lost - dropping the client", self.cfg.name)
                writer.close()
                return
            await asyncio.sleep(self.RETRY_SECONDS)

    # called from the reader thread
    def _on_backend_data(self, data: bytes) -> None:
        session = self._session
        if session is not None:
            session.from_backend(data)

    def _on_backend_error(self, msg: str) -> None:
        if self._loop is not None:
            try:
                self._loop.call_soon_threadsafe(self._device_lost_now)
            except RuntimeError:  # loop already closed
                pass

    def _device_lost_now(self) -> None:
        self._device_lost.set()


# ---------------------------------------------------------------------------
# configuration


def parse_port_arg(text: str) -> PortConfig:
    """'4000=/dev/ttyUSB0' -> PortConfig"""
    if "=" not in text:
        raise argparse.ArgumentTypeError(f"{text!r}: expected TCPPORT=DEVICE")
    port, device = text.split("=", 1)
    try:
        tcp = int(port)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"{port!r} is not a TCP port") from exc
    if not device:
        raise argparse.ArgumentTypeError(f"{text!r}: no device given")
    return PortConfig(tcp_port=tcp, device=device)


def load_config(path: str) -> tuple[list[PortConfig], dict]:
    """Reads the TOML file; returns the ports and the global settings.

    [bridge]
    bind = "0.0.0.0"
    mdns = false

    [[port]]
    name = "UT61E"
    tcp = 4000
    device = "/dev/serial/by-id/usb-Prolific_..."
    # optional, normally QtDMM sets these:
    # baudrate = 19200
    # dtr = false
    """
    import tomllib

    with open(path, "rb") as f:
        data = tomllib.load(f)
    ports = []
    for i, entry in enumerate(data.get("port", [])):
        try:
            ports.append(PortConfig(
                tcp_port=int(entry["tcp"]),
                device=str(entry["device"]),
                name=str(entry.get("name", "")),
                baudrate=entry.get("baudrate"),
                bytesize=entry.get("bytesize"),
                parity=entry.get("parity"),
                stopbits=entry.get("stopbits"),
                dtr=entry.get("dtr"),
                rts=entry.get("rts"),
            ))
        except KeyError as exc:
            raise SystemExit(f"{path}: [[port]] entry {i + 1} lacks {exc}") from exc
    return ports, data.get("bridge", {})


def list_devices() -> str:
    """Serial ports (pyserial) and the known USB-HID meter cables."""
    lines = ["Serial ports:"]
    try:
        from serial.tools import list_ports

        found = sorted(list_ports.comports(), key=lambda p: p.device)
        # Linux registers 32 legacy /dev/ttyS* ports whether or not they exist;
        # the real ones (and all USB adapters) come with a description
        found = [p for p in found if not (p.device.startswith("/dev/ttyS") and (not p.description or p.description == "n/a"))]
        for p in found:
            desc = p.description if p.description and p.description != "n/a" else ""
            by_id = ""
            if sys.platform.startswith("linux") and os.path.isdir("/dev/serial/by-id"):
                for link in os.listdir("/dev/serial/by-id"):
                    full = os.path.join("/dev/serial/by-id", link)
                    if os.path.realpath(full) == p.device:
                        by_id = full
            lines.append(f"  {p.device:<20} {desc}")
            if by_id:
                lines.append(f"  {'':<20} stable name: {by_id}")
        if not found:
            lines.append("  (none)")
    except ImportError:
        lines.append("  pyserial is not installed (pip install pyserial)")

    lines.append("USB-HID meter cables (device hid:VID:PID or hid:/dev/hidrawN):")
    cables = [(path, vid, pid) for path, vid, pid in hidraw_devices() if (vid, pid) in HID_CABLES]
    for path, vid, pid in cables:
        chip, desc = HID_CABLES[(vid, pid)]
        lines.append(f"  {path:<20} hid:{vid:04x}:{pid:04x}  {desc}")
    if not cables:
        lines.append("  (none)")
    return "\n".join(lines)


def detected_ports(first_tcp: int = 4000) -> list[PortConfig]:
    """One PortConfig per USB serial adapter (by stable by-id name where there
    is one) and per known HID cable - the starting point for a configuration."""
    ports: list[PortConfig] = []
    tcp = first_tcp
    try:
        from serial.tools import list_ports

        for p in sorted(list_ports.comports(), key=lambda p: p.device):
            if p.device.startswith("/dev/ttyS") and (not p.description or p.description == "n/a"):
                continue
            device = p.device
            by_id = "/dev/serial/by-id"
            if os.path.isdir(by_id):
                for link in os.listdir(by_id):
                    if os.path.realpath(os.path.join(by_id, link)) == p.device:
                        device = os.path.join(by_id, link)
            ports.append(PortConfig(tcp, device, p.description if p.description not in ("", "n/a") else ""))
            tcp += 1
    except ImportError:
        pass
    for path, vid, pid in hidraw_devices():
        if (vid, pid) in HID_CABLES:
            ports.append(PortConfig(tcp, f"hid:{vid:04x}:{pid:04x}", HID_CABLES[(vid, pid)][1]))
            tcp += 1
    return ports


def print_config(ports: list[PortConfig], bind: str, mdns: bool = False) -> str:
    out = ["# qtdmm-bridge configuration - see tools/qtdmm-bridge/README.md",
           "[bridge]", f'bind = "{bind}"', f"mdns = {'true' if mdns else 'false'}", ""]
    for p in ports:
        out += ["[[port]]", f'name = "{p.name}"', f"tcp = {p.tcp_port}", f'device = "{p.device}"', ""]
    return "\n".join(out)


# ---------------------------------------------------------------------------
# mDNS (optional, needs the zeroconf package)


class Announcer:
    """Registers one _qtdmm-bridge._tcp service per port so clients can find
    the bridge without knowing its address. Silently does nothing when the
    zeroconf package is missing."""

    SERVICE = "_qtdmm-bridge._tcp.local."

    def __init__(self):
        self._zc = None
        self._infos = []

    async def start(self, ports: list[PortConfig]) -> bool:
        try:
            from zeroconf import ServiceInfo
            from zeroconf.asyncio import AsyncZeroconf
        except ImportError:
            log.warning("mDNS announcement needs the zeroconf package (pip install zeroconf)")
            return False
        host = socket.gethostname().split(".")[0]
        try:
            addresses = [socket.inet_aton(a) for a in _local_addresses()]
        except OSError:
            addresses = []
        # the async flavour: the sync Zeroconf() blocks when called from inside a running loop
        self._zc = AsyncZeroconf()
        for p in ports:
            name = f"{host} {p.name}".replace(".", "_")[:60]
            info = ServiceInfo(self.SERVICE, f"{name}.{self.SERVICE}", addresses=addresses, port=p.tcp_port,
                               properties={"device": p.device, "name": p.name, "version": __version__},
                               server=f"{host}.local.")
            await self._zc.async_register_service(info)
            self._infos.append(info)
        log.info("mDNS: announcing %d service(s) as %s", len(ports), self.SERVICE)
        return True

    async def stop(self) -> None:
        if self._zc is None:
            return
        try:
            await self._zc.async_unregister_all_services()
            await self._zc.async_close()
        except Exception:  # noqa: BLE001
            pass
        self._zc = None


def _local_addresses() -> list[str]:
    """IPv4 addresses of this host, best effort (the one used for the default route first)."""
    addrs = []
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("10.255.255.255", 1))
        addrs.append(s.getsockname()[0])
        s.close()
    except OSError:
        pass
    try:
        for a in socket.gethostbyname_ex(socket.gethostname())[2]:
            if a not in addrs and not a.startswith("127."):
                addrs.append(a)
    except OSError:
        pass
    return addrs or ["127.0.0.1"]


# ---------------------------------------------------------------------------
# main


async def serve(ports: list[PortConfig], bind: str, strict: bool, mdns: bool = False) -> None:
    servers = [PortServer(cfg, make_backend_factory(cfg.device), bind, strict) for cfg in ports]
    for s in servers:
        await s.start()
    announcer = Announcer()
    if mdns:
        await announcer.start(ports)

    stop = asyncio.Event()
    loop = asyncio.get_running_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        try:
            loop.add_signal_handler(sig, stop.set)
        except NotImplementedError:  # Windows
            pass
    await stop.wait()
    log.info("shutting down")
    await announcer.stop()
    for s in servers:
        await s.stop()


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        prog="qtdmm-bridge",
        description="Serve multimeters to QtDMM over the network (RFC 2217).",
        epilog="No authentication: bind to 127.0.0.1 and tunnel with SSH outside a trusted network.")
    parser.add_argument("-c", "--config", metavar="FILE", help="TOML configuration file")
    parser.add_argument("-p", "--port", metavar="TCPPORT=DEVICE", type=parse_port_arg, action="append",
                        default=[], help="serve DEVICE on TCPPORT (repeatable), e.g. 4000=/dev/ttyUSB0")
    parser.add_argument("-b", "--bind", default=None, help="address to listen on (default 0.0.0.0)")
    parser.add_argument("--strict", action="store_true",
                        help="drop the client when the device vanishes instead of waiting for it")
    parser.add_argument("--list", action="store_true", help="list serial ports and known meter cables, then exit")
    parser.add_argument("--mdns", action="store_true",
                        help="announce the ports via mDNS/DNS-SD (_qtdmm-bridge._tcp; needs the zeroconf package)")
    parser.add_argument("--print-config", action="store_true",
                        help="print the ports as a TOML configuration and exit; without --port/--config "
                             "one entry per detected serial adapter and HID cable")
    parser.add_argument("-v", "--verbose", action="count", default=0, help="more log output")
    parser.add_argument("--version", action="version", version=f"%(prog)s {__version__}")
    args = parser.parse_args(argv)

    logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO,
                        format="%(asctime)s %(levelname)s %(message)s", datefmt="%H:%M:%S")

    if args.list:
        print(list_devices())
        return 0

    ports: list[PortConfig] = []
    bind = "0.0.0.0"
    mdns = False
    if args.config:
        ports, glob = load_config(args.config)
        bind = str(glob.get("bind", bind))
        mdns = bool(glob.get("mdns", False))
    ports += args.port
    if args.bind:
        bind = args.bind
    if args.mdns:
        mdns = True

    if args.print_config:
        if not ports:
            ports = detected_ports()
        print(print_config(ports, bind, mdns))
        return 0
    if not ports:
        parser.error("nothing to serve: give --port TCPPORT=DEVICE or --config FILE (--list shows devices)")

    seen = set()
    for p in ports:
        if p.tcp_port in seen:
            parser.error(f"TCP port {p.tcp_port} is used twice")
        seen.add(p.tcp_port)

    try:
        asyncio.run(serve(ports, bind, args.strict, mdns))
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
