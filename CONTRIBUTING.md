# Contributing to QtDMM

Thank you for helping. Most of what makes QtDMM better comes from people
with a meter on their desk: a report that a model works, a capture of a
meter that decodes wrongly, a translation. You do not need to write code.

Issues and pull requests go to <https://github.com/qtdmm/QtDMM>. A new
issue offers forms for the common cases; please use them, they ask for
what is needed to act on a report. Security problems: see
[SECURITY.md](SECURITY.md).

## Reporting a bug

Please give:

1. What you expected and what happened instead.
2. The QtDMM version (`qtdmm --version`, or *About* in the main menu) and where it
   came from: release package, AppImage, `.dmg`, Windows installer, or a
   build from source.
3. Your operating system and its version.
4. The meter model as printed on the meter, and how it is connected:
   serial or USB cable (with the chip, if you know it), USB-HID cable,
   Bluetooth LE, network (SCPI) or sigrok.
5. For a wrong or missing reading: the [debug output](#capturing-a-meter)
   with a note of what the meter's display showed at the time. This is the
   single most useful thing you can attach.

## Reporting a meter

The [list of supported devices](docs/user/supported-devices.md) says which
models were added from datasheets and protocol documentation only (marked
¹): no one has tried them with QtDMM yet. A short report that such a meter
works - or which function does not - is valuable, and so is a report that
a model not in the list works with one of the listed protocols. The form
*Device report* asks for the details.

## Asking for a new meter

A new meter needs its protocol. Helpful, in order:

1. The model, a link to the manufacturer's page and, if there is one, the
   protocol description or the PC software that reads the meter.
2. A [capture](#capturing-a-meter) of the meter in every function it has.
3. For a Bluetooth LE meter: the name it advertises and its services and
   characteristics (shown, for instance, by the app *nRF Connect*).

### Capturing a meter

Start QtDMM from a terminal (on Windows from a command prompt) with
`--debug`:

```sh
qtdmm --debug 2> capture.txt
```

Connect the meter (a protocol close to it, or *Manual settings*, if your
model is not listed yet), then go slowly through its functions. For each
step write down the time and what the display shows, for example:

```
10:42:05  V DC, 12.34 V
10:42:30  HOLD on, display shows H
10:43:00  Ohm, open leads, display shows OL
```

Each function and range is one step; also the keys (HOLD, MIN/MAX, REL,
°C/°F), an overload (OL) and, if you can, a low battery. Attach
`capture.txt` and your notes to the issue. Every frame is one line of hex
bytes; the [command line](docs/user/command-line.md#debug-output) page
shows an example.

Please check the file before you attach it: a Bluetooth LE meter's
address appears in it. Replace it with `XX:XX:XX:XX:XX:XX` if you prefer
not to publish it.

## Translations

The translations are Qt Linguist files in `assets/translations/`
(`qtdmm_de.ts`, `qtdmm_es.ts`, `qtdmm_fr.ts`, `qtdmm_pl.ts`). To fix or
complete one, edit it with Qt Linguist (`linguist6`) and open a pull
request. For a new language, open an issue first: the file is created from
the sources, so it starts with every string of the current version.

## Code

1. Fork the repository and create a branch from `master`.
2. Build and run the tests (see [INSTALL](INSTALL) for the requirements):

   ```sh
   ./compile.sh ctest
   ```

   Another Qt than the system's, e.g. one from the Qt installer:
   `QTDMM_QT_DIR=~/Qt/6.8.3/gcc_64 ./compile.sh ctest` (switching between
   Qts needs `clean` as well).

3. Keep the change to one subject, with tests for new logic - for a
   decoder, test frames in its protocol specification under
   `docs/protocols/spec/`. The [developer documentation](docs/dev/index.md)
   describes how a protocol, a meter and a cable chip are added, and which
   files are generated (`tests/generate_docs.py`,
   `tests/generate_fixtures.py`); ctest fails when they are out of date.
   Write the code as described in [Coding style](docs/dev/coding-style.md).
4. Update the user documentation in `docs/user/` in the same pull request
   when the change is visible to users, and add a line to `CHANGELOG.md`.
5. Open the pull request against `master` and say what it changes and why.
   A change to a decoder should say whether it was tried with the real
   meter.

QtDMM is developed with the help of AI assistants, and contributions made
with them are welcome. Please say so in the pull request, and check and
test the result as you would your own code: you are its author.

Pull requests made by bots without a person behind them (bounty hunters,
automated "fix" generators) are closed without review. An issue labelled
*help wanted* asks for someone to try, measure or report something; a pull
request that only rewords the issue and "closes" it does not help.

## License

QtDMM is licensed under the GNU General Public License, version 3 or later
(see [LICENSE](LICENSE)).
By contributing you agree that your contribution is published under the
same license.
