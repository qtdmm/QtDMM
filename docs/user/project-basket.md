# Project basket

A project basket keeps the files of a project in one folder: its benches,
recordings, pictures of the graph, data sheets as PDF, LabPlot projects,
notes - any file. One project can have several benches, one for each
measuring set-up.

The basket is optional. As long as none is open, QtDMM works as before.

Switch the window on with **Project basket** in the menu (Ctrl+6).

## Making or opening a basket

Without a basket the window offers **New project basket...** and **Open
project basket...**; later the basket's name at the top of the window has
the same and **Close project basket**. New baskets are made in the folder
set under **Settings → General → Folder for new projects**
(Documents/QtDMM at first).

The basket stays open until you open or close another, also across
restarts and when you load a bench from somewhere else.

A basket is a plain folder. QtDMM puts a `project.json` into a new one,
with its name, a description and when it was made; it is kept for what a
project needs beyond its files.

**Description...** in the menu at the basket's name says what the project
is about; the window shows it under the name.

## Putting files in

- Drop files onto the window, or choose **Add...** (the + at the top).
  Files are copied, the originals stay where they are.
- **Into the project basket** in the context menu of the graph writes the
  recording as CSV and the graph as a picture; in the readings table it
  writes the readings as CSV. The names begin with date and time and the
  device, `2026-11-08 0714 UT61E recording.csv`.
- **Export...** (Ctrl+Shift+E) can write any format into the basket, see
  [Export and import](recorder.md#export-and-import).
- **Save bench...** and **Load bench...** start in the basket while one is
  open.

A file is never overwritten: a second `ripple.csv` becomes `ripple (2).csv`.

## Working with the files

| Action | What it does |
|---|---|
| Double click, **Open** | A bench is loaded, a recording of QtDMM (CSV) goes into the graph; any other file opens in the program the system has for it |
| **Open with the system** | Opens a CSV in the spreadsheet or LabPlot instead |
| **Rename** (F2) | Renames the file in the folder |
| **Move to trash** | Moves the selected files to the system's trash, never deletes them for good |
| **Open folder** (the folder at the top) | Shows the basket in the file manager |

The list shows the names, newest first.
