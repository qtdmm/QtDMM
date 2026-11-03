# Bluetooth LE

QtDMM reads two kinds of Bluetooth LE meters:

- **UNI-T UT60BT** (and, untested, the UT161B/D/E and the UT61B+/D+/E+
  with the UT-D07B adapter), the **OWON** Bluetooth meters (tried on the
  CM2100B) and, untested, the **APPA** Bluetooth meters
  and their BENNING, KPS, MEGGER, METRAVI, RS PRO and Sefram twins and the
  **ZOTEK** meters (ZOYI, BSIDE, ANENG), and
  the **HoldPeak HP-570C-APP** clamp - QtDMM *connects* to the meter and
  reads it, like a serial meter on a cable.
- **Victron** SmartShunt, BMV-712, MPPT chargers and Phoenix inverters - QtDMM
  *listens* to the readings they broadcast, without a connection.

## UNI-T UT60BT

The UT60BT has Bluetooth built in. Switch it on with the meter's Bluetooth
key (the Bluetooth symbol appears on the LCD); the meter switches it off
again after a while without a connection.

In QtDMM, **Add device**, *Bluetooth*: choose the meter found, then vendor
**Uni-Trend** and the model **UT60BT**. Instead of the port box a **Bluetooth** group appears:

- **Device** - press **Scan** to list the UT60BT meters in range (15
  seconds), or type the address. The meter advertises as *UT60BT*.

Then **Connect**. QtDMM connects to the meter, asks it for a reading about
three times a second and shows what the LCD shows - value, unit and range,
AC/DC, HOLD, manual/auto range, the diode and continuity symbols and OL.
When the meter goes out of range or switches Bluetooth off, the status line
says so and QtDMM connects again as soon as the meter is back.

**Connecting takes about five seconds.** QtDMM first looks for the meter
(the computer's Bluetooth forgets a meter that is not paired a while after
it last saw it), then connects; meanwhile the status line says
*Connecting ... over Bluetooth*. The keys under the display work once the
first reading is in. If nothing comes after 20 seconds, the status line
says so: switch on the meter's Bluetooth, and QtDMM tries again by itself.

Only one program can be connected to the meter at a time: close the UNI-T
app or any other tool first.

Like the meter itself, QtDMM shows 0 in the dead bands the UT60BT keeps
around zero (about ±10 µV in the mV position, ±5 mA on the current
ranges). The rotary switch is mechanical; changing it produces one frame
with the new function and the old number, which QtDMM passes on like the
meter's own display does.

## UNI-T UT61B+ / UT61D+ / UT61E+ and UT161B / D / E

These meters speak the same protocol as the UT60BT. The **UT161** have
Bluetooth built in: choose the model and set it up exactly like the UT60BT.
The UT61+ have no Bluetooth of their own; either use the **UT-D09 USB cable** that comes with them -
choose the model *UT61E+* (or B+/D+) and the cable's HID port, as for any
[USB-HID cable](connecting.md) - or the **UT-D07B Bluetooth adapter**:
choose *UT61E+ (UT-D07B Bluetooth)* and set it up like the UT60BT above
(the adapter advertises as *UT-D07B*).

