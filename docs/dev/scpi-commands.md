# SCPI command reference

Everything the [SCPI server](../user/scpi-server.md) understands, as
implemented in `src/service/scpiserver.cpp` (`ScpiServer::handle()`) and checked by
`tests/test_scpi.cpp`. No command changes what the meter measures (its
function or range); the commands under *Control* start and stop the
recorder, connect the meter, reset min/max, set marks and press the keys
of the meters that have remote keys.

## Message format

- Transport: raw TCP, one **program message per line** (`\n`, `\r\n` also
  accepted); or HiSLIP 1.0 (`src/service/hislipserver.cpp`, synchronized
  mode, no locks, messages up to 1 MB, `tests/test_hislip.cpp`), where a
  message may also hold several lines and each query line answers with a
  message of its own; or VXI-11 (`src/service/vxi11core.cpp`,
  `vxi11server.cpp`, `oncrpc.cpp`; core and abort channel, no interrupt
  channel, 16 links, messages up to 1 MB (the rest of a bigger one is
  dropped up to its END), a link counts only on the connection that created
  it, a lock ends with its connection,
  `tests/test_vxi11.cpp`), the same way: a `device_write` with END runs
  the lines, each query answer is read with `device_read` of its own.
  `device_clear` (and HiSLIP's device clear) leaves the error queue and
  the status alone, as IEEE 488.2 asks; only `*CLS` empties them.
- Several commands per message separated by `;`. Their query answers come
  back in one line, separated by `;`, terminated by `\n`. A message without
  a query gets no answer at all.
- **Header prefix rule** (SCPI-99 7.2): after `SYST:ERR?` a following
  `VERS?` means `SYST:VERS?`; a header starting with `:` or `*` is absolute.
- Mnemonics in **long or short form**, case-insensitive: `MEASure?`,
  `MEAS?`, `meas?`. The short form is the upper-case part of the long form.
- A **channel list** after a reading query selects the meter's values,
  channel 1 the main value: `READ? (@2)`, `FETC? (@1,3)`, `READ? (@1:3)`,
  also mixed (`(@1,2:3)`). Several channels answer in the list's order,
  separated by commas. The channels are the values on the Device page
  (`ScpiServer::setChannelCount()`), 1 and 2 always, and any value a
  reading came for; another gives `-222`, a broken list `-104`.
- A **numeric suffix** on the last mnemonic is short for a single channel:
  `READ?` = `READ1?` = main value, `READ2?` = the second value (Victron,
  Fluke 45), `READ3?` ... A suffix beyond the channels answers
  `-114,"Header suffix out of range"`; a channel list wins over a suffix.
- Numbers are **NR3** in SI base units: `+1.234500E+00`, six decimals.
  `9.9E+37` is +INF (overload), `-9.9E+37` −INF, `9.91E+37` is NaN (no
  usable reading: not connected, nothing received for 5 s, or no second
  value).
- Strings are double-quoted: `"V"`, `"VOLT:DC AUTO"`.

## IEEE 488.2 common commands

| Command | Answer | Notes |
|---|---|---|
| `*IDN?` | `QtDMM,<meter>,0,<version>` | `<meter>` is the model chosen in the settings (`UNI-T UT61E`), `<version>` the QtDMM version. Manufacturer field is `QtDMM`, so clients can tell it from a real bench meter |
| `*RST` | – | clears the error queue; nothing else to reset |
| `*CLS` | – | clears the error queue |
| `*OPC?` | `1` | always complete |
| `*OPC`, `*WAI` | – | accepted, no effect |
| `*TST?` | `0` | self-test passed |
| `*STB?` | `0` or `4` | status byte: bit 2 (4, EAV) while the error queue holds errors |
| `*ESR?`, `*ESE?`, `*SRE?` | `0` | no event registers; `*ESE n` / `*SRE n` are accepted and ignored |

## Readings

| Command | Answer | Notes |
|---|---|---|
| `READ?`, `FETCh?`, `MEASure?`, `MEASure:SCALar?` | NR3 | the last reading the meter sent; `READ?` does not trigger a measurement |
| `MEASure:<function>[:<type>]?` (e.g. `MEAS:VOLT:DC?`, `MEAS:RES?`) | NR3 | accepted for compatibility with instrument-oriented clients; the function is **ignored** – the meter decides what it measures. Check `CONF?` |
| `[SENSe:]DATA?` | NR3 | same as `READ?` |
| `VALue1?`, `VALue2?` | NR3 | Fluke 45 style names for main and second value |
| `READ2?`, `FETCh2?`, `MEASure2?`, `DATA2?` | NR3 | the second value |
| `READ? (@<list>)` and the others above | NR3`,`NR3… | the channels of the list, in its order |
| `UNIT?`, `UNIT2?`, `UNIT? (@<list>)` | `"V"`, `"A"`, `"Ohm"`, `"F"`, `"Hz"`, `"C"`, `"dF"`, `"%"`, … | the meter's base unit without SI prefix, spelled as the decoder emits it (`C` and `dF` for the temperature scales) |
| `CONFigure?`, `CONFigure2?`, `CONFigure? (@<list>)` | `"<function> <range>"` | see below |

`CONFigure?` derives the function from the meter's mode and unit:

| Function | When |
|---|---|
| `VOLT:DC` / `VOLT:AC` | unit V, mode DC / AC |
| `CURR:DC` / `CURR:AC` | unit A |
| `RES` | unit Ω |
| `CONT` | continuity (beeper mode) |
| `CAP` | unit F |
| `FREQ` | unit Hz |
| `TEMP` | temperature (unit `C` or `dF`) |
| `DIOD` | diode test |
| `PER` | unit % (duty cycle, Victron SoC) |
| `POW` | unit W |
| `PWID` | unit s |
| other | the unit in upper case; `NONE` when nothing is known |

The range is `AUTO`, `MANUAL` or the meter's own range text.

## Status

| Command | Answer | Notes |
|---|---|---|
| `STATus:QUEStionable?`, `STATus:QUEStionable:CONDition?`, `…:EVENt?` | integer | bit 0 (1) overload · bit 1 (2) hold · bit 2 (4) meter not connected (connecting, silent, or lost and reconnecting) · bit 3 (8) no current reading (nothing for 5 s; bits 0 and 1 are then 0) · bit 11 (2048) limit test FAIL LO · bit 12 (4096) FAIL HI (`Sources::verdict`, as Keysight's) |
| `STATus:OPERation?`, `…:CONDition?`, `…:EVENt?` | integer | bit 4 (16) *measuring*: the recorder is running · bit 9 (512, QtDMM's own) an alarm is raised, acknowledged or not |
| `INPut?`, `INPut:STATe?` | `1` / `0` | meter connected and sending? `0` also while QtDMM reconnects a lost meter |
| `SYSTem:VERSion?` | `1999.0` | SCPI version |

## Control

| Command | Effect |
|---|---|
| `INITiate`, `INITiate:IMMediate` | starts the recorder (like the Start button) |
| `ABORt` | stops the recorder |
| `INPut ON`, `INPut 1`, `INPut:STATe ON` | connects the meter (like the Connect button) |
| `INPut OFF`, `INPut 0` | disconnects the meter, also one that is lost and being reconnected |
| `CALCulate:AVERage:CLEar` | resets the min/max memory (F4), the display's and every value's (`MeterController::valueMinMax()`) |
| `CALCulate:AVERage:CLEar (@n)` `CALCulate<n>:AVERage:CLEar` | resets the memory of those values alone (`Sources::clearChannelStats`) |
| `CALCulate:LIMit[:STATe] ON\|OFF\|1\|0` | the PASS/FAIL limit test on or off (34465A); `CALCulate:STATe` the same, `CALCulate:FUNCtion LIMit` accepted (34410A, other functions `-224`) |
| `CALCulate:LIMit:LOWer[:DATA] <v>\|MIN\|MAX\|DEF`, `…:UPPer[:DATA] …` | sets a limit in SI base units (±1.0E+15, DEF 0; beyond `-222`, not a number `-104`); the test becomes `LimitTest::Range`, the other limit kept from `low()`/`high()`. Through `Sources::setLimitTest` → `MeterController::setLimitTest()` → `limitTestChanged`, which the window keeps in its settings |
| `CALCulate:LIMit:CLEar` | resets the PASS/FAIL count (`MeterController::resetLimitCount()`) |
| `QTDMM:ALARm:ACKnowledge` | acknowledges the raised alarms |
| `QTDMM:MARK "text"` | a mark at the newest reading, graph and readings table; `SCPI` without text |
| `QTDMM:NODE:RESet` | a calculated value's state back to 0 (an integral, `NodeRunner::reset()`); `-241` for a device |
| `QTDMM:KEY "hold"` | presses a key of the meter (`DmmDecoder::keyNames()` the decoder supports); `-241` for a meter without keys, `-224` for a key it lacks, `-240` when not sent |

There is no lock: every client that reaches the port may use these. The
qtdmm-mcp tool offers them only with `--allow-control`.

## Min/max and QtDMM's own queries

| Command | Answer | Notes |
|---|---|---|
| `CALCulate:AVERage:MINimum?` `…:MAXimum?` `…:AVERage?` | NR3 | the display's min/max memory, of its primary value (`MinMaxMemory`), arithmetic mean; `9.91E+37` while empty. With `(@list)` or `CALCulate<n>`: the memory of each value of its own (`Sources::channelStats`), comma separated |
| `CALCulate:AVERage:COUNt?` | integer | readings since the last reset, overloads and held readings not counted; per channel as above |
| `QTDMM:RECord?` | `IDLE\|LIVE\|REC\|VIEW,<s>,<n>,"<unit>"` | `RecordingStore` state; `IDLE` = view with no readings; seconds = `duration()` |
| `QTDMM:RECord:DATA? [<n>\|MAX][,(@list)]` | definite-length block | CSV `seconds,value`, the last n points (100 default, `ScpiServer::kMaxDataPoints` = 10000 at most, more is cut to it); overload `9.9E+37`, gap `nan`; `-222` for n < 1 or not whole or a channel the meter has not, `-104` for not a number. A channel list picks the series (channel 1 the main one, n the last `RecordingStore::extra()` of id n-1); several give `channel,seconds,value`, the last n of each in the list's order |
| `QTDMM:ALARm?` | integer | alarms raised or acknowledged |
| `QTDMM:ALARm:LIST?` | `"<name>",<state>,…` | `OFF\|PEND\|RAIS\|ACKN` per alarm, `""` when none is set up; quotes in a name doubled |
| `CALCulate:LIMit[:STATe]?`, `CALCulate:STATe?` | `1`/`0` | limit test on |
| `CALCulate:LIMit:LOWer?` `…:UPPer?` `[MIN\|MAX]` | NR3 | the limits as the test uses them (`LimitTest::low()`/`high()`), also for nominal ± tolerance |
| `CALCulate:FUNCtion?` | `LIM` | the only function |
| `QTDMM:LIMit:VERDict?` | `PASS\|HI\|LO\|NONE` | `MeterController::verdict()`, on the last reading of the tested value |
| `QTDMM:PHOTo:LAST?` | `"<path>","<taken>"` | the newest photo of the open project (`PhotoStore::newest(ProjectFolder::current())`): in `photos` (or the project itself, files not sorted by kind), by the time in its name `photo_<date>_<time>[_n]`; taken as ISO 8601 local time; `"",""` without one or without a project |
| `QTDMM:PHOTo:DATA?` | definite-length block | that photo's file as it is on disk (JPEG, PNG ...); `-230` without one |
| `QTDMM:LIMit:COUNt?` | `<pass>,<high>,<low>` | `LimitTest::Count` since the last reset (new test, min/max reset, `CALC:LIM:CLE`) |
| `QTDMM:KEY:CATalog?` | `"<KEY>",…` | upper case, `""` for none |
| `QTDMM:CHANnel:CATalog?` | `<n>,"<port>","<unit>",…` | the channels of `READ? (@n)`: what each measures (`PortKey`: `voltage.dc`, `temperature`, `carbon_dioxide`) and its unit, empty strings before its first reading |

What the server does not hold itself it asks for through
`ScpiServer::Sources`, set by `MeterController::installScpiSources()`.

## Screen dump

| Command | Answer |
|---|---|
| `HCOPy:SDUMp:DATA?`, `DISPlay:DATA?` | the main window as an image in an IEEE 488.2 definite-length block: `#` + number of length digits + length + bytes, e.g. `#676714<PNG…>`, then `\n` |
| `HCOPy:SDUMp:DATA:FORMat PNG|BMP|JPG`, `…:FORMat?` | image format, PNG by default |

The same command Keysight and Rigol instruments use for their screenshots,
so a generic "grab screen" routine works. Read the block, not a line:

```python
s.sendall(b"HCOP:SDUM:DATA?\n")
head = s.recv(2)                       # b"#6"
length = int(s.recv(int(head[1:2])))
png = b""
while len(png) < length:
    png += s.recv(length - len(png))
```

(`lxi screenshot` and the lxi-gui button talk VXI-11 only and pick a
plugin by `*IDN?`; with VXI-11 switched on they work with the plugin given
by hand: `lxi screenshot -a host -p keysight-dmm`.)

## Error queue

| Command | Answer |
|---|---|
| `SYSTem:ERRor?`, `SYSTem:ERRor:NEXT?` | the oldest entry, `0,"No error"` when empty |
| `SYSTem:ERRor:COUNt?` | number of entries |

The queue holds 20 entries; when full, the last one becomes
`-350,"Queue overflow"`. `*CLS` and `*RST` empty it. Codes used:

| Code | Text | When |
|---|---|---|
| `-100` | Command error | a query used as a command (`READ`) or the other way round, an unparsable header |
| `-109` | Missing parameter | `INPut` without ON/OFF |
| `-113` | Undefined header | unknown command; the query is skipped in the answer line |
| `-114` | Header suffix out of range | a suffix beyond the channels (`READ5?` on a meter with 3) |
| `-104` | Data type error | `QTDMM:RECord:DATA? lots`, a broken channel list (`(@`, `(@3:1)`) |
| `-222` | Data out of range | `QTDMM:RECord:DATA? 0`, `READ? (@4)` on a meter with 3 values |
| `-224` | Illegal parameter value | `INPut MAYBE`, `QTDMM:KEY "peak"` on a meter without PEAK |
| `-230` | Data corrupt or stale | a reading query answered `9.91E+37`; `QTDMM:PHOTo:DATA?` without a photo |
| `-240` | Hardware error | `HCOPy:SDUMp:DATA?` when no window is there to dump; `QTDMM:KEY` not sent (meter not connected) |
| `-241` | Hardware missing | `QTDMM:KEY` for a meter without keys |
| `-350` | Queue overflow | more than 20 errors unread |
| `-363` | Input buffer overrun | 64 KB without a line terminator; the buffer was discarded |

## Examples

```
*IDN?                        → QtDMM,Uni-Trend UT61E,0,2026.9.22
READ?;UNIT?;CONF?            → +1.234500E+00;"V";"VOLT:DC AUTO"
READ2?                       → 9.91E+37            (UT61E has no second value)
FETC? (@1:3)                 → +8.000000E+02,+2.250000E+01,+4.500000E+01   (TFA AirCO2ntrol)
QTDMM:CHAN:CAT?              → 1,"carbon_dioxide","ppm",2,"temperature","C",3,"relative_humidity","%"
SYST:ERR?;VERS?              → -230,"Data corrupt or stale";1999.0
INIT;STAT:OPER?              → 16
:ABOR;:STAT:OPER?            → 0
FOO?;READ?                   → +1.234500E+00       (FOO? skipped, -113 queued)
```

Not implemented, on purpose: `TRIGger`, `SAMPle`, `CALCulate` beyond
`AVERage`, range and
function setters (`CONF:VOLT:DC 20`) – the meters QtDMM reads have no
remote control – and the `SYSTem:COMMunicate` tree.
