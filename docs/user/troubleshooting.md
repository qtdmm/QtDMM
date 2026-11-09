# Troubleshooting

## The log

**Log...** in the menu at the top right opens QtDMM's log in a window of its
own, beside the others: the time, the scope (a device's name, *app*, *Graph*),
the level and the message of what happened - errors, connection states and
the program's messages. Above the table you tick the levels to show (*Debug*
is off at first), search with the arrows or Enter, and **Export...** saves all
lines kept, also those not shown, as `qtdmm-<date>.log`.

When something goes wrong, attach that file to your report. It holds the
versions of QtDMM and Qt, and no Victron keys, but the names of your devices,
their ports and Bluetooth addresses: look it over if that matters to you.
The log keeps the last 10 000 lines and starts afresh with each start of QtDMM.

## "No permission to access /dev/ttyUSB0"

On Linux, serial ports belong to a group, and your user has to be in it. QtDMM
names the group in the error message; it is `dialout` on Debian, Ubuntu and
their relatives and `uucp` on Arch Linux and CachyOS. Add yourself with

```
sudo usermod -aG dialout $USER
```

(or `uucp`), then **log out and back in** — group membership is only read at
login.

On FreeBSD the ports are `/dev/cuaU0`, `/dev/cuau0` … and belong to group
`dialer`: `sudo pw groupmod dialer -m $USER`, then log in again.

## The display and the needle turn pale

The meter has stopped sending: QtDMM fades the last value instead of showing
it as if it were current, and the graph gets a gap. It comes back with the next
value. If the meter is on, see the next section.

## "Timeout on device" although the meter is on

- Many meters only send data after their RS-232 or USB function is switched on
  — look for a button labelled RS232, PC, USB or a serial symbol. With a
  USB-HID cable QtDMM can tell the two apart and says *"The USB cable answers,
  but the meter sends nothing"* when the cable is fine and only the meter's
  output is off.
- Some cables draw their power from the DTR or RTS line. The device table in
  [Supported devices](supported-devices.md) says what a known model uses;
  with manual settings, try DTR on and RTS off first.
- With manual settings, double-check baud rate, data bits and parity. Most
  meters in the device table use 7 data bits with odd or no parity.

## USB-HID cables

Meters with a USB-HID cable (many Uni-Trend models) appear in the port list as
`HID 0x1a86:0xe008 /dev/hidrawN`. The number (and on Windows and macOS the
whole path) can change when the cable is plugged in again or the computer
restarts. QtDMM then looks for the cable by its USB ids (`1a86:e008`) and
takes it if it is the only one of its kind; with several equal cables it asks
you to choose the port. On Linux the udev rule below also gives each cable a
link that stays the same as long as it is in the same USB port,
`/dev/qtdmm/ut-d04-ch9325-pci-…-usb-0_2_1_0`; the port list shows that link
instead of `/dev/hidrawN`. If the entry is missing or cannot be opened,
the `/dev/hidraw*` device is usually only readable by root. A udev rule grants
access to your user. The DEB and RPM packages install one,
`/usr/lib/udev/rules.d/70-qtdmm.rules`, for every cable and sensor QtDMM
knows; with the AppImage or the Flatpak, **Add device** shows the line for
your cable and the commands (**How to fix...**), or copy the file from
`assets/linux/70-qtdmm.rules` in the sources to `/etc/udev/rules.d/`, then
`sudo udevadm control --reload-rules && sudo udevadm trigger` and plug the
cable in again. The rules use `TAG+="uaccess"` - the user logged in at the
computer may open the device - which only works in a file numbered below 73;
a rule of your own with a group (`GROUP="plugdev"`) works at any number. QtDMM knows three cable chips: the WCH CH9325 / Hoitek
HE2325U (`1a86:e008`, `04fa:2490`; UT-D04 and most older cables), whose speed
QtDMM sets from the device table when it opens the port; and the two
revisions of the newer UNI-T UT-D09, which look identical — a Silicon Labs
CP2110 (`10c4:ea80`, configured like the CH9325) or a WCH CH9329
(`1a86:e429`), which runs at a fixed 9600 8N1, so choose a model or manual
settings with that rate. `lsusb` tells you which one you have. Both UT-D09
variants follow the chips' documentation and have not been verified with a
real cable yet; a report either way is welcome.

On FreeBSD hidapi talks to the cable through libusb, so the entry shows a USB
address instead of a device file, and your user needs access to the `ugen`
node — a `devfs.rules` entry (`add path 'ugen*' mode 0660 group dialer`, same
for `usb/*`) does that; `usbconfig` lists the attached devices.

## The AppImage does not start

