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
the same and **Close project**, which also hides the window. New
projects are made in the folder
set under **Settings → General → Folder for new projects**
(Documents/QtDMM at first).

The second button of the toolbar shows and hides the project's window;
without an open project it makes a new one. Its arrow has **New
project...**, **Open project...**, **Import project from file...**, the
bench entries (once there is a device), the projects opened last (the open
one ticked) and **Close project**.

A project also opens without any device: the start page has **Open
project...** and **Import project from file...** below *Add device*, and
the projects opened last (up to five; a click opens one).

A project you open, import or pick from the last ones loads a bench of
its own when nothing is connected: the one used last in it (kept in
`project.json`), else its only one. With more benches and none used last
the start page lists them, a click loads one; without any it says so. The
project's window does not open by itself; a new project made with the
project button shows it. A bench in use stays when a project is opened. A
bench loaded from another project switches to it and leaves the windows as
the bench has them. A window not yet placed goes to the right, like the
readings table. The list leaves out `project.json`, the file in which
QtDMM keeps the project's name, description and the bench used last.

With devices but none connected, the project's window stands at the
right beside the start page. The project's node in the sidebar shows once
there is a device.

The window title shows the project and the bench, the bench only once
it lies in the project's folder.

The project stays open until you open or close another, also across
restarts.

While a project is open, the sidebar shows **Project: name** at the top
with its benches (the one in use in bold; a double click loads one) and
**+ New bench...**, which saves the devices and windows as they are as a
new bench of the project.

**Put the bench into a new project...** (in the menu of the node Bench
and at Recent benches in the sidebar, at the project button's arrow) is the way from a
single bench to a project: it makes a new project, opens it and saves the
bench into its folder.

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
  Files are copied, the originals stay where they are. Each goes into the
  folder of its kind (see below); dropped onto a folder of the project (or
  a file in it) they go into that folder.
- **Into the project** in the context menu of the graph writes the
  recording as CSV into `recordings` and the graph as a picture under the
  same name into `images`; in the readings table it writes the readings as
  CSV into `recordings`. The names are date and time and the device,
  `recordings/2026-11-08 0714 UT61E.csv`.
- **Live in LabPlot** writes its CSV and LabPlot project into `analysis`.
- **Export...** (Ctrl+E) can write any format into the project, see
  [Export and import](recorder.md#export-and-import).
- **Save bench as...** and **Load bench...** start in the project while one
  is open.
- Every other save dialog starts in the project too while one is open, in
  the folder of the file's kind: the graph's *Export* and the readings
  table's export in `recordings`, *Export image* and the Poincaré plot's
  picture in `images`, the application log and printing into a PDF in
  `documents`, a bench loaded from somewhere else in `benches`. A bench of
  the project is saved where it lies. The name proposed stays; you can still
  choose another folder.

A file is never overwritten: a second `ripple.csv` becomes `ripple (2).csv`.

### Photos of the setup

The camera button next to **Add...** opens **Photo of the setup**: a
picture of how the meter was connected, kept with the measurement.

- **Phone**: scan the QR code with the phone's camera and open the link.
  The page that opens has a big **Take a photo** button, which opens the
  phone's camera; each photo is sent to QtDMM at once, and the page stays
  open for the next one. **Choose from the gallery** sends photos taken
  before. No app is needed.
- **Webcam**: a camera of the computer. Choose it under **Device**; its
  live picture shows in the tab, **Take photo** takes one. The camera is
  on only while this tab shows. macOS asks once whether QtDMM may use the
  camera (*System Settings → Privacy & Security → Camera* changes it
  later). Not there in builds without Qt Multimedia.
- **From file**: **Add file...** takes a picture in, or drop pictures onto
  the dialog.
- The dialog shows the newest photo and keeps the others of this time in a
  strip below it; a click there shows one again. A photo that did not turn
  out well goes with the delete button in its corner (or **Del** on the one
  chosen): the file goes to the trash, its marks in the graph and the
  table go too.
- Each photo goes into the folder `photos`, named by the time it came:
  `photos/photo_2026-11-22_14-03-12.jpg`. It is kept as it came, its EXIF
  data too; a photo taken upright is shown upright.
- Each photo sets a mark in the readings table at the time it came, and a
  note **Photo** in the graph used last, if a graph is shown. A click on the
  note's flag shows the photo, so does **Show photo** in the flag's menu.
  Photos taken at once (a series on the phone) share one flag, "3–5 · 3
  photos"; a click on it lists them to choose from. The recording keeps
  the photo's path in the project; the photo is found again as long as the
  project is open.

The button is there only while a project is open. Closing the project
closes the dialog.

The newest photo is also there for programs: `QTDMM:PHOTo:LAST?` and
`QTDMM:PHOTo:DATA?` of the [SCPI server](scpi-server.md), and the tool
`last_photo` of qtdmm-mcp, through which an AI assistant can look at the
setup.

**The phone's connection.** Phone and computer must be in the same
network. QtDMM waits for the phone only while the dialog shows the
**Phone** tab, on one address of the computer: **Network** lists them,
your LAN or WLAN first; pick the one of the network the phone is in. The
link is shown below the code too, to type it in. Each time the dialog
opens the link is a new one with a random key in it, and it stops working
when the dialog closes; without the key QtDMM sends nothing and takes
nothing. Only pictures are taken, up to 25 MB each. It is plain HTTP inside
your network, nothing goes through the internet.

If the phone cannot open the page: a guest WLAN usually keeps its devices
apart from the rest of the network; a firewall on the computer (Windows
asks the first time) has to let QtDMM accept connections; a VPN on the
phone may route the address elsewhere. iPhones send photos as JPEG; a
picture QtDMM cannot read (some HEIC files) is refused with a message on
the phone.

### Folders by kind

The files of a project sort themselves into folders:

| Folder | What goes in |
|---|---|
| `benches` | benches (`.qtdmm-workspace`) |
| `recordings` | recordings (`.qtdmm-rec`), CSV from the graph and the table, any CSV |
| `images` | pictures of the graph and the Poincaré plot, any picture |
| `analysis` | LabPlot projects and their CSV |
| `documents` | PDF, printouts, the log, any other file |
| `photos` | photos of the setup (**Photo of the setup**) |

A folder is made with the first file of its kind, a new project starts
empty. On disk the folders are always named in English, so a project passed
on works in any language; `project.json` keeps their names in each language,
and the window shows them in yours (*Aufzeichnungen* in German). A folder
you rename is a folder of your own from then on; the next file of its kind
makes the English one again.

**Settings → General → Files into folders by type** (on at first) switches
this off: files then go into the project itself. Files already in a project
are never moved, either way.

## Working with the files

| Action | What it does |
|---|---|
| Double click, **Open** | A bench is loaded, a recording of QtDMM (CSV) goes into the graph; any other file opens in the program the system has for it |
| **Open with the system** | Opens a CSV in the spreadsheet or LabPlot instead |
| **Rename** (F2) | Renames the file in the folder |
| **Move to trash** | Moves the selected files to the system's trash, never deletes them for good |
| **New folder** | Makes a folder in the folder clicked on (beside the file clicked on, else at the top) and lets you name it |
| Dragging in the list | Moves files and folders into the folder they are dropped on; dragged out, they go to other programs |
| **Open folder** (the folder at the top) | Shows the project in the file manager |

The list shows the names, newest first; folders open with the arrow or a
double click. `project.json`, QtDMM's own file, is not listed.

## Exporting the project

**Export project as file...** in the menu at the project's name packs the whole
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

**Import project from file...** (in the menu at the project's name, or as a button
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
