# Calculated values

A meter shows what it measures. To see a quantity nobody measures
directly - the power a load draws, the resistance from a voltage and a
current, the dew point from temperature and humidity - let your devices
measure what they can and a *calculated value* compute the rest.
It is a device like any other: display, needle meter, graph, recorder,
export, alarms and SCPI all work on it exactly as on a measured one.

## Setting it up

1. Put the meters into [My devices](my-devices.md).
2. *Add device*, the tile **Calculated**, in its list **Formula** (a
   double click or *Next*). The list says beside it what each kind does;
   the search field above it finds one by its name or text. Its settings
   follow:
    - **Name** - how it is listed in My devices.
    - **Inputs** - tick your devices in the list on the left, also one
      that is not connected yet (grey; the calculated value then waits for
      it). Opened, a device shows its values with their names in the
      formula: `v1` is the device's main value, `v1.2`, `v1.3`, `v1.4` its
      other values (a TFA sensor: temperature, humidity, CO₂). The inputs
      ticked are listed on the right as `v1`, `v2`, ...: drag them or move
      them with the arrows into another order - the formulas follow - and
      mark one with **Leads**: a new reading of it computes a new result.
      A device that would close a loop is greyed out.
    - **Values** - up to four results, each with a name, a formula and a
      unit. A value may use the ones above it as `y1`, `y2`, `y3`. The unit
      is proposed from the inputs (V times A gives W) until you choose one.
      Each value shows its result with the readings of right now; a formula
      that does not parse keeps *Next* off and says where.
    - **Maximum age of an input** - a reading older than this counts as
      none (3 s by default).
3. *Next*: the last page shows it as set up; **Connect** adds it to My
   devices and connects it, **Save only** adds it without connecting.

*Info and settings* in its context menu in the sidebar, or the gear on its
front panel, opens its settings again.

## On the front panel

A calculated, simulated or integral value has a front panel like a meter:
its values on the LCD, and below it, where a meter has its keys:

- the **status LED**: green while it computes, yellow while it waits for
  an input (why in its tooltip) or is held, dark while disconnected;
- its own keys: an integral has **0** and **HOLD**.

The **gear** at the right end of the name above every front panel, of a
meter too, opens *Info and settings*. Connect and disconnect start and
stop a calculated value as they do a meter. The small handle at the lower
edge folds the keys away and back.

Values are combined in their base units (volts, amperes, ohms), whatever
range the meters happen to be in. A result is in the base unit of the
unit you give it, and shown with a fitting prefix: 12 V times 0.5 mA is
`6.00 mW`. A unit with a prefix (`mW`) takes the result as 0.006 W, shown
as 6 mW.

## Formulas

- Operators `+ - * / ^`, parentheses. `^` binds tightest, so `-2^2` is `-4`
  and `v1^2/v2` is what you expect.
- Numbers may have an exponent or an SI suffix: `1e-3`, `1.5k`, `22u`, `4.7n`;
  `pi` is a constant.
- Functions: `sqrt(x)`, `abs(x)`, `log10(x)`, `ln(x)`, `sin(x)`, `cos(x)`, `exp(x)`,
  `floor(x)`, `min(a, b)`, `max(a, b)` and `rand()` (a new value in [0, 1)
  every time).
- Variables: the inputs `v1`, `v1.2` ..., the values above `y1` ..., and `t`,
  the seconds since the calculated value started. A calculated value can
  itself be an input of another one, as long as they do not form a loop.

Examples:

| What | Unit | Formula |
|---|---|---|
| Power | `W` | `v1 * v2` |
| Resistance of a load | `Ohm` | `v1 / v2` |
| Voltage drop across a shunt | `V` | `v1 - v2` |
| Magnitude of two components | `V` | `sqrt(v1^2 + v2^2)` |
| A reading scaled by a probe factor | `V` | `v1 * 10` |
| Dew point (Magnus, above 0 °C) of a TFA sensor | `°C` | `243.12*((17.62*v1.2)/(243.12+v1.2)+ln(v1.3/100))/((17.62*243.12)/(243.12+v1.2)-ln(v1.3/100))` |

## Simulated values

For trying QtDMM without a multimeter, for demonstrations or for feeding a
known signal into a calculation: *Add device*, the tile **Simulated**, in
its list **Test signals**. It has no inputs and makes up to four values on a clock (every 250 ms by
default), each with:

- **Name**, **Unit** and **AC/DC** - what the display and the graph label
  the value with.
- A **shape** - *Constant* (the max value), *Random* (a new value between
  min and max each time), *Sine*, *Triangle*, *Square*, *Sawtooth* (between
  min and max, once per period), *Discharge* (from max towards min with the
  period as time constant, like a battery under load) - with **min**,
  **max**, **period** (s) and **noise** (peak to peak, added on top), or
  *Formula*: one of your own over `t` and the values above it (`y1` ...).

Each value shows where it is right now.

