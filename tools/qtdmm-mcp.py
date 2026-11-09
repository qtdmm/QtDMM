#!/usr/bin/env python3
"""MCP server (stdio) that reads a running QtDMM through its SCPI server.

A development tool: lets an assistant session read values, status, the
recording, the alarms and a screenshot of a QtDMM test instance. Read-only
unless started with --allow-control, which adds tools that connect the
meter, record, set marks, reset min/max, acknowledge alarms and press the
meter's keys. Not part of QtDMM and not installed with it.

    pip install mcp
    python3 tools/qtdmm-mcp.py --port 5099 [--allow-control]

See docs/dev/qtdmm-mcp.md.
"""

import argparse
import math
import socket
import time
from datetime import datetime

try:  # mcp 2.x renamed FastMCP to MCPServer
    from mcp.server.mcpserver import Image, MCPServer as FastMCP
    from mcp.server.mcpserver.exceptions import ToolError
except ImportError:
    from mcp.server.fastmcp import FastMCP, Image
    from mcp.server.fastmcp.exceptions import ToolError

parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
parser.add_argument("--host", default="localhost", help="host of the QtDMM instance (default: localhost)")
parser.add_argument("--port", type=int, default=5099, help="its SCPI port (default: 5099)")
parser.add_argument("--timeout", type=float, default=3.0, help="seconds to wait for an answer (default: 3)")
parser.add_argument("--allow-control", action="store_true",
                    help="also offer the tools that change what QtDMM does (connect, record, mark, keys, ...)")
args = parser.parse_args()

mcp = FastMCP("qtdmm")


class Scpi:
    """One connection for a few queries; every error becomes a ToolError, which the client reads as a message."""

    def __enter__(self):
        try:
            self.sock = socket.create_connection((args.host, args.port), timeout=args.timeout)
        except OSError as e:
            raise ToolError(f"No QtDMM SCPI server at {args.host}:{args.port} ({e.strerror or e}). "
                               "Is QtDMM running with the SCPI server on?") from None
        self.buf = b""
        return self

    def __exit__(self, *exc):
        self.sock.close()

    def _read(self, n):
        while len(self.buf) < n:
            self._more()
        data, self.buf = self.buf[:n], self.buf[n:]
        return data

    def _more(self):
        try:
            chunk = self.sock.recv(65536)
        except socket.timeout:
            raise ToolError(f"No answer from QtDMM within {args.timeout} s") from None
        if not chunk:
            raise ToolError("QtDMM closed the connection")
        self.buf += chunk

    def query(self, cmd):
        self.sock.sendall(cmd.encode() + b"\n")
        while b"\n" not in self.buf:
            self._more()
        line, self.buf = self.buf.split(b"\n", 1)
        return line.decode(errors="replace").strip()

    def channel(self, queries, channel):
        """The answers of queries ("READ?", "UNIT?") for one channel; a channel the meter has not
        becomes a ToolError with the server's error (a refused query answers nothing, *OPC? does)."""
        ch = f" (@{int(channel)})"
        answers = self.query(";".join(q + ch for q in queries) + ";*OPC?").split(";")
        if len(answers) != len(queries) + 1:
            # the queue may hold older errors: the newest is this query's
            last = None
            for _ in range(50):
                e = self.query("SYSTem:ERRor?")
                if e.startswith("0,"):
                    break
                last = e
            raise ToolError(f"Channel {channel}: {last}")
        return answers[:-1]

    def block(self, cmd):
        """An IEEE 488.2 definite length block: #<n><length><data>."""
        self.sock.sendall(cmd.encode() + b"\n")
        if self._read(1) != b"#":
            raise ToolError(f"{cmd} did not answer with a block")
        digits = int(self._read(1))
        data = self._read(int(self._read(digits)))
        self.buf = self.buf.lstrip(b"\r\n")
        return data


