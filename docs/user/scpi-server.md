# SCPI server: the meter as a network instrument

Bench multimeters have a LAN port and answer SCPI queries - `*IDN?`,
`READ?` - from any program that can open a TCP socket. QtDMM can do the
same for the meter it reads: switch on the **SCPI server** under
**Settings → SCPI server** and a consumer multimeter on a USB cable, a
Victron device over Bluetooth or the simulated meter becomes an instrument
that [lxi-tools](https://github.com/lxi-tools/lxi-tools), LabVIEW,
a Python script or a second QtDMM can query - over a plain TCP socket,
over **HiSLIP**, the LAN protocol of VISA (NI-VISA, LabVIEW, Keysight IO
Libraries, PyVISA), or over **VXI-11**, its older one (lxi-tools, older
VISA).

The server only reports. It never sends anything to the meter; the meter
keeps deciding what it measures.

## Switching it on

| Setting | Meaning |
|---|---|
| Enable the SCPI server | off by default |
| Port | 5025 is the usual raw-socket SCPI port. When it is taken - a second QtDMM, say - the next free one is used; the sidebar shows the actual port |
| Listen on | *This computer only* binds to `localhost`; *All network interfaces* opens the port to the network |
| Also HiSLIP | the same commands for VISA on a second port, off by default; see *VISA (HiSLIP)* below |
| HiSLIP port | 4880 is the port VISA tries when the address names none; taken, the next free one is used |
| Also VXI-11 | the same commands over VXI-11, off by default; needs port 111, see *VXI-11* below |
| Portmapper | for VXI-11: *Automatic* (the system's rpcbind when it runs, else an own one), *System rpcbind* or *Own, on port 111* |
| Announce by mDNS | with the port open to the network, QtDMM registers as `_scpi-raw._tcp` (and `_hislip._tcp`, `_vxi-11._tcp`) so that `lxi discover -m` and every DNS-SD browser list it as "QtDMM *host*" (or "QtDMM *instance*" with `--config-id`) |

The sidebar (F9) lists them under **Services**: *SCPI*, *HiSLIP* and
*VXI-11* with their port (for VXI-11 the portmapper's, 111), or *off*. The dot is dark green while the service waits for a
client and turns bright green while one is connected, the tooltip has the
address and the number of clients. *failed* with a red dot means switched
on but not running, usually because all ten ports from the one set here
are taken; the tooltip and the status line of these settings say why. A
double click opens these settings; the context menu also copies the
address (`localhost:5025`, `TCPIP::localhost::hislip0::INSTR`,
`TCPIP::localhost::INSTR`).
There is **no authentication**: whoever can reach the port can read the
meter, start the recorder, connect or disconnect - and fetch a picture of
the QtDMM window (`HCOPy:SDUMp:DATA?`), which shows the readings, the
window title and whatever the readings table holds. Keep *This computer
only* unless the network is yours; from elsewhere use an SSH tunnel
(`ssh -L 5025:localhost:5025 bench`).

## Talking to it

```
$ lxi scpi -a bench.local -p 5025 -r "*IDN?"
QtDMM,Uni-Trend UT61E,0,2026.9.22
$ lxi scpi -a bench.local -p 5025 -r "READ?;UNIT?;CONF?"
+1.234500E+00;"V";"VOLT:DC AUTO"
$ printf 'READ?\n' | nc bench.local 5025
```

```python
import socket
s = socket.create_connection(("bench.local", 5025))
s.sendall(b"READ?\n")
print(float(s.recv(64)))      # 1.2345
```

One program message per line; several commands go on one line separated
by `;` and their answers come back the same way. As in IEEE 488.2, a
command after `;` continues at the level of the one before
(`SYST:ERR?;VERS?`); start it with `:` to begin at the top again
(`QTDMM:REC?;:CALC:AVER:MIN?`). Mnemonics take the long or the short form
(`MEASure?`, `MEAS?`), case does not matter.

| Command | Answer |
|---|---|
| `*IDN?` | `QtDMM,<meter>,0,<version>` |
| `READ?` `FETCh?` `MEASure?` `MEASure:VOLTage:DC?` `[SENSe:]DATA?` `VALue1?` | the reading in SI base units as `+1.234500E+00`; `9.9E+37` for overload, `9.91E+37` (not a number) when nothing arrived in the last 5 s, or the meter shows no number (`----`, a fault word such as `FUSE`) |
| `READ? (@2)` `FETCh? (@1:3)` `READ? (@1,3)` | the meter's other values as channels, 1 the main value: one (`(@2)`), a list or a range; several answer in the list's order, separated by commas: `+8.0E+02,+2.25E+01,+4.5E+01`. The channels are the values on the meter's Device page; a channel it has not gives error -222, a broken list -104 |
| `READ2?` `FETCh2?` `VALue2?` | short for `(@2)` (Victron, Fluke 45), same rules; `READ3?` ... alike |
| `UNIT?` `UNIT2?` `UNIT? (@1:3)` | the base unit, `"V"` |
| `CONFigure?` `CONFigure2?` `CONFigure? (@2)` | function and range, `"VOLT:DC AUTO"` - functions `VOLT:DC`, `VOLT:AC`, `CURR:DC`, `CURR:AC`, `RES`, `CONT`, `CAP`, `FREQ`, `TEMP`, `DIOD`, `PER`, `POW` |
| `STATus:QUEStionable?` | bit 0 overload, bit 1 hold, bit 2 meter not connected (also while QtDMM reconnects it), bit 3 no current reading, bit 11 the [PASS/FAIL test](limit-test.md)'s FAIL LO, bit 12 its FAIL HI (as Keysight's) |
| `STATus:OPERation?` | 16 while the recorder runs, plus 512 while an alarm is raised (acknowledged or not) |
| `INITiate` / `ABORt` | starts / stops the recorder; while it records, or over a recording shown and not saved yet, `INITiate` is ignored with error -213 *Init ignored*. On any meter they start and stop the recording of all meters of this QtDMM, as *Record* does (in the window with its questions) |
| `INPut ON|OFF`, `INPut?` | connects / disconnects the meter, asks whether it is; the connection starts after the command, so `INPut ON;INPut?` still answers 0 |
| `HCOPy:SDUMp:DATA?` | a screenshot of the QtDMM window as PNG (IEEE 488.2 block), like a bench instrument's screen dump; `HCOPy:SDUMp:DATA:FORMat PNG\|BMP\|JPG` chooses the format |
| `SYSTem:ERRor?` `SYSTem:ERRor:COUNt?` | the error queue: `0,"No error"`, `-113,"Undefined header"`, `-230,"Data corrupt or stale"`, ... |
| `CALCulate:AVERage:MINimum?` `…:MAXimum?` `…:AVERage?` `…:COUNt?` | the min/max memory as the display shows it, of its primary value (the main value at first): minimum, maximum, mean and number of readings since the last reset (overloads and held readings not counted); `9.91E+37` while it is empty |
| `CALCulate:AVERage:MAXimum? (@2)` `CALCulate2:AVERage:MAXimum?` `…? (@1:3)` | the same for a value of the meter (channels as for `READ?`), its own memory whatever the display shows; several answer separated by commas |
| `CALCulate:AVERage:CLEar` | resets min/max, like F4: the display's and those of all values |
| `CALCulate:AVERage:CLEar (@2)` `CALCulate2:AVERage:CLEar` | resets the memory of that value alone |
| `CALCulate:LIMit[:STATe] ON\|OFF`, `CALCulate:LIMit[:STATe]?` | the [PASS/FAIL test](limit-test.md) on or off, as the display's menu (34465A); `CALCulate:FUNCtion LIMit` with `CALCulate:STATe ON\|OFF` does the same (34410A), `CALCulate:FUNCtion?` answers `LIM` |
| `CALCulate:LIMit:LOWer[:DATA] <value>\|MIN\|MAX\|DEF`, `…:UPPer[:DATA] …`, and `…?` | the limits in SI base units (MIN/MAX ±1.0E+15, DEF 0). Setting one turns the test into *Lower and upper limit*, the other limit kept; the queries answer the limits as the test uses them, also for nominal ± tolerance |
| `CALCulate:LIMit:CLEar` | resets the PASS/FAIL count |
| `*RST` `*CLS` `*OPC?` `*TST?` `*ESR?` `*STB?` `SYSTem:VERSion?` | the usual IEEE 488.2 replies |
| `INSTrument:CATalog?`, `INSTrument:CATalog:FULL?` | the meters of this QtDMM by name, `"UT61E_BENCH"`, with `FULL` also by number: `"UT61E_BENCH",1`. The name is the one in *My devices* (else the instance's), in upper case, other characters than letters and digits as `_`; two of the same name: the later one `_2`. A name stays with its meter while it is open |
| `INSTrument[:SELect] <name>`, `INSTrument:NSELect <n>`, `INSTrument?`, `INSTrument:NSELect?` | chooses the meter the other commands go to, by name or number, as on a multi-channel bench instrument; the first meter at first. One choice for all clients: two clients that choose in turn reach each other's meter, so send `INST SIM_B;MEAS?` in one line. When the meter chosen is closed, none is chosen: queries answer as without a meter (`9.91E+37`, `INST?` `""`), other commands error -241, until `INSTrument` chooses again or the first meter is connected after none |

### QtDMM's own commands

What a bench meter does not have - the graph, the alarms, the keys of a
Bluetooth meter, the other open devices - is under `QTDMM:`.

| Command | Answer |
|---|---|
| `QTDMM:RECord?` | what the graph does - `LIVE`, `REC` (recording), `VIEW` (a recording that ended, a file) or `IDLE` (nothing yet) - with the seconds it spans, the number of readings and the unit: `REC,42.250,170,"V"` |
| `QTDMM:RECord:DATA? [<n>\|MAX][,(@list)]` | the last n readings of the graph (100 without n, at most 10000) as CSV in an IEEE 488.2 block: a line `seconds,value`, then one line per reading, seconds since the start; `9.9E+37` for an overload, `nan` for a gap. A channel list picks the value (1 the main value, the default): `QTDMM:RECord:DATA? 500,(@2)`; with several the lines begin with the channel, `channel,seconds,value`, the last n of each, one channel after the other (each value has its own times) |
| `QTDMM:RECord:SAVE "<file>"[,RAW]` | writes the recording that ended to `<file>` in the folder *Documents/QtDMM* (made when missing), as the export does: on the grid of the sample time, or every reading with `RAW`; CSV without a suffix, `.xlsx`, `.ods` and the QtDMM recording `.qtdmm-rec` (with the marks) by theirs. A file name only, no folder (error -257), and no file that exists already (-257); -221 while it records or with nothing recorded, -250 when it cannot be written. Its columns are named after the meter (`Sim A: value` ...); at the meter that led the recording the devices that recorded with it follow (`Sim B: value` ...). The recording counts as saved then, theirs too |
| `QTDMM:ALARm?` | the number of alarms raised (acknowledged ones too) |
| `QTDMM:ALARm:LIST?` | every alarm with its state: `"Over 5 V",RAIS,"Low",OFF` - `OFF`, `PEND` (the condition holds, not yet for its minimum duration), `RAIS`, `ACKN` |
| `QTDMM:ALARm:ACKnowledge` | acknowledges the alarms, as the banner's button |
| `QTDMM:LIMit:VERDict?` | the PASS/FAIL test's verdict on the last reading of its value: `PASS`, `HI`, `LO`, `NONE` (off, not connected, no number) |
| `QTDMM:LIMit:COUNt?` | the verdicts counted since the last reset: `<pass>,<high>,<low>` |
| `QTDMM:MARK "text"` | a mark at the newest reading, in the graph and the readings table (`SCPI` without text) |
| `QTDMM:KEY "hold"`, `QTDMM:KEY:CATalog?` | presses a key of the meter (UNI-T UT60BT, UT61B+/D+/E+, UT161, UT202BT, the iENV meters UT333BT, UT353BT, UT363BT, UT383BT, the ZOTEK and OWON Bluetooth meters), lists the keys it has: `"SELECT1","RANGE","AUTO","HOLD",...`; a meter without keys gives error -241, a key not sent (the meter not connected) -240 |
| `QTDMM:CHANnel:CATalog?` | the channels of `READ? (@n)`: number, what it measures and unit - `1,"carbon_dioxide","ppm",2,"temperature","C"`; empty strings before a channel's first reading |
| `QTDMM:DEVice:CATalog?` | the entries of *My devices*: id, name, and 1 when this QtDMM has it open - `"ab12cd34","UT61E bench",1,"9f3e01aa","SmartShunt",0` |
| `QTDMM:DEVice:OPEN "<id>"`, `QTDMM:DEVice:CLOSe "<id>"` | connects an entry of *My devices* in this QtDMM, as *Connect* in the sidebar, or closes it again; `INSTrument` then reaches it. Error -224 for an unknown id, -221 for one connected already, -240 when it does not connect. It records with the others: its recording starts and stops with theirs, on the same time base and sample time, also when it is connected while they record. Save it with `QTDMM:RECord:SAVE`: the next start overwrites one not saved (the status bar says so) |

```
$ lxi scpi -a localhost -p 5025 -r "QTDMM:REC?;:CALC:AVER:MIN?;MAX?"
LIVE,61.500,246,"V";+1.318300E+00;+5.000000E+00
$ lxi scpi -a localhost -p 5025 'QTDMM:MARK "load on"'
```

The commands that change something - `INPut`, `INITiate`, `ABORt`,
`CALCulate:AVERage:CLEar`, `CALCulate:LIMit`, `QTDMM:ALARm:ACKnowledge`, `QTDMM:MARK`,
`QTDMM:KEY`, `QTDMM:DEVice:OPEN`/`CLOSe`, `QTDMM:RECord:SAVE` - work for every client that reaches the port. With *This
computer only* that is a program on this computer; open the port to the
network only where everyone on it may do so.

## VISA (HiSLIP)

VISA libraries address a LAN instrument by a resource string. With
**Also HiSLIP** switched on, QtDMM is

```
TCPIP::bench.local::hislip0::INSTR          (port 4880)
TCPIP::bench.local::hislip0,4881::INSTR     (another port: a second QtDMM)
```

The settings page shows the address for this computer. It works with
NI-VISA (LabVIEW, NI MAX: *Add network device*, *Manual entry*, HiSLIP),
the Keysight IO Libraries and [PyVISA-py](https://pyvisa.readthedocs.io/projects/pyvisa-py/) 0.6
or newer. Older PyVISA-py, as some distributions still package it (Ubuntu
24.04: 0.5.1), knows no HiSLIP and tries VXI-11 on port 111 instead:
"Connection refused". Install a current one with
`python3 -m venv ~/visa && ~/visa/bin/pip install pyvisa pyvisa-py`.
lxi-tools speaks VXI-11 and raw sockets only, not HiSLIP; `lxi discover`
finds QtDMM with `-m` (mDNS), without it only when VXI-11 is on (see
below).

```python
import pyvisa
dmm = pyvisa.ResourceManager("@py").open_resource("TCPIP::bench.local::hislip0::INSTR")
print(dmm.query("*IDN?"))
print(float(dmm.query("READ?")))
```

HiSLIP needs no extra rights and no other service. QtDMM speaks HiSLIP 1.0
in synchronized mode; it grants no locks (`viLock` fails), and a message
may be 1 MB at most. It serves 16 sessions at a time; a connection that has
not opened both channels of its session within 5 seconds is closed. The raw socket is a VISA resource too, with `\n` as
read termination:

```
TCPIP::bench.local::5025::SOCKET
```

The raw socket and HiSLIP share one instrument state: the error queue and
`*STB?` are the same for all clients, so one client may read another's
error. Unknown headers answer nothing and leave `-113,"Undefined header"`
in the queue; a query that could not be answered is skipped in the `;`
list. The complete list of accepted forms, answers and error codes is the
[SCPI command reference](../dev/scpi-commands.md).

## VXI-11

VXI-11 is the LAN protocol VISA used before HiSLIP. Older VISA versions,
old PyVISA-py and lxi-tools (`lxi scpi -a host` without `-p`, `lxi discover`
without `-m`) speak it. With **Also VXI-11** switched on, QtDMM is

```
TCPIP::bench.local::INSTR
```

```
$ lxi discover
Searching for LXI devices - please wait...
  Found "QtDMM,Uni-Trend UT61E,0,26.3" on address 192.168.1.20
$ lxi scpi -a bench.local "READ?"
+1.234500E+00
```

`lxi screenshot` (and the screenshot page of lxi-gui) fetches a picture of
the QtDMM window over VXI-11. lxi picks its plugin by the `*IDN?` answer
and knows no QtDMM, so name the plugin: QtDMM answers the commands of
Keysight's Truevolt multimeters.

```
$ lxi screenshot -a bench.local -p keysight-dmm
```

In lxi-gui choose the plugin *keysight-dmm* instead of *auto*.

**Prefer HiSLIP** where the client has it: VXI-11 needs a portmapper on
port 111, which clients ask for the VXI-11 ports. A computer has one
portmapper, so **only one program per computer** can offer VXI-11: a
second QtDMM with VXI-11 shows *failed* and says which port
serves it already. HiSLIP and the raw socket keep running in either case.

QtDMM never raises its own rights. The portmapper comes from one of two
places (setting *Portmapper*, or `--vxi11-rpc auto|system|own` for one
run):

| Portmapper | How | What it needs |
|---|---|---|
| System rpcbind | QtDMM registers with the rpcbind of the system, over its Unix socket `/run/rpcbind.sock` | rpcbind running: Debian/Ubuntu `sudo apt install rpcbind`, then `sudo systemctl enable --now rpcbind.socket`; Fedora the same with `dnf`. No root for QtDMM |
| Own, on port 111 | QtDMM answers on 111 itself (TCP and UDP, also the broadcasts of `lxi discover`) | Windows and macOS (10.14 and newer): nothing. Linux: port 111 is privileged - either `sudo setcap cap_net_bind_service=+ep /usr/bin/qtdmm` (applies to the whole program, not possible for the AppImage) or `sudo sysctl net.ipv4.ip_unprivileged_port_start=111` (applies to the whole system). FreeBSD: root, or `sysctl net.inet.ip.portrange.reservedhigh=0` |

*Automatic* takes the system rpcbind when one runs and an own portmapper
otherwise. A system rpcbind opens port 111 to the network: mind the
firewall. With *This computer only* QtDMM's own portmapper and the VXI-11
channels listen on `localhost` only; then `lxi discover` (a broadcast to
the network) does not find it, `lxi scpi -a localhost` does.

QtDMM serves 16 links at a time; a message may be 1 MB at most. A lock
(`viLock`) is held until the client unlocks, closes the link or its
connection goes away. There is no interrupt channel (no service
requests).

## Notes

- Values are in base units: a meter showing `12.34 mV` answers
  `+1.234000E-02`, `UNIT?` says `"V"`.
- Units are plain ASCII with one spelling each, whatever the meter's
  protocol writes: `"C"` for °C, `"F"` for °F, `"K"`, `"Ohm"` for Ω - in
  `UNIT?`, `CONFigure?`, `QTDMM:CHANnel:CATalog?`, and `QTDMM:RECord?`
  alike, so a script can compare against them. (`"F"` is
  also farad: `CONFigure?` says `TEMP` or `CAP`.)
- The reading is the last one the meter sent; `READ?` does not trigger a
  measurement. Meters deliver two to four readings a second, so polling
  faster than that repeats values.
- Every QtDMM (`--config-id`) has its own server; with the
  same port configured they line up on 5025, 5026, ... and for HiSLIP on
  4880, 4881, ... VXI-11 only one of them can offer (see above).
- sigrok's `scpi-dmm` driver identifies instruments by `*IDN?` and does not
  know QtDMM, so it will not talk to this server. lxi-tools, PyVISA and
  plain sockets work; the PyVISA resource string is

  ```
  TCPIP::bench.local::5025::SOCKET
  ```
