# Troubleshooting

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
`HID 0x1a86:0xe008 /dev/hidrawN`. If the entry is missing or cannot be opened,
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

## "Another instance is running"

QtDMM instances coordinate through shared memory so that several meters can be
recorded side by side (see [Command line](command-line.md)). If an instance
crashed or was killed, its entry could be left behind; QtDMM now checks whether
that process still exists and replaces a stale entry automatically. Should the
message appear anyway, no other QtDMM is running, and it persists across
restarts, remove the shared memory segment with `ipcs -m` / `ipcrm` or simply
reboot.

## A calculated instance shows OL and "Waiting for instance ..."

The formula names an instance that is not running, or one that has not
delivered a reading for three seconds (*No current value from instance
...*), or whose meter shows an overload itself. The status line names the
variable; the *Formula* group in the settings of the device (*Settings...*
in the sidebar, Shift+F8) lists all variables
with their current values and the running instances. Remember that instance
names with a hyphen are written with an underscore in formulas. *Formula
error at position n* means the formula does not parse; the position is
counted from 1.

## Readings look right on the meter but wrong in QtDMM

Check the model of the device (*Settings...* in the sidebar, Shift+F8) — several meters share a protocol and
differ only in range or resolution, and a near match can decode with a wrong
factor. If the model is right and a specific range or function still decodes
wrongly, please report it with the meter model and, if possible, the debug
output (`qtdmm --debug` prints every frame as hex) on the project page.
