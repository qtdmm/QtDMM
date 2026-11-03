# Calculated values

A meter shows what it measures. To see a quantity nobody measures
directly - the power a load draws, the resistance from a voltage and a
current, the difference between two channels - let one meter measure the
voltage, another the current, and a *calculated value* compute from both.
It is a device like any other: display, needle meter, recorder, export and
printing all work on the calculated value exactly as on a measured one.

## Setting it up

1. Put the meters into [My devices](my-devices.md) and connect them in one
   QtDMM: *Connect* in their context menu in the sidebar (or *Connect* at
   the end of *Add device*). The node **Bench** of the sidebar
   (F9) lists them.
2. Each one is a variable by its name in My devices: lower case, other
   characters than letters and digits as `_`, so *UT61E Bench* is
   `ut61e_bench` (a name beginning with a digit gets a `_` in front). Short
   names make short formulas: rename the entries `u` and `i` (Rename in
   their context menu) for `u * i`.
3. Add the calculation: *Add device*, *Simulated / calculated*, the model
   **Calculated value**. Instead of the port and protocol settings a
   *Formula* group appears:
    - **Unit** - the unit of the result without SI prefix: `W`, `V`, `A`,
      `Ohm`, ... QtDMM adds `m`, `k` and so on itself.
    - **Formula** - e.g. `u * i`. Below it QtDMM lists the variables with
      the values the open devices currently deliver, and which devices are
      open; a formula that does not parse is shown in red with the
      position of the error.
4. *Connect*: it connects at once. The status line
   reads *Calculating u * i* and the display shows the power.

Values are combined in their base units (volts, amperes, ohms), whatever
range the meters happen to be in, and the result is shown with a fitting
prefix: 12 V times 0.5 mA is `6.00 mW`.

## Formulas

- Operators `+ - * / ^`, parentheses. `^` binds tightest, so `-2^2` is `-4`
  and `u^2/r` is what you expect.
- Numbers may have an exponent or an SI suffix: `1e-3`, `1.5k`, `22u`, `4.7n`;
  `pi` is a constant.
- Functions: `sqrt(x)`, `abs(x)`, `log10(x)`, `sin(x)`, `cos(x)`, `exp(x)`,
  `floor(x)`, `min(a, b)`, `max(a, b)` and `rand()` (a new value in [0, 1)
  every time).
- Variables are the meters connected in this QtDMM, by their names in My
  devices as above (also those connected with `--connect` or SCPI
  `QTDMM:DEVice:OPEN`, see [command line](command-line.md)).
  A calculated value can itself be an input for another one. `t` is
  reserved: the seconds since the calculated value connected.

Examples:

| What | Unit | Formula |
|---|---|---|
| Power | `W` | `u * i` |
| Resistance of a load | `Ohm` | `u / i` |
| Voltage drop across a shunt | `V` | `u_in - u_out` |
| Apparent power from RMS readings | `VA` | `u * i` |
| Magnitude of two components | `V` | `sqrt(a^2 + b^2)` |
| A reading scaled by a probe factor | `V` | `probe * 10` |

## The simulated meter

For trying QtDMM without a multimeter, for demonstrations or for feeding a
known signal into a calculation there is a second built-in model,
**QtDMM / Simulated meter** (Virtual meter before 26.2). It is a calculated value with the formula
built for you from a few fields:

- **Waveform** — *Constant* (the Max value), *Random* (a new value between
  Min and Max each time), *Sine*, *Triangle*, *Square*, *Sawtooth* (between
  Min and Max, once per Period), *Discharge* (from Max towards Min with the
  time constant Period, like a battery under load) or *Custom formula*.
- **Unit** and **Coupling** (DC or AC) — what the display and the graph
  label the value with.
- **Min**, **Max**, **Period (s)**, **Noise** — noise is the peak-to-peak
  amplitude of uniform noise added on top, in the unit above.
- **Formula** — what the fields expand to; with *Custom formula* you edit
  it directly and may use everything from the section above, including the
  readings of the other open devices.
- **Values** — 1 to 4. From 2 on, a row for each further value with its
  unit, coupling and formula; besides `t` and the other open devices a formula
  can use the values before it as `v1`, `v2`, `v3`. Preset: value 2 a
  current around 0.5 A, value 3 the power `v1 * v2` in W, value 4 a slowly
  swinging temperature in °C. The further values go where a meter's second
  display goes: the display's small line, the graph, the readings table,
  the alarms and SCPI `READ? (@2)`.

The simulated meter needs no other device and delivers its value like a
real one, so `u * i` works just as well with a virtual `u`.

## When an input is missing

A calculated value polls its inputs four times a second. While a variable
has no open device, that device has not delivered a value for three
seconds, or it shows *OL*, the calculated display shows **OL** and
the status line says which input is the problem, for example *Waiting for
'i'*. The recorder keeps running; the gap is recorded like an
overload from a real meter.

Readings arrive at the pace of the slowest meter and the poll interval, so
a calculated value can lag its inputs by up to a quarter of a second plus
one meter update. *Record* records the open devices together, on one time
base (see [The recorder](recorder.md)); every CSV export carries
timestamps.
