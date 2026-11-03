# PASS/FAIL limit test

The limit test judges every reading of a value of a device against
limits: **PASS** inside, **FAIL** outside, with **HI** or **LO** for the
side. For checking parts on the bench - resistors, batteries, a supply
voltage - without setting up alarms.

## Setting it up

Each open device has its own test. Right-click the device's front panel
(its digital display), or its row under *Bench* in the sidebar:

- **PASS/FAIL test** switches the test on and off. The limits stay when it
  is off. Switched on without limits, it asks for them.
- **Limits...** opens the dialog:
  - **Form**: *Nominal ± percent* (5 V ± 2 %), *Nominal ± amount*
    (2 V ± 0.05 V) or *Lower and upper limit* (4.9 V to 5.1 V).
  - **= reading** takes the value shown now as the nominal - measure a
    good part, take its value, set the tolerance.
  - The line **PASS** shows the limits as the test uses them.
  - **Display in green or red** (on by default): the display takes the
    verdict's colour. A raised
    [alarm](alarms.md)'s colour comes first.
  - **Sound on HI**, **Sound on LO** and **Sound on PASS**: an alarm
    sound when the verdict turns FAIL HI, FAIL LO or PASS; with
    **repeat** again every second while the verdict stays - a steady
    tick while the trimmer is right, say. A new verdict ends it.

The test belongs to the device: it is kept with its entry in
[My devices](my-devices.md), also when it is set over SCPI. **Limits...**
in the menu of a value judges that value instead of the main one. The limits themselves
pass: 4.9 V passes 5 V ± 2 %.

## On the display

While the test is on, the display grows wider by a column right of the
value: **PASS** on green or **FAIL** on red, **HI** or **LO** below it,
and the deviation from the nominal in percent - from the middle of the
range for *Lower and upper limit*. Without a reading (not connected, no
number shown) the column is faint, like the unlit annunciators.

The verdict is recorded with each reading: the [readings
table](readings-table.md) gets a column *Limit*, and the
[recorder](recorder.md)'s CSV export a column `limit`.

An overload counts as FAIL HI, a negative one (-OL) as FAIL LO. There is
no minimum duration and no hysteresis: each reading has its verdict. For
"too long too high", use an [alarm](alarms.md).

## In the graph, on the needle meter and in the status line

The [graph](recorder.md) draws the limits as two dashed green lines, faintly
green between them, while the tested value is the one on its left axis (see [the graph's values](recorder.md#looking-at-the-data)). The
reference line snaps to them. The [needle meter](analog-meter.md) shows
the passing range as a green band on its arc, in place of the red zone,
while its needle shows the tested value. A limit beyond the scale ends
the band at the scale's end. The dial keeps its colour, so the needle
stays easy to follow.

The status line counts the verdicts, one per reading: `PASS 812 · FAIL
23 (2.8 %)`, the tooltip splits FAIL into too high and too low. The
count starts anew when the limits change and with **Reset min/max**.

## Over the network

A program reads and sets the test through the [SCPI
server](scpi-server.md) with Keysight's commands: `CALCulate:LIMit:STATe
ON`, `CALCulate:LIMit:LOWer 4.9`, `CALCulate:LIMit:UPPer 5.1`; bits 11
and 12 of `STATus:QUEStionable?` say FAIL LO and FAIL HI.
`QTDMM:LIMit:VERDict?` answers `PASS`, `HI`, `LO` or `NONE`, and
`QTDMM:LIMit:COUNt?` the count.