The AppImage brings its own Qt, but takes the C library from the system and
needs **glibc 2.43 or newer**, as on Ubuntu 26.04.
On older systems (Ubuntu 24.04, Debian 13) it stops at once with
`version 'GLIBC_2.43' not found`; take the Flatpak there, or build QtDMM from
the sources.

It contains only the X11 platform plugin. Under Wayland it runs through
XWayland, unless `QT_QPA_PLATFORM=wayland` is set: then it ends with "no Qt
platform plugin could be initialized". Start it with
`QT_QPA_PLATFORM=xcb ./QtDMM-*.AppImage`.

## Windows

Serial ports appear as `COM3`, `COM4` … and need no permissions. USB-serial
cables need the chip vendor's driver once (CH340/CH341, FTDI, Prolific); until
it is installed the port is missing from the list. HID cables use the Windows
HID class driver and work without any driver installation. Debug output
(`qtdmm --debug`) is shown when QtDMM is started from a command prompt.

## macOS

The `.dmg` from the release page runs on macOS 13 or newer, on Apple Silicon
and Intel Macs. Open it and drag **QtDMM** onto **Applications**. The app is not
signed with an Apple developer certificate, so the first start is blocked
with "QtDMM cannot be opened" or "Apple could not verify QtDMM". Allow it once:

- Open *System Settings → Privacy & Security*, scroll down to the message
  about QtDMM and click **Open Anyway**, then confirm; or
- in Terminal: `xattr -dr com.apple.quarantine /Applications/QtDMM.app`

After that it starts normally. Serial ports appear as `/dev/cu.usbserial-…`
(use the `cu.` ports, not `tty.`). Bluetooth LE is not available on macOS yet.

## VXI-11 shows "failed"

The status line of Settings, SCPI server and the tooltip in the sidebar
say why:

- *port 111 is not allowed for this program*: no rpcbind runs, and an own
  portmapper on the privileged port 111 needs rights QtDMM does not take.
  Start the system rpcbind (`sudo systemctl enable --now rpcbind.socket`)
  or see [SCPI server, VXI-11](scpi-server.md#vxi-11) for the other ways.
- *another program or QtDMM offers VXI-11 already*: one per computer. Use
  HiSLIP in this QtDMM, or switch VXI-11 off in the other.
- *rpcbind refused the registration over TCP*: rpcbind runs, but its Unix
  socket `/run/rpcbind.sock` is missing (an old setup, or a container or
  Flatpak without access to it).
- *no rpcbind is running*: the Portmapper setting is *System rpcbind*;
  choose *Automatic*, or start rpcbind.

## "QtDMM is running already with these settings"

One QtDMM per settings file: a second start with the same `--config-id`
(or none, twice) would write over the first one's settings, so it stops
with this message and the process number of the first. Use that one, or
start the second with another `--config-id`. The lock is the file
`<settings file>.running`; one left by a QtDMM that crashed is taken over
at the next start.

## A calculated value shows OL and "Waiting for ..."

The formula names a device that is not open, or one that has not
delivered a reading for three seconds (*No current value from ...*), or
whose meter shows an overload itself. The status line names the
variable; the *Formula* group in the settings of the device (*Settings...*
in the sidebar, Shift+F8) lists all variables
with their current values and the open devices. Remember that a name is
written in lower case with `_` for other characters than letters and
digits. *Formula
error at position n* means the formula does not parse; the position is
counted from 1.

## Readings look right on the meter but wrong in QtDMM

Check the model of the device (*Settings...* in the sidebar, Shift+F8) — several meters share a protocol and
differ only in range or resolution, and a near match can decode with a wrong
factor. If the model is right and a specific range or function still decodes
wrongly, please report it with a capture of the meter's frames on the project
page.

### Capturing the frames of a meter

Right-click the device in the sidebar and choose *Capture...*. The window
lists the last 500 frames the meter sent: the time, the bytes and what
QtDMM read from them, or *not understood*.

1. Set the meter to the range or function QtDMM reads wrongly.
2. Type what its display shows into *The meter shows* — the value, the unit
   and what else is lit, e.g. `-1.234 V DC AUTO HOLD` — and press *Keep*
   (or Enter). The newest frame is kept with your text; with a frame
   selected in the list (*Pause* first), that one.
3. Repeat for every reading that is wrong; a reading that is right helps too.
4. *Export...* writes a `.yaml` file, in the folder you choose (it is
   remembered for the next time). Attach it to the report.

The file has the meter's vendor, model, protocol and the kind of connection,
and the kept frames in the form of the test vectors in
`docs/protocols/spec`, so a developer can add them as they are. It holds no
port name, Bluetooth address or key. *Export also all frames of the list*
adds the whole list, which helps when a reading changes over time.

`qtdmm --debug` still prints every frame as hex on the console
([Command line](command-line.md)).
