# Histogram

The histogram shows how the readings of one value are distributed: the range
from the smallest to the largest reading is cut into bars, and each bar is as
high as the number of readings that fall into it. A steady value with a
little noise makes one narrow peak; two peaks point to a value jumping
between two levels, a long tail to rare outliers.

Show it with **Histogram** in the toolbar or the menu; it sits in the column
next to the graph. **New...** in the list at the arrow of the button asks for
its value first and opens another one; **New histogram** in the menu of a
value in the sidebar opens one for that value. A click on the button shows
or hides all of them. **Close window** in its right-click menu
removes an added histogram; the window layout and benches keep them.

**Sources...** in its right-click menu chooses the value, of any connected
device; another value starts afresh, and so does a device that is replaced
by another one. Above the plot stand the device and the name of the value.

## What counts

- The last **1000** readings, in base units, shown with a fitting prefix
  (mV, kΩ ...) below the bars, with the smallest and the largest reading at
  the two ends.
- Overload (OL), a held display (HOLD) and a display without a number are
  left out.
- The number of bars follows the number of readings: the square root of it,
  at least 5 and at most 40.
- The height axis counts readings, from 0 to the highest bar.

The histogram takes the colours of the window design chosen in the settings.
