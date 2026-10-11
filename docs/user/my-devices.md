# My devices

If you own several meters and use them one at a time - a UT61E on the
bench, a Brymen for the car, a Victron shunt in the van - **My devices**
keeps each of them with its connection, so switching is one click in the
sidebar instead of filling in the meter page again.

An entry holds the meter and its connection only, and of the connection
what its way needs: model and protocol, the port with its line settings, a
Bluetooth address (and for a Victron device its key and the values to
show), sigrok options, the signal of the simulated meter. Display, graph,
recorder and alarms stay with the window. An older `devices.conf` with more
in it is tidied up when QtDMM starts.

The window remembers which entry it uses. **Settings...** of an entry in
the sidebar changes the entry (Shift+F8: the one in use); the window that
uses it takes the change at once.

## The sidebar

**Devices** in the toolbar shows and hides the sidebar at the left of the
window, with the node **My devices**. The device in use is bold, with a dot
that is green while its readings come in; where a device is connected shows
when the mouse rests on it. Click another one: QtDMM disconnects, takes over
its settings and connects again. A running recording stops and the graph
starts empty - another meter is another measurement; readings not exported
yet are offered for export first, and **Cancel** stays with the current
device. Minimum and maximum start afresh.

With a single device the sidebar stays closed; adding the second one opens
it, and the window remembers whether you leave it open. A window without a
meter shows it whenever there are devices to choose from - pick one, or add
a new one with the big **Add device** button.

The context menu of a device:

- **Settings...** - the meter's settings as in the assistant; the device in
  use takes them at once.
- **Rename** - in place.
- **Open in a new window** - a new instance with the settings of this one
  and the device. A device another window already uses brings that window
  to the front instead.
- **Remove from My devices** - after a question; a window using it keeps its
  meter.

Drag a device to change the order.

## Adding a device

**Add device...** (the device symbol with a + at the left of the toolbar,
or **+ Add device...** at the end of My devices in the sidebar) asks step
by step: how the meter is connected - *Cable*, *Bluetooth*, *Network*,
*sigrok* or *Simulated / calculated* -, which meter it is, with a name, and
where it goes: **In this window** as the current device, or **In a new
window**, a new instance with the settings of this one. Either way it is
kept in My devices. The page of the meter offers only the models of that
connection; *Next* waits until what the meter needs is filled in (the port,
the key of a Victron device, a formula that works).

A serial port is saved under its stable name on Linux, the link in
`/dev/serial/by-id/` (`usb-WCH.CN_USB_Quad_Serial_…`): `/dev/ttyUSB0` may be
`/dev/ttyUSB1` after plugging the cable in again, the by-id name stays.

## Several windows

The list is the same for all instances: it is kept in `devices.conf` next
to their settings files. *Open in a new window* starts a new instance
with the device; the node **Instances** of the sidebar shows them all. Two windows cannot use the same serial port at
the same time; the second one says the port is busy.

A Victron key is kept in `devices.conf` in plain text, as in the settings
of the window: whoever gets the file gets the key.