In the DC V position with AC shown alongside (the meter's DC+AC mode), the
meter sends DC and AC readings in turn: QtDMM records the DC reading and
shows the AC one on the display's second line and in the readings table.

Not tried with a real UT61+ or UT161 yet. Their ranges come from the tables
of UNI-T's own app (preserved by the
[unit_ut61eplus](https://github.com/ljakob/unit_ut61eplus) project) and agree
with [ut61xpy](https://github.com/olegv142/ut61xpy); the same tables confirm
what a UT60BT sends. Please report how it works for you.

## APPA and relatives

The APPA meters with Bluetooth - the S0 to S3, the 150B (155B to 158B), the
170B clamps (172B to 179B), A17N, sFlex-10A/18A, and the 208B and 506B -
speak over Bluetooth the same protocol as on their optical cable, and so do
the meters APPA builds for BENNING (MM 10, MM 12, CM 9 to CM 12), KPS,
MEGGER, METRAVI, RS PRO and Sefram. Choose the vendor and the model - for
the 208B, 506B, BENNING MM 12, KPS DMM9000BT and Sefram 7352B the entry
*(Bluetooth)*, the other one is for the cable - and set it up like the
UT60BT above. The main display is the reading, the sub display the second
value.

**Scan** and **Add device** know them by the name they advertise ("APPA S1",
"BENNING MM10-1", ...): their Bluetooth module offers a service many other
meters have as well, so a meter that advertises another name is not
offered.

Not tried with a real meter yet: the Bluetooth details come from the
appa-dmm driver of libsigrok (Martin Eitzenberger's branch), the display
counts from the data sheets. Please report how it works for you.

## ZOTEK, ZOYI, BSIDE and ANENG

ZOTEK makes Bluetooth meters that are sold as ZOYI, ZOTEK, BSIDE and
ANENG: the **ZT-5B** (ANENG V05B), the **ZT-5BQ** clamp (ANENG ST207), the
**ZT-300AB** (ANENG AN9002) and the **ZT-5566SE** (ANENG AN999S). Switch
Bluetooth on at the meter - a short press of the power button on the ZT-5B,
Power and Hz together on the ZT-5BQ, Hz% held for two seconds on the
ZT-300AB - until the Bluetooth symbol shows.

They all advertise as *Bluetooth DMM*. **Add device** finds them by that
name; choose the meter found and QtDMM listens to it for a moment: the
first reading tells which model it is (*Its first reading says: ...*), and
page 3 has that model chosen. Without a reading in 15 seconds, choose the
model yourself.

The meter sends about three readings a second. QtDMM shows value, unit,
AC/DC, HOLD, REL, MIN/MAX, the low battery symbol and OL; *Auto* (the
ZT-5B's automatic function before a reading) and the NCV display (*EF*,
dashes) as the meter shows them. The ZT-5566SE's second display (frequency,
duty cycle) is the second value.

The keys under the display are those of ZOTEK's app: on the ZT-5B and the
ZT-5BQ **AUTO**, **CAP**, **Diode**, **NCV**, **Hz**, **°C/°F**, **HOLD** and
**ZERO** (capacitance only), on the ZT-300AB **MIN/MAX**, on the ZT-5566SE
**MIN/MAX** and **HOLD**. Each key chooses its function directly.

Not tried with a real meter yet: the protocol comes from the
[dmm-tools](https://github.com/antoinecellerier/dmm-tools) project, worked
out from ZOTEK's apps and manuals and checked there on a ZOYI ZT-5B (its
readings and keys); which model sends which packet layout is inferred
except for the ZT-5B. Please report how it works for you.

## OWON

OWON's Bluetooth meters with the six-byte reading: the **B33T+**,
**B35T+**, **B41T+**, **OW16B**, **OW18B**, **OW18E** and the **CM2100B**
clamp. Switch Bluetooth on at the meter by holding its Bluetooth key
(△/ᛒ on the B models, Hz/Duty△/ᛒ on the OW16B and OW18, ZERO/ᛒ for about
two seconds on the CM2100B) until the Bluetooth symbol shows.

They advertise as *BDM* (or the name given to them in OWON's app). **Add
device** finds them by that name; choose the meter found and QtDMM asks it
which model it is (the meter tells it on connecting), and page 3 has that
model chosen. The OW18B and OW16B give the same answer: choose yours.

The meter sends about three readings a second. QtDMM shows value, unit,
AC/DC, AUTO, HOLD, REL, MIN/MAX, the low battery symbol, OL and the NCV
display (*EF*, dashes).

The keys under the display are those of OWON's app: **SELECT**, **RANGE**,
**Hz/%**, **LIGHT** (HOLD held) and **HOLD** on all but the CM2100B; the
B33T+, B35T+ and B41T+ also have **REL** (the B33T+ no LIGHT), the B35T+
and B41T+ **MIN/MAX**; the CM2100B has **SELECT**, **ZERO**, **LIGHT** and
**HOLD**.

Tried on the CM2100B: all keys and ranges, the display as on the meter.
The others are built much the same but not tried yet. The protocol comes
from the [dmm-tools](https://github.com/antoinecellerier/dmm-tools)
project, worked out from OWON's app, its PC software and manuals and
compared there with readings other projects recorded from these meters.
Please report how it works with yours.

## HoldPeak HP-570C-APP

The HoldPeak **HP-570C-APP** clamp, also sold as **AOPUTTRIVER
AP-570C-APP**, sends what its LCD shows over Bluetooth LE all the time it
is on. It advertises as *FS9721-LP3* (the name of its chip); **Add
device** finds it by that name, its Bluetooth service or its manufacturer
data. Choose the meter found and the model. There is nothing to pair.
Close the maker's app on the phone first: the meter connects to it by
itself and is then no longer seen.

The meter advertises seldom: the search may take up to 20 seconds to find
it, and QtDMM waits up to 30 seconds for it before it connects (also after
the meter was out of range).

QtDMM shows value, unit, AC/DC, AUTO, HOLD, REL, the diode symbol, the
low battery symbol and OL. The meter has no keys QtDMM could
press: the link only reads.

Tried by a user: it connects and shows readings. The Bluetooth details
come from the maker's app, the frame is the one of the HP-90EPC (its
FS9721 chip).

## A meter is not found

On Linux QtDMM runs `bluetoothctl scan le` (from the bluez package) beside
its own search for the HoldPeak HP-570C-APP - in Add device, for Scan and
before connecting: it was found only so. Without bluetoothctl the search
works as before.

When Bluetooth cannot be used - no adapter, the Bluetooth service not
running, Bluetooth switched off, or a QtDMM built without Bluetooth - the
**Bluetooth** tile of **Add device** and the **Scan** button of the
settings are greyed out; their tooltip says why. A search that fails says
so, with the reason, instead of "nothing found".

Start QtDMM from a terminal with

    QT_LOGGING_RULES="qtdmm.ble.debug=true" qtdmm

and search again: for each Bluetooth device the scan sees, the terminal
shows a line `scan: <address> name "..." services [...] manufacturer data
[...]`. Please add those lines of your meter to a bug report.

## Victron Instant Readout

Victron Energy's SmartShunt, BMV-712 Smart, the SmartSolar / BlueSolar
MPPT chargers and the Phoenix Inverter Smart broadcast their readings in their Bluetooth LE
advertisements ("Instant Readout"). QtDMM listens to those broadcasts - no
connection, no pairing - and shows them like a meter's reading:

Each device broadcasts several values; you choose which one QtDMM treats
as the reading (**Main value**: display, needle meter, recorder) and which
go to the display's second line, the graph and the
[readings table](readings-table.md) (**Second value**, **Third value**,
**Fourth value**, each or none):

| Device | Values |
|---|---|
| SmartShunt, BMV-712 | battery voltage (V), battery current (A, negative when discharging), battery power (W, computed), state of charge (%), consumed Ah, time to go (in min below an hour, in hours with a decimal above - 1.5 h is 90 min; *--* while charging, when there is none), aux input (starter or midpoint voltage in V, or temperature in °C - as configured on the device) |
| SmartSolar, BlueSolar MPPT | PV power (W), battery voltage (V), battery charging current (A), yield today (Wh), load current (A) |
| Phoenix Inverter Smart | AC apparent power (VA), battery voltage (V), AC voltage (V), AC current (A) |

The defaults are the first two of each row, no third and fourth value.
About one reading per second arrives. The graph, min/max and the export
keep the time to go in minutes throughout; while charging it has a gap.
Four values are the most at once.

### Setting it up

The broadcasts are encrypted with a key that only the device's owner has.
In the VictronConnect app open the device, then **Settings → Product info →
Instant readout via Bluetooth** and tap **Show** under *Instant readout
details* (or *Encryption data*): it shows the **MAC address** and the
**encryption key** (32 hex digits). Instant readout must be enabled there.

In QtDMM, **Add device**, *Bluetooth*: choose the device found, then vendor
**Victron** and the model. The port and serial settings make way for a **Bluetooth** group:

- **Device** - press **Scan** to list the Victron devices in range (five
  seconds; their names as in VictronConnect), or type the address.
- **Key** - the 32-digit key. It is stored in QtDMM's settings file and
  never shown in the status line.
- **Main value** / **Second value** / **Third value** / **Fourth value** -
  which of the device's values to show (table above).

Then **Connect** as usual. The status line says *Connected* once the first
advertisement decrypted. With the wrong key it reports *The encryption key
does not match* after a few advertisements; when the device goes out of
range or is switched off the usual timeout appears and QtDMM keeps
listening.

### What is not there yet

The device state (eco mode, bulk/absorption/float), alarms and charger
errors are decoded but not shown. Other Victron products (DC-DC converters,
Smart Lithium, Multi RS, AC chargers) broadcast in the same way and can
follow once someone has one to test with.

## Requirements

A Bluetooth LE adapter and a QtDMM built with Qt's Bluetooth module
(`QTDMM_WITH_BLE`, on by default when Qt6 Bluetooth is found - on Debian /
Ubuntu that is the `qt6-connectivity-dev` package at build time and
`libqt6bluetooth6` at run time). Linux uses BlueZ over D-Bus; QtDMM does
not need root. Windows builds include Bluetooth as well. Without the module
the Bluetooth models are listed but cannot be connected; the settings page
says so.

**macOS:** not supported yet. macOS does not tell programs the Bluetooth
address of a device, and QtDMM finds and addresses the meters by that
address, so the macOS build leaves Bluetooth out (`QTDMM_WITH_BLE` is off by
default there).
