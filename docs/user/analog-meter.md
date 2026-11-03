# Analog meter

Besides the digital display, QtDMM can show the reading on a moving-coil style
instrument: a dial with a scale on an arc, a needle that swings with the
inertia of a real meter, a red zone at the top end, boxes with the minimum
and the maximum, and an overload lamp.

Switch it on and off with **Analog meter** in the toolbar or in the menu
(Ctrl+2). It is one of the windows of the main window, next to the digital
display (see [Window layout](connecting.md#window-layout)); the dial, scale
and lettering scale with the window.

The frame above the dial names the device the value comes from, as on the
front panels; the unit on the dial says which value it is.
Until a value comes in, the dial says why in place of the unit, as the
[display](connecting.md) does.

## More analog meters

**Add analog meter** in the menu opens another analog meter window, for the
main value of the active device at first; in an analog meter's right-click
menu it opens one with the same value as that meter. Each one shows its own
value, of any connected device, and has its own style. Choose the value with
**Value** in the meter's right-click menu (the connected devices, each with
its values); **New analog meter** in the menu of a value in the sidebar
opens one for that value. **Close window** in that menu (or the X in its title bar)
removes an added meter; the first one is only hidden, as before. The window
layout and workspaces keep the added meters with their values and styles.

## The scale

The scale is drawn in the unit the multimeter shows, prefix included, and its
full scale follows the meter's range: a 4000-count meter reading `3.856 V`
gets a 0 … 4 V scale, `385.6 mV` a 0 … 400 mV scale. When the meter changes
range the scale relabels itself. A percentage (state of charge, duty cycle)
always gets a 0 … 100 % scale.

Negative readings push the needle into the short stub left of zero. On the
**GUI** settings page you can choose
how the scale is laid out:

- **Automatic** — zero at the left; as soon as a clearly negative reading
  arrives the scale switches to centre zero (−FS … 0 … +FS) and stays there
  until you reset the min/max memory (F4) or the meter switches to another function (V DC to Ω,
  DC to AC); a range change (mV to V) keeps it, the values are shown in the current range.
- **Zero left** and **Centre zero** fix one layout.

The **red zone** starts at 90 % of full scale by default; the same settings
page lets you move it.

While the [PASS/FAIL test](limit-test.md) is on for the value the needle
shows, a **green band** on the arc marks the passing range and the red
zone is left out.

## Readouts and lamp

**MIN** and **MAX** show the minimum and the maximum since the last reset,
in the unit of the scale and with as many decimals as the multimeter shows.
The current value is what the needle and the digital display show. The
min/max memory is also marked on the scale: a small red triangle at the minimum, a green one at
the maximum; *Reset* (F4) clears both. The **OL** lamp lights and the
needle rests against the right stop while the meter reports overload; **HOLD**
appears while the meter's hold function is active.

When the meter stops sending, the needle and the readouts fade (the
digital display does the same) until the next value arrives, so a frozen
value is not taken for a live one. See [Gaps](recorder.md#gaps) for when a
value counts as gone.

## Style

Two colour schemes are available: a dark studio dial with a white scale and needle, and
a classic ivory dial with black lettering (the default). **Meter style** in a
meter's right-click menu sets it for that meter only; the *Appearance* settings
page sets it for all of them.

For a meter with more than one value, **Value** in the analog meter's
right-click menu chooses the one the needle shows (kept with the window
layout). **MIN**, **MAX** and their marks are those of the value the needle
shows, whichever the display shows.
**Needle inertia** can be switched off to make the needle jump straight to each
new reading.
