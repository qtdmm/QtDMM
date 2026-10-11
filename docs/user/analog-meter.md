# Analog meter

Besides the digital display, QtDMM can show the reading on a moving-coil style
instrument: a dial with a scale on an arc, a needle that swings with the
inertia of a real meter, a red zone at the top end, boxes with the minimum
and the maximum, and an overload lamp.

Switch it on and off with **Analog meter** in the toolbar or in the menu
(Ctrl+2). It is one of the windows of the main window, next to the digital
display (see [Window layout](connecting.md#window-layout)); the dial, scale
and lettering scale with the window.

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
a classic ivory dial with black lettering (the default). Choose one with **Meter style** in the
meter's right-click menu, or on the *Appearance* settings page.
**Needle inertia** can be switched off to make the needle jump straight to each
new reading.
