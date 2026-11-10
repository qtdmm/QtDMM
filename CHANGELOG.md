# Changelog

## Unreleased — 27.1 (not released yet)

### GUI

- **More than one graph** — *New...* at the arrow of the Graph button (or *New graph* in a value's menu in the sidebar, or a value dropped on the free area) opens another graph on the same readings. All graphs are alike; each has its own curves, axes, time window and zoom, fixed ranges, cursors, statistics, reference line, overview, buttons and colours, and prints and exports its own picture. The recording stays one for the bench: Record, Live and Clear act on all. With several graphs a value's *Graph* entry in the sidebar has a submenu per graph; print, export and the zoom and note keys work on the graph used last. *Close window* removes an added graph; layout and benches keep them. *Settings → Graph → Time axis* is the window of the graph used last.
- **Graph: buttons for its switches, View in the menu** — beside the time buttons: overview, min/max band, reference line (with its menu), remove the time cursors, note now; they show the graph's state. The right-click menu gathers statistics, overview, band, reference line and *Buttons* (hides them, kept with the graph) under *View*. *Settings → Graph → Statistics* puts the statistics left or right of every graph.
- **Graph: statistics beside the plot** — the floating box of the time cursors is gone; a column at the right of each graph shows the values at cursor A and B, from A to B (Δt, and per value Δ, mean, σ, min, max, readings, ∫ for A/W/VA) and over the window shown (mean, σ, min, max, peak-peak, readings), updated a few times a second. On at first; *Σ* beside the time buttons or *Statistics* in the menu hides it, kept with the graph. Mean and σ are weighted by time.
- **Graph: fixed range for a right axis** — a right click on the right or third axis (or *Right axis [unit]* in the graph's menu) chooses *Automatic* or *Fixed range...* (minimum and maximum, e.g. 49.5 to 50.5 Hz while the left axis scales itself). The axis covers at least that range on the left axis' grid lines, in steps of 1, 1.5, 2, 3, 4, 5, 6 or 8 times a power of ten (0 to 100 % over 8 divisions: 0 to 120 in 15); kept per unit with the window layout and the bench. A right click on the left axis opens the graph's settings.
- **Symbols: System + plain / System + coloured** — *System* is two entries now: the desktop's icon theme filled up with the plain or with the coloured set (QtDMM's own symbols and what the theme lacks). A coloured desktop theme (Oxygen, Papirus, Humanity ...) looked odd with the plain ones; the old *System* takes the one that suits the desktop's theme. The project's list shows folders and file types (image, CSV, spreadsheet, PDF, LabPlot project, bench, text) with the symbols of the set chosen; the dark folder Qt found on a dark window was hard to see.
- **Graph: zoom on a frame, cursors from the menu** — Shift + drag a frame over the graph zooms on its time and height (all y axes); *Reset zoom* in the right-click menu gives back the window and axes of before, Esc while dragging cancels. The menu has *Cursor A here* / *Cursor B here* at the time clicked, and opens from the keyboard too (Shift+F10, menu key).
- **Sample rate in the device's Info** — *Info and settings...* of a connected device shows *Sample rate: 2.0 Hz* below its values: the frames the meter sent in the last 10 s, counted only while the dialog is open.
- **Notes: drag and CSV** — a note's flag can be dragged along the time axis (Esc puts it back; alarm and trigger marks stay put), its mark in the readings table moves along. CSV, Excel and OpenDocument exports end each row with `mark;note`: the marks nearest to the row, several joined by ` | `, quoted where they hold `;`, quotes or line breaks; *Import* reads them back as notes. The CSV inside a `.qtdmm-rec` stays without them (meta.json keeps them exactly).
- **Notes in the graph** — *Note here...* in the graph's context menu and *Note now...* (N in the graph, Ctrl+Shift+N) set a note: a title on a flag under the graph, beside the alarm marks, and an optional text. A click on a flag shows title, time and text, a double click edits them (an alarm's or the trigger's mark can get a text too), its context menu removes the mark. The marks are numbered by time; a note marks the nearest reading in the readings table; a QtDMM recording keeps kind and text (`meta.json`, older files read as before). `QTDMM:MARK "title","text"` sets a note over SCPI.
- **Capture polish** — *QtDMM reads* and the proposal show the reading as a display does (`0.270 nF AUTO`, `V Diode`), without the decoders' codes (CA, OH, DI ...) and surplus leading zeros; the YAML keeps the decoder's fields. *Pause* says *Continue* while pressed. The guided step's box keeps its height, so the buttons below no longer jump. *Capture...* is in the context menu of My devices too (a device not connected is connected first).
- **Capture of a meter QtDMM does not know** — protocol *Unknown meter: bytes only, for a capture* (model *Manual*, serial or USB-HID): QtDMM reads no value, cuts the bytes into frames at a pause of 50 ms and the capture window keeps them with what the display showed, guided by the multimeter steps but without proposals; the status bar and its front panel say *Capture only*, *Capture...* is bold in the device's menu. Bluetooth LE later.
- **Capture: guided steps** — for a multimeter, clamp meter, thermometer, luxmeter or sound level meter the capture window walks through the meter's functions (DC V, negative, AC V, Ω, OL, diode, continuity, capacitance, Hz, mA, HOLD, REL, MIN/MAX, manual range; shorter lists for the other kinds). Once the meter shows something new for a second, QtDMM proposes what it read as *The meter shows*; Enter keeps the frame and goes on. A hint says when QtDMM reads something else than the step wants; *Skip*, *Back*, and *Send HOLD* etc. for keys QtDMM can press. The export names the step above each frame and lists those skipped.
- **Capture of a meter's frames** — *Capture...* in the context menu of a device in the sidebar lists its last 500 frames with their bytes and what QtDMM read (or *not understood*). *The meter shows* and *Keep* keep a frame with what its display showed; *Export...* writes them as a `.yaml` in the form of the spec test vectors (vendor, model, protocol, kind of connection; no port, address or key), to attach to a report of a meter read wrongly. The folder chosen is remembered; *Export also all frames of the list* adds the whole list.
- **Project: folders by kind** — files sort themselves into `benches`, `recordings`, `images`, `analysis` and `documents`, each made with its first file; names on disk always English, `project.json` keeps them in each language and the window shows yours. *Into the project* drops the type word (`recordings/2026-11-08 0714 UT61E.csv`, the picture in `images`), save dialogs start in the folder of the kind, added and dropped files go into theirs. A folder is named in `project.json` only once a file is in it: a save dialog cancelled, or saved elsewhere, leaves the project as it was. *Settings → General → Files into folders by type* (on) switches it off; nothing already there is moved.
- **Folders in the project** — the project's list opens its folders in place; *New folder* in its context menu makes one where you clicked (and names it at once), files dropped onto a folder go into it, and dragging inside the list moves files and folders into another folder.
- **Project and bench without a device** — while My devices is empty, the project button with its arrow, Load/Save bench, Recent benches, *Put the bench into a new project...* and the project and benches in the sidebar are off: neither makes sense without a device.
- **Project window beside the start page** — with devices but none connected, the start page covered the project's window (the button looked pressed, nothing showed, even right after *New project*); the window now stands at the right beside it.
- **Project polish** — the title shows only the project while the open bench lies outside its folder; *New project* asks for a folder (no "All files (*)"); "Bench loaded" comes first in the status bar, the path in its tooltip; a message of the project before goes when another opens; Recent benches name their project.
- **The project's window** — making, opening (also from the last ones) or importing a project shows its window; arranged for the first time it goes to the right like the readings table, not below the graph. Its list leaves out `project.json`, and a project with nothing else in it says it is empty.
- **Put the bench into a new project...** — in Arrange, at Recent benches and at the project button's arrow: makes a new project, opens it and saves the bench into it, the way from a single bench to a project.
- **Sidebar: the project's benches** — while a project is open, "Project: name" at the top lists its benches (the one in use in bold, a double click loads one) and "+ New bench..." saves the set-up as a new bench of the project.
- **Project button: the project's window** — a click shows or hides the project's window (pressed while it shows); without a project it makes a new one. The arrow stays: new, open, the last ones, close. *Close project* also hides the window, as does a bench outside every project.
- **Window title: project and bench** — "Ripple › Bench 1 – QtDMM" while a project and a bench are open, either alone without the other; with neither the device names as before.
- **Loading a bench opens its project** — a bench belongs to the project in whose folder it lies (subfolders too); loading it opens that project, a bench outside every project closes the open one, the status bar says so. Before, the open project stayed and saving proposed its folder for a bench of another.
- **Project and bench: two words** — the project basket is called project now (German: Projekt instead of Projektmappe), in menus, dialogs, messages and the help; a bench stays a bench (Messplatz).
- **Project button in the toolbar** — the second button opens a project basket; its arrow makes a new one, has the ten opened last (the open one ticked) and closes the open one.
- **A bench's meter not plugged in waits for it** — at the start and when a bench is loaded, a meter that does not connect gets its front panel as saved, showing *No connection*, and connects once it is there (tried every 5 s); before, QtDMM left it out and showed *Add device* when it was the only one.
- **Live in LabPlot** — while it records, *Live in LabPlot* in the graph's context menu starts LabPlot with a project whose live data source follows a CSV QtDMM writes row by row: LabPlot's plots (one per unit) grow with the recording. The CSV and the project go into the project basket or *Documents/QtDMM*.
- **One Export** — the graph's context menu has only *Export...* (the export dialog) now, *Export data...* is gone; Ctrl+E opens the export dialog (was Ctrl+Shift+E), the quick CSV export without it is gone, every format is in the dialog.
- **LabPlot project** — the export dialog writes a LabPlot project (`.lml`): the numbers in a spreadsheet and a graph with a plot per unit, scaled and with a legend; it opens in LabPlot without the import dialog, and *Then open in LabPlot* now takes it (the CSV for LabPlot stays as a format). `QTDMM:RECord:SAVE` writes it too.
- **Save dialogs start in the open project basket** — graph export and image, readings table, Poincaré picture, the export dialog's file, application log, printing into a PDF and Save bench (also for a bench loaded from elsewhere) propose the basket's folder while one is open.
- **Import project** — *Import project...* at the project basket unpacks a project ZIP (*Export project*) into a new basket and opens it; files missing or changed since the export (size, SHA-256) or a path out of the folder stop it before anything is written.
- **QtDMM recording: the meters in meta.json** — each meter that recorded with its name, model, protocol, values (unit, quantity), alarms and limit test, and start, end and sample time; a recording loaded and saved again keeps it.
- **Open in LabPlot** — the export dialog writes a CSV for LabPlot (`.labplot.csv`: numbers only, a column per value with its unit, `time[s];Sim A: value [V]`) and with *Then open in LabPlot* starts LabPlot with it (found in the PATH, as Flatpak or set under Settings → General).
- **Export project** — *Export project...* at the project basket's name packs the basket into a ZIP, with a `project.json` that lists every file with size and SHA-256.
- **QtDMM recording** (`.qtdmm-rec`) — a format of its own for Export and the export dialog: the recording's CSV and a `meta.json` with the marks in a ZIP; *Import* and a double click in the project basket bring the marks back.
- **Export dialog** — *Export...* (Ctrl+Shift+E, in the context menus of graph and table) asks what (recording, readings table, picture of the graph), the format (CSV on the grid or every reading, xlsx, ods; PNG, SVG, PDF, JPEG) and where to (a file or the open project basket), and keeps the last choice. *Export* (Ctrl+E) stays the quick way.
- **The open project basket is remembered** — it was kept only until the settings were saved for another reason.
- **Project basket** — optional: a folder with the files of a project, its benches, recordings, pictures, PDFs, LabPlot projects, as a window (Ctrl+6). Made or opened in the window, new ones in the folder set under Settings → General. Files dropped in are copied; *Into the project basket* in the graph and the table writes the recording, a picture of the graph or the readings; a double click loads a bench or a recording, anything else opens in its program. *Description...* at the basket's name describes the project (in `project.json`), shown under its name.
- **The repository page links qtdmm.de.**
- **A file loaded is always shown** — *Import* turns the graph on also with a meter connected, before only the status bar changed.
- **The statistics line of the readings table follows four times a second** — not at each reading: it goes over all rows, about half the time of a reading in a full table.
- **Overview strip: the last digit around 0 stays small** — an open input between 0.000 and 0.001 no longer fills the strip as a square wave.

### Meters

- **UT61+: modes 0x16 and 0x17 as UNI-T's own software reads them** — frequency (Hz to MHz) and duty in %, not a clamp's AC A and DC A; the UT61D+ may send them for LoZ's Hz and duty. No meter has sent either yet.
- **UT171A/B/C read anew** — after dmm-tools re-read UNI-T's UT171C software from its frame handler (the earlier reading came from its database engine): the unit is the frame's own unit code (kΩ, µF, kHz ... as sent, no scaling by the range), the decimals and OL, -OL, dashes, LEAD, DISC, Lo and Hi come from byte 13, the flags from bytes 5 and 6 (HOLD, REL, Peak, AUTO, MAX/AVG/MIN), capacitance is mode 0x0B and the diode 0x0D, the aux display's value comes with its own unit, the square wave output shows frequency and duty. Keys as the software sends them: HOLD, RANGE, AUTO, REL (with the reading shown), MAX/MIN, Peak, Hz%. No UT171 has been read here or there yet.
- **dmm-tools sync (09.11.)** — what dmm-tools learned since 06.11.: the UT803 shows low battery and MAX/MIN, and hFE without a unit (was mA); the UT8802 reads its flags from frame byte 6 (as every uci.dll build does) and OL only from the status bit; the UT8803's lowest inductance range is µH (was mH); ZOTEK meters show OL only for the meter's 0L, other text as it is; a UT181A NaN without an overload bit is no reading (`?`); a UT61+ on a mode the table lacks shows its digits instead of looking dead; no PEAK key on the UT61B+ and UT161B, whose manual has none.
- **Damaged frames of meters without a checksum are no reading** — ES51922, ES51962, ES51986 and VC940: a function or range byte they do not know gave a valid 0, now the reading is a gap.
- **ES51962: the 4000 V range** — it was missing, its readings showed as overload.
- **ES51962: overload compared the wrong bytes** — digits from the first byte on, not the second.

## v26.4-rc1 — Many Meters, One Bench (2026-11-08)

### Multi-meter core

- **Readings take their time from when their bytes came in** — not from when they were decoded, so frames read together share one time and meters read side by side drift less against each other.
- **sigrok meters start and stop without holding up the program** — and when sigrok-cli fails to start or exits, the status bar says why (its own message) and QtDMM connects again, instead of a timeout without reason.
- **More meters in one QtDMM** — entries of My devices connect in the same QtDMM: `--connect <id>` at start (repeatable; `--also <id>` still works), or over SCPI `QTDMM:DEVice:OPEN "<id>"`, `QTDMM:DEVice:CLOSe` and `QTDMM:DEVice:CATalog?`.
- **SCPI chooses the meter with `INSTrument`** — `INSTrument:CATalog?` names the meters of the program (the My devices name in upper case), `INSTrument <name>` or `INSTrument:NSELect <n>` chooses one, and `READ?`, `CALC`, `QTDMM:` and the other commands go to it. The first meter is chosen at start, so existing clients see no change. The network services run once per program.
- **They record together** — one recording (Record, trigger, start time or SCPI) records all meters connected on the same time base and stops them together; `INITiate`/`ABORt` on one of them starts and stops them all. `QTDMM:RECord:SAVE "<file>"[,RAW]` saves the chosen meter's recording into `Documents/QtDMM` (file name only, never overwrites).
- **Calculated values read the meters of the program** — by their My devices name (`UT61E Bench` is `ut61e_bench`). A formula without variables (the Simulated meter, a formula of `t`) needs no other meter.
- **The recording trigger can watch another value** — on the Recording page, a meter with more than one value chooses which value the rising/falling threshold watches, e.g. record the voltage when the current crosses 5 A.
- **Connect instead of instances** — *Use* in the context menu of My devices, **Connect** in a device's info or on the last page of Add device, or dragging it onto **Bench** connects a device in the same QtDMM; the sidebar node **Bench** lists them with their readings and closes them. One QtDMM no longer starts others: the node **Instances**, *In a new window*, renaming and deleting instances, *All instances* for Record and the shared memory between QtDMMs are gone. A calculated value reads the open devices only.
- **Views show values of every meter connected** — a value of a meter in the window layout (the graph's values, `Windows/meter-value=0@device:<id>` and the like) now shows that meter's readings: the display, the needle meter and the Poincaré plot follow it, the graph draws it from that meter's recording as a curve of its own, on the left axis in its unit or on the right one while that is free, the meter's name in the legend. While that meter is not connected the view shows no value and keeps the choice.
- **A front panel for each device** — every meter connected gets its own front panel (LCD and keys) as a window next to the needle meter, all alike; **LCD** (Ctrl+1) shows and hides all of them, the X of one only hides it, the meter stays connected. In **Bench** a double click or *Show front panel* brings a panel back, *Connect*/*Disconnect* switches one. The window layout remembers the meters connected and connects them again at the next start. The needle meter takes the scale of the meter whose value it shows.
- **Values of every open device in every view** — the sidebar lists the values of each open device (a device with one value has the menu in its row, *Value*); the value menu puts any of them on its device's front panel, in the graph on the left or right axis, on the needle meter, the Poincaré plot or in the readings table. **Sources...** in the context menus of the graph, the table, the needle meter and the Poincaré plot ticks the values a view shows (with more than one device open). The readings table lists several devices' readings in time order with a *Device* column, also when copied or exported. A newly opened device changes no view by itself. The active device now changes only with a click on a front panel.
- **Alarms and PASS/FAIL test for every open device** — they belong to the device now and are kept with its entry in My devices (also when set over SCPI), not with the window; the window's alarms from before move into the entry of its device at the first start. Every front panel takes its own device's alarm and PASS/FAIL colours; *PASS/FAIL test*, *Limits...* and *Alarms...* are in the context menu of each front panel and of each row under **Bench**, *New alarm...* and *Limits...* in the value menu. **Settings → Alarms** chooses the device at the top. One banner shows the alarms of all devices with the device's name in front, and *Acknowledge* acknowledges them all; each alarm keeps its own sound.
- **Record and export with every open device** — *Record* records all open devices on one time base; *Start recording* in the graph's context menu only the devices whose values the graph shows. *Export* names every column after its device, the graph's first device too (`Sim A: value;Sim A: unit;Sim A: status`, then `Sim B: value` ..., `Sim B: limit`), on the same grid or with every reading; *Import* shows the other devices as curves of their own and still reads files without the names. `QTDMM:RECord:SAVE` at the meter that led the recording writes the devices that recorded with it too.
- **The min/max band moves with the curve** — it lagged behind the curve and flickered while Live ran on between the readings, and stood still after *Disconnect*; its columns now count from the start, so moving on does not cut them anew.
- **The graph's left axis can belong to another device** — with none of the graph's device's values on the left axis, the first value of another device there takes over the overview strip, the reference line, the cursor levels, the window statistics and its device's PASS/FAIL band. The graph still draws at most two units, one per axis.
- **Workspaces keep the whole set-up** — a bench now has a name, a description and a picture of the windows, and keeps the devices connected (with a copy of their entries in My devices, alarms and PASS/FAIL test) besides the layout and the values of the views. Loading one switches to its devices and closes the others; a device missing in My devices is added back from the file. The name of the bench in use stands at the right of the sidebar's **Saved benches** node, the list shows the names, the tooltip description and picture. The file is JSON now (version 2); benches saved before are still read.
- **Sidebar: "My devices" folded at the start** — the devices in use are under **Bench**; unfold My devices to take out another.
- **One QtDMM per settings file** — a second start with the same `--config-id` says that one runs already (process number) and stops; another `--config-id` runs on its own, without a link.
- **A meter no longer stays faded between its readings** — after a burst of held-up readings the display of a slow meter blinked for good, and a meter slower than one reading in 3 s never learned its rhythm. Two long gaps in a row now count as the rhythm.

### Devices

- **VC-880, VC650BT: the setup screen is no reading** — a number in the menu counted as a measurement in the function's unit, and an OL there as an overload.
- **UT363BT is an anemometer** — it was listed as an air quality meter.
- **UT363BT, UT383BT: data sheets** — ranges, resolution and accuracy from their manuals in the Data tab.
- **TFA AIRCO2NTROL: humidity in %RH** — like the hygrometers; it showed a plain %.
- **Brymen BM78xBT: OL by its text code is an overload** — it was a gap, without the overload alarm.
- **Damaged frames no longer read 0** — on protocols without a checksum a disturbed digit gave a valid 0 in the graph and the recording; it is a gap now.
- **UNI-T UT171: small and large values no longer read OL** — 4.7 nF or 12.345 MHz came with an exponent, and its letter counted as overload.
- **UT71, UT804, VC 920/940/960, Tenma 72-7732: overload no longer reads 0** — OL, HI and LO came out as a valid 0 with odd characters on the LCD, in the graph and in exports. Now OL and HI are overloads, LO of the 4-20 mA loop is no reading. Four digit ranges lost their value the same way.
- **UNI-T UT363BT and UT383BT, TFA AIRCO2NTROL Mini confirmed** — tried on the meters, no longer marked with `*`.
- **AOPUTTRIVER AP-570C-APP no longer marked as untried** — it is the HoldPeak HP-570C-APP under another name, which a user confirmed in 26.3.
- **USB-HID cables keep their udev link** — Add device's search and the saving of a device now take the link in `/dev/qtdmm/` (UT-D04 and similar cables, the TFA CO2 monitor) instead of `/dev/hidrawN`, which changes with every plug; a serial adapter found is saved under `/dev/serial/by-id/` as before. An entry saved with `/dev/hidrawN` gets the link when Add device finds it again.
- **UNI-T UT61E+ and UT161E: AC readings show as AC** — they showed as DC (V AC, mV AC, µA/mA/A AC, LoZ, LPF): the function says AC or DC, not the flag QtDMM read, which real meters set only in the alternating AC+DC position. Found with frames of a real UT61E+ and UT161E (captured for dmm-tools), now test vectors. The UT161 series also on the UT-D09 USB cable; UT61E+ and UT161E on the cable without `*`. An NCV bar or other text is no reading, not 0.
- **Brymen BM525s and BM827s confirmed, also as Minipa ET-2615A and ET-2517A** — frames of real meters (captured for dmm-tools by a user, issues #35 and #36) are now test vectors. With them: AC+DC readings show as AC+DC (they showed DC), °F temperatures in °F, and a display of dashes (no thermocouple, during a range or function change), a lone point or "Auto" (AutoCheck waiting) shows as no reading instead of being dropped.
- **UNI-T UT217A (OWON CMS061) confirmed** — 20 frames of a real meter whose readings its display confirmed (captured for dmm-tools by a user, issue #45) are now test vectors. With them: the NCV field strength (EF, one to four dashes) shows as such; the meter sends it with one decimal and it showed as 0.4.
- **Brymen BM788BT and BM787BT** — over their built-in Bluetooth LE: QtDMM logs in with the default password 0000, as Brymen's protocol asks, and reads the main value with AUTO, HOLD, REL, MIN/MAX/AVG and CREST, the meter's texts (EF level, InEr, Auto) and OL. No remote keys; low battery not read. After dmm-tools' protocol work (A. Cellerier, GPL-3.0-or-later) from Brymen's published protocol; not tried here (*).
- **EEVblog 121GW** — over its built-in Bluetooth LE (hold 1ms PEAK until BT shows): the main reading with AUTO, HOLD, REL, MIN/MAX/AVG, 1 ms peak, the 1 kHz filter and low battery, and the secondary display (frequency, internal temperature, battery, burden voltage, dBm; in the VA modes voltage and current each as their own value); keys RANGE, HOLD, REL, 1ms PEAK, MODE, MIN/MAX and light. Bluetooth meters whose characteristic only indicates are now subscribed with indications. After dmm-tools' protocol work (A. Cellerier, GPL-3.0-or-later) from EEVblog's packet documents and apps, checked against two packets of real meters; not tried here (*).
- **UNI-T UT8802/UT8802N and UT8803/UT8803E/UT8803N bench meters** — over their built-in USB port (CP2110), streamed without a command: the main reading with HOLD, REL, MIN/MAX and manual range; the UT8803 also inductance, Q, ESR and D. Find device lists them with the UT-D09 cable's chip. After dmm-tools' protocol work (A. Cellerier, GPL-3.0-or-later) from UNI-T's UCI SDK; no meter read there or here yet (*).
- **UNI-T UT171A, UT171B and UT171C** — over the UT-D09 USB cable (CP2110 or CH9329), the UT171B/C also over the UT-D07B Bluetooth adapter: QtDMM starts the meter's stream (SETUP > Communication must be ON) and reads the main value with HOLD, manual range and low battery, beside V AC the frequency; NCV as a level. No remote keys. After dmm-tools' protocol work (A. Cellerier, GPL-3.0-or-later) from UNI-T's software; no meter read there or here yet (*).
- **Voltcraft VC-880, VC650BT and VC-890** — over their CP2110 USB cable (press the meter's PC key): the main reading with HOLD, REL, MIN/MAX/AVG and manual range, and the keys RANGE, AUTO, REL, MAX/MIN/AVG, HOLD, light and SHIFT/SETUP; the VC-890 is polled. Find device lists them with the cable. After dmm-tools' protocol work (A. Cellerier, GPL-3.0-or-later) from Voltcraft's software; not tried on a meter (*).
- **UNI-T UT181A** — over the UT-D09 USB cable (CP2110 or CH9329) or the UT-D07B Bluetooth adapter: QtDMM switches the meter's stream on (SETUP > Communication must be ON) and reads the main value, both sub-displays (Hz and period, the AC and DC parts of AC+DC, the second thermocouple, dBm's voltage), REL with reference and absolute value, MIN/MAX with maximum, minimum and average, and PEAK; keys HOLD, REL, MAX/MIN and AUTO. After dmm-tools' protocol work (A. Cellerier, GPL-3.0-or-later) and its frames from two meters; not tried here (*).
- **OWON's 15-byte Bluetooth meters** — OWON CMS101 and CMS061 clamps (UNI-T UT217B/A), OW65B, OW67B and OW69B, Voltcraft VC871, VC891, VC915 and VC925 PV, with the sub-display as second value, power, energy, 4-20 mA, AC+DC, motor, solar and high-voltage functions, and the keys of OWON's app; Add device tells them by their model code. Also the OWON B33. After dmm-tools' protocol work (A. Cellerier, GPL-3.0-or-later); not tried on a meter yet (*).
- **OWON meters over Bluetooth LE** — tried on the CM2100B clamp (all keys and ranges); the B33T+, B35T+, B41T+, OW16B, OW18B and OW18E are built much the same but not tried yet. All of them with OL, NCV, HOLD, REL, MIN/MAX and low battery, and the keys of OWON's app under the display. Add device asks the meter which model it is.

### GUI

- **A file loaded without a device is shown** — the start page covered the graph and the view buttons were grey.
- **`--debug` with several devices** — each frame line now says when and from which device, and `--debug-device <id>` prints the frames of that device only.
- **Drag values onto the views** — a value dragged out of the sidebar becomes the value of the needle meter or Poincaré plot it is dropped on, a curve of the graph, a column of the readings table, or the big line of its own device's front panel; on the free area it offers a new needle meter, a new Poincaré plot or the graph. An entry of My devices dragged onto the windows connects it.
- **More than one Poincaré plot** — **Add Poincaré plot** in the menu (or a plot's right-click menu, or *New Poincaré plot* in the value menu of the sidebar) opens another one with its own value, in the column next to the graph; closing an added one removes it, the window layout keeps them (`Windows/poincares`, `poincare-<n>-value`).
- **Bench: where each value is shown** — at the end of a value's row in the sidebar, marks show where it is: a stroke in its curve's colour (instead of a dot like the states), the symbols of its front panel, of the needle meters and the Poincaré plots with it; a click frames those windows for a moment. The rows name the values as the graph's legend (*Voltage DC*, the number in the tooltip), the readings have a column of fixed width with tabular digits so the names stay put, a device's own row shows its reading only while its values are folded, and temperatures read °C as on the displays. *Info and settings...* is in the context menu of a device under Bench too.
- **Terms: Bench and needle meter** — the sidebar node *Connected devices* and the workspaces are now the **Bench** (*Save bench...*, *Load bench...*, *Recent benches*, the node **Saved benches**); the analog meter is the **needle meter**. Bench files keep their suffix `.qtdmm-workspace`, settings and window layouts stay as they are.
- **Limits & alarms in one submenu** — the front panel's right-click menu has *PASS/FAIL test*, *Limits...* and *Alarms...* under **Limits & alarms**.
- **Name plates like on an instrument** — the frame of a front panel and a needle meter shows the manufacturer bold on the left and the model on the right, in capitals; a name given in My devices stays whole. Too narrow for both, the manufacturer goes first; the lettering grows with the panel up to the size of the sidebar's text.
- **A new needle meter or Poincaré plot asks for its value** — *Add needle meter* and *Add Poincaré plot* in the menu open **Sources...** first; *Cancel* adds none instead of a half empty meter.
- **Simulated meter: every chosen value on the display** — with two or more values (*Values* on its page) the front panel took only the first: the others could not become its big line (sidebar *LCD ▸ Main value*, dragging) and stayed off the small line. The display now has the values chosen, as do SCPI and the value menus.
- **The needle meter and the Poincaré plot choose their value themselves** — **Sources...** in their right-click menu, now also with one device connected. The value menu of the sidebar no longer has *Needle meter* and *Poincaré plot*; it opens a **New needle meter** for the value instead. The main menu has *Arrange* only while the toolbar (with its own Arrange button) is hidden, and no longer the graph's zoom and copy entries (they stay on the graph and keep their keys).
- **Every view says where its value comes from** — the front panel and the needle meter have the device's name on their frame like the model name on an instrument, the Poincaré plot names it above the plot; a device renamed in My devices takes the new name there and under Bench. Until a value comes in, the views say why instead of showing empty digits (*Connecting ...*, *No data*, *Not connected*), and a connected device without readings gets a grey dot and the same in short in the sidebar.
- **More than one needle meter** — **Add needle meter** in the menu (or in a needle meter's right-click menu) opens another needle meter window, each with its own value of any connected device and its own style; the value menu then has **Needle meter ▸** with one entry per window. Closing an added one removes it; the window layout keeps them. *Meter style* in the right-click menu now applies to that meter only, the *Appearance* page sets it for all.
- **A third axis in the graph** — a third unit gets an axis of its own outside the right one (a voltage left, a temperature right, a frequency on the third), automatically or chosen in the value menu (**Graph ▸ Third axis**); the right axes' titles are in the colour of their curves. A value pushed off the right axis moves to the third one when it is free.
- **OWON, EEVblog, Minipa and AOPUTTRIVER meters get their own colours in the device pictures** — they showed the grey generic symbol.
- **New designs: Light Metal and Dark Metal** — the digital display, the needle meter and the table stand on brushed metal, light or dark; the sidebar is light grey or near black, everything else (bench, graph, Poincaré plot) is as in Light or Dark. In the Design menu and on the Appearance page of the Preferences.
- **The window has no device of its own** — My devices is the cupboard, *Use* takes a device out (its dot shows it), *Remove from bench* or dragging it back puts it away; every device connected is alike, with its front panel, recording, alarms and SCPI name. Without a device the window shows **Add device** and **Connect a device from My devices**. The device settings are in each device's info (Shift+F8: the one chosen in the sidebar); the window's settings no longer hold a meter.
- **Views go by their values** — the graph draws from the store of the device of its first value (without any an empty one); the readings table lists the devices it is given, all connected when none is chosen; an empty view takes the device connected. The window keeps the graph's and the table's choice with the devices (`Windows/graph-values`, `Windows/table-devices`), the needle meter's and the Poincaré plot's value with its device; the keys from before are still read.
- **The recording belongs to all connected devices** — *Record*, the graph's menu, an alarm's program and SCPI `INIT` start one recording of the session; the device of the value the start threshold watches leads it (threshold, clock time, length), the others start and stop with it on one time base. The threshold can now watch a value of any connected device (Recording page). A recording keeps its lead until it ends; closing that device ends it for all.
- **Bench: the curve colour of a device with one value** — its row shows the colour of its curve after the green dot while the graph draws it, as the rows of the values of a device with several do.
- **Export writes the whole recording** — *Export* (Ctrl+E) of a recording writes every device that recorded with all its values, as `QTDMM:RECord:SAVE` does, not only the values the graph shows; while live or for a file viewed it writes what the graph has.
- **LCD colours per display** — the colours of all digital displays are now chosen on the Appearance page (*LCD colours*, with the tint for *Custom* below); a display's right-click menu gives that display colours of its own, until *As in the settings* or until its device is closed. The bench keeps them (`Windows/panel-<id>-lcd`). Changing the tint no longer switches the colours to *Custom* by itself.
- **Add device: Connect or Save only** — the last page asks whether to connect the device at once or only keep it in My devices, instead of "in this window" or "beside it"; it is there for an empty window too.
- **Bench: a disconnected one says so** — grey, with "disconnected" instead of its last reading.
- **My devices: a click no longer switches** — a click on a device in the sidebar does nothing; *Use* in its context menu (formerly *Open beside*), greyed while it is on the bench, or the new button **Connect** at the bottom left of its info (double click) connects it.
- **Dark Metal is the default design** — for a new installation; a design chosen before stays.
- **Recent benches** — the benches loaded or saved last, in **Arrange** > **Recent benches** and in the node **Saved benches** at the top of the sidebar (folded at the start, hidden while empty; a double click loads one). Shared by all windows; how many is set on the Appearance page (10 by default).
- **Alarm sounds** — instead of the Beep box an alarm chooses its sound: none, the system's beep, Warning, High, Low, Critical, OK or a WAV file of your own, with a button to try it. It plays once, while the alarm is raised, or a number of times while it is raised, a second apart; Acknowledge ends it. Optionally "OK" sounds when the alarm clears. Alarms with Beep keep the system's beep.
- **PASS/FAIL limit test** — in the display's right-click menu: nominal ± percent, nominal ± amount, or lower and upper limit (**= reading** takes the value shown as the nominal). The display grows a column with PASS or FAIL, HI or LO and the deviation in percent, and turns green or red; optionally a sound for HI, for LO and for PASS, each repeated while the verdict stays if wanted. The verdict is recorded: a column *Limit* in the readings table and `limit` in the CSV and spreadsheet exports. The graph draws the limits as green lines with a faint band between, the needle meter a light green band on its arc in place of the red zone, and the status line counts PASS and FAIL (reset with min/max). Over SCPI as on Keysight's bench meters: `CALCulate:LIMit[:STATe]`, `:LOWer`, `:UPPer`, `:CLEar` (also `CALCulate:FUNCtion LIMit` + `CALCulate:STATe`), FAIL LO/HI in bits 11/12 of `STATus:QUEStionable?`, and `QTDMM:LIMit:VERDict?` / `:COUNt?`. qtdmm-mcp: `limit_test`, and with `--allow-control` `set_limit_test` and `clear_limit_count`.
- **Silver is now called Light** — and the designs are named Light, Dark, Light Metal and Dark Metal in every language; the saved choice stays.
- **First steps** — the first start opens an 800×600 window (at most 84 % of the screen) that the views fill; without a device the status line is empty, a device connected says how (`UT61E: connected (USB)`, the port in the tooltip), and the tooltip of the time (`0:26 / 8:20 - Live`) says what the numbers are. Showing or hiding the sidebar widens or narrows the window by its width, so the views keep their arrangement (maximized they share the room). The graph's automatic axis now fits the data closely for a new installation (**Include zero line** in Settings → Graph brings zero back; a setting saved before stays). Add device shows the picture of a meter it recognises at a port, and the link in the warning about unconfirmed settings takes the design's link colour.
- **The LCD after the kind of meter** — a front panel shows only the annunciators the meter's own display has (a thermometer HOLD, MAX and MIN, a Victron device none) and the bar graph only for multimeters and clamp meters; their room stays, so the panels line up. An annunciator a reading lights shows anyway.
- **Graph: no leftovers after an axis change** — switching a value to another axis or off left old labels at the time axis and beside the right axes, and a piece of curve between them, until the next full repaint. The sidebar's reading column is a little wider, so "387.58 mA DC" is no longer cut.
- **Needle meter: MIN and MAX always with their unit** — the unit stood there only for a value from another range, so 0.4148 on a mV scale could be read as volts.
- **Toolbar: calmer view buttons** — without a device they are greyed out and no longer look switched on, the arrow looks the same whether the view is shown or not. The menu lists the views in the toolbar's order; the shortcuts stay.
- **A log for reports** — **Log...** in the menu opens a window beside the others with what QtDMM did: errors, connection states and the program's messages, with time, scope, level and message. The levels to show, a search with previous and next, and **Export...** as `qtdmm-<date>.log` to attach to a report. It keeps the last 10 000 lines.
- **Add device: the first line of a find in bold** — it reads better.
- **Values... at the front panel** — the context menu of a device's front panel chooses which of its values the LCD shows big and on the small line, all places at once; a value chosen for a second place swaps with the one there.
- **A shorter toolbar** — eleven buttons instead of seventeen: Add device, Devices, Connect, Reset, the views, Arrange and the menu. Record, Live, Clear, Export, Import and Print moved to the graph's right-click menu and keep their keys; Record is in the menu too, as it records the whole bench. A view's button shows or hides all views of its kind (with no value chosen it asks for one first), the arrow beside it lists the windows to tick, the front panels by device, and *New...* opens another needle meter or Poincaré plot, instead of the entries *Add needle meter* and *Add Poincaré plot* in the menu.
- **No active device any more** — **Connect** (F3) and **Reset** (F4) in the toolbar act on the whole bench: Connect stays pressed while one device is connected and then disconnects them all; one device alone connects or disconnects by its context menu. No device is bold on the bench, the status line has no "Active: …" and no PASS/FAIL count (the front panel's tooltip has it). Shift+F8 opens the settings of the device chosen in the sidebar. On the empty start, **Connect a device from My devices** lists the entries and connects the one chosen. In My devices a device on the bench stays bold while it is disconnected.
- **A calmer sidebar** — **Bench** shows no readings and no states any more (they flickered and ran long; the front panels have them), only each device's dot and a row for each value, also for a single one: its name with the unit and the marks of where it is shown, one symbol per kind of view, the tooltip naming the windows; its rows sit one step in from their device. A double click on a value row shows the front panel. **My devices** shows each device's picture without a dot, bold while it is on the bench. **Saved benches** is now **Recent benches**: a star makes one a favourite (first, never dropped, not counted), and the node's own menu loads or saves a bench; the bench in use is named after **Bench** (*Bench · Lab*). *Show front panel* comes first in a device's menu, and a view's menu says **Hide view** instead of *Hide window*. Context menus stand out from the sidebar and the windows in every design: a background a step lighter (dark designs) or darker (light ones) and a frame.
- **Adding or closing a device no longer freezes the program** — with many readings the table was filled again row by row, and the program stood still for up to half a minute, a gap in all recordings. Now it takes a fraction of a second.
- **Needle meter for sound levels and humidity** — sound levels scaled to 1000 dB and the needle stayed at the left, they now end at 130 dB; %RH is always 0 to 100.
- **Fixed arrangement keeps its layout** — closing a device or changing its settings threw the whole layout away, and a device added again landed below everything. A window that returns now gets its place back, and a new one stands beside its kind.
- **Benches saved before 26.4** — loading one with a meter connected left the graph empty; it now shows that meter.
- **Clearer names** — in My devices **Use** instead of Connect, on the bench **Remove from bench** instead of Close, and **LCD** instead of Display in the toolbar and the menu.
- **A calmer overview strip** — the last digit of a steady value no longer fills the strip below the graph as a square wave: its scale spans at least 2 % of the value. It also moves in small steps instead of with every reading, so the strip stands still in between.

### Please note when updating from 26.3

- **The window's meter becomes one of My devices.** The device a window used is connected at the first start like the others, with the alarms set up for the window; a meter set up in the window alone (*Manual* settings, no entry) becomes an entry named after its model (*Meter* for Manual), or takes the entry at its place with its model. The window's settings keep no meter keys any more. The big display is gone: each device has its front panel.
- **Instances are gone, and they are not taken over.** The settings files of other instances (`QtDMM_<id>.conf`) stay where they are and are not read. Add their devices again with *Add device* (their alarms and PASS/FAIL test do not come along) and connect them; *Save bench* keeps such a set-up. `--config-id <id>` still starts an old instance as a QtDMM of its own, without a link to the others. A Victron with more than four values at once took two instances; four are the most now.
- **SCPI:** `QTDMM:INSTances?` is gone (error -113), and with it the qtdmm-mcp tool `instances`; `INSTrument:CATalog?` names the meters of the program.

### Platforms and build

- **The Flatpak reads and writes in your home folder** — files saved without a dialog (`QTDMM:RECord:SAVE` into Documents/QtDMM), benches and recent files by their path stayed inside the sandbox and were lost.
- **The Linux AppImage brings Qt 6.8.3** — instead of Ubuntu's Qt 6.4.2. It is built on Ubuntu 26.04 now and needs glibc 2.43 or newer; on older systems take the Flatpak.
- **`compile.sh` builds with another Qt** — `QTDMM_QT_DIR=<Qt directory> ./compile.sh`; CMake prints the Qt version it found.
- **Qt Multimedia for sound and video** — built in when found (`QTDMM_WITH_MULTIMEDIA`, for the alarm sounds; without it every sound is the system's beep); the Flatpak may play sound.

## v26.3 — Design and Victron Fixes (2026-10-28)

Changes after rc1; the release candidate below lists everything new before.

### Devices

- **HoldPeak HP-570C-APP confirmed** — tried by a user: it is found, connects and shows readings; no longer marked as untried (the AOPUTTRIVER AP-570C-APP twin still is).

### Bug fixes

- **A design chosen in the menu stays** — OK in the Preferences afterwards, for whatever else, set the design shown there before again. Quitting did the same.
- **System after another design** — the device sidebar now and then kept the last design's colours, and its grey entries (services, stopped instances) kept them until they changed.
- **A Victron device chosen in the sidebar shows its own values** — taken over straight after another Victron model (SmartShunt, then SmartSolar MPPT), it showed the values of the one before mapped onto its own (the voltage twice, an empty third value) until another kind of meter was chosen in between.
- **The window dividers no longer react through a dialog** — over the Preferences (or another dialog of a window) lying on top of a divider, the mouse showed the split cursor and could drag the divider; Ctrl+drag in such a dialog moved the window below it.
- **Silver: the keys under the display stand on its background** — the gradient covered only the digits, the key bar below showed the plain window colour.

## v26.3-rc1 — Multi-Value Meters, Time Cursors and Flatpak (2026-10-25)

Keys of your own, a hideable toolbar, alarm colours on the displays, time cursors and an overview in the graph, meters with up to four values (a sidebar row for each), and a much larger SCPI server (channels, VXI-11). New Bluetooth meters from UNI-T, APPA, ZOTEK and HoldPeak; a Flatpak joins the release files.

### Devices

- **UNI-T UT202BT clamp meter over Bluetooth LE** — with its V~, A~ (held: inrush), Ω and NCV/PEAK keys under the display; `QTDMM:KEY` knows these function keys as well.
- **UNI-T iENV meters over Bluetooth LE** — UT333BT (humidity + temperature, tried), UT353BT (sound level, tried), UT363BT (wind speed + temperature) and UT383BT (light); the models not tried yet are marked with `*`. Their keys under the display: HOLD, MAX and MIN (UT363BT: AVG), the unit of the main value, °C/°F, FAST/SLOW, as each meter has them. The meter sends its value in °C, m/s and lx, QtDMM shows it in the unit set on the meter.
- **APPA meters over Bluetooth LE (not tried with a real meter yet)** — the S0–S3, 155B–158B, the 170B clamps, A17N, sFlex, 208B and 506B, and the BENNING, KPS, MEGGER, METRAVI, RS PRO and Sefram meters built on them; found by the name they advertise, as their Bluetooth service is a common one.
- **ZOTEK Bluetooth meters (not tried with a real meter yet)** — ZT-5B, ZT-5BQ, ZT-300AB and ZT-5566SE, sold also as ZOYI, BSIDE and ANENG (V05B, ST207, AN9002, AN999S), with the keys of ZOTEK's app. They all advertise as "Bluetooth DMM": Add device listens to the meter found and chooses the model its first reading names.
- **HoldPeak HP-570C-APP clamp over Bluetooth LE** — also sold as AOPUTTRIVER AP-570C-APP; the FS9721 frame of the HP-90EPC, found by its service 0xFFB0, by its manufacturer data (company 0xFFAC) and by its name "FS9721-LP3". From the maker's app, not tried on a meter yet. The FS9721 meters (VC 820, UT60E, HP-90EPC, ...) show REL and the low battery symbol.
- **New units: m/s (wind speed), %RH (relative humidity), dBA and dBC (sound level)** — no longer read as milli-"/s" or unknown.
- **Meters that show dashes or a fault word instead of a number** — "----" (APPA), "--" (Victron's time to go while charging) and ZOTEK's NCV dashes are no value: a gap, not an overload, in the CSV `none`. "FUSE", "Er", "PROBE", "BATT" (APPA) are a fault: the display shows the word, the status line says it, a red mark where it began, in the CSV `fault`. APPA's dashes and fault words gave no reading at all before.
- **Benetech GM1020 shows its temperature as second value** — it came along, but only SCPI had it.
- **Tenma 72-1016 has no second value any more** — it was the copy of the main value the meter sends with every reading.

### Display and windows

- **Keys of your own** — the new settings page Keyboard lists the commands of the main window with their key and the default; a key can be changed, taken away or set back, a key in use is taken from the other command after a question. One set for all instances (`shortcuts.conf`), a running instance takes a change over at once.
- **Hide toolbar (Ctrl+M)** — in the menu and the right-click menu of the windows: the toolbar goes away for more room, the same key brings it back (a note says so, until "Do not show this again"); the workspace keeps it. The menu moves from Ctrl+M to F7. The toolbar also comes back with the small arrow at the right end of the status line, always there.
- **Alarms colour the displays** — while an alarm is raised, the display and the analog meter take its colour (the LCD's face, light or dark digits for contrast; a third of it in the meter's dial), also after Acknowledge until the reading is back; with several alarms the one raised last. A switch per alarm (on by default), and "Coloured displays blink" on the Alarms page (off by default).
- **MAX, MIN and AVG light on the display** — when the meter shows its own maximum, minimum or average (UNI-T iENV, APPA); a "P" before MAX and MIN for the peak values (UT202BT); the AVG of APPA meters was lost on the way to the keys and the display.
- **Window menus tidied** — "Title bar" and "Hide toolbar" are gone from the window menus (the title bars stay in Arrange, F10); the graph and the readings table have "Hide window" first in their menus as the other windows.
- **Status line without a readings dot** — the dot of the device in My devices blinks with the readings instead. Readings table: "Clear table" instead of "Clear" (it looked like it deleted a row).
- **Design Silver: the area behind the windows is a plain grey** — its gradient showed as stripes across it.
- **Dark design** — empty check boxes have a light edge, in menus too; links in a light blue (the help, the device info).
- **Analog meter: MIN/MAX of the value the needle shows** — on another value than the display's it showed "—".
- **Axis, table and statistics write the units as the display does** — °C instead of C, kΩ instead of kOhm (the Brymen BM869s with two temperatures showed "[C]").
- **Units of their own take no SI prefix** — the graph's axis, the reference line, the cursors, the Poincaré plot, the readings table, the sidebar and the export write 1500 ppm, not 1.5 kppm (also %RH, m/s, dBA, cosφ).

### Multi-value meters

- **Up to four values per meter** — the main one and three more; the display's small line shows two, any of them can be the big one. The Metex M-3850D and its relatives keep all four lines they send; the views' value menus, the alarms and SCPI (`@4`) offer the fourth. A meter set up by hand shows as many values as it sends, not only as many as its lines per reading.
- **Victron: a third and a fourth value** — besides the main and the second one (device settings, none at first), on the display's small line, in the graph and the export.
- **Simulated meter with up to four values** — Device settings, "Values"; the further ones each with unit, coupling and a formula that may use the values before it (`v1 * v2`). sigrok: a meter's channels P2 to P4 are its second to fourth value; they went into the main value by turns.
- **Primary value** — the display's right-click menu chooses the value shown big (the min/max memory follows it); the analog meter and the Poincaré plot choose theirs under Value; kept with the window layout. The recording stays the main value's.
- **A sidebar row for each value** — below this window's instance, with its reading and a dot in its curve's colour while the graph shows it. Its right-click menu says which window shows the value: the LCD (main value, or left or right on the small line), the analog meter, the graph — off or on the left or right axis, where the same unit joins and another one takes the axis over — and the Poincaré plot. The value menus of the windows are gone.
- **Names the meter gives its values** — Victron: "Battery voltage", "State of charge" — in the sidebar, the value menu, the alarm's choice of value and the graph legend; without one the sidebar row shows unit and coupling ("A DC"), the tooltip the full name.
- **Alarms can watch another value** — Settings, Alarms, Value; its thresholds in that value's unit. The graph draws the thresholds of the main value's alarms.
- **Readings table** — a column No. for a meter with more values; the third and fourth value are marked 3rd and 4th in the Mode column, not 2nd like the second one (the humidity of an AirCO2ntrol was "2nd"); Mode is the function alone ("DC", not "2nd DC"; the export stays as it was). "Readings table" in the value menu of the sidebar leaves a value out of the table and of copying.

### Graph

- **Time cursors** — click sets A, Shift+click B, both can be dragged, Esc removes them. Time and value at each, with both the span, the change, the mean and the integral (Ah for a current, Wh for a power); a dashed line across the graph at the value under each cursor, the names A and B as flags under the graph. The cursors show a line of statistics for every value drawn (Δ, mean, the integral for A, W and VA); a voltage no longer gets an integral in V·s, unless it is the value of the integration curve.
- **Reference line at 0** — whenever 0 is in the scale, its value at the right end; phosphor draws it with its fine scale. It can be dragged and snaps to 0, the mean, minimum and maximum shown and the alarm thresholds (also in the right-click menu, Reference line, which can also switch it off); kept with the window layout and in workspaces.
- **Overview below the graph** — shows all it holds, the window between two brackets; each pixel the lowest to the highest reading (a spike stays visible), overloads, gaps and alarm marks. Drag it to move, drag a bracket to zoom; moved back while live the graph pauses there until the right end, a time button or Live. Can be switched off in the right-click menu (the scroll bar is back); kept with the window layout. With the main value switched off it shows the value on the left axis, in its colour.
- **Min/max band behind the curve** — right-click menu, off at first: the lowest to the highest the curve reaches within four pixels either side, so the scatter around it shows at any window; kept with the window layout.
- **A meter's second display is recorded and drawn as another curve** — in a colour of its own, with a legend; a value in another unit than the main one gets an axis on the right. A second display that changes what it shows (Hz to %) begins a new curve without stopping the recording. They are off at first and switched on in the right-click menu, Values (recorded either way), where the main value can be switched off too: the left axis then takes the unit of the next value shown. At most two units at a time, an axis each; a value in a third unit waits until another goes off. The time cursors show them. The integration curve can integrate value 2 instead of the main value (Settings, Graph, Integration curve, Apply to).
- **Flags for alarm marks** — under the graph with their number and, as room allows, the alarm's name; phosphor draws a vertical line on the trigger of a pre-triggered recording. The mark at a change of function reads "Function changed", not "Alarm Function changed".
- **Phosphor: no cross in the middle any more** — it said nothing; the fine scale sits on the reference line and the trigger line, else on the bottom and left edge.
- **Smaller points on the curves** — 5 px, large 8 px instead of 7 and 11.
- **The time axis runs on smoothly with the clock** — live (and recording, with its end in view), about 30 times a second, instead of jumping with each reading (a Victron sends one a second); one drawing clock for the graph and the Poincaré plot, slower by itself when drawing takes too long (`Graph/frame-rate`, 0 = as before). Each curve's newest value is drawn on to now, faint, so the curves run on with it too; All stretches its axis to now until the window is full, the curves reach the right edge.
- **A line style for each value** — value 2 dashed, 3 dotted, 4 dash-dot, a little thinner than the main value and drawn on top of it: curves on top of each other are all seen. The legend shows each curve's line instead of a coloured square; Scope blue has a solid, fainter grid.
- **Poincaré plot: the y axis title stands at its labels in a wide window** — not at the left edge of the window.
- **With the main value hidden, graph helpers follow the left axis** — the reference line, the min/max band and the cursors' level lines follow the value on the left axis instead of going away.

### Export

- **The other values the graph shows go into the file too** — three columns each (value 2, unit 2, status 2), on the grid or at every reading; Import reads them back as curves.
- **Every value has a status column** — `ok`, `OL`, `-OL` or `gap` (the value still `nan`), so an overload and a gap are told apart. Older files without it are read as before.

### SCPI server

- **VXI-11** — the older LAN protocol of VISA (`TCPIP::host::INSTR`), for VISA libraries without HiSLIP and for lxi-tools (`lxi discover`, `lxi screenshot` with `-p keysight-dmm`). Off by default, switched on in Settings, SCPI server. It needs a portmapper on port 111: the system's rpcbind, or an own one where QtDMM may open that port (QtDMM never asks for extra rights; the handbook says how); `--vxi11-rpc` picks it for one run, `--vxi11-portmapper-port` puts an own one on another port (for tests). Sidebar Services row VXI-11, mDNS `_vxi-11._tcp`. Only one program per computer can offer VXI-11.
- **New commands** — `CALCulate:AVERage:MINimum?`/`MAXimum?`/`AVERage?`/`COUNt?` read the min/max memory, `CLEar` resets it; `QTDMM:RECord?` (live, recording, view, with seconds, readings and unit), `QTDMM:RECord:DATA?` (the last readings as CSV, at most 10000), `QTDMM:ALARm?`/`:LIST?`/`:ACKnowledge`, `QTDMM:MARK "text"`, `QTDMM:KEY "hold"` and `:CATalog?` (the UNI-T Bluetooth meters' keys), `QTDMM:INSTances?`; `STATus:OPERation?` bit 9 while an alarm is raised.
- **The meter's values as channels** — `READ? (@2)`, `FETCh? (@1:3)`, `(@1,3)` and the like for `READ?`, `FETCh?`, `MEASure?`, `DATA?`, `VALue?`, `UNIT?` and `CONFigure?`, several answered in one line; `READ3?` alike. `QTDMM:CHANnel:CATalog?` lists them. qtdmm-mcp: `reading` and `watch` take a channel, the tool `channels` lists them; `reading2` is gone.
- **Min/max memory and recorded readings per value** — of each value of a meter, not only of the display's or the main one: `CALCulate:AVERage:MAXimum? (@2)` (or `CALCulate2:...`) and its `CLEar`, `QTDMM:RECord:DATA? 500,(@2)`; several channels at once. qtdmm-mcp: `min_max` and `recording_data` take a channel.
- **Units in one ASCII spelling each** — "C", "F", "Ohm"; the UNI-T iENV meters answered "°C", meters with °F "dF" or "°F".
- **VXI-11 on Windows** — the own portmapper on a free port could fail to start ("The address is protected") when Windows kept the ports in a row for itself; it now takes a port Windows allows for UDP first.

### Devices and connections

- **My devices: Info and settings** — a double click on a device, or "Info and settings..." in its menu, opens its info — the picture and data as on the assistant's last page, where it is in use, connected or not, the last reading, a low battery, its values, a link to the maker's website and to the supported devices — next to its settings. A click switches after the double-click time, so a double click does not switch first; the tooltip says whether it is in use here.
- **Add device, last page** — the TFA sensor's connection reads "USB-HID /dev/hidraw3", without serial parameters.
- **Device settings: "Lines per reading"** — the box "Number of values" of the manual settings is called "Lines per reading", what it always counted (a Metex M-3850D sends four lines, one per value). How many values a model from the table has is told by the table apart from that; a meter with both values in one frame no longer counts its frames.
- **Every model has a kind** — handheld or bench multimeter, thermometer, luxmeter, air quality meter, battery monitor, solar charge controller, inverter, ... (by its protocol, a few models named one by one). The model list of Add device and the device settings shows it as a tooltip, the list of supported devices in a column of its own. The last page of Add device ("Where should it go?") shows a picture of the kind with the name, model, type, protocol and connection of the device.
- **Device pictures in the makers' colours** — a red UNI-T, a yellow Fluke ... — on the last page of Add device and, small and without outline, before each entry of My devices in the sidebar; with a light outline in a dark design.
- **Specifications in a Data tab** — My devices, Info and settings shows the manual's ranges, resolution, accuracy, input impedance and overload protection of the UT803, UT804, UT61B+/D+/E+, UT161B/D/E and ZT-5B / V05B (transcribed by dmm-tools; `tools/update_device_specs.py` takes them over), of the HP-570C-APP from its manual and of the CMT 3503 from its data sheet.

### Bluetooth

- **Seldom advertising meters are found** — Add device searches 20 s (was 7), the settings' Scan 15 s (was 6), the connect waits up to 30 s for the meter (was 8; BlueZ connects only to a device it has just seen, so a meter not seen in time was not connected and not found again after a loss). With `QT_LOGGING_RULES="qtdmm.ble.debug=true"` the search logs each Bluetooth device it sees.
- **Without an adapter, with Bluetooth switched off or in a build without it** — the Bluetooth tile of Add device and the Scan button of the settings are off, their tooltip says why; a search that fails says so with the reason (and in the log, also when the connect searches for the meter) instead of finding nothing.
- **Linux, workaround for the HP-570C-APP** — Add device's search, the settings' Scan and the connect run `bluetoothctl scan le` beside Qt's search (when installed); a user's meter was found only so.
- **UNI-T Bluetooth meters: a key takes effect sooner** — no poll for 0.6 s after it (a poll before the meter's acknowledgement went unanswered).

### Platforms and packaging

- **Flatpak** — each release carries `QtDMM-<version>-x86_64.flatpak` (KDE runtime 6.11, hidapi inside): serial, USB/HID and Bluetooth meters, the SCPI server and several instances. HID meters still need the udev rule on the computer.
- **Building needs CMake 3.19** now.

### Bug fixes

- **A long recording no longer slows down with every reading** — at 2000 readings the graph took ~7 ms for each one (a whole curve built anew every few readings, and the hidden points and integral kept up to date); now ~0.5 ms, ~1.6 ms with points shown. The chart gets only the points around the window, so a reading costs the same at 20000 readings as at 2000 (it grew to ~8 ms at 40000). With points or the integral's points shown it grew much faster, to ~50 ms at 6000; they now take the same way.
- **A meter chosen on the Device page no longer keeps the values of the one before** — SCPI still listed and answered its channels (`UNIT? (@4)` after a four-value meter), and its units stayed until the first reading; choosing a meter in the sidebar did this right already.
- **Switching to another device starts with an empty graph** — before, the last reading of the device before stayed in it with its second display: the axis kept that scale and prefix (k%RH after a CO2 meter), the right axis and the legend kept its temperature. The Poincaré plot starts anew too; the axis title comes with the first reading, also when the new device measures in the same unit.
- **The LCD fills both places on its small line** — a choice of secondary values made with a meter of two values left the third value of the next meter (TFA AIRCO2NTROL: CO2, °C, %RH) off the display until it was put there by hand.
- **A CSV file with a tiny value loads again** — an average around 0 was written as 5.55e-17, and QtDMM refused its own file as "not a valid file".
- **ppm, min and m/s from sigrok, calculated values and the Simulated meter are no longer scaled** — 415 ppm was read as 415 p·pm (4.15e-10) in the graph, min/max, alarms and SCPI; the display was right.
- **Min/max survive "----" and "Er" in continuity** — such a word has no unit, and the memory started afresh.
- **Alarm thresholds carry the unit of the reading** — the settings page labels them with A, Ohm, ppm ...; it always said V. It follows another device also while the settings are open, and before the first reading the thresholds go without a unit.
- **Victron: the time to go shows in minutes below an hour and in hours with a decimal above** — 1.5 h, not 90 min, and "--" while charging instead of nothing (as VictronConnect has it); the graph keeps minutes.
- **The meter's keys under the LCD no longer lie over each other after choosing another meter** — they were placed anew only when the window was resized.
- **Dark design: the sidebar's background stayed unpainted** — after the desktop switches between light and dark, the design is applied anew.
- **Readings table statistics** — the statistics line is of one value: it counted the readings of all values, and says which.

## v26.2 — Nix Flake (2026-10-18)

Changes after rc1; the release candidate below lists everything new before.

### Packaging

- **Nix flake in the repository** — `nix run github:qtdmm/QtDMM` starts QtDMM, `nix develop` gives a shell to build it; on NixOS the udev rule comes with `services.udev.packages` (see INSTALL).

## v26.2-rc1 — New Main Window, Workspaces, My Devices and Add Device (2026-10-12)

First release candidate for 26.2. The main window arranges display, analog meter, graph and readings table as windows in one area; devices are kept in a sidebar and added with an assistant; the graph gets Live, Record and View, a Poincaré plot joins, and the recorder keeps every reading. Many new meters: APPA, Agilent/Keysight, Brymen, luxmeters and thermometers; the SCPI server speaks HiSLIP.

### Devices

- **APPA 501/502/503/505/506/506B/507/208/208B and relatives** — BENNING MM 12, Sefram 7351/7352B, KPS DMM9000BT, Voltcraft VC-930/VC-950, ISO-TECH IDM503/505, CMT 3503, all `*`, over the optical cable: new protocols AppaDmm and Appa500 after libsigrok's appa-dmm, main and sub display; the overload words OL/OLA/OLV/OLVA are an overload.
- **Luxmeters UNI-T UT382 and Benetech GM1020** — both `*`: new unit lx (and fc, foot-candle) and quantity illuminance; the GM1020 also gives its temperature as second value. Protocols after github.com/parametrek/gm1020.
- **Agilent/Keysight U1231A to U1282A handheld meters** — 21 models, `*`, over the IR-USB cable U1173A/B: new protocol KeysightU12xx, polled with `CONF?` and `FETC?` as libsigrok's agilent-dmm does.
- **Thermometers: UNI-T UT325 and MASTECH MS6514** — the UT325 (UT321, UT322, Tenma 72-7712/72-7715 `*`) over the HE2325U USB cable, which now also sends (the UT325 needs a start byte), and the MS6514 `*` with its second display. Temperatures in kelvin as the meters show them. Until QtDMM reads several channels only the large display / main and auxiliary display.
- **19 more meters (all `*`, not tried on hardware)** — Victor 70C/86C, Voltcraft VC 850, EZA EZ-735 (FS9922); Brymen BM255, Greenlee DM-210A (BM25x); Elma BM869s, Greenlee DM-860A, Sanwa PC7000 (BM86x); Brymen BM521s, Elma BM525s (BM52x); Brymen BM821s/822s/827s, Elma BM821s/829s, Greenlee DM-810A/820A/830A (BM82x).
- **Protek 504/505/506 are gone from the model list** — they were listed with the wrong protocol (FS9721 instead of an ASCII line).
- **USB-HID cables are found again after the path changed** — another `/dev/hidraw` number after plugging in again or a restart (on Windows and macOS another path): QtDMM finds the cable again by its USB ids if it is the only one of its kind. The udev rule also makes a stable link per cable and USB port, `/dev/qtdmm/<cable>-<port>`, which the port list shows instead of `/dev/hidrawN`.
- **Meter keys under the display with a fold button** — UNI-T UT60BT, UT61B+/D+/E+ and UT161: SELECT, RANGE, AUTO, HOLD, REL and LIGHT (the UT61x+ also Hz/%, MIN/MAX and PEAK) press the meter's keys; the states light up as the meter reports them, holding MIN/MAX or PEAK leaves it.
- **Bluetooth GATT meters (UT60BT, UT161, UT-D07B) connect again after a restart** — QtDMM looks for the meter before it connects (before, only right after the meter had been found by the assistant).
- **Linux packages install a udev rule for the USB-HID cables and sensors** — `70-qtdmm.rules`.

### Main window and appearance

- **New main window** — display, analog meter, graph and readings table are windows in one area, arranged automatically ("Displays on top", "Displays on the left"), fixed or freely. In the arranged modes the gaps between the windows can be dragged, and Ctrl+drag (or the title bar) swaps two windows; the layout is remembered. In Free mode Ctrl+drag moves a window, also one without title bar.
- **Workspaces** — the window layout can be saved to and loaded from a file (Arrange menu); "Save layout on exit" can be switched off to always start with the same layout.
- **Designs System, Silver and Dark** — the window grows with the content when a view is shown (also for a view shown while it was maximized, when it comes back).
- **Colour variants for the LCD and the graph** — LCD: classic, backlight blue, amber, high contrast, custom; graph: neutral, scope blue, phosphor, chart recorder, custom.
- **Symbols** — coloured (KDE's Oxygen, the default), plain (KDE's Breeze, light and dark) or, on Linux and BSD, the desktop's icon theme; Settings, Appearance, Symbols. They replace the XPM icons.
- **Defaults for a new configuration** — design Dark, LCD backlight blue, analog meter classic ivory, graph scope blue, LCD without min/max. The design can also be chosen in the settings (Appearance).
- **Digital display** — HOLD is an H in a square at the right of the annunciators, MANU is gone (AUTO off is a manual range, as on the meters), a "+" between AC and DC lights for AC+DC.
- **Toolbar in groups** — device (Add device, Devices, Connect, reset Min/Max), windows (display, analog meter, table, Poincaré, graph), recording (Record, Live, Clear), file (Export, Import, Print) — with clearer symbols: Add device with a +, an eraser for Clear (it was a red X, like close), Export and Import of their own. Toolbars are always shown and fixed: the switches for the DMM, graph and file toolbars and the right-click menu that hid them are gone (a toolbar hidden before comes back); "Symbols with text" is on the Appearance page.
- **Shortcuts in function keys** — F3 Connect, F4 reset min/max, F8 settings (Shift+F8 the device, Ctrl+F8 Recording), F10 title bars; Ctrl+C only copies, Ctrl+D, Ctrl+S, Ctrl+X, Ctrl+G, Ctrl+L and Ctrl+R are gone.
- **Status bar** — "Simulated" or "Calculated" instead of the formula, no "Using ..." after a switch.
- **The tip-of-the-day dialog is gone** — and so are the greeting for configurations from before version 0.8.4 and the note on converted `~/.qtdmmrc` files.
- **Analog meter** — the red zone's band on the arc is paler, so the red labels and the min/max marks stand out; the scale arc runs through it and is its outer edge. ppm and % start at zero also with the scale Centre zero. Same bezel thickness for the LCD and the analog meter.
- **LCD and analog meter fade the value when the meter stops sending** — and are back as soon as a value arrives.
- **Temperature: no bar graph** — the analog scale follows the values (1-2-5).
- **Readings table** — columns fit their content, the time without the date; the Mode column names the quantity (Frequency, Duty cycle, ...) where it showed the meter's code (HZ, PC, ...).

### My devices and Add device

- **Add device** — the device symbol in the toolbar: how the meter is connected, then for a cable, Bluetooth or the network a search there — every port found, the recognised meters first: the meter family of a USB cable, the Bluetooth meters and Victron devices, the chip of a USB-serial adapter, the ports of qtdmm-bridge (mDNS; "Find bridges" on the Special ports page is gone); a device QtDMM may not open says why, with the fix —, which meter it is with a name, then this window or a new one; it is kept in My devices, one found there is changed instead of added twice. A window without a meter (the first start) shows only a big Add device button and skips the question where the device goes; the welcome dialog is gone — to try QtDMM without a meter, choose Simulated.
- **My devices** — your meters with their connections, kept for all windows in `devices.conf`, in the sidebar Devices at the left: a click switches (a running recording stops), the one in use shows whether readings come in; Settings, Rename, Open in a new window and Remove in its context menu, drag to reorder. The sidebar opens with the second device and in a window without a meter when there are devices; Devices in the toolbar (F9) shows and hides it. Serial ports are kept under their `/dev/serial/by-id` name. An entry keeps what its way of connecting needs; Find device recognises a saved device by its place (by-id name or ttyUSB, a USB cable plugged in again, a Bluetooth address), connects it without a second entry and says which window uses a port.
- **Instances in the sidebar** — the node Instances lists the instances with their reading and device: a click brings one to the front, a double click starts a stopped one, which can also be renamed — the formulas that use it follow — and deleted. The instances dialog is gone. F2 renames the selected device or stopped instance, as Rename in the context menu.
- **"+ Add device..." at the end of My devices**
- **Settings dialog tidied** — six pages: General, Appearance (now with "Symbols with text"), Graph (colours, line, Y axis, time axis and the integration curve, whose section opens when it is switched on), Recording, Alarms, SCPI server. Section titles without frames, one column, fields that belong to a choice show up with it, the explanations in tool tips, a narrower page list with plain symbols; "Reset page" instead of "Factory defaults". The dialog is as tall as its tallest page (within the screen), so the pages need no scroll bar; it opens on General, not on the page left last time.
- **The pages Multimeter and Special ports are gone** — a meter is set up with Add device and changed with Settings... in the sidebar (Shift+F2 for the one in use). A new first page General holds what happens at program exit (unsaved data, window position and size) and the path of sigrok-cli; the rest stays on Appearance. Ports entered on Special ports before are still offered.
- **Device settings and the assistant: no frames** — the fixed port parameters and protocol of a known model are no longer shown (they were greyed under "Advanced"), manual settings show them at once — also when a model is chosen after manual settings.
- **"QtDMM Virtual meter" is "Simulated meter"** — a new entry; old configurations keep working. SCPI `*IDN?` says "QtDMM Simulated meter".

### Graph and recording

- **Live, Record and View** — the graph runs right after connecting (the last recording length, newest at the right edge); Record (one button, grey dot / red square, Space) clears it and records, "● REC 0:42 / 10:00" top left; a recording that ended or a file imported stands to zoom, save and export. Live, a new recording and an import ask about unsaved readings first; the clock time and the threshold start wait in Live only, the pre-trigger takes its seconds from the live graph. Live starts with "All": the graph fills with what there is instead of a needle at the left of a long window; without a recording length Live keeps an hour at most.
- **Record asks for the length of the recording** — the last one ready: Enter starts it, ∞ records until stopped. The length (Sample time) is no longer on the Recording page. Record asks in a larger window, with "Until Stop" as a choice of its own and a word on a start at a clock time or threshold.
- **Time buttons All / 1 min / 5 min / 30 min** — at the top right of the graph.
- **The recorder keeps every reading with its time** — instead of sampling the last value ten times a second. Short spikes are no longer lost, a slow meter is no longer drawn as steps, and the integral is one over time (unit × s; a scale from before is converted once). The export still uses the sample time as its grid, now the time-weighted mean of each interval; the new file type "CSV, every reading" exports every reading at its time, and the import keeps the times.
- **Max. length is gone** — a recording keeps all its readings from the start, up to two million; one longer than Max. length (1 hour by default) lost its beginning without a word. The time buttons go as far as there is a recording, and while recording the graph follows the newest reading until you scroll back.
- **The pre-trigger works** — it was a setting without function. A recording started by a threshold reaches back by the pre-trigger time, a green mark shows the trigger.
- **Overloads and silent meters are gaps** — an overload (OL) or a meter that stopped sending is a gap in the curve, no longer a 0 or the last value drawn on; the cursor says "OL" or "no value" there. The average of a sample time is taken over the values there were, the integral carries over a gap.
- **Another function stops the recording** — switching the meter to another function (V DC to Ω, DC to AC, °C to °F) stops the recording with a message and a mark in the graph, so one recording never mixes two quantities. A stopped recording keeps its unit, also in the export (it was relabelled with the new unit). A prefix change (mV to V) goes on.
- **Min/max survive a range change** — mV to V, kΩ to MΩ; they start afresh only when the meter switches to another function. Before, every change of unit cleared them.
- **Long recordings drawn with a minimum and a maximum per pixel column** — so a spike stays visible and the drawing stays fast; the grid and labels are there before the first value. The vertical axis is in the unit the meter shows (350 mV, not 0.35 V); the status bar says what the third time is ("1:59:17 left").
- **Export and import of gaps** — a gap is "nan" in CSV (as older versions wrote an overload) and an empty cell in XLSX/ODS; on import "nan" is a gap again instead of a 0. The import counts the recording length in seconds.
- **Poincaré plot (Ctrl+5)** — each reading against the next, so the scatter of a value shows at a glance: noise across the diagonal, drift along it, with SD1 and SD2 and their ellipse. Distance, number of points, hold and image export in its context menu.
- **Alarms: Disconnect first** — the port is closed while the alarm's program runs and opened again after it, so the program can use the meter itself. The settings page External application is gone: an alarm with a condition and a program does what it did (its settings are not taken over).

### SCPI server

- **HiSLIP** — SCPI also over HiSLIP (VISA: NI-VISA, LabVIEW, PyVISA-py), `TCPIP::host::hislip0::INSTR` on port 4880, switched on in Settings, SCPI server. No extra rights needed.
- **Sidebar node Services** — with the SCPI server and HiSLIP (port, off or failed, green dot with a client, double click for the settings, Copy address); the SCPI field in the status bar is gone. `*STB?` reports bit 2 (EAV) while errors are queued.

### Packaging

- **Source release** — releases carry the source as `QtDMM-<version>.tar.gz`, with a fixed checksum for distributions ([#184](https://github.com/qtdmm/QtDMM/issues/184)).
- **AppImage built with linuxdeploy and its Qt plugin** — so it brings Qt, the Qt plugins (platform, SVG symbols, image formats) and a RUNPATH to them along. Before it ran only where a fitting Qt 6 was installed; the libraries copied in were never used. The Linux CI checks the AppImage for that. Qt's own texts (OK / Cancel, Ctrl, file dialogs) are translated too. The AppImage needs glibc 2.38 or newer (Ubuntu 24.04, Debian 13), see Troubleshooting.
- **Licences** — the symbol sets Oxygen and Breeze are LGPL-3.0-or-later. Packages carry their licence texts (Linux `share/licenses/qtdmm`, Windows `licenses\`, macOS inside the bundle), the RPM, PKGBUILD and AppStream name GPL-3.0-or-later AND LGPL-3.0-or-later, and the About box names Oxygen too.
- **Man page** — the example instance name works in formulas (`psu`, not `power-supply`); `devices.conf` under FILES.

### Bug fixes

- **Meter and unit display**
  - The graph's vertical axis takes its prefix from its range, not from the last reading; a meter switching between V and mV turned the whole axis to mV with labels like 1.6e+04.
  - Analog meter: MIN and MAX with no more digits than the display has, and from another range in a prefix of their own with the unit ("10.000 V" in the mV range, not "10000.0000").
  - Virtual meter and calculated values are rounded like an autoranging meter (9.640 V on a 40000 count model, not 9.6402), so the analog meter shows the 40 V range instead of 0..4 with OL.
  - Readings table: min, max, mean and span only over the readings of the current function; after a switch from Ω to mV the old ohms showed as "Max 611 V". The columns are measured again after a design or font change, so no cell is cut off.
- **Poincaré plot** — a meter showing the same value all the time (0.0 mV) got an axis of 1e-12 with endless decimals; the axis now spans ten digits of the display around it.
- **The graph's time begins with the first reading** — the curve starts at 0, also when the meter took a while to connect or after Clear.
- **A gap is no longer bridged by a line** — where the meter fell silent, in a long recording or Live with "All", a pause of minutes was now and then drawn as a straight line, so a part of the curve seemed to come and go; the newest reading was drawn twice when the window grew with it. The gap left by reconnecting after the settings dialog was drawn as a line too.
- **Mouse wheel over a list or number field in Settings scrolls the page** — instead of changing the value (scrolling down Appearance could switch the analog meter to Centre zero).
- **Settings: OK and Apply open the port again only when the meter changed** — before, every OK reconnected (seconds for a Bluetooth meter) and reconnected a meter you had disconnected yourself. Opening the dialog no longer disconnects the meter and no longer stops a recording. Quitting and Apply no longer connect the meter (quitting connected a Bluetooth meter twice).
- **Connecting again no longer clears the recording** — after the settings dialog, or after an alarm program with Disconnect first, the readings were gone without a question; a new start clears it. An alarm program with Disconnect first no longer stops the recording: the time it has the port is a gap.
- **Unsaved readings are asked about first** — switching to another device in the sidebar (or with the assistant, or by changing the one in use), Clear and Clear graph in the graph's menu; Cancel stays with the device.
- **An empty window stays empty after OK in the settings dialog** — it counted as set up and tried to connect without a port, also at every start.
- **Recorder: threshold and pre-trigger** — while waiting for a threshold, another function crosses nothing (0 V, then 1000 Ω over 5), and the pre-trigger keeps no readings of the function before.
- **Recording in all instances** — after a recording that ended without Stop (its length, a switch to another device), the next "All instances" started none, not even the own one — the command was the same as the last one and went unnoticed. Start and stop are new commands each time now.
- **A recording started by an alarm, SCPI `INITiate` or another instance (All instances) no longer clears one that is not saved yet** — and no longer starts one running afresh: the status bar says why, SCPI gets −213.
- **Record works right after a meter is connected** — by Add device, the sidebar or the start; it stayed off until Connect was clicked by hand.
- **My devices: a simulated meter no longer counts as using a port** — entries saved before kept the port of the meter before (the UT61E's), so Open in a new window — from the sidebar or the assistant — took the real meter for "in use" and opened nothing; new entries keep their formula as their device.
- **Instances** — renaming refuses a name that differs from another only in case; the message says the running instances take the new formula after a restart (not after reconnecting), and they no longer write the old one back into My devices. Open in a new window brings the window that already uses the device to the front.
- **Add device: symbol and text of the tiles sit together in the middle** — also of the big Add device button; the target page says "In this window — as the current device".
- **Bluetooth: a meter that stays half connected is tried again after 10 s, not 20 s** — a UT60BT now and then.
- **SCPI** — `CONF?` answers CONT for continuity (it said RES); `READ?` on "-OL" gives −9.9E37. `STAT:QUES?` bit 2 and `INPut?` follow the connection: a meter lost and reconnecting (a Bluetooth meter switched off) is not connected; `INPut OFF` also ends the reconnecting. An old reading no longer sets the overload and hold bits.
- **UNI-T UT71/UT804 (VC940 protocol)** — AC+DC readings are shown as AC+DC (they showed as AC).
- **Voltcraft VC870** — °F on the second display was read as farad.
- **sigrok-cli** — a reading without unit is continuity.
- **Update from 26.1: the window layout starts fresh** — the analog meter is shown and the readings table is hidden, whatever 26.1 had; the new layout is saved from then on.

## v26.1 — First Release since 0.9.8 (2026-10-04)

The first release since 0.9.8; the changes after rc2 are listed here, the two release candidates below list everything new before.

### Display and recording

- **Analog meter** — MIN and MAX readouts (the left one read CURRENT before), no percent arc, the unit on the dial and written as the digital display does (kΩ, °C), long unit labels such as "V DIODE" fit, the bipolar scale is counted from 0, readouts without "-0".
- **Digital display and readings table: leading zeros stay dark** — "0.00", not "000.00", the minus always in front of the digits, as on the meter.
- **Graph** — the background reaches the edges and the scrollbar takes the graph's colours; SVG export fills the requested size; an imported recording shows at once.
- **Record button: a red dot** — instead of the "back to start" arrow.

### Meters

- **Mastech/ELV M9803R, McVoice M-980T** — values in mV, kΩ, nF and other prefixed ranges reach the recorder and the readings table in base units (123.4 mV was recorded as 123.4 V).
- **Brymen BM869s/BM867s/BM525s/BM829s, Voltcraft GDM 703–705, Victron** — the second value (e.g. Hz next to V AC) shows in the display, not only in the readings table; it no longer carries the main value's mode ("2nd PO"); readings table without empty "2nd" rows (also Voltcraft VC870).
- **sigrok-cli meters** — continuity readings are no longer dropped; resistance and temperature units match the other meters (Ω, °C, °F); auto range is recognised wherever sigrok lists it.

### Platforms and packaging

- **macOS disk image (.dmg)** — for Apple Silicon and Intel, macOS 13 or newer; not signed with an Apple certificate, see the handbook (Troubleshooting) for the first start.
- **Desktop file and icons are named after the application id `io.github.qtdmm.qtdmm`** — launchers pinned to the old `qtdmm.desktop` have to be pinned again.
- **API documentation for developers on GitHub Pages** — qtdmm.github.io/QtDMM.
- **Tests** — decoder fixtures for every decoder (from real captures of a UT61E through sigrok-cli, a VC870 and an M9803R, the rest from the protocol documentation); a check for doubled Alt letters in all languages; CI tests stop after 5 minutes instead of hanging.

### Bug fixes

- **Integration curve starts at 0** — it started at the threshold, a vertical line at the left edge.
- **The external program at a threshold runs again** — it did not start since the move to Qt 6.
- **PeakTech 451: readings are decoded correctly** — the frame was read one byte off.
- **Radio Shack 22-812: readings without a prefix (1.234 V, 50.00 Hz) show their unit again** — Voltcraft DO3122: kΩ ranges are recognised.
- **Menus and settings: no Alt letter used twice in one menu or page** — in all languages.

## v26.1-rc2 — Fixes after rc1 (2026-09-29)

Second release candidate for 26.1.

### Packaging

- **App icon as scalable SVG** — installed to `hicolor/scalable` on Linux/BSD; the macOS icon and a new 256 px Windows icon entry are rendered from it.
- **Fedora COPR builds** — thanks to tuxmaster; CMake option `QTDMM_EMBED_TRANSLATIONS=OFF` installs the translations as files for distribution packages.
- **Man page rewritten** — all options, files and examples, version and date from the build.
- **Build** — gcc and clang warnings fixed (thanks to tuxmaster); CI builds treat warnings as errors (CMake option `QTDMM_WERROR`).
- **Security policy (`SECURITY.md`)** — report vulnerabilities privately through GitHub.

### Devices

- **Brymen BM869s confirmed on hardware** — thanks to the tester; saved settings keep the meter.

### Bug fixes

- **AppImage: command-line options reach QtDMM again** — `--version`, `--config-id`, ...
- **The settings dialog fits small screens** — 1080p on Windows: pages scroll, the dialog stays on screen.
- **Recorder: the time window no longer comes out stretched after scrolling** — when the sample time is not 0.1 s.
- **Alarm messages say "Alarm 1: ..."** — instead of "Alarm Alarm 1: ...".
- **A new instance starts with the settings of the current one** — only the meter has to be chosen.
- **No "could not load translation" warning under an English or C locale**
- **Tests: the handbook test no longer depends on the CPU speed** — it failed on RISC-V.

## v26.1-rc1 — Calendar Versioning, Qt Charts, Windows and macOS Builds (2026-09-24)

First release candidate for 26.1; versions are now YY.N, see `docs/dev/releasing.md`.

### Platforms, build and packaging

- **Calendar versioning YY.N** — 26.1, 26.1-rc1, 26.1.1, see calver.org; taken from git tags, no manual version bumps.
- **Qt Charts for the recorder graph** — replaces the own plotting code.
- **Windows build (MSVC, ZIP and Inno Setup installer) and FreeBSD build** — both in CI.
- **Arch/CachyOS fixes ported from duke2421's fork**
- **Unit tests for decoders, IPC and I/O** — decoder fixtures generated from protocol specs.
- **Documentation** — Doxygen developer documentation, handbook built into the app (help browser) and published on qtdmm.de.
- **Translations** — German, Spanish, French, Polish.
- **Project moved to github.com/qtdmm/QtDMM** — AppStream id is now `io.github.qtdmm.qtdmm`.
- **Release builds from version tags** — Linux AppImage, Windows installer and ZIP; DEB and RPM packaging fixed.
- **Known limit: no Bluetooth on macOS yet** — the macOS build leaves it out.

### Devices and connections

- **Device table split into vendor and model** — with chip and connection columns.
- **Fluke 87-IV/89-IV/187/189/287/289 (IR cable) and Fluke 45 bench meter**
- **Brymen meters over the BU-86X USB-HID cable**
- **Keysight, Agilent, HP and Siglent bench meters through sigrok-cli**
- **UNI-T UT-D09 cable in both revisions** — CP2110 and CH9329.
- **UNI-T UT60BT over Bluetooth LE; UT61B+/D+/E+ (UT-D09, UT-D07B) and UT161B/D/E**
- **Victron SmartShunt/BMV, SmartSolar/BlueSolar MPPT and Phoenix inverter** — encrypted Bluetooth LE advertisements.
- **More meters and decoder chips from data sheets and sigrok** — meters that need a hardware modification are marked.
- **qtdmm-bridge** — serve serial and USB-HID meters over the network (RFC 2217, mDNS), found by QtDMM automatically.
- **Connection watchdog with automatic reconnect**
- **Multiple instances with instance manager** — calculated values as a formula over other instances; virtual meter.

### Display and recording

- **New digit display and an analog meter** — both as lockable dock panels.
- **Readings table with every value the meter sent**
- **Alarms** — named conditions with banner, beep, popup, program call and graph marks.
- **Export as CSV, Excel (.xlsx) and OpenDocument (.ods)** — graph as SVG, PDF, PNG or JPEG.
- **CSV import of recordings** — sample time taken from the timestamps.
- **SCPI server** — other programs (lxi-tools, PyVISA, ...) read the meter over the network, announced by mDNS.
- **Keyboard shortcuts everywhere** — settings dialog cleaned up.

## v1.0.0-alpha.1 — Qt 6, CMake and USB-HID (2025-08-06)

Pre-release.

### Platforms and build

- **Switch to Qt6** (redpanther)
- **Switch to the CMake build system**
- **Use C++17**
- **Compile script for easy compiling and rpm/deb creation**
- **pkgconfig for the spec file**
- **Code clean-up and new project structure**
- **macOS support**
- **Test framework to test decoders**

### Devices

- **Basic support for different interfaces** — currently serial, HID and RFC 2217; USB-HID based interfaces; RFC 2217 remote serial port.
- **sigrok as backend** — read and decode DMM data.
- **HoldPeak HP-90EPC** (tstenner)
- **UniTrend UT61B and UT61D, Metex M-3840** — ported from the 0.9.2 original branch.
- **UniTrend UT61E, UT803, UT804, UT70B, UT71B/C/D/E** (redpanther)
- **Voltcraft VC960**
- **PeakTech 3315**
- **Basic support for DTM0660 based DMMs**
- **UniTrend UT30A/E renamed to UT60A/E** — because the UT30 has no data port.

### GUI

- **`--debug` with several devices** — each frame line now says when and from which device, and `--debug-device <id>` prints the frames of that device only.
- **Cleaned-up UI**
- **Support for hold, auto/manual range, continuity test, combined AC+DC and Fahrenheit**
- **Export file in ms precision** — to support more than 1 S/s.

### Bug fixes

- **Mouse wheel zooming in the graph fixed**
- **Manual serial settings load/restore fixed**

## v0.9.7 (2016-10-30)

### Devices

- **Voltcraft VC870** (Florian Evers)

### GUI

- **`--debug` with several devices** — each frame line now says when and from which device, and `--debug-device <id>` prints the frames of that device only.
- **VA symbols to display apparent power**
- **cosφ symbols to display the power factor**

## v0.9.6 (2016-01-28)

### GUI

- **`--debug` with several devices** — each frame line now says when and from which device, and `--debug-device <id>` prints the frames of that device only.
- **Code clean-up**
- **Infinity sign to set an endless recorder**

### Bug fixes

- **Translation loading fixed**
- **Message handler fixed**
- **File export fixed** — leading zero of the time stamp.
- **Console logging on Windows fixed**
- **1/10 s sample rate removed** — it did not work with most devices.
- **Hang of the M-4660A fixed**

## v0.9.5 (2015-07-14)

- **Port to Qt5**
- **Use C++14**
- **Translation support added**

## v0.9.0 (2014-04-27)

- **QtDMM ported to Qt4.x** — still a few minor bugs, but usable.
- **Relicensed from GPL 2.0 to GPL 3**
- **TekPower TP4000ZC / Digitek DT4000ZC added**
- **PeakTech 3430 added**

## v0.8.14 (2009-07-11)

- **Terminal flags**
- **Voltcraft VC 920 added**

## v0.8.13 (2009-07-11)

- **One-hour recording crash bug fixed**
- **Metex M-4650C settings** (Samuel Hildebrandt)
- **Tenma 72-1016 settings** (Simon Morris)
- **Metex M-3870D settings** (Zdeněk Šigut)

## v0.8.12 (2007-12-25)

- **PeakTech 4015A added** (Jochen Puchalla)
- **Tenma 72-7745 added** (Laurent Perez)
- **McVoice M-980T bugfix** (Erik Hobel)
- **Digitech QM1462 added** (Peter Halasz)
- **Digitek INO2513 added**
- **Radio Shack 22-812 added** (Peter Halasz)
- **Typo in dmmgraph corrected** (Erik Hobel)

## v0.8.11 (2007-11-20)

- **GCC 4.3 compilation issues fixed**
- **Compiles on OSX**

## v0.8.10 (2007-09-30)

- **Sinometer MAS-343 added**
- **PeakTech 3330 added** (Jonny Bijlsma)
- **VC820 protocol bugfix**
- **Voltcraft VC940 added**
- **Digitech QM1537 added** (Rob Brown)
- **Uni-Trend UT30A and UT30E added**
- **Digitek DT-9062 added**
- **Voltcraft M-3860 confirmed**
- **PeakTech 4360 added** (Klaus Kaiser)

## v0.8.9 (2007-08-19)

- **Voltcraft M-3610D added**
- **Iso-Tech IDM 73 added** (Paolo Bernardelli)

## v0.8.8 (2006-11-19)

- **Compiles with gcc 4.1**
- **Digitech QM1538 added**
- **MASTECH MAS-345 added**
- **McVoice M-345pro added**
- **Voltcraft M-4650CR added**
- **Context menu for the graph**
- **Import works better** — but not 100 %.

## v0.8.7 (2005-12-20)

- **Support for VC820, VC840, MASTECH M9803R, McVoice M-980T and Radioshack RS22-168A**
- **Manual settings can be saved to a file**
- **GUI configuration**
- **Up to 6 digits in the display**

## v0.8.6 (2005-08-15)

- **Compiler warnings cleaned up**
- **Metex M-3850M and PeakTech-4390 added** (thanks to Hubert Mayer)
- **Digitech QM 1350 added** (James Cameron)
- **Import/export issue fixed** (Hubert Mayer)
- **Configuration problem for large port numbers fixed** (Hubert Mayer)

## v0.8.5 (2005-05-12)

- **Window save behaviour fixed**
- **Sampling counter fixed**
- **ME-42 added**
- **`--console` option** — this might or might not help to identify problems.

## v0.8.4 (2005-04-29)

- **Header file for Debian added**
- **`QtDMM.desktop` added** (Radek Liboska)
- **PNG icon added** (Radek Liboska)
- **Device name and number separated** — this way ttyS0–ttyS99 can be configured without a one-kilometre-long popup menu.

## v0.8.3 (2005-04-24)

- **Finally ported to Qt 3.x**

## v0.8.2 (2002-10-20)

- **Preset for Voltcraft M-3650D**
- **Performance bug in the graph removed** — manual scaling.
- **Improved automatic scaling** — not completed.
- **Mouse panning in the graph**
- **Thresholds can be moved with the mouse**
- **Configuration dialog reworked** — fully non-modal now.

## v0.8.1 (2002-10-15)

- **Include added to fix compilation problems on RedHat based systems**
- **Zoom in/out of the graph with the mouse wheel**

## v0.8 (2002-10-13)

- **Protocol for ELV M9803R added** — thanks to Ralph Fischer for providing a multimeter for testing.
- **Protocol for Voltcraft VC 635 and VC 655 added**
- **Display resides in the toolbar now**
- **Bargraph for the display**
- **More configuration options for the display**
- **Parity setting for the serial port**
- **Setting for display digits** — 3½ = 2000 digits, 3¾ = 4000 digits, 4½ = 20000 digits and 4¾ = 50000 digits.
- **Improved graph scale**
- **Display shows up to four values** — not in the graph yet.
- **Bugfix for continuous polling protocols** — this should fix the problems reported by some users.
- **Display update separated from the graph** — displays as fast as the multimeter provides data.
- **Numerous small bugfixes**
- **"Tip of the day" dialog introduced** — still missing some text.

## v0.7 (2002-03-02)

- **Voltcraft ME-42, M-3860, M-4660A, M-4660M, MXD-4660A added**
- **Protocol for Voltcraft VC 630, VC 650, VC 670 added**
- **Bugfix in the 14 byte polling protocol**
- **Small configuration dialog bug fixed**

## v0.6.2 (2001-12-15)

- **More drawing options and integration curve**

## v0.6.1 (2001-11-25)

- **Online help completed and minor internal clean-up**

## v0.6 (2001-11-24)

- **Chromified display**
- **Lines at triggers in the graph**

## v0.5.1 (2001-10-27)

- **Metex M-3660D added**
- **Some online help chrome**

## v0.5 (2001-09-05)

- **External commands can be started at reached thresholds**

## v0.4.1 (2001-09-04)

- **Improved data import**

## v0.4 (2001-09-01)

- **More DMMs added**
- **Import of data**
- **Configuration dialog is non-modal now**
- **Some GUI chrome**
- **Persistent printer settings**
- **Support for the 22-805 Radioshack DMM** (Roger M.)
- **Support for Metex/Voltcraft ME-11** (Dough LaRue)
- **Support for Voltcraft ME-22T** (Radek Liboska PhD)

## v0.3 (2001-05-09)

- **Support for Voltcraft M-4660** (Michael Petruzelka)

## v0.2 (2001-05-08)

- **Support for PeakTech 451** (Dr. Ralf Wieland)

## v0.1 (2001-04-15)

- **Initial release version 0.1**

---

Original author: Matthias Toussaint <qtdmm@mtoussaint.de>