def split_list(text):
    """A SCPI answer list such as "a",1.5E+00,"b" as a list of strings, the
    quotes removed (a doubled quote inside is one)."""
    out, cur, quoted, i = [], "", False, 0
    while i < len(text):
        c = text[i]
        if c == '"':
            if quoted and text[i + 1:i + 2] == '"':
                cur += '"'
                i += 1
            else:
                quoted = not quoted
                if not quoted:
                    out.append(cur)
                    cur = None
        elif c == "," and not quoted:
            if cur is not None:
                out.append(cur.strip())
            cur = ""
        elif cur is not None:
            cur += c
        i += 1
    if cur is not None and cur.strip():
        out.append(cur.strip())
    return out


def value(text):
    """+1.2E+00 as a number; overload (-overload for -OL) and no reading as words."""
    v = float(text)
    # the SCPI marks 9.9E+37 and 9.91E+37, compared without float equality
    if math.isclose(abs(v), 9.9e37, rel_tol=1e-6):
        return "overload" if v > 0 else "-overload"
    if math.isclose(v, 9.91e37, rel_tol=1e-6):
        return "no reading"
    return v


@mcp.tool()
def identify() -> str:
    """*IDN? of the QtDMM instance: QtDMM,<meter>,0,<version>."""
    with Scpi() as s:
        return s.query("*IDN?")


@mcp.tool()
def reading(channel: int = 1) -> dict:
    """The current reading of a channel in SI base units, with unit and function (CONFigure?).
    Channel 1 is the main value, 2 and on the meter's other values (see channels())."""
    with Scpi() as s:
        v, unit, function = s.channel(["READ?", "UNIT?", "CONFigure?"], channel)
    return {"channel": int(channel), "value": value(v), "unit": unit.strip('"'), "function": function.strip('"')}


@mcp.tool()
def channels() -> list[dict]:
    """The meter's values as channels for reading() and watch() (QTDMM:CHANnel:CATalog?): what each
    measures and its unit, empty before its first reading."""
    with Scpi() as s:
        items = split_list(s.query("QTDMM:CHANnel:CATalog?"))
    return [{"channel": int(items[i]), "measures": items[i + 1], "unit": items[i + 2]}
            for i in range(0, len(items) - 2, 3)]


@mcp.tool()
def status() -> dict:
    """STATus:QUEStionable? (overload, hold, not connected, no reading), STATus:OPERation? and *STB?."""
    with Scpi() as s:
        q = int(s.query("STATus:QUEStionable?"))
        op = int(s.query("STATus:OPERation?"))
        stb = int(s.query("*STB?"))
    return {"questionable": q, "overload": bool(q & 1), "hold": bool(q & 2),
            "not_connected": bool(q & 4), "no_reading": bool(q & 8),
            "operation": op, "recording": bool(op & 16), "alarm": bool(op & 512), "stb": stb}


@mcp.tool()
def errors() -> dict:
    """Empties the error queue (SYSTem:ERRor? until 0,"No error"). The queue is shared by all clients."""
    out = []
    with Scpi() as s:
        for _ in range(50):
            e = s.query("SYSTem:ERRor?")
            if e.startswith("0,"):
                break
            out.append(e)
    return {"errors": out}


@mcp.tool()
def screenshot() -> Image:
    """A picture of the QtDMM window (HCOPy:SDUMp:DATA?), as PNG."""
    with Scpi() as s:
        return Image(data=s.block("HCOPy:SDUMp:DATA?"), format="png")


@mcp.tool()
def watch(count: int = 10, interval: float = 1.0, channel: int = 1) -> list[dict]:
    """Collects count readings of a channel (1 the main value), one every interval seconds
    (at most 300 readings and 120 s)."""
    count = max(1, min(count, 300))
    interval = max(0.1, min(interval, 120.0 / count))
    out = []
    with Scpi() as s:
        unit = s.channel(["UNIT?"], channel)[0].strip('"')
        for i in range(count):
            if i:
                time.sleep(interval)
            out.append({"time": datetime.now().isoformat(timespec="milliseconds"),
                        "value": value(s.channel(["READ?"], channel)[0]), "unit": unit})
    return out


