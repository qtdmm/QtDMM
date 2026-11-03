# Connecting a meter

## Add device

The quickest way: plug the meter in, switch it on and choose **Add
device...**, the device symbol at the left of the toolbar. It sets the meter up step
by step and keeps it in [My devices](my-devices.md). A window without a
meter - the first start - shows nothing but a big **Add device** button.

1. **How is it connected?** *Cable* (USB cable of the meter, USB-serial
   adapter, serial port), *Bluetooth*, *Network*, *sigrok* or *Simulated /
   calculated* - the simulated meter, a signal of its own to try QtDMM without
   hardware, or a value calculated from other windows.
2. **Where?** For a cable, Bluetooth or the network QtDMM looks right away,
   only there, and lists every port it finds, the recognised meters first. A USB cable
   names the meter family it belongs to (UT-D04: the serial UNI-T meters,
   UT-D09: UT61B+/D+/E+ and UT161, BU-86X: Brymen, the TFA AIRCO2NTROL), a
   Bluetooth meter its name, a USB-serial adapter its chip. A device you keep
   in My devices says so - also when the port has another name now
   (`/dev/ttyUSB0` for the saved `/dev/serial/by-id/...`, another
   `/dev/hidraw` number) - and a port in use says *In use here* or which
   open device has it. In the network these are the ports of a
   [qtdmm-bridge](remote-bridge.md) announcing itself by mDNS. Click one, or
   type the port (a Bluetooth address, `host:port` of a bridge or another
   RFC 2217 server) into **Port**. A device QtDMM may not open is listed with the
   reason, and **How to fix...** shows the commands - see
   [Troubleshooting](troubleshooting.md).
3. **Which meter?** The name, then the meter's settings, with the likeliest
   model chosen - only the models of that connection are offered. What is
   behind a serial port, a UT-D04 cable or a bridge port QtDMM cannot tell yet: choose
   yours. A Victron device needs its key here. One of My devices comes with
   its own name and model: it is changed, not added a second time.
4. **Connect it now?** *Connect* keeps it in My devices and connects it at
   once, *Save only* keeps it in My devices without connecting - *Connect*
   in its context menu reads it later (see [My devices](my-devices.md)).

sigrok and *Simulated / calculated* skip step 2.

## Choosing the device by hand

