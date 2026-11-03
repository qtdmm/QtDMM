# Readings table

Next to the display, the needle meter and the graph, QtDMM can
list every reading the meter sends: one row per value, with the time it
arrived, the value as the meter displayed it, unit, mode and range. Where
the recorder samples the reading on a fixed grid (once a second by
default) and keeps only the number, the table is the raw protocol of the
session - each of the meter's two to four readings per second, including
mode changes, `OL` and HOLD.

Switch it on with **Readings table** in the toolbar or the menu (Ctrl+4).
It opens as a window in a column right of the graph; see
[Window layout](connecting.md#window-layout) for arranging the windows.

## The columns

| Column | Content |
|---|---|
| Time | when the reading arrived, to the millisecond; the date is added once the rows span more than one day |
| Device | which open device the row is from - only while the table shows more than one |
| No. | which value of the meter, 1 the main one - only for a meter with more than one |
| Value | the value as shown on the meter, `OL` for overload |
| Unit | with its SI prefix, as on the display (`mV`, `kOhm`) |
| Mode | `DC`, `AC`, `AC+DC`, `Resistance`, `Diode`, `Continuity`, `Capacitance`, `Frequency`, `Temperature` |
| Range | `AUTO` or `MANU`, when the meter reports it |
| Hold | `HOLD` while the meter's hold function is on |
| Limit | `PASS`, `FAIL HI` or `FAIL LO`, in green or red - the [limit test](limit-test.md)'s verdict; the column comes with the first one |

The columns are as wide as their content and never narrower, so they do
not jump while readings come in; a table narrower than that scrolls
sideways. Copied rows always carry the date.

Meters that send a second value with each reading (a frequency next to the
voltage, say) get an extra row for it, with 2 in the No. column; a third
and fourth value (the humidity of an AirCO2ntrol, the fields chosen for a
Victron) have 3 and 4. Four rows for each reading are a lot: **Readings
table** in the menu of a value in the sidebar (see
[The meter's values](my-devices.md#the-meters-values)) leaves a value out
of the table and switches it on again. Kept with the window layout.

The table lists the readings of the connected devices it is given;
without a choice it lists all of them. A
value in the table (its value menu, or **Sources...** in the table's
context menu) brings a device in or takes it out. With more than one device
the rows come in time order, the Device column names each row's device, and
copying and the export carry it as well (`device;timestamp;...`). The
statistics line is of the first value the table shows, with the device's
name in front.

## Following, keeping, clearing

The **pause** button at the left of the bar stops the table from taking
new readings (the meter keeps running, the recorder is not affected); it
turns into a play button, and a second click resumes.

The table **follows** the newest reading: it scrolls to the bottom as rows
arrive. Scroll up to look at older rows and following pauses; scroll back
to the end, or tick **Follow**, to resume.

The table keeps the newest **N rows** (10 000 by default, set it next to
**Keep**) and drops the oldest beyond that - at four readings a second
that is about forty minutes. **Clear table** empties it; the recorder's graph is
not affected, and clearing the graph (Ctrl+Del) leaves the table alone.

Below the table a line sums up what it holds for one value - the main
one, or the first one the table shows: the number of its rows and the
minimum, maximum, mean and span of its readings in the function of the
newest one (V DC, Ω, ...; overloads and readings of another function are
left out). For a meter with more values the line begins with the value it
is of: *Value 1 (Battery voltage): 4 readings  Min ...*.

## Getting the data out

- **Copy** (Ctrl+C while the table has the focus, or the context menu) puts
  the selected rows - all rows the table shows when nothing is selected -
  on the clipboard as tab-separated text with a header line, ready to
  paste into a spreadsheet. Ctrl+A selects everything.
- **Export...** writes the whole table as CSV, the values left out of it
  too: `timestamp;value;unit;mode;range;hold`, and `;limit` with the
  limit test's verdicts
  with ISO 8601 timestamps (`2026-09-21T14:03:05,250`), the value and unit
  exactly as displayed - or, with a `.xlsx` / `.ods` file name or the file
  type picked in the dialog, as an Excel / OpenDocument sheet with date and
  number cells and an extra *Alarm* column. The recorder's own export
  ([The recorder](recorder.md)) remains the one to use for the time-gridded
  series the graph shows. The export has no No. column: a second, third and
  fourth value are marked `2nd`, `3rd` and `4th` in its mode
  (`2nd DC`), as before.
