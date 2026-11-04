# My devices

If you own several meters - a UT61E on the bench, a Brymen for the car, a
Victron shunt in the van - **My devices** keeps each of them with its
connection: it is the cupboard. **Connect** takes a device out of it, one
or several at a time; the window itself has no meter of its own.

An entry holds the meter and its connection only, and of the connection
what its way needs: model and protocol, the port with its line settings, a
Bluetooth address (and for a Victron device its key and the values to
show), sigrok options, the signal of the simulated meter, and its rules
(alarms, PASS/FAIL test). Graph, recorder and the views stay with the
window. An older `devices.conf` with more in it is tidied up when QtDMM
starts.

**Info and settings...** of an entry in the sidebar, or a double click on
it, shows and changes the entry (Shift+F8: the settings of the device
chosen in the sidebar); a device connected takes the change at once.

## The sidebar

**Devices** in the toolbar shows and hides the sidebar at the left of the
window, with the node **My devices** (folded at the start: the devices in
use are under **Bench**; unfold it to take out another). Each entry has a small picture of its
kind of instrument (handheld, clamp, bench meter, ...) in its maker's
colours. A device on the bench is bold. A click on
a device does nothing: **Connect** in its context menu, the button
**Connect** in its info (a double click), or dragging it onto **Connected
devices** or onto the windows connects it (see below).

With a single device the sidebar stays closed; adding the second one opens
it, and the window remembers whether you leave it open. With no device
connected the window shows the big **Add device** button and, when My
devices has entries, **Connect a device from My devices**, which opens the
sidebar.