Step 3 of [Add device](#add-device) and **Settings...** of a device in the
sidebar (Shift+F8 for the one in use) hold the settings of the meter:

1. Pick the **vendor** in the first box. The second box then lists only that
   vendor's models. *All vendors* shows the complete list; *Manual settings*
   lets you enter the serial parameters yourself.
2. Pick the **model**. Baud rate, data bits, parity, stop bits, DTR / RTS,
   protocol and display resolution come from QtDMM's device table; there is
   nothing to set, so they are not shown. You can also start typing
   into the model box; the search covers every vendor and switches the vendor
   box for you.
3. Choose the **port** (see below).

With *Manual settings* the serial parameters, the protocol and **DTR** /
**RTS** are shown: set them, DTR / RTS if the meter's cable needs them
powered. **Lines per reading** is for meters that send one line per value
(the Metex M-3850D sends four): each line becomes a value of its own, the
first one the main value. Leave it at 1 for all others.

Manual settings are for meters that are not in the table yet. If you find a
combination that works, please report it on the project page so it can be
added — see [Supported devices](supported-devices.md).

## Port types

The port box lists everything QtDMM found, prefixed with its type:

| Prefix | Backend | Typical entry |
|---|---|---|
| `Serial` | RS-232 and USB-serial adapters | `/dev/ttyUSB0`, `/dev/ttyS0` (Linux), `/dev/cuaU0` (FreeBSD), `COM3` (Windows) |
| `HID` | USB-HID cables: WCH CH9325 / Hoitek HE2325U (UT-D04 and compatible, many Uni-Trend meters), both revisions of the newer UT-D09 (SiLabs CP2110, or WCH CH9329 at a fixed 9600 8N1; the cable of the UT61B+/D+/E+) and Brymen's BU-86X adapter (BM52x/BM82x/BM86x) | `HID 0x1a86:0xe008 /dev/hidraw2` (Linux), `HID 0x1a86:0xe429 \\?\hid#…` (Windows) |
| `RFC2217` | serial port on another machine, via an RFC 2217 server | `localhost:4000` |
| `Sigrok` | any meter that `sigrok-cli` supports; the SCPI bench meters in the model list set this up in their own group ([Bench meters](bench-meters.md)) | `scpi-dmm:conn=/dev/ttyUSB0` |
| `BLE` | Bluetooth LE broadcasts: Victron SmartShunt / MPPT / Phoenix Inverter ([Bluetooth LE](bluetooth.md)); set up in its own group instead of the port box | `CB:09:E4:16:33:DB` |
| `BLEGATT` | Bluetooth LE connection: UNI-T UT60BT ([Bluetooth LE](bluetooth.md)); set up in the same group | `18:90:67:F1:AB:9E` |

RFC2217 and sigrok entries are not detected automatically - except the
ports of a [qtdmm-bridge](remote-bridge.md) announcing itself by mDNS, which
[Add device](#add-device) lists under *Network*, where a `host:port` can be
typed as well. A sigrok meter is set up with its model, see
[Bench meters](bench-meters.md); `sigrok-cli --help` and the
[sigrok hardware list](https://sigrok.org/wiki/Supported_hardware#Multimeters)
tell you the driver string. The path to `sigrok-cli` is under **Settings →
General → Programs** if it is not in your `PATH`. Ports set up on the former
settings page *Special ports* are still offered.

Any program that fully implements RFC 2217 (including remote port setup) works
as the server, e.g. `ser2net`. QtDMM ships its own: **qtdmm-bridge**
(`tools/qtdmm-bridge/` in the sources), a single Python file for a Raspberry
Pi or any other box next to the meters:

```
qtdmm_bridge.py --list                                   # what is connected?
qtdmm_bridge.py --port 4000=/dev/ttyUSB0 --port 4001=/dev/ttyUSB1
```

It serves one meter per TCP port, needs only Python 3.11 and pyserial, keeps
the connection when a cable is unplugged and re-plugged, and takes the line
settings from QtDMM - the meter model is chosen in QtDMM as usual. On Linux
it also serves the **USB-HID cables** (`hid:1a86:e008` for a UT-D04/UT803
type cable, UT-D09, Brymen BU-86X), so a HID meter on a Raspberry Pi is
reachable over the network too. See its `README.md` for the configuration
file, the udev rule and the SSH-tunnel setup; there is no authentication, so
keep it on a trusted network. The whole setup, including running the bridge
as a service, is on [Meters over the network](remote-bridge.md).

## Connecting

Click **Connect** (F3) in the toolbar. QtDMM connects on its own at
start-up once a meter has been chosen in the settings; a fresh start
waits for you to configure one. The status line at the bottom shows what is
happening:

- *Connecting …* — the port is open, waiting for the first frame.
- *connected (USB)* — readings arrive; the display and the recorder are
  live. The way (USB, serial port, Bluetooth, network) is in the status
  line, the port in its tooltip. Without a device the status line is empty.
- *Timeout on device …* — the port is open but nothing has arrived for three
  seconds. Check that the meter is switched on, that its RS-232 output is
  enabled (many meters have a button or menu entry for it), and DTR/RTS. With
  a USB-HID cable the message says whether the cable answers while the meter
  stays silent.
- *Lost connection to … Retrying every 5 s.* — the port went away: cable
  unplugged, bridge or sigrok-cli gone, connection refused. QtDMM keeps
  trying as long as **Connect** is pressed and picks up again as soon as the
  device is back; the recording continues where it was.
- *No permission to access …* — see [Troubleshooting](troubleshooting.md).

Clicking **Connect** again disconnects and frees the port for other programs.

## The display

The LCD-style display mirrors the meter: value, unit, the annunciators -
AUTO at the left (lit while the meter ranges by itself, off in a manual
range), AC, DC, diode, continuity and HOLD (an H in a square) at the right;
for AC+DC a "+" lights between AC and DC. Unlit ones stay faintly visible,
like on the meter itself. Then the bar graph and, below the value, the minimum
and maximum since the last **Reset** (F4), each as the meter showed it
("MIN 221.18 mV" stays so when the meter has moved on to V). A range change
(mV to V, kΩ to MΩ) keeps them; they start afresh when the meter switches to
another function (V DC to Ω, DC to AC, °C to °F). An overload changes
nothing.

The display is a window like the [needle meter](analog-meter.md), one for
each device connected (its front panel); digits and lettering scale with
it. The device's name stands on the frame above the display, like the model
name on an instrument. Until the device sends a value, the display says why
instead of showing digits: *Connecting ...*, *No data*,
*No connection* or *Not connected*; the sidebar shows the same in short
next to the device, with a grey dot. **Display** in the toolbar or the menu
(Ctrl+1) hides and shows all of them. Bar graph and min/max (off by default) are set on the
*Appearance* settings page, in the group *Digital display (LCD)*.

**LCD colours** on the *Appearance* settings page, for all displays:
*Classic* (the yellow-green of most meters), *Backlight blue* (light
segments on blue, the default), *Amber*, *High contrast* (black on white) or
*Custom*, the tint set below it (*Custom face*). The same colours in a
display's right-click menu are that display's own, until *As in the
settings* or until its device is closed; the bench keeps them. Colours
changed in the settings reach every display without colours of its own.

**PASS/FAIL test** and **Limits...** in the same menu judge each reading
against limits; see [PASS/FAIL limit test](limit-test.md).

For a meter with more than one value (a second display, the CO2,
temperature and humidity of the TFA AirCO2ntrol, or the four values of a
Metex M-3850D) the sidebar chooses the value shown big and the two at most
on the small line (**LCD** in the menu of a value, see
[The meter's values](my-devices.md#the-meters-values)); until then the small
line has the first values after the big one. A meter set up by
hand gets as many values as it sends. The min/max memory follows it too. It only changes the display:
the graph's recording, its triggers and the export stay with the main value
(value 1). It is kept with the window layout; a meter without that value
shows its main value.

For meters whose keys QtDMM knows (the UNI-T UT60BT, UT61B+/D+/E+ and
UT161), a row of the meter's keys sits under the display: SELECT (yellow),
RANGE and AUTO, REL, LIGHT and HOLD (blue, on the right as on the meter);
the UT61B+/D+/E+ and UT161 also
have Hz/%, MIN/MAX and PEAK. A click presses the key on the meter. HOLD, AUTO, REL, MIN/MAX and PEAK
light up as the meter reports them. As on the meter, MIN/MAX and PEAK are
left by holding the key down (about a second). Over Bluetooth the keys
work once the link is up, about five seconds after connecting (see
[Bluetooth](bluetooth.md)); before that the status bar says the key did
not reach the meter. Tried on the UT60BT; the UT61B+/D+/E+ and UT161 use
the same commands.

The UNI-T iENV meters (UT333BT, UT353BT, UT363BT, UT383BT) have HOLD, MAX
and MIN (the UT363BT AVG instead of MIN), as UNI-T's app offers them. The
UT363BT and UT383BT have UNIT for the next unit of the main value (m/s,
km/h, ft/min, knots, mph; lx/fc), the UT333BT °C/°F, the UT353BT
FAST/SLOW. UNIT and °C/°F work once the first reading has told the unit.
The display shows MAX, MIN and AVG as the meter reports them. The keys are
tried on the UT333BT.

The ZOTEK meters (ZOYI, BSIDE, ANENG) have the keys of ZOTEK's app, each
choosing its function: on the ZT-5B and ZT-5BQ AUTO, CAP, Diode, NCV, Hz,
°C/°F, HOLD and ZERO (in capacitance), on the ZT-300AB MIN/MAX, on the
ZT-5566SE MIN/MAX and HOLD ([Bluetooth](bluetooth.md)). The OWON meters
have the keys of OWON's app: SELECT, RANGE, Hz/%, REL, MIN/MAX, LIGHT and
HOLD, as each model has them; the CM2100B SELECT, ZERO, LIGHT and HOLD
([Bluetooth](bluetooth.md#owon)).

The small triangle in the display's top right corner,
or **Hide controls** in its right-click menu, folds the row away.

## Window layout

The main window holds five windows: the digital display, the needle meter,
the graph, the [readings table](readings-table.md) and the
[Poincaré plot](poincare-plot.md). The toolbar buttons (or Ctrl+1 … Ctrl+5)
show and hide them. A fresh QtDMM starts as a
compact instrument with display and meter only; starting a recording shows
the graph.

The first start opens the main window at 800×600 (at most 84 % of the
screen); the views fill it. When you switch on
the graph or the table for the first time, the window grows to make room -
down for the graph, to the right for the table - up to most of the screen;
it never shrinks by itself. Once you have sized or snapped the window
yourself, it keeps your size. A view switched on while the window is
maximized gets its room when the window comes back to its size. Showing or
hiding the sidebar (**Devices**, F9) widens or narrows the window by its
width, so the views keep their arrangement; maximized, they share the room. The window title names the configured meter; in the sidebar its dot in
[My devices](my-devices.md) blinks green while readings come in and turns
grey when none has come for three seconds.

**Arrange** in the toolbar (in the menu while the toolbar is hidden) chooses how the windows are
placed:

- **Displays on top** (the default): display and meter share a strip at the
  top, the graph takes the space below and the readings table a column on
  the right. Everything follows the size of the main window.
- **Displays on the left**: the same with display and meter in a column on
  the left.
- **Fixed**: keeps the layout as it is. From *Displays on top* or *on the
  left* it takes that layout over; from *Free* the windows snap into a grid
  made from where they are (it shows for a moment first). A window shown
  later gets a place at the bottom edge, a hidden one leaves its space to its
  neighbours.
- **Free**: place and size the windows yourself; QtDMM remembers where they
  are.

In the arranged modes the gaps between the windows are dividers: drag one to
share the space differently (the mouse pointer changes on them; they can
also be taken a few pixels inside a window's edge). Drag a window with
Ctrl held - or at its title bar - onto another one and the two swap places;
dropped elsewhere it goes back. A layout changed like this is remembered.
In *Displays on top* and *on the left* it lasts until a window is shown or
hidden; then the windows are placed by the rule again.

**Load bench...** and **Save bench...** in the same menu keep a
measuring set-up in a file (`*.qtdmm-workspace`): the devices connected
with their front panels, which windows are shown with
which values, the arrangement with its dividers, the title bars, the
toolbar, the design and the size of the main window - for instance one
set-up for the bench and one for the battery in the van. *Save bench*
first asks for a name and a description and shows the picture of the
windows that goes into the file.

Loading a bench connects its devices; devices connected now that it
does not have are closed (with
a question when one has a recording not saved). The file keeps a copy of
each device's entry in My devices, with its alarms and PASS/FAIL test: an
entry that is there wins (a port changed since), one that is missing - on
another computer, or deleted - is added back from the copy. Its alarms
come back without their programs (a file may come from anyone): set them
again under *Alarms...* if you want them. A Victron device's key is not
written to the file; enter it again in the device's settings. Saved benches of
QtDMM 26.2 to 26.4 before this (layout only) are still read; their
window's device is connected first.

**Save layout on exit** (on by default) makes QtDMM start with
the layout it had when it was closed. Switched off, it starts with the
layout it had when the option was switched off, or with the bench
loaded last.

**Recent benches** in the same menu lists the benches loaded or saved
last by their names, newest first; the tooltip shows the description, the
picture and the file. **Clear list** empties it. The node **Saved benches** at
the top of the sidebar shows the same list, folded at the start, with the
name of the bench in use at its right: a double click or Enter loads one, its context menu also opens its folder or takes
it off the list. A file that is gone is grey. The list is shared by all
QtDMM windows; how many it shows is set under **Preferences** >
**Appearance** > **Recent benches** (10, *none* hides it).

**Hide title bars** (F10) takes the title bars away so the windows sit
flush next to each other - the default for the arranged modes. Right-click
a window for its menu: **Hide window** comes first, in the graph and the
readings table too; in *Free* mode Ctrl+drag moves a window without title
bar. Window
layouts of versions before 26.1 are not taken over.

**Hide toolbar** (Ctrl+M) in the menu takes the toolbar away for more room;
the same key brings it back, and so does the small arrow at the right end of
the status line, which is always there. A note says so when
the toolbar goes (*Do not show this again* switches it off). The menu then
opens with F7 at the top right of the window, and the other keys keep working.

**Design** in the menu, or on the *Appearance* settings page, sets the colours
of the window: *System* (the look of your desktop), *Light* (polished
aluminium), *Dark*, or *Light Metal* and *Dark Metal* (the default), the two
with the instruments on brushed metal. The readings table
and a graph in the colours *Neutral* follow; the LCD colours, the graph's
colours and the style of the needle meter are chosen separately.

**Settings → Appearance → Symbols** chooses the toolbar and menu symbols:
*Coloured* (KDE's Oxygen icons, the default), *Plain* (KDE's monochrome Breeze
icons, light or dark to match the window) or, on Linux and BSD where the
desktop has an icon theme, *System* - your desktop's symbols, with the
plain set for QtDMM's own symbols and anything the theme lacks. **Symbols
with text** below it puts the names under the toolbar buttons. The toolbars
cannot be moved; Ctrl+M hides and shows them.
