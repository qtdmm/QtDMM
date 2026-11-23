#!/usr/bin/env python3
# Copyright (c) 2026 The QtDMM developers
# SPDX-License-Identifier: GPL-3.0-or-later
"""Tests for qtdmm_bridge: the telnet/RFC 2217 state machine and the server
with a fake backend (no serial port, no pyserial needed).

    python3 -m unittest discover -s tools/qtdmm-bridge/tests
"""

import asyncio
import os
import sys
import threading
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

import qtdmm_bridge as qb  # noqa: E402

IAC, SB, SE = qb.IAC, qb.SB, qb.SE


def sub(cmd, payload=b""):
    """A COM-PORT-OPTION subnegotiation as QtDMM sends it (IAC escaped)."""
    return bytes([IAC, SB, qb.OPT_COM_PORT, cmd]) + payload.replace(b"\xff", b"\xff\xff") + bytes([IAC, SE])


class FakeBackend(qb.Backend):
    """Records writes and settings; test code injects meter data."""

    instances = []

    def __init__(self, on_data, on_error):
        super().__init__(on_data, on_error)
        self.name = "fake"
        self.opened = False
        self.written = bytearray()
        self.applied = []
        self.purged = []
        self.fail_open = 0
        FakeBackend.instances.append(self)

    def open(self):
        if self.fail_open > 0:
            self.fail_open -= 1
            raise OSError("no such device")
        self.opened = True

    def close(self):
        self.opened = False

    @property
    def is_open(self):
        return self.opened

    def write(self, data):
        self.written += data

    def apply(self, settings):
        self.settings = settings
        self.applied.append(settings)
        return settings

    def purge(self, what):
        self.purged.append(what)

    def inject(self, data):
        self.on_data(data)

    def unplug(self):
        self.opened = False
        self.on_error("read failed: unplugged")


class ProtocolTest(unittest.TestCase):
    def setUp(self):
        self.sent = bytearray()
        self.data = bytearray()
        self.options = []
        self.proto = qb.Rfc2217Protocol(self.sent.extend, self.data.extend, self.option)

    def option(self, cmd, value):
        self.options.append((cmd, value))
        return value

    def test_plain_data_and_iac_escape(self):
        self.proto.feed(b"DC 1.2\xff\xff34 V\n")
        self.assertEqual(bytes(self.data), b"DC 1.2\xff34 V\n")
        self.assertEqual(self.sent, b"")
        self.assertEqual(qb.Rfc2217Protocol.escape(b"\x00\xff\x01"), b"\x00\xff\xff\x01")

    def test_suboption_split_across_feeds(self):
        pkt = sub(qb.SET_BAUDRATE, (19200).to_bytes(4, "big"))
        self.proto.feed(pkt[:3])
        self.proto.feed(pkt[3:7])
        self.proto.feed(pkt[7:])
        self.assertEqual(self.options, [(qb.SET_BAUDRATE, (19200).to_bytes(4, "big"))])
        self.assertEqual(bytes(self.sent), sub(qb.SET_BAUDRATE + 100, (19200).to_bytes(4, "big")))
        self.assertEqual(self.data, b"")

    def test_iac_inside_suboption(self):
        # a baud rate whose bytes contain 0xff arrives escaped
        pkt = sub(qb.SET_BAUDRATE, b"\x00\x00\xff\x00")
        self.proto.feed(pkt)
        self.assertEqual(self.options, [(qb.SET_BAUDRATE, b"\x00\x00\xff\x00")])

    def test_negotiation(self):
        self.proto.feed(bytes([IAC, qb.WILL, qb.OPT_COM_PORT, IAC, qb.DO, qb.OPT_BINARY, IAC, qb.DO, qb.OPT_ECHO]))
        self.assertEqual(bytes(self.sent), bytes([IAC, qb.DO, qb.OPT_COM_PORT, IAC, qb.WILL, qb.OPT_BINARY,
                                                  IAC, qb.WONT, qb.OPT_ECHO]))

    def test_signature(self):
        self.proto.feed(sub(qb.SIGNATURE))
        self.assertIn(b"qtdmm-bridge", bytes(self.sent))
        self.assertEqual(self.options, [])

    def test_parity_codes_match_rfc2217(self):
        # RFC 2217 section 2.6; pyserial's client is the independent witness
        self.assertEqual(qb.PARITY_NAMES, {1: "N", 2: "O", 3: "E", 4: "M", 5: "S"})
        try:
            from serial import rfc2217
        except ImportError:
            self.skipTest("pyserial not installed")
        self.assertEqual(qb.PARITY_CODES, rfc2217.RFC2217_PARITY_MAP)

    def test_oversized_subnegotiation_dropped(self):
        self.proto.feed(bytes([IAC, SB]) + b"x" * 1000 + b"data")
        self.assertEqual(self.options, [])
        self.assertTrue(bytes(self.data).endswith(b"data"))

    def test_unknown_command_ignored(self):
        self.proto.feed(bytes([IAC, qb.NOP]) + b"abc")
        self.assertEqual(bytes(self.data), b"abc")


