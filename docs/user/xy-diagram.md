# XY diagram

The XY diagram plots one measured value against another: X to the right, Y
upwards, one point per pair of readings. It shows how two quantities depend
on each other - the current of a diode against its voltage, the output of a
sensor against a reference thermometer, the voltage of a battery against its
current.

Show it with **XY diagram** in the toolbar or the menu; it sits in the column
next to the graph. **New...** in the list at the arrow of the button asks for
its two values first and opens another one; a click on the button shows or
hides all of them. **Close window** in its right-click menu
removes an added diagram; the window layout and benches keep them.

## Its values

**Sources...** in its right-click menu chooses one value for each axis, of
any connected device: the main value or a secondary one (a meter with a
second display), or a [calculated value](calculated-values.md). Another
choice starts afresh, and so does a device that is replaced by another one.
Above the plot stands what each axis shows, device and name of the value.

## What counts

- **Two values of one device** make a point when both come in the same
  reading, once per reading.
- **Values of two devices** make a point whenever one of them sends; it is
  paired with the last reading of the other one.
- Overload (OL), a display without a number or an invalid reading on either
  axis makes no point; the next valid reading on that axis starts pairing
  again.
- The diagram keeps the last **2000** points. The axes follow the smallest
  and the largest value in them; the numbers at the corners are the limits
  in base units (V, A, Ω ...).

## The context menu

Right-click the plot to:

- **Hold picture** - freeze the current picture while new points continue to
  collect; the **HOLD** mark appears in the plot. Releasing Hold shows the
  accumulated points again.
- **Reset** - remove all points.
- **Save image...** - save the visible diagram as a PNG image.

The diagram takes the colours of the window design chosen in the settings.
