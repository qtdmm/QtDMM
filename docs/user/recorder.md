# The recorder

The recorder graph — shown with the **Graph** toolbar button, hidden by
default and brought up automatically when a recording starts — shows
every reading of the meter's primary value, at the time it came: a short
spike between two others is kept, and a slow meter is drawn from reading to
reading. What it records is the value in base units — a reading of 12.3 mV
is stored as 0.0123 V — so the curve stays continuous when the meter
changes range.

## Live, recording and view

The graph is in one of three modes:

- **Live** — right after connecting the graph runs with the meter: it
  holds the readings of the last recording length, or of the last hour
  when the recording has no length, and older ones fall out on the left.
  A shorter time span for a while loses nothing: back on **All** the curve
  still starts where Live started (up to the hour). The newest
  reading is at the right edge. Live starts with **All**: the graph shows
  what there is and grows with it up to the recording length (or the
  hour). The time begins with the first reading, so the curve starts at 0
  also when the meter took a while to connect or after *Clear*. Live is no recording: there is
  nothing to lose, and leaving it asks nothing.
- **Recording** — *Record* (the grey dot ● in the toolbar, Space) asks
  how long to record, with the last length ready: Enter starts it, *Until
  Stop* records until you stop it. Then it clears the graph and records
  from now on, until the length is reached or you press the button again
  (now a red square ■, *Stop*). Top left in the graph stands
  `● REC 0:42 / 10:00`. A recording keeps all its readings from the start,
  up to two million; beyond that the oldest go.
- **View** — a recording that ended, or a file loaded with *Import*,
  stands in the graph to zoom, scroll, save and export. Top left in grey
  stands where it comes from: `Recording of 06.10.26 14:32 · 10:00`, or
  the file's name.

▶ *Live* in the toolbar is pressed while live and locked while recording;
in View it goes back to Live. Going back to Live, starting a new recording
and importing a file ask first when the readings viewed are not saved yet:
**Export data first**, drop them, or cancel.

*Export* while live saves what the graph holds up to now.

The time axis runs on smoothly with the clock, also between two readings of
a meter that sends one a second: live, and while recording with the end of
the recording in view. A slow computer draws it less often by itself.

## Sampling

**Settings → Recording** (Ctrl+F8) sets:

