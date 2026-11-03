# QtDMM Handbook

QtDMM reads a digital multimeter over a serial, USB-HID, RFC2217 or sigrok
connection, shows the live reading on an LCD-style display and an analog
meter, and records it in a transient recorder with thresholds, an
integration curve, CSV export and printing. Several meters run side by
side, one window each, and can be combined into calculated values.

## Contents

- [Connecting a meter](connecting.md) — choosing the device, port types,
  manual settings, the status line.
- [My devices](my-devices.md) — your meters with their connections, switched
  with two clicks.
- [Needle meter](analog-meter.md) — the moving-coil style instrument: scale,
  red zone, readouts, styles.
- [The recorder](recorder.md) — live graph, recording and view; starting manually, at a time or on a threshold;
  zoom, pan and cursor; integration; CSV export and import.
- [Readings table](readings-table.md) — every reading the meter sent, with
  time, mode and range; copy to a spreadsheet or export as CSV.
- [Poincaré plot](poincare-plot.md) — each reading against the next: how
  much the value scatters, noise against drift, SD1 and SD2.
- [Alarms](alarms.md) — a banner, sound, popup, program or the recorder when
  the reading leaves its range or stops coming.
- [PASS/FAIL limit test](limit-test.md) — nominal ± tolerance or lower and
  upper limit: PASS or FAIL, HI or LO and the deviation on the display.
- [Supported devices](supported-devices.md) — every meter QtDMM knows, with
  protocol and serial settings.
- [Calculated values](calculated-values.md) — power from a voltage and a
  current meter: formulas over the readings of the open devices; the
  simulated meter for demos without hardware.
- [Bench meters through sigrok-cli](bench-meters.md) — Keysight, Agilent, HP
  and Siglent SCPI meters over USB-TMC, LAN or serial, with sigrok-cli doing
  the talking.
- [Bluetooth LE](bluetooth.md) — the UNI-T UT60BT over a Bluetooth connection;
  Victron SmartShunt and MPPT chargers over their encrypted broadcasts.
- [Meters over the network](remote-bridge.md) — qtdmm-bridge on a Raspberry Pi,
  serial and HID meters over RFC 2217, running it as a service.
- [SCPI server](scpi-server.md) — the meter as a network instrument: `*IDN?`,
  `READ?` over TCP for lxi-tools, Python or LabVIEW, announced by mDNS.
- [Command line](command-line.md) — options, several meters at once, debug output.
- [Keyboard and mouse](keyboard.md) — shortcuts in the main window and the
  graph, and how to give a command a key of your own.
- [Troubleshooting](troubleshooting.md) — permission errors, "QtDMM is
  running already", HID devices.

Press **Shift+F1** in QtDMM and click any control to get a short explanation
of it right where it is.