class ServerTest(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        FakeBackend.instances.clear()
        self.cfg = qb.PortConfig(tcp_port=0, device="fake", name="fake")
        self.server = qb.PortServer(self.cfg, FakeBackend, "127.0.0.1")
        self.server.RETRY_SECONDS = 0.05
        await self.server.start()

    async def asyncTearDown(self):
        # a failed test may leave a client connected; do not hang on it
        await asyncio.wait_for(self.server.stop(), 3)

    async def connect(self):
        """Connects and waits for the server to open a backend for it."""
        before = len(FakeBackend.instances)
        reader, writer = await asyncio.open_connection("127.0.0.1", self.server.port)
        for _ in range(100):
            if len(FakeBackend.instances) > before and FakeBackend.instances[-1].opened:
                break
            await asyncio.sleep(0.01)
        self.assertGreater(len(FakeBackend.instances), before, "server did not open a backend")
        return reader, writer, FakeBackend.instances[-1]

    async def read_some(self, reader, n, timeout=1.0):
        return await asyncio.wait_for(reader.readexactly(n), timeout)

    async def test_qtdmm_negotiation(self):
        reader, writer, backend = await self.connect()
        # exactly what RFC2217SerialDevice::sendRFC2217Negotiation() sends for 19200 7E1 (RFC parity code 3
        # = even), DTR on, RTS off
        writer.write(sub(qb.SET_BAUDRATE, (19200).to_bytes(4, "big")) + sub(qb.SET_DATASIZE, b"\x07")
                     + sub(qb.SET_PARITY, b"\x03") + sub(qb.SET_STOPSIZE, b"\x01")
                     + sub(qb.SET_CONTROL, b"\x08") + sub(qb.SET_CONTROL, b"\x0c"))
        await writer.drain()
        expected = (sub(101, (19200).to_bytes(4, "big")) + sub(102, b"\x07") + sub(103, b"\x03") + sub(104, b"\x01")
                    + sub(105, b"\x08") + sub(105, b"\x0c"))
        got = await self.read_some(reader, len(expected))
        self.assertEqual(got, expected)
        s = backend.settings
        self.assertEqual((s.baudrate, s.bytesize, s.parity, s.stopbits, s.dtr, s.rts), (19200, 7, "E", 1, True, False))
        writer.close()

    async def test_data_both_ways_with_escaping(self):
        reader, writer, backend = await self.connect()
        # meter -> client: 0xff must be doubled
        backend.inject(b"\x1b\x00\xff\x30")
        self.assertEqual(await self.read_some(reader, 5), b"\x1b\x00\xff\xff\x30")
        # client -> meter: poll request, escaped 0xff undone
        writer.write(b"\x00\x86\x66\xff\xff")
        await writer.drain()
        for _ in range(50):
            if len(backend.written) >= 4:
                break
            await asyncio.sleep(0.01)
        self.assertEqual(bytes(backend.written), b"\x00\x86\x66\xff")
        writer.close()

    async def test_data_from_reader_thread(self):
        reader, writer, backend = await self.connect()
        t = threading.Thread(target=backend.inject, args=(b"hello",))
        t.start()
        t.join()
        self.assertEqual(await self.read_some(reader, 5), b"hello")
        writer.close()

    async def test_new_client_replaces_old(self):
        reader1, writer1, backend1 = await self.connect()
        reader2, writer2, backend2 = await self.connect()
        self.assertIsNot(backend1, backend2)
        # the first connection is closed by the server, its device released
        self.assertEqual(await asyncio.wait_for(reader1.read(), 1.0), b"")
        self.assertFalse(backend1.opened)
        backend2.inject(b"x")
        self.assertEqual(await self.read_some(reader2, 1), b"x")
        writer2.close()

    async def test_device_appears_later(self):
        # the device is not there yet when the client connects
        orig_init = FakeBackend.__init__

        def init(self, on_data, on_error):
            orig_init(self, on_data, on_error)
            self.fail_open = 2

        FakeBackend.__init__ = init
        try:
            reader, writer = await asyncio.open_connection("127.0.0.1", self.server.port)
            await asyncio.sleep(0.02)
            backend = FakeBackend.instances[-1]
            self.assertFalse(backend.opened)
            for _ in range(100):
                if backend.opened:
                    break
                await asyncio.sleep(0.01)
            self.assertTrue(backend.opened, "backend should be retried until open() succeeds")
            writer.close()
        finally:
            FakeBackend.__init__ = orig_init

    async def test_device_unplugged_and_back(self):
        reader, writer, backend = await self.connect()
        backend.unplug()
        for _ in range(100):
            if backend.opened:
                break
            await asyncio.sleep(0.01)
        self.assertTrue(backend.opened, "backend should be reopened after the device came back")
        # the client never noticed
        backend.inject(b"y")
        self.assertEqual(await self.read_some(reader, 1), b"y")
        writer.close()

    async def test_purge_and_unknown_suboption(self):
        reader, writer, backend = await self.connect()
        writer.write(sub(qb.PURGE_DATA, b"\x03") + sub(99, b"\x01") + b"Z")
        await writer.drain()
        self.assertEqual(await self.read_some(reader, len(sub(112, b"\x03"))), sub(112, b"\x03"))
        for _ in range(50):
            if backend.written:
                break
            await asyncio.sleep(0.01)
        self.assertEqual(backend.purged, [3])
        self.assertEqual(bytes(backend.written), b"Z")
        writer.close()


class SerialLoopTest(unittest.IsolatedAsyncioTestCase):
    """End to end through the real SerialBackend on pyserial's loop:// port,
    which echoes everything written to it."""

    async def test_echo_through_loop_port(self):
        try:
            import serial  # noqa: F401
        except ImportError:
            self.skipTest("pyserial not installed")
        cfg = qb.PortConfig(tcp_port=0, device="loop://")
        server = qb.PortServer(cfg, qb.make_backend_factory(cfg.device), "127.0.0.1")
        await server.start()
        try:
            reader, writer = await asyncio.open_connection("127.0.0.1", server.port)
            writer.write(sub(qb.SET_BAUDRATE, (2400).to_bytes(4, "big")))
            writer.write(b"\x1b\x00\xff\xff\x30")
            await writer.drain()
            reply = await asyncio.wait_for(reader.readexactly(len(sub(101, (2400).to_bytes(4, "big"))) + 5), 3)
            self.assertEqual(reply, sub(101, (2400).to_bytes(4, "big")) + b"\x1b\x00\xff\xff\x30")
            writer.close()
        finally:
            await asyncio.wait_for(server.stop(), 3)


class PyserialClientTest(unittest.IsolatedAsyncioTestCase):
    """pyserial's own RFC 2217 client as an independent witness: whatever it
    negotiates must land unchanged in the backend."""

    async def test_pyserial_rfc2217_client(self):
        try:
            import serial  # noqa: F401
        except ImportError:
            self.skipTest("pyserial not installed")
        FakeBackend.instances.clear()
        server = qb.PortServer(qb.PortConfig(0, "fake", "fake"), FakeBackend, "127.0.0.1")
        await server.start()
        try:
            def client():
                ser = serial.serial_for_url(f"rfc2217://127.0.0.1:{server.port}", baudrate=2400, bytesize=7,
                                            parity="O", stopbits=2, timeout=1)
                ser.dtr = False
                ser.write(b"D\n")
                ser.close()

            await asyncio.get_running_loop().run_in_executor(None, client)
            backend = FakeBackend.instances[-1]
            s = backend.settings
            self.assertEqual((s.baudrate, s.bytesize, s.parity, s.stopbits, s.dtr), (2400, 7, "O", 2, False))
            self.assertEqual(bytes(backend.written), b"D\n")
        finally:
            await asyncio.wait_for(server.stop(), 3)


class ConfigTest(unittest.TestCase):
    def test_port_arg(self):
        p = qb.parse_port_arg("4000=/dev/ttyUSB0")
        self.assertEqual((p.tcp_port, p.device, p.name), (4000, "/dev/ttyUSB0", "/dev/ttyUSB0"))
        for bad in ("4000", "x=/dev/ttyUSB0", "4000="):
            with self.assertRaises(Exception):
                qb.parse_port_arg(bad)

    def test_toml_roundtrip(self):
        import tempfile

        ports = [qb.PortConfig(4000, "/dev/ttyUSB0", "UT61E"), qb.PortConfig(4001, "loop://")]
        text = qb.print_config(ports, "127.0.0.1")
        with tempfile.NamedTemporaryFile("w", suffix=".toml", delete=False) as f:
            f.write(text + '\n[[port]]\ntcp = 4002\ndevice = "/dev/ttyUSB1"\nbaudrate = 2400\ndtr = false\n')
            name = f.name
        try:
            loaded, glob = qb.load_config(name)
        finally:
            os.unlink(name)
        self.assertEqual(glob["bind"], "127.0.0.1")
        self.assertEqual([(p.tcp_port, p.device, p.name) for p in loaded],
                         [(4000, "/dev/ttyUSB0", "UT61E"), (4001, "loop://", "loop://"), (4002, "/dev/ttyUSB1", "/dev/ttyUSB1")])
        s = loaded[2].initial_settings()
        self.assertEqual((s.baudrate, s.dtr, s.rts), (2400, False, True))

    def test_print_config_and_detected(self):
        text = qb.print_config([qb.PortConfig(4000, "hid:1a86:e008", "UT803")], "0.0.0.0", mdns=True)
        self.assertIn("mdns = true", text)
        self.assertIn('device = "hid:1a86:e008"', text)
        # detection never raises, gives distinct TCP ports from 4000 up
        ports = qb.detected_ports()
        self.assertEqual([p.tcp_port for p in ports], list(range(4000, 4000 + len(ports))))

    def test_device_prefixes(self):
        self.assertIsInstance(qb.make_backend_factory("serial:/dev/ttyUSB0")(None, None), qb.SerialBackend)
        if sys.platform.startswith("linux"):
            self.assertIsInstance(qb.make_backend_factory("hid:1a86:e008")(None, None), qb.HidBackend)


class FakeHidRaw:
    """Stands in for /dev/hidrawN: records feature/output reports, hands out
    queued input reports."""

    instances = []

    def __init__(self, path):
        self.path = path
        self.features = []
        self.writes = []
        self.reports = []
        self.closed = False
        self.fail_read = False
        FakeHidRaw.instances.append(self)

    def read(self, timeout):
        if self.fail_read:
            raise OSError(19, "No such device")
        if self.reports:
            return self.reports.pop(0)
        threading.Event().wait(min(timeout, 0.01))
        return None

    def write(self, report):
        self.writes.append(bytes(report))

    def send_feature(self, report):
        self.features.append(bytes(report))

    def close(self):
        self.closed = True


VECTORS = os.path.join(os.path.dirname(__file__), "..", "..", "..", "tests", "data", "hid_cables.json")


def load_vectors():
    import json

    with open(VECTORS, encoding="utf-8") as f:
        return json.load(f)


class HidSharedVectorsTest(unittest.TestCase):
    """tests/data/hid_cables.json - the same file QtDMM's test_hid reads, so
    the C++ and the Python implementation are held to one truth."""

    @classmethod
    def setUpClass(cls):
        if not os.path.exists(VECTORS):
            raise unittest.SkipTest("hid_cables.json not available (bridge checked out alone)")
        cls.v = load_vectors()

    @staticmethod
    def hx(text):
        return bytes.fromhex(text.replace(" ", "")) if text else b""

    def test_cable_table(self):
        table = {(int(c["vid"], 16), int(c["pid"], 16)): c["chip"] for c in self.v["cables"]}
        self.assertEqual(table, {k: v[0] for k, v in qb.HID_CABLES.items()})

    def test_unpack(self):
        for t in self.v["unpack"]:
            got = qb.unpack_hid_report(t["chip"], self.hx(t["report"]))
            with self.subTest(chip=t["chip"], report=t["report"], why=t["why"]):
                if t["expect"] is None:
                    self.assertIsNone(got)
                else:
                    self.assertEqual(got, self.hx(t["expect"]))

    def test_pack(self):
        for t in self.v["pack"]:
            got = qb.pack_hid_write(t["chip"], self.hx(t["data"]))
            with self.subTest(chip=t["chip"], why=t["why"]):
                if "expect_prefix" in t:
                    self.assertEqual(len(got), t["expect_length"])
                    self.assertTrue(got.startswith(self.hx(t["expect_prefix"])))
                else:
                    self.assertEqual(got, self.hx(t["expect"]))

    def test_config(self):
        for t in self.v["config"]:
            s = qb.LineSettings(baudrate=t["baud"], bytesize=t["bits"], parity=t["parity"], stopbits=t["stopbits"])
            got = qb.cp2110_config_report(s) if t["chip"] == "CP2110" else qb.ch9325_config_report(s)
            with self.subTest(chip=t["chip"], baud=t["baud"]):
                self.assertEqual(got, self.hx(t["expect"]))


class HidReportTest(unittest.TestCase):
    """Report layouts per chip (the shared vectors above cover the same; these
    stay as readable examples)."""

    def test_ch9325_unpack(self):
        # 0xF0 | count, payload bytes carry the top bit
        self.assertEqual(qb.unpack_hid_report("CH9325", bytes([0xF3, 0xB1, 0xB2, 0xB3, 0x80, 0x80])), b"123")
        self.assertEqual(qb.unpack_hid_report("CH9325", bytes([0xF0, 0x80])), b"")
        self.assertIsNone(qb.unpack_hid_report("CH9325", bytes([0xF5, 0xB1])))
        self.assertIsNone(qb.unpack_hid_report("CH9325", b""))

    def test_ch9329_cp2110_unpack(self):
        rep = bytes([3, 0x41, 0x42, 0x43]) + b"\x00" * 60
        self.assertEqual(qb.unpack_hid_report("CH9329", rep), b"ABC")
        self.assertEqual(qb.unpack_hid_report("CP2110", rep), b"ABC")
        self.assertIsNone(qb.unpack_hid_report("CH9329", bytes([64]) + b"\x00" * 63))
        self.assertIsNone(qb.unpack_hid_report("CP2110", bytes([5, 1, 2])))

    def test_bu86x_unpack(self):
        self.assertEqual(qb.unpack_hid_report("BU86X", b"\x00\x86\x66\x01\x02\x03\x04\x05"), b"\x00\x86\x66\x01\x02\x03\x04\x05")

    def test_pack_write(self):
        self.assertEqual(qb.pack_hid_write("BU86X", b"\x00\x86\x66"), b"\x00\x00\x86\x66")
        r = qb.pack_hid_write("CH9329", b"D\n")
        self.assertEqual(len(r), 65)
        self.assertEqual(r[:4], b"\x00\x02D\n")
        self.assertEqual(qb.pack_hid_write("CP2110", b"D\n"), b"\x02D\n")
        self.assertEqual(qb.pack_hid_write("CH9325", b"D\n"), b"\x00\x02D\n\x00\x00\x00\x00\x00")

    def test_config_reports(self):
        s = qb.LineSettings(baudrate=19200, bytesize=7, parity="O", stopbits=1)
        self.assertEqual(qb.cp2110_config_report(s), bytes([0x50, 0, 0, 0x4B, 0, 2, 0, 2, 0]))
        self.assertEqual(qb.ch9325_config_report(s), bytes([0, 0, 0x4B, 0, 0, 2]))
        s = qb.LineSettings(baudrate=2400, bytesize=8, parity="E", stopbits=2)
        self.assertEqual(qb.cp2110_config_report(s), bytes([0x50, 0, 0, 0x09, 0x60, 1, 0, 3, 1]))
        self.assertEqual(qb.ch9325_config_report(s), bytes([0, 0x60, 0x09, 0, 0, 3]))


class HidBackendTest(unittest.TestCase):
    def setUp(self):
        FakeHidRaw.instances.clear()
        self.data = bytearray()
        self.errors = []
        self._orig_devices = qb.hidraw_devices
        self._orig_raw = qb.HidBackend.raw_factory
        qb.hidraw_devices = lambda: [("/dev/hidraw0", 0x046D, 0xC077), ("/dev/hidraw2", 0x1A86, 0xE008),
                                     ("/dev/hidraw3", 0x10C4, 0xEA80), ("/dev/hidraw4", 0x0820, 0x0001)]
        qb.HidBackend.raw_factory = FakeHidRaw

    def tearDown(self):
        qb.hidraw_devices = self._orig_devices
        qb.HidBackend.raw_factory = self._orig_raw

    def backend(self, spec):
        return qb.HidBackend(spec, self.data.extend, self.errors.append)

    def wait_for(self, cond, timeout=1.0):
        import time

        end = time.time() + timeout
        while time.time() < end and not cond():
            time.sleep(0.005)
        return cond()

    def test_resolve(self):
        self.assertEqual(self.backend("1a86:e008").resolve(), ("/dev/hidraw2", "CH9325"))
        self.assertEqual(self.backend("/dev/hidraw3").resolve(), ("/dev/hidraw3", "CP2110"))
        self.assertEqual(self.backend("0820:0001").resolve(), ("/dev/hidraw4", "BU86X"))
        with self.assertRaises(FileNotFoundError):
            self.backend("1a86:e429").resolve()          # not attached
        with self.assertRaises(FileNotFoundError):
            self.backend("/dev/hidraw9").resolve()
        with self.assertRaises(ValueError):
            self.backend("nonsense").resolve()

    def test_ch9325_open_configures_and_reads(self):
        b = self.backend("1a86:e008")
        b.settings = qb.LineSettings(baudrate=19200, bytesize=7)
        b.open()
        try:
            raw = FakeHidRaw.instances[-1]
            self.assertEqual(raw.features, [bytes([0, 0, 0x4B, 0, 0, 2])])
            raw.reports.append(bytes([0xF2, 0xB0, 0xBD]))     # "0="
            raw.reports.append(bytes([0xF1, 0x8A]))           # "\n"
            self.assertTrue(self.wait_for(lambda: bytes(self.data) == b"0=\n"), bytes(self.data))
            # a new baud rate from the client -> another feature report
            b.apply(qb.LineSettings(baudrate=2400, bytesize=8))
            self.assertEqual(raw.features[-1], bytes([0, 0x60, 0x09, 0, 0, 3]))
            b.write(b"D\n")                                   # count, data, an 8-byte report
            self.assertEqual(raw.writes, [b"\x00\x02D\n\x00\x00\x00\x00\x00"])
            b.write(b"0123456789")                            # in reports of seven bytes
            self.assertEqual(raw.writes[1:], [b"\x00\x070123456", b"\x00\x03789\x00\x00\x00\x00"])
        finally:
            b.close()
        self.assertTrue(raw.closed)

    def test_cp2110_open_and_write(self):
        b = self.backend("/dev/hidraw3")
        b.settings = qb.LineSettings(baudrate=9600, bytesize=8, parity="N", stopbits=1)
        b.open()
        try:
            raw = FakeHidRaw.instances[-1]
            self.assertEqual(raw.features, [b"\x41\x01", bytes([0x50, 0, 0, 0x25, 0x80, 0, 0, 3, 0])])
            b.write(b"D\n")
            self.assertEqual(raw.writes, [b"\x02D\n"])
            raw.reports.append(bytes([2, 0x4F, 0x4B]))
            self.assertTrue(self.wait_for(lambda: bytes(self.data) == b"OK"))
        finally:
            b.close()

    def test_bu86x_poll_and_unplug(self):
        b = self.backend("0820:0001")
        b.open()
        try:
            raw = FakeHidRaw.instances[-1]
            self.assertEqual(raw.features, [])                # fixed speed
            b.write(b"\x00\x86\x66")
            self.assertEqual(raw.writes, [b"\x00\x00\x86\x66"])
            raw.fail_read = True
            self.assertTrue(self.wait_for(lambda: self.errors), "unplug should be reported")
            self.assertFalse(b.is_open)
        finally:
            b.close()

    def test_open_without_cable(self):
        with self.assertRaises(FileNotFoundError):
            self.backend("1a86:e429").open()


if __name__ == "__main__":
    unittest.main()