@mcp.tool()
def min_max(channel: int = 0) -> dict:
    """The min/max memory (CALCulate:AVERage): minimum, maximum, mean and number of readings since
    the last reset, in SI base units. Without a channel the display's, of its primary value; with
    one (1 the main value, see channels()) that value's own, whatever the display follows."""
    ch = f" (@{int(channel)})" if channel else ""
    with Scpi() as s:
        # through channel(): a channel the meter has not is a ToolError
        count = int(s.channel(["CALCulate:AVERage:COUNt?"], channel)[0] if channel
                    else s.query("CALCulate:AVERage:COUNt?"))
        unit = s.query("UNIT?" + ch).strip('"')
        if not count:
            return {"count": 0, "unit": unit}
        return {"count": count, "min": value(s.query("CALCulate:AVERage:MINimum?" + ch)),
                "max": value(s.query("CALCulate:AVERage:MAXimum?" + ch)),
                "mean": value(s.query("CALCulate:AVERage:AVERage?" + ch)), "unit": unit}


@mcp.tool()
def recording() -> dict:
    """What the graph does (QTDMM:RECord?): live, recording, view or idle, the seconds it spans,
    the number of readings and the unit."""
    with Scpi() as s:
        mode, seconds, count, unit = split_list(s.query("QTDMM:RECord?"))
    names = {"LIVE": "live", "REC": "recording", "VIEW": "view", "IDLE": "idle"}
    return {"mode": names.get(mode, mode), "seconds": float(seconds), "readings": int(count), "unit": unit}


@mcp.tool()
def recording_data(count: int = 100, channel: int = 1) -> dict:
    """The last count readings of the graph (QTDMM:RECord:DATA?, at most 10000), oldest first:
    seconds since the start and the value in SI base units, "overload" or "gap". Channel 1 is the
    main value, 2 and on the meter's other values (see channels())."""
    count = max(1, min(count, 10000))
    with Scpi() as s:
        if int(channel) == 1:
            unit = s.query("QTDMM:RECord?").rsplit(",", 1)[-1].strip('"')
        else:
            unit = s.channel(["UNIT?"], channel)[0].strip('"')
        csv = s.block(f"QTDMM:RECord:DATA? {count},(@{int(channel)})").decode()
    points = []
    for line in csv.splitlines()[1:]:
        t, v = line.split(",")
        points.append({"seconds": float(t), "value": "gap" if v == "nan" else value(v)})
    return {"unit": unit, "readings": points}


@mcp.tool()
def alarms() -> dict:
    """The alarms of the meter with their state (QTDMM:ALARm:LIST?): off, pending (the condition
    holds, not yet long enough), raised or acknowledged."""
    states = {"OFF": "off", "PEND": "pending", "RAIS": "raised", "ACKN": "acknowledged"}
    with Scpi() as s:
        items = split_list(s.query("QTDMM:ALARm:LIST?"))
    pairs = [] if items == [""] else list(zip(items[0::2], items[1::2]))
    return {"alarms": [{"name": n, "state": states.get(st, st)} for n, st in pairs]}


@mcp.tool()
def keys() -> dict:
    """The meter's keys QtDMM can press (QTDMM:KEY:CATalog?); empty for a meter without."""
    with Scpi() as s:
        items = split_list(s.query("QTDMM:KEY:CATalog?"))
    return {"keys": [k.lower() for k in items if k]}


@mcp.tool()
def limit_test() -> dict:
    """The PASS/FAIL limit test (CALCulate:LIMit): on or off, the limits in SI base units, the
    verdict on the last reading of its value (pass, high, low or none) and the verdicts counted
    since the last reset."""
    verdicts = {"PASS": "pass", "HI": "high", "LO": "low", "NONE": "none"}
    with Scpi() as s:
        on = s.query("CALCulate:LIMit:STATe?") == "1"
        lower = float(s.query("CALCulate:LIMit:LOWer?"))
        upper = float(s.query("CALCulate:LIMit:UPPer?"))
        verdict = s.query("QTDMM:LIMit:VERDict?")
        passed, high, low = (int(n) for n in s.query("QTDMM:LIMit:COUNt?").split(","))
    return {"enabled": on, "lower": lower, "upper": upper, "verdict": verdicts.get(verdict, verdict),
            "count": {"pass": passed, "high": high, "low": low}}