## Simulated power supply

A power supply to try a setup before the real one is there, or to feed a
known voltage into a calculation: *Add device*, the tile **Simulated**, in
its list **Power supply**. It has one to four channels (*Channels*; a dual
supply has two), each 0 to 30 V in steps of 10 mV, and is operated on its
front panel:

- **The LCD** has a block for each channel shown: its name on its colour,
  the annunciators `OUT` (output on), `CV`, `EXT`, `SET`, and the voltage
  measured. With two channels they stand side by side, with three or four
  as 2×2.
- **The knob** of a channel sets its voltage: the mouse wheel, dragging up
  or down or the arrow keys turn it in steps (*Step*), with Shift held in
  fine steps (*Fine step*); a double click opens a field on the knob to
  type the voltage, Enter takes it, Esc leaves it. While it turns the LCD
  shows the setpoint with `SET` blinking, two seconds after the last turn
  the measured voltage again.
- **OUT** switches the channel's output; its LED is green while it is on.
  **ALL OUT** switches every output off. The outputs are always off when
  the supply is opened; the setpoints stay as they were.

Each channel's measured voltage is a value as a meter's, `CH1 U` ... in V:
in the graph, the table, alarms, the limit test (its verdict shows in the
channel's block) and calculated values. An output off gives 0 V.

Its settings (the gear on the front panel, or *Settings...* in the sidebar)
have per channel: **shown** (a channel not shown is neither on the front
panel nor recorded, and QtDMM never switches it), its **colour**, the
**lowest** and **highest** voltage the knob sets, and the knob's **steps**.
All 0 at first, **A worse supply** makes it less than ideal:

- **Rise time** - from 0 V to the setpoint when the output goes on (and per
  step up); **Fall time** to 0 V when it goes off (and per step down).
- **Overshoot** - above the setpoint after a rise, in percent of the step;
  it settles within about one rise time.
- **Ripple** (mV peak to peak) and **Noise** (mV) while the output is on.
  A new value comes every 250 ms, too seldom to show a 100 Hz hum: the
  ripple shows as a band of random values, not as a sine.
- **Drift** - mV per minute away from the setpoint since the output went on.

Current, a current limit with CV/CC, a load and protection (OVP, OCP) will
follow; so will real supplies on the same front panel.

## Integral

The energy a load took, the charge into a battery, the light a plant got:
*Add device*, the tile **Calculated**, in its list **Integral**, or
*Integrate...* in the menu of a value (a right click on it in the sidebar
or a view: the assistant starts at its settings). It sums up one value
over time:

- **Value** - open a device in the list and choose one of its values.
- **Unit** - the value's unit times hours: W gives **Wh**, A **Ah**, lx
  **lxh**, any other its unit with an `h`. A prefix may be chosen: `kWh`,
  `mAh`.
- **Gap after** - two readings further apart than this, or one without a
  number (OL, ----), are a gap: the sum pauses over it and goes on after.
- **Start at 0 when a recording starts** - on by default: the recording
  shows the energy since its start.
- **Start at 0 when the value rises above** a threshold - e.g. from the
  moment a load is switched on. Staying above it does not start it again;
  a value that wavers around the threshold starts it at each rise, so put
  the threshold clearly between off and on. It is in the value's base
  unit, shown beside the field: 0.5 A for 500 mA.

The key **0** on its front panel, *Set integral to 0* in its context menu
in the sidebar, the button *Set to 0* in its dialog and SCPI
`QTDMM:NODE:RESet` start it at 0 by hand. After a restart of QtDMM it
starts at 0; changed settings keep the sum as long as the value stays the
same. **HOLD** on its front panel keeps its sum and adds nothing until it
is pressed again; it stays held with the next start.

A reading in another unit than the sum's - the meter switched from W to
V - adds nothing: the sum pauses, and its status says *Unit does not fit:
V instead of W*.

Each new reading of the value adds the trapezoid since the one before: the
mean of both times the time between them.

## Entries from before

The models **QtDMM / Simulated meter** (Virtual meter before 26.2) and
**QtDMM / Calculated value** of earlier versions open as simulated and
calculated values: a simulated meter keeps its waveform and its further
values, a calculated value takes the devices its formula names (by their
names in My devices, *UT61E Bench* as `ut61e_bench`) as its inputs. Their
settings stay as they were until you save them in the dialog. One that
names a device no longer in My devices does not open and says which;
*Info and settings* lets you set it up anew.

## When an input is missing

While an input is not connected, has not delivered a reading within the
maximum age, or shows *OL*, a value that needs it has no number: the
display shows dashes, the graph a gap, and the status says which input it
waits for. The other values go on.

A result is computed at the time of the leading input's reading, with the
others' last readings. An input that is itself calculated from the leading
device is waited for: its result of the same reading is taken. *Record* records the open devices together, on one
time base (see [The recorder](recorder.md)); every CSV export carries
timestamps.
