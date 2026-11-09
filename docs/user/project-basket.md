# Project

A project keeps its files in one folder: its benches,
recordings, pictures of the graph, data sheets as PDF, LabPlot projects,
notes - any file. One project can have several benches, one for each
measuring set-up.

The project is optional. As long as none is open, QtDMM works as before.

Switch the window on with **Project** in the menu (Ctrl+6).

## Making or opening a project

Without a project the window offers **New project...** and **Open
project...**; later the project's name at the top of the window has
the same and **Close project**. New projects are made in the folder
set under **Settings → General → Folder for new projects**
(Documents/QtDMM at first).

The second button of the toolbar shows and hides the project's window;
without an open project it makes a new one. Its arrow has **New
project...**, **Open project...**, the projects opened last
(the open one ticked) and **Close project**.

The project stays open until you open or close another, also across
restarts.

While a project is open, the sidebar shows **Project: name** at the top
with its benches (the one in use in bold; a double click loads one) and
**+ New bench...**, which saves the devices and windows as they are as a
new bench of the project.

A bench belongs to the project in whose folder it lies, also in a
subfolder of it. Loading a bench opens its project; a bench that lies in
no project closes the open one. The status bar says so, nothing asks.

A project is a plain folder. QtDMM puts a `project.json` into a new one,
with its name, a description and when it was made; it is kept for what a
project needs beyond its files.

**Description...** in the menu at the project's name says what it
is about; the window shows it under the name.

## Putting files in

- Drop files onto the window, or choose **Add...** (the + at the top).
  Files are copied, the originals stay where they are.
- **Into the project** in the context menu of the graph writes the
  recording as CSV and the graph as a picture; in the readings table it
  writes the readings as CSV. The names begin with date and time and the
  device, `2026-11-08 0714 UT61E recording.csv`.
- **Export...** (Ctrl+E) can write any format into the project, see
  [Export and import](recorder.md#export-and-import).
- **Save bench...** and **Load bench...** start in the project while one is
  open.
- Every other save dialog starts in the project too while one is open: the
  graph's *Export* and *Export image*, the readings table's export, the
  Poincaré plot's picture, the export dialog's file, the application log,
  printing into a PDF, and a bench loaded from somewhere else. The name
  proposed stays; you can still choose another folder.

A file is never overwritten: a second `ripple.csv` becomes `ripple (2).csv`.

## Working with the files

| Action | What it does |
|---|---|
| Double click, **Open** | A bench is loaded, a recording of QtDMM (CSV) goes into the graph; any other file opens in the program the system has for it |
| **Open with the system** | Opens a CSV in the spreadsheet or LabPlot instead |
| **Rename** (F2) | Renames the file in the folder |
| **Move to trash** | Moves the selected files to the system's trash, never deletes them for good |
| **Open folder** (the folder at the top) | Shows the project in the file manager |

The list shows the names, newest first.

## Exporting the project

**Export project...** in the menu at the project's name packs the whole
project into one ZIP file, to pass it on or to keep it. The file is
offered next to the project, `<name>.zip`. Inside, everything lies in a
folder with the project's name, so unpacking it gives the project back.

The `project.json` in the ZIP gets an `export` part: the date, the QtDMM
version and every file with its size and SHA-256 checksum, to check that
nothing went missing or changed on the way. The `project.json` in the
project itself stays as it is.

Hidden files and folders (names beginning with a dot, such as `.git`) stay
out, as does the ZIP itself when it is saved into the project. A ZIP holds
at most 4 GB.

## Importing a project

**Import project...** (in the menu at the project's name, or as a button
while no project is open) unpacks a project ZIP into a new project in the
folder for new projects and opens it. It is named like the folder in the
ZIP, `Bench (2)` when that name is taken; a project that is there already
is never written into.

Before anything is unpacked, QtDMM checks the ZIP: every file the `export`
part of its `project.json` lists must be there with the same size and
SHA-256 checksum, and no file may lie outside its folder (a path with
`..`). If something is missing or changed, nothing is written and the
message names the files. The `export` part goes again; the project's
`project.json` is as it was before the export.

Any other ZIP works too: its files become a project named like the ZIP
file.
