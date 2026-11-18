# Your own board: Arduino, ESP32, Pico

A microcontroller that measures something itself - a sensor on an Arduino,
an ESP32 or a Raspberry Pi Pico - can send its values to QtDMM as plain
text lines over USB serial. No meter protocol has to be imitated: the
lines the Arduino IDE's *Serial Plotter* or the *Teleplot* extension read
are enough, and QtDMM shows them like a meter: display, recorder, export,
XY diagram, alarms.

## Setting it up

1. *Add device*, the maker **Generic**, the model **Microcontroller (text
   lines: Serial Plotter, Teleplot)**.
2. Choose the board's serial port. The baud rate is 115200; for another one,
   tick *External device setup* and set it.
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
