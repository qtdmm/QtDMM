# Your own board: Arduino, ESP32, Pico

A microcontroller that measures something itself - a sensor on an Arduino,
an ESP32 or a Raspberry Pi Pico - can send its values to QtDMM as plain
text lines over USB serial. No meter protocol has to be imitated: the
lines the Arduino IDE's *Serial Plotter* or the *Teleplot* extension read
are enough, and QtDMM shows them like a meter: display, recorder, export,
XY diagram, alarms.

## Setting it up

1. *Add device*, the maker **Generic**, the model **Microcontroller (text
   lines: Serial Plotter, Teleplot)**. *Find device* names a board with its
   own USB as such - "Microcontroller board (Arduino)", likewise Raspberry
   Pi Pico, ESP32 (S2, S3, C3, C6 on their own USB), Adafruit, Teensy,
   Seeed, SparkFun, STM32 - with the microcontroller first. Boards with a
   CH340, CP210x or FTDI chip (most clones, ESP32 DevKits) show up as that
   chip, like a meter's cable.
2. Choose the board's serial port and the baud rate the sketch sets with
   `Serial.begin()` - 115200 unless you change it; up to 2000000. It is
   the one port setting of this model you choose; the others are fixed
   (8N1).
3. *Connect*. Each name the board sends becomes a value of the device, in
   the order they first come, up to four. A fifth name is left out.

## What the board sends

One line per reading, ending with a line feed (`Serial.println`). Two
formats are understood, line by line, also mixed:

**Arduino Serial Plotter** - numbers, or `name:number`, separated by commas,
spaces or tabs. All values of a line are one reading:

```cpp
void loop() {
  float u = analogRead(A0) * 5.0 / 1023.0;
  float t = readTemperature();          // your sensor
  Serial.print("voltage:");
  Serial.print(u, 3);
  Serial.print(",temp:");
  Serial.println(t, 1);
  delay(200);
}
```

Without names (`Serial.println(u)`) the values are named by their place:
1, 2, 3, 4.

A name that is a unit is the value's unit too: `V:3.31,mA:12.5` is a
voltage of 3.31 V and a current of 12.5 mA - a voltage in the recorder, the
alarms and the graph's axes. Units QtDMM takes so: V, A, Ohm (or Ω), F, Hz,
W, Wh, Ah, VA, %, %RH, lx, ppm, dB, °C, °F, m/s, each also with an SI prefix
(mV, kOhm, µA). A single letter like `C` stays a name; write `°C`. Other
names (`voltage`, `temp`) are values without a unit; the graph draws them
on an axis of their own.

**Teleplot** - one variable per line, `name:value`, optionally with a unit
after `§`:

```cpp
Serial.print("voltage:"); Serial.print(u, 3); Serial.println("§V");
Serial.print("current:"); Serial.print(i, 4); Serial.println("§mA");
```

QtDMM knows the usual units (V, mV, A, mA, Ω, °C, Hz, ...) and treats such a
value like a meter's: a voltage is a voltage in the recorder and the alarms.
Other units are shown as they are.

`name:x:y|xy` gives two values, `name.x` and `name.y`, read at the same
moment, for the [XY diagram](xy-diagram.md).

## Good to know

- **Time:** a reading has the time QtDMM received its line. Teleplot's own
  time stamps (`name:ms:value`) are accepted but not used; of several points
  in one line (`name:t1:v1;t2:v2`) the last counts.
- **Skipped:** lines of text, words, `nan` or `inf`, Teleplot's text values
  (`|t`), log lines (`>...`) and 3D shapes. Nothing breaks the connection;
  the line is simply left out.
- **Line length:** up to about 98 characters. A longer line is dropped as a
  whole.
- **Wrong baud rate:** the lines come garbled and are all skipped - the
  device then shows no readings. Check the rate the sketch sets with
  `Serial.begin()`.
- **Scaling** (a shunt, a divider) is done best in the sketch for now.