def command(cmd):
    """Sends a command and reads the error queue once: an error there becomes the tool's error.
    The queue is shared by all clients, so an older error may show up here too."""
    # the server reads line by line: a line break in a mark's text or a key would send
    # commands of its own
    if "\n" in cmd or "\r" in cmd:
        raise ToolError("no line breaks in a text or key")
    with Scpi() as s:
        s.sock.sendall(cmd.encode() + b"\n")
        e = s.query("SYSTem:ERRor?")
    if not e.startswith("0,"):
        raise ToolError(f"QtDMM answered {cmd} with {e}")
    return "ok"


if args.allow_control:
    @mcp.tool()
    def connect(on: bool = True) -> str:
        """Connects (on) or disconnects the meter (INPut ON|OFF). The connection starts after the
        command; read status a moment later."""
        return command("INPut ON" if on else "INPut OFF")

    @mcp.tool()
    def record_start() -> str:
        """Starts a recording (INITiate). QtDMM may ask for the length in a dialog first; while it
        records, or over a recording not saved yet, it refuses (error -213)."""
        return command("INITiate")

    @mcp.tool()
    def record_stop() -> str:
        """Stops the recording (ABORt)."""
        return command("ABORt")

    @mcp.tool()
    def mark(text: str = "", note: str = "") -> str:
        """A note at the newest reading: title `text` on its flag in the graph, the longer `note` on a click,
        and a mark in the readings table (QTDMM:MARK)."""
        cmd = 'QTDMM:MARK "' + text.replace('"', '""') + '"'
        if note:
            cmd += ',"' + note.replace('"', '""') + '"'
        return command(cmd)

    @mcp.tool()
    def reset_min_max() -> str:
        """Resets the min/max memory, like F4 (CALCulate:AVERage:CLEar)."""
        return command("CALCulate:AVERage:CLEar")

    @mcp.tool()
    def acknowledge_alarms() -> str:
        """Acknowledges the raised alarms, like the banner's button (QTDMM:ALARm:ACKnowledge)."""
        return command("QTDMM:ALARm:ACKnowledge")

    @mcp.tool()
    def set_limit_test(enabled: bool | None = None, lower: float | None = None,
                       upper: float | None = None) -> str:
        """Sets the PASS/FAIL limit test (CALCulate:LIMit): limits in SI base units (a limit set
        makes it a test of lower and upper limit), and/or switches it on or off. What is not
        given stays."""
        cmds = []
        if lower is not None:
            cmds.append(f"CALCulate:LIMit:LOWer {float(lower)!r}")
        if upper is not None:
            cmds.append(f"CALCulate:LIMit:UPPer {float(upper)!r}")
        if enabled is not None:
            cmds.append("CALCulate:LIMit:STATe " + ("ON" if enabled else "OFF"))
        if not cmds:
            raise ToolError("nothing to set: give enabled, lower or upper")
        return command(";:".join(cmds))

    @mcp.tool()
    def clear_limit_count() -> str:
        """Resets the PASS/FAIL count (CALCulate:LIMit:CLEar)."""
        return command("CALCulate:LIMit:CLEar")

    @mcp.tool()
    def press_key(key: str) -> str:
        """Presses a key of the meter (QTDMM:KEY), one of those the tool keys lists: "hold", "range",
        "select1", "max", "unit", ... Only UNI-T Bluetooth meters (UT60BT, UT61B+/D+/E+, UT161, UT202BT, the iENV
        meters UT333BT, UT353BT, UT363BT, UT383BT) have keys."""
        return command('QTDMM:KEY "' + key.replace('"', '""') + '"')


if __name__ == "__main__":
    mcp.run()
