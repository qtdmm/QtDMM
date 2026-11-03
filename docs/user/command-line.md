# Command line

```
qtdmm [options]
```

| Option | Meaning |
|---|---|
| `--config-id <id>` | Use the named configuration instead of `default`. Each id has its own settings file: its window, its views and the devices it connects. |
| `--connect <id>` | Connect the entry `<id>` of *My devices* at the start, after those connected last time; repeat it for more. Its readings reach the SCPI server (`INSTrument:SELect`), and it records with the others; see [SCPI server](scpi-server.md#qtdmms-own-commands). The ids are listed by `QTDMM:DEVice:CATalog?`. `--also <id>` (before 26.4) does the same. |
| `--config-dir <dir>` | Directory for the configuration files (default: the platform's user config location). |
| `--vxi11-rpc auto\|system\|own` | For this run, where VXI-11 gets its portmapper from: the system's rpcbind, an own one on port 111, or automatically (see [SCPI server](scpi-server.md#vxi-11)). Replaces the setting without changing it. |
| `--vxi11-portmapper-port <port>` | For tests and development: VXI-11 with an own portmapper on this port instead of 111 (0: any free one), for this run. VISA libraries and lxi-tools ask port 111 and do not find it there; a client of your own (or a test) asks this port. |
| `--debug` | Print every frame received from the meter as hex to the console. Useful when a meter is not decoded correctly — include this output in a bug report. |
| `-h`, `--help` | Show the options. |
| `-v`, `--version` | Show the version. |

## Several meters at once

One QtDMM reads several meters, all alike. *Connect* in the context menu
of an entry of [My devices](my-devices.md) (or dragging it onto
**Bench**, or *Connect* at the end of the assistant *Add
device*) connects the device in the same QtDMM, with a front panel of its
own; `--connect <id>` does the same at the start, and over SCPI
`QTDMM:DEVice:OPEN`. The node **Bench** of the sidebar (F9)
lists them with their current reading, the active device in bold; *Close*
in the context menu closes one (see [My devices](my-devices.md#several-meters-front-panels-and-the-active-device)). They record together, on one
time base, and a [calculated value](calculated-values.md) reads them by
their names.

`--config-id` keeps a set of settings of its own (a file of its own).
A second QtDMM started with another id runs on its own, without a link to
the first; started with the same id it says that this one runs already.
The settings files of the instances of QtDMM 26.3 and earlier stay where
they are.

## Debug output

`--debug` writes each received frame as a line of hex bytes, e.g.

```
30 30 30 30 30 31 3B 30 30 30 3A 30 0D 0A
```

These lines are exactly what the decoder test fixtures are made of, so a short
capture together with what the meter displayed at the time is the most useful
thing to attach when reporting a decoding problem.