- **Sample every** — the grid of the [export](#export-and-import), in
  tenths of a second, seconds, minutes, hours or days: one row per
  interval, the average of the readings in it, each weighted with how long
  it was shown. A reading counts until the next one, at most until it
  [goes stale](#gaps). The graph always shows every reading.

How long a recording runs is asked when you press *Record* (see above).

**Settings → Graph** sets the visible window (**Time axis**) and the
vertical scale (**Y axis**: automatic, or a fixed minimum and maximum).

The status bar shows how much is recorded and the recording's length, the
time left until the length stops the recording, and whether it records:
`0:42 / 10:00 - 9:18 left - Recording`. While live it shows how much the
live graph holds and the time span shown (the recording length, if one is
set): `12:30 / 1:00 - Live`.

The vertical axis is in the unit that suits its range - mV for an axis up to
0.4 V. A meter that changes range (V, mV) does not change the axis.

## Starting and stopping

Three ways to start, chosen under **Start** on the Recording page:

- **By hand** — *Record* in the toolbar (Space) or the graph's right-click
  menu. With several devices connected, *Record* records all of them
  on one time base; *Start recording* in the graph's menu only those whose
  values the graph shows (the others stay Live). A device opened while it
  records joins when the graph shows a value of it.
- **At a clock time** — recording begins at the given time of day.
- **At a threshold** — recording begins when the reading crosses a
  threshold, rising above it or falling below it. The threshold is drawn
  into the graph as a line you can drag. With **Pre-trigger** the recording
  reaches back by the time set there: it takes the readings of that time
  from the live graph, so the recording shows how the value got to the
  threshold, and a green mark shows where it crossed. The length counts
  from the crossing.
  With more than one value - of one meter, or of the devices connected
  beside the window's - a list in front of the edge chooses the value the
  threshold watches (start a recording of the voltage when the current
  rises above 5 A, also of another device). Its device then leads the
  recording: the threshold, the clock time and the length are its, and all
  the others start and stop with it. A value of a device not connected now
  stays chosen and is marked so. The recording keeps all values; the line
  in the graph is only drawn for the main value.

A recording started at a clock time or a threshold runs for the length
last chosen when *Record* asked.

The predefined time and the trigger wait while the graph is live. A
recording that ended stands in View and is not overwritten by the next
crossing: press ▶ *Live* to wait for the next one. *Record* starts at once
in every mode.

*Clear* (Ctrl+Del) empties the recording, or the live graph; in View it
goes back to Live. QtDMM warns before you lose unsaved data
by clearing, importing, going back to Live, starting a new recording, switching to another device or
quitting; the warning can be switched off under
**Settings → General → At program exit**.

[Alarms](alarms.md) can start and stop the recorder as well, on any of
their conditions. Such a start - and one from the [SCPI server](scpi-server.md) - asks
nobody: a recording
running goes on instead of starting afresh, and a recording shown that is
not saved yet stays; the status bar says why the start was ignored.

A recording measures one thing. When you switch the meter to another
function while it runs - V DC to Ω, DC to AC, °C to °F - the recording
stops, the status bar says what changed, and an orange mark in the graph
shows where. *Record* begins a new recording in the new function. The
stopped one keeps its unit, on the axis and in the export. A change of the
range or prefix (mV to V) is no new function; the recording goes on.

## Gaps

Where the meter showed no value, the curve has a gap instead of a line:

- **OL** - the reading was out of range (overload).
- **No value** - the meter stopped sending, or the connection dropped.
  QtDMM learns how often the meter sends and takes a value as gone after
  three of its intervals (at least 1 s, at most 30 s). The digital display
  and the needle meter fade their value at the same time.
- **No number** - the meter shows dashes or nothing instead of a number
  (`----` of an APPA, `--` of a Victron's time to go while charging, the
  field strength of a ZOTEK's NCV). The display shows what the meter shows,
  the needle rests.
- **Fault** - the meter shows a fault word instead of a number (`FUSE`,
  `Er`, `PROBE`, `BATT` of an APPA). The display shows the word, the status
  line says it, and the graph and the readings table get a red mark where
  it began.

Hovering over a gap shows "OL", "fault" or "no value". In the export, an interval
that holds some values and some gaps is the average of the values there
were; the integration curve carries on across a gap.

## Looking at the data

- The buttons at the top right of the graph set the time window: **All**
  shows the whole recording so far and grows with it, **1 min**, **5 min**
  and **30 min** show the last minutes. Buttons longer than the recording
  (its length, or what a stopped recording or a file holds) are left out.
  Zooming ends **All**; the window set under **Settings → Graph → Time
  axis** stays the start. While a recording runs and the graph shows its
  end, the graph follows it; scroll back to stay at a part.
- The **overview** below the graph shows everything the graph holds, the
  part on screen between two brackets - of the value on the left axis, the
  main one unless it is off (see *Other values of the meter* below). Each
  pixel shows the lowest to the highest reading there, so a short spike
  stays visible in a recording of hours. A red band along the top marks an
  overload, hatching a gap, dots on the bottom edge the alarm marks. Drag
  the part between the brackets (or click beside it) to move there; drag a
  bracket to widen or narrow the window. Moved back while live, the graph
  stays at that part (`Live paused` top left) while the readings go on; a
  click at the right end of the overview, a time button or *Live* follows
  the newest reading again. **Overview** in the right-click menu switches it
  off: the scroll bar is back. Whether it shows is kept with the window
  layout and in a saved bench.
- **Min/max band** in the right-click menu draws a faint band behind the
  curve: at each point the lowest to the highest the curve reaches within
  four pixels to either side. It shows how much a value scatters around
  the curve, as wide on screen at 1 minute as at 1 hour. It is off at
  first; whether it shows is kept with the window layout.
- **Other values of the meter:** a meter with a second display (the
  temperature of a hygrometer or an anemometer, the frequency of a UT61E,
  the second value of a Victron or a Fluke 45) shows it as another curve,
  recorded with the main value on the same time axis. Each value has a
  colour of its own; a legend at the top left names the curves. A value in
  the main value's unit shares its axis, the first other unit gets an axis
  on the right with a scale of its own, a third unit a third axis outside
  it. The titles of the right axes ([°C], [Hz]) are in the colour of their
  first curve, so it is clear which axis is whose. At most three units are
  drawn at a time: putting a value in a fourth unit on an axis switches off
  the curves of the unit there. When the second display
  changes what it shows (Hz to %), its curve ends and a new one begins; the
  recording goes on. The other values are off at first, so the graph shows
  the main value as before: the menu of a value in the sidebar (**Graph**,
  see [The meter's values](my-devices.md#the-meters-values)) puts each of
  them on the left, right or third axis or switches it off - the main value too. Without it the left axis takes the unit of the
  next value shown (the temperature of a CO2 meter, say), and the reference
  line, the band and the cursors' level lines follow it: they are of the
  value on the left axis. The trigger line belongs to the main value and
  goes with it. Every change sets the axes and
  scales afresh from the curves shown; the recording stays as it is, and
  the main value is still recorded and exported. The choice is kept with
  the window layout. They are recorded either way. The time cursors show
  every value drawn, with a line of statistics each; the overview shows
  the value of the left axis (with none there the first one on the right),
  in its colour. With none of the graph's device (the device of its first
  value) on the left axis, the first value of another device there is the
  left axis' value: the overview, the reference line, the cursor levels
  and the PASS/FAIL band are then that device's; the triggers are the main value's; the integration curve can
  take value 2 (see *Integration* below). The export of a recording has all
  values of every device that recorded, also those switched off here (see
  [Export and import](#export-and-import)).
- **Mouse wheel** zooms the time axis; the **middle button** drags it. On the
  keyboard: Ctrl++ / Ctrl+- zoom, Ctrl+0 shows the whole recording, after a
  click into the graph also `+`, `-`, `0`, the arrow keys, Home and End (see
  [Keyboard and mouse](keyboard.md)).
- **Copy image** (right-click menu or Ctrl+Shift+C) puts a picture of the graph
  on the clipboard.
- **Export image...** (right-click menu) writes the graph to a file: **SVG** or
  **PDF** keep the curve, the axes and their labels as vectors, so they stay
  sharp at any size and can be edited in Inkscape or dropped into a document;
  **PNG** and **JPEG** are pixels. The picture is what the graph shows, so zoom
  and pan first.
- Hovering shows a crosshair with the time (to the millisecond) and the
  value of the reading at the cursor.
- The **reference line** runs across the graph at 0 whenever 0 lies inside
  the vertical scale, with its value small at the right end (`0 A`): a
  current that charges and discharges, or a voltage around zero, reads
  against it at a glance. The phosphor colours draw it with their fine
  scale.
  Drag the line to move it: it snaps to 0, to the mean, minimum and
  maximum of what the graph shows, to the thresholds of the alarms and to
  the limits of the [PASS/FAIL test](limit-test.md); in
  between it takes the value where you let go. The right-click menu,
  **Reference line**, chooses the same, and **Off** hides the line. On the mean, minimum or maximum the
  line follows the readings and says what it is (`mean 1.83 A`). Where it
  stands is kept with the window layout and in a saved bench.
- **Time cursors:** a click into the graph sets cursor **A**, Shift+click
  cursor **B**; drag them to move them, Esc or **Remove time cursors** in
  the right-click menu takes them away. Top right the graph shows the time
  and the values drawn at each cursor and, with both, the time between them
  and a line for each value: its change (Δ), its mean and for a current or
  a power its integral, in Ah, Wh or VAh - the charge a battery took
  between two points, for instance. The value of the integration curve
  gets its integral too, in its unit times seconds (V·s). When a value
  changed what it measures in between (Hz to %), the line is of what it
  measures at the later cursor. A dashed line across the graph at the
  value of the left axis under each cursor shows the change as the distance between them;
  the cursors' names hang as flags under the graph, at the time labels.
- **Integration** (**Settings → Graph → Integration curve**) draws a second curve: the
  integral over time of the readings above a threshold, in the unit times
  seconds (V·s, A·s, W·s), scaled and offset as configured — for charge or
  energy over time. Each reading counts for the time it was shown. For
  ampere-hours or watt-hours set the scale to 1/3600, `0.000277778`. A
  scale set before version 26.2, when the curve was the sum of the samples,
  is converted once, so the curve looks as before. **Apply to** chooses the value
  it integrates: value 1, the main display, or value 2, the second display
  of a meter that has one (a current on a Victron, for instance); it then
  goes on that value's axis. Without a second display there is no curve
  for value 2.
- **Print** (Ctrl+P) prints the graph with a title and comment.

## Colours

**Settings → Graph → Colours** sets the colours of the graph:

- **Neutral** follows the window's design.
- **Scope blue** (the default), **Phosphor green**, **Phosphor amber** and **Chart
  recorder** look like an oscilloscope or a paper recorder. The vertical
  scale has 8 divisions of 1, 2 or 5 units each (1 V, 2 V, 5 V, 10 V, ...)
  and grows to whole divisions. The time axis shows exactly the chosen time,
  in up to 10 divisions of a whole step (10 min: 10 × 1 min, 1 min:
  6 × 10 s); in Live the grid lines run along with the readings, like the
  paper of a recorder.
  The phosphor colours dash the integration curve and add a fine scale:
  on the reference line, else along the bottom edge, and on the trigger
  of a pre-triggered recording, else along the left edge. They have no
  cross in the middle - it looked like a reference and said nothing.
- **Custom** uses the colour buttons that show up beside it.

A curve colour you chose yourself stays in every variant. The graph's
right-click menu, **Graph colours**, can choose other colours for this graph
only; *Default* goes back to the setting.

## Export and import

*Export* (Ctrl+E) writes the recording on the grid of **Sample every** as
CSV, or - pick the file type in the dialog or just name the file `.xlsx` /
`.ods` - as an Excel or OpenDocument spreadsheet with the same columns.
The file type **CSV, every reading** writes each reading at its own time
instead, a gap as a row of its own; *Import* reads its times back. In the spreadsheet
the timestamps are date cells (shown with milliseconds) and the values
numbers, so charts and formulas work without converting anything; the
header row is bold and stays put when you scroll. CSV is what *Import*
reads back (spreadsheets are not imported; save as CSV from there).

The CSV is semicolon-separated:

```
timestamp;time (s);value;unit;status
2026-09-19T10:00:00,000;0;12.3;mV;ok
2026-09-19T10:00:01,000;1;12.4;mV;ok
2026-09-19T10:00:02,000;2;nan;mV;OL
```

The value is written with an SI prefix and the matching unit, exactly as a
meter would show it. The status says what the value is: `ok`, `OL` or `-OL`
for an overload, `gap` for a [gap](#gaps), `none` when the meter showed no
number and `fault` for a fault word. A value that is no number is
written as `nan` in CSV and as an empty cell in a spreadsheet.

With the [PASS/FAIL limit test](limit-test.md) on, a column `limit` at the
end of the row keeps its verdict: `PASS`, `FAIL HI`, `FAIL LO`, or empty
where the test was off. On the grid a step with a FAIL in it is FAIL, even
when the other readings in it passed. A file read back shows the verdicts
again.

The other values of the meter follow, three columns each - `value 2;unit 2;status 2`, then
`value 3` ... - on the same rows. On the grid each is the mean over the step
like the main value; with *CSV, every reading* there is a row at every
reading of any of the values, and each value holds until its next one. Before
a value began, its cells are empty:

```
timestamp;time (s);value;unit;status;value 2;unit 2;status 2
2026-09-19T10:00:00,000;0;12.3;mV;ok;;;
2026-09-19T10:00:01,000;1;12.4;mV;ok;50.02;Hz;ok
```

Since 26.4 every column is named after its device, the graph's first
device too: `Sim A: value;Sim A: unit;Sim A: status`, its second value
`Sim A: value 2` ..., its PASS/FAIL verdict `Sim A: limit`; the values of
the other devices follow the same way (`Sim B: value` ...). A recording is
written whole: every device that recorded with all its values, whichever
the graph shows - as `QTDMM:RECord:SAVE` at the meter that leads it writes
it - and all of them count as saved then. *Export* while live, or of a file
viewed, writes what the graph has. Files without the names (before 26.4)
are read as before.

```
timestamp;time (s);value;unit;status;Sim B: value;Sim B: unit;Sim B: status
2026-09-19T10:00:00,000;0;12.3;mV;ok;;;
2026-09-19T10:00:00,500;0.5;12.3;mV;ok;1.02;A;ok
```

*Import* (Ctrl+I) reads such files back, scaling the values into base units
again; the other values come back as curves (switch them on in the
sidebar), another device's values as curves of their own with its name in
the legend, until *Live* or the next recording. It also accepts files without the status column (`nan` is a
gap there) and the tab-separated format of QtDMM versions before 0.9.5. The
unit of the first row becomes the graph's unit.
