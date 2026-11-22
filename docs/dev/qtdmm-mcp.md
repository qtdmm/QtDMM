# MCP server for test sessions

`tools/qtdmm-mcp.py` is a small [MCP](https://modelcontextprotocol.io/)
server for AI assistants (Claude Code and others) that test QtDMM: it reads
a running instance through its [SCPI server](../user/scpi-server.md) and
offers the values, the status, the recording, the alarms and a screenshot
of the window as tools. It is a development tool, not part of QtDMM, and
is not installed with it.

By default it only reads: no tool starts or stops the recorder, connects
the meter or resets anything, also not when it is pointed at the wrong
port. Started with `--allow-control` it adds the tools that change what
the instance does (see *Control* below) - only for a test instance or a
meter of your own on the bench.

## Setup

It needs Python 3.10 or newer and the MCP SDK:

```
pip install mcp
```

Versions 1 and 2 of the SDK both work.

Start the QtDMM instance to test with the SCPI server on (Settings → SCPI
server). The tool uses port 5099 unless told otherwise, so that it does not
reach a QtDMM in everyday use on the default port 5025 by accident. For
Claude Code, a `.mcp.json` in the working directory (not committed):

```json
{
  "mcpServers": {
    "qtdmm": {
      "command": "python3",
      "args": ["tools/qtdmm-mcp.py", "--port", "5099"]
    }
  }
}
```

Options: `--host` (default `localhost`), `--port` (default 5099),
`--timeout` in seconds (default 3), `--allow-control` for the control
tools.

A test instance needs no display: with `QT_QPA_PLATFORM=offscreen` QtDMM
runs without a window on screen, and `screenshot` still returns the
window as it would look. Give it its own configuration (and, beside a QtDMM
in use, its own SCPI port):

```
QT_QPA_PLATFORM=offscreen qtdmm --config-dir /tmp/qtdmm-test --config-id mcp
```

## Tools

| Tool | SCPI | Answer |
|---|---|---|
| `identify` | `*IDN?` | `QtDMM,<meter>,0,<version>` |
| `reading` | `READ? (@n)`, `UNIT? (@n)`, `CONFigure? (@n)` | value in base units, unit, function of `channel` (1 the main value, the default); `overload` (`-overload` for -OL) or `no reading` instead of the value; a channel the meter has not is an error with the server's `-222` |
| `channels` | `QTDMM:CHANnel:CATalog?` | the meter's values as channels: number, what it measures (`voltage.dc`, `temperature` ...), unit; empty before the first reading |
| `status` | `STATus:QUEStionable?`, `STATus:OPERation?`, `*STB?` | the registers and their bits: overload, hold, not connected, no reading, recording, alarm |
| `errors` | `SYSTem:ERRor?` | the error queue, emptied; it is shared by all clients of the instance |
| `screenshot` | `HCOPy:SDUMp:DATA?` | the window as PNG image |
| `watch` | `READ? (@n)` | `count` readings of `channel` with time stamps, one every `interval` seconds (at most 300 and 120 s) |
| `min_max` | `CALCulate:AVERage:…? [(@n)]` | minimum, maximum, mean and number of readings since the last reset, and the unit: of the display's primary value, or with `channel` that value's own memory |
| `recording` | `QTDMM:RECord?` | `live`, `recording`, `view` or `idle`, the seconds it spans, readings, unit |
| `recording_data` | `QTDMM:RECord:DATA? <n>,(@n)` | the last `count` readings of `channel` in the graph (1 the main value, the default; at most 10000): seconds since the start and value, `overload` or `gap` |
| `alarms` | `QTDMM:ALARm:LIST?` | each alarm with its state: `off`, `pending`, `raised`, `acknowledged` |
| `limit_test` | `CALCulate:LIMit:…?`, `QTDMM:LIMit:VERDict?`, `QTDMM:LIMit:COUNt?` | the PASS/FAIL test: on or off, lower and upper limit in base units, the verdict (`pass`, `high`, `low`, `none`) and the count since the last reset |
| `keys` | `QTDMM:KEY:CATalog?` | the meter's keys QtDMM can press, empty for most meters |
| `last_photo` | `QTDMM:PHOTo:LAST?`, `QTDMM:PHOTo:DATA?` | the newest photo of the setup in the open project (*Photo of the setup*): when it was taken, its path and the picture, for a model that sees images; an error without one |

### Control

Only with `--allow-control`; without it these tools do not exist. Each
sends one command and then reads the error queue once: an error there is
the tool's error (the queue is shared, so an older error of another client
may show up too).

| Tool | SCPI | Effect |
|---|---|---|
| `connect` | `INPut ON\|OFF` | connects (`on`, default) or disconnects the meter |
| `record_start` | `INITiate` | starts a recording; refused while one runs or over one not saved |
| `record_stop` | `ABORt` | stops it |
| `mark` | `QTDMM:MARK` | a mark with `text` in the graph and the readings table (one line: a line break is refused) |
| `reset_min_max` | `CALCulate:AVERage:CLEar` | resets min/max, like F4 |
| `acknowledge_alarms` | `QTDMM:ALARm:ACKnowledge` | acknowledges the raised alarms |
| `set_limit_test` | `CALCulate:LIMit:LOWer`, `:UPPer`, `:STATe` | sets `lower` and/or `upper` (base units; makes it a test of lower and upper limit) and/or switches the test `enabled`; what is not given stays |
| `clear_limit_count` | `CALCulate:LIMit:CLEar` | resets the PASS/FAIL count |
| `press_key` | `QTDMM:KEY` | presses one of the keys `keys` lists (UNI-T Bluetooth meters) |

When QtDMM is not running, its SCPI server is off or an answer takes longer
than the timeout, a tool returns a short error message saying so.
