# Alarms

An alarm watches the reading and tells you when it leaves the range you
expect - a battery that sags below 12 V, a supply that drifts high, a
sensor that stops answering. Every open device has its own list of
alarms. They belong to the device: they are kept with its entry in
[My devices](my-devices.md) and come along when another window or a later
run connects it.

Where to set them up:

- **Settings → Alarms**: **Device** at the top chooses whose alarms the
  list shows (only with more than one device connected); it starts on the
  [active device](my-devices.md#several-meters-front-panels-and-the-active-device).
- **Alarms...** in the context menu of a device's front panel or of its
  row under *Bench* in the sidebar opens that page on the device.
- **New alarm...** in the menu of a value (the sidebar's value rows)
  starts a new alarm on that value.

## Conditions

| Condition | Raises when |
|---|---|
| Reading below | the reading is under the threshold |
| Reading above | the reading is over the threshold |
| Reading outside a range | it is under the lower or over the upper bound |
| Reading inside a range | it is between the bounds |
| Overload (OL) | the meter shows an overload |
| No readings for a while | nothing arrived for the given number of seconds. Counted from the moment the meter is connected, so it does not raise while QtDMM is not connected |

Thresholds are in the reading's base unit and take SI suffixes (`4.7m`,
`12k`). Two settings keep an alarm from chattering:

- **For at least** - the condition must hold this long before the alarm
  raises; a spike shorter than that is ignored.
- **Hysteresis** - once raised, the alarm clears only when the reading has
  come back by this much (below 12 V with 0.2 V hysteresis clears at
  12.2 V).

An unticked alarm in the list is kept but never fires.

For a meter with more than one value (a second display, or the CO2,
temperature and humidity of the TFA AirCO2ntrol), **Value** at the top of
the condition chooses the one the alarm watches - humidity above 60 %, say;
the thresholds are in its unit. *No readings for a while* is the meter's
silence and always looks at the main value.

## Actions

What happens when an alarm raises; tick any of them:

- **Banner** over the graph in the alarm's colour with its message (or the
  condition in words); one banner for all devices, with the device's name
  in front when more than one is open. **Acknowledge** hides it and
  acknowledges the alarms of all devices; an alarm stays raised until the
  reading is back and raises again only after that.
- **Display and needle meter in the alarm's colour** (on by default): the
  LCD of the device's front panel takes the colour (the needle meter while
  it shows a value of that device), its digits turn light or dark, whichever
  reads better; the needle meter's dial takes a third of it, so scale and
  pointer stay readable. The colour stays after Acknowledge until the
  reading is back. With several alarms raised, the one raised last gives
  the colour. The colour is never the only sign: banner, sound and the
  status line tell the same.
- **Sound** - none, the system's beep, one of the built-in sounds
  (Warning, High, Low, Critical, OK) or a WAV file of your own (**Choose
  file...**); the button next to it plays it. **Repeat** says how often:
  **once**, **while the alarm is raised**, or a number of **times, while
  the alarm is raised** - a second from start to start. Acknowledge ends
  the repeating, and so does the reading coming back. **Play "OK" when the
  alarm clears** sounds once more when it is over. Where QtDMM was built
  without Qt Multimedia, every sound is the system's beep, repeated the
  same way.
- **Popup window** with message, value and time.
- **Bring the QtDMM window to the front**.
- **Mark in the recorder graph** - a dashed vertical line in the alarm's
  colour at the moment it raised, with a flag under the graph: the mark's
  number and, where there is room, the alarm's name. Kept with the
  recording; and **in the
  readings table** - the row gets the colour.
- **Recording** - start or stop a recording.
- **Run program** - a command line; `%v` is the value, `%u` its unit,
  `%n` the alarm's name. That is the hook for anything else: a
  notification (`notify-send "%n" "%v%u"`), a mail, a webhook with `curl`,
  a relay. The command is the program followed by its arguments, as in a
  terminal but without a shell: put arguments containing spaces in double
  quotes; for pipes or redirections call the shell yourself
  (`sh -c "echo done > /tmp/qtdmm.log"`). The button next to the field
  chooses the program file; the arguments already typed stay.
- **Disconnect first** (with a program) - QtDMM closes the port before the
  program starts and opens it again when the program has ended, so the
  program can talk to the meter itself. One such program runs at a time.
  A running recording goes on; the time without readings is a gap in it.

The status line reports every raise and clear too.

**Coloured displays blink**, below the list of alarms, makes the display
and the needle meter switch between the alarm's colour and their own once
a second. It is off by default.

An alarm judges one value of its device. For a condition on several meters -
power over a limit, say - set up a [calculated value](calculated-values.md)
and put the alarm on it. A mark of an alarm of a device goes into the graph and the readings table while they show a value of it,
with the device's name in front.

## Example: setting a voltage

An alarm can also say "on target" instead of "wrong". Setting the reference
voltage of a stepper motor driver with its trimmer, say to 2.00 V:

- Condition **inside**, from 1.95 V to 2.05 V; duration 0, no hysteresis.
- Colour **green**, **Display and needle meter in the alarm's colour** on.
- **Banner** and the two **Mark** boxes off, **Sound** none: it would sound
  every time the reading enters the range, and every entry would be a mark.

Now turn the trimmer and watch the screen from the corner of your eye:
the display turns green while the voltage is in the range and goes back
to its own colour as soon as it leaves it. With a narrower range the
target is hit more exactly.