Below My devices the node **Bench** lists the meters this
QtDMM reads, in the order they were connected, each with a dot that is
green while its readings come in and grey when none has come for three
seconds. The readings and why none come are on the front panels, not here.
One disconnected is grey. A double click or
*Show front panel* in the context menu brings its front panel back;
*Info and settings...* there opens its entry of My devices (all its values
for a look), *Connect*/*Disconnect* connects or disconnects it, *Close* (or
dragging it back onto My devices) puts it back into the cupboard.

### Several meters: front panels

Each connected meter has its own front panel: its LCD with its keys, as a
window next to the needle meter; all are alike. **Display** (Ctrl+1) shows
or hides all of them, the X of one only hides it - the meter stays
connected and keeps recording. **Connect** (F3) and **Reset** (F4, min/max)
in the toolbar act on the whole bench: Connect is pressed while one device
is connected and then disconnects them all; one device alone connects or
disconnects by its context menu here or at its front panel. All front
panels take the LCD colours of the settings, one can have its own (see
[LCD colours](connecting.md)). Each device has its own [alarms](alarms.md) and
[PASS/FAIL test](limit-test.md); its front panel takes their colours, and
*PASS/FAIL test*, *Limits...* and *Alarms...* are in the context menu of
its front panel and of its row. The window layout remembers the meters
connected and connects them again at the next start; a bench file does
the same when it is loaded. **Services** shows what this window offers to other
programs: the [SCPI server and HiSLIP](scpi-server.md) with their port, or
*off*.

Above My devices, folded at the start, the node **Recent benches** lists the
benches loaded or saved last by their names (the one in use stands after
**Bench**, as *Bench · Lab*);
a double click loads one (see
[Window layout](connecting.md#window-layout)). Without any it is hidden.
The star at the right of one (or *Favourite* in its context menu) makes it
a favourite: favourites stand first, do not count to the number of recent
benches shown and never drop off the list; *Remove from list* takes one off
all the same, *Clear list* keeps them. The context menu of the node itself
loads or saves a bench, as **Arrange** does.

### The meter's values

Each device has a row for each of its values below it in **Bench** - one
for a simple meter, several for a second display, the CO2, temperature and
humidity of the TFA AirCO2ntrol or the voltage, current and state of charge
of a Victron: its name with the unit, *Carbon dioxide (ppm)* (*Value 1*
until the first reading). The tooltip has its number. A double click shows
the device's front panel. At the end of the row, marks show where the value
is shown: a stroke in its curve's colour while the graph draws it (the rows
are the graph's legend as well), then the symbol of its front panel, of the
needle meters and of the Poincaré plots with it - one of each kind, the
tooltip names the windows. A click on the marks frames those windows for a
moment. This is where you say which window shows which value - right-click
a value:

- **LCD** ▸ **Main value**, **Secondary value left**, **Secondary value
  right** - the place on the device's front panel: big (the min/max memory
  follows it) or on the small line below it. The value that had the place goes where
  this one was, or off the display when this one was not on it.
- **Graph** ▸ **Off**, **Left axis**, **Right axis**, **Third axis** - the
  value's curve off or on that axis; the third axis sits outside the right
  one. A value in the unit the axis has already joins it (two voltages on
  the left, a current on the right); one in another unit takes the axis
  over: the curves of the old unit there move to the other right axis when
  they are on automatic and it is free, else they go off - the status line
  says which. The main value is always on the left. A value of
  another device joins an axis in its unit; one with no room there is not
  drawn, and the status line says so.
- **New needle meter**, **New Poincaré plot** - another window of that kind
  showing it.
- **Readings table** - whether the [readings table](readings-table.md)
  lists it (all values of the devices it shows; another device comes in
  with the value chosen).

A choice opens the window it is for, also when the toolbar is hidden. The
choices are kept with the window layout.

A value can also be **dragged** out of the sidebar: onto a needle meter or
the Poincaré plot it becomes the value shown, onto the graph a curve, onto
the readings table a column of the table, onto the front panel of its own
device the big line (the value there moves to the small line). Dropped on
the free area between the windows (in the arrangement *Free*), it offers a
new needle meter, a new Poincaré plot or the graph.

A needle meter and the Poincaré plot show one value each: **Sources...** in
their right-click menu chooses it, from the connected devices and their
values. With more than one device connected, the graph and the readings
table have **Sources...** too, to tick the values they show. A device connected later changes nothing by itself - unless
a view shows nothing: an empty graph then takes its main value, the analog
meter and the Poincaré plot take it when their device is not connected.
The graph draws its values from the recording of the device of its first
value: that value is its main curve (with the overview, the reference line
and the limit band of it), the others are drawn on its time.

The context menu of a device:

- **Info and settings...** (also a double click) - two or three tabs:
  - **Info**: the device at a glance as on the assistant's last page
    (picture, name, model, type, protocol, connection); whether it is
    connected, how long ago the last reading came, a low battery, and its
    values with their readings, updated every second. Below
    a link to the maker's website (in the browser) and to the list of
    supported devices in this help.
  - **Data** (only for some meters - UNI-T UT803, UT804, UT61B+/D+/E+,
    UT161B/D/E, ZOTEK ZT-5B / ANENG V05B, HoldPeak HP-570C-APP, CMT 3503): the specifications from the
    meter's manual, per function the ranges with resolution and accuracy
    (per frequency band for AC), the input impedance, the overload
    protection, the manual's notes and its page. They were transcribed from
    the manuals by the [dmm-tools](https://github.com/antoinecellerier/dmm-tools)
    project (the HP-570C-APP's from its manual filed with the FCC, the CMT
    3503's from its data sheet, one accuracy per function) - check the
    manual before relying on a number.
  - **Settings**: the meter's settings as in the assistant; with **OK** a
    device connected takes them at once (it connects anew).
- **Rename** - in place.
- **Connect** (also the button at the bottom left of its info, or drag it
  onto **Bench**) - takes the device out of the cupboard, with
  a front panel of its own: it shows its reading there and in **Connected
  devices**, records with the others, is a variable in
  [calculated values](calculated-values.md) and answers over SCPI
  (`INSTrument`). Greyed while the device is connected; a port another
  device connected uses is not opened twice.
- **Remove from My devices** - after a question; a device connected stays
  so until it is closed.

Drag a device within My devices to change the order.

## Adding a device

**Add device...** (the device symbol with a + at the left of the toolbar,
or **+ Add device...** at the end of My devices in the sidebar) asks step
by step: how the meter is connected - *Cable*, *Bluetooth*, *Network*,
*sigrok* or *Simulated / calculated* -, which meter it is, with a name, and
whether to connect it now: **Connect** keeps it in My devices and connects
it at once, **Save only** keeps it in My devices without connecting. The page of the meter offers only the models of that
connection; *Next* waits until what the meter needs is filled in (the port,
the key of a Victron device, a formula that works).

A serial port is saved under its stable name on Linux, the link in
`/dev/serial/by-id/` (`usb-WCH.CN_USB_Quad_Serial_…`): `/dev/ttyUSB0` may be
`/dev/ttyUSB1` after plugging the cable in again, the by-id name stays.

## Where the list is kept

The list is kept in `devices.conf` next to the settings file, the same for
every `--config-id`. An entry keeps the device's rules too: its alarms and
its PASS/FAIL test. A window of a version before 26.4 had a meter of its
own: at the first start that entry is connected like the others, and alarms
set up for the window move into it. A meter set up in the window alone
(*Manual settings*, no entry) becomes an entry of its own first, named
after its model (*Meter* for Manual settings) - rename it in the sidebar. A port is read by one meter at a time:
a device whose port another device connected uses is not opened again, and
another program (or another QtDMM) using it says the port is busy.

A Victron key is kept in `devices.conf` in plain text, as in the settings
of the window: whoever gets the file gets the key.
