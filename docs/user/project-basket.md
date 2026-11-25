# Project

A project keeps its files in one folder: its benches,
recordings, pictures of the graph, data sheets as PDF, LabPlot projects,
notes - any file. One project can have several benches, one for each
measuring set-up.

There is always a project. Until you make or open one of your own it is
the **default project**: the folder `default` in the folder for new
projects (shown as *Default*). It is made when it is first needed - the
first bench saved, the first file into the project - and made anew,
empty, when it was deleted or its `project.qtdmm` is broken (that one
stays beside as `project.qtdmm.broken`, the other files stay). As long as
it is the only project, QtDMM does not speak of projects at all: the
start page, the sidebar and the window title show the benches only. The
project's name appears once a second project was made or opened.

Switch the window on with **Project** in the menu (Ctrl+6).

## Making or opening a project

The project's name at the top of the window has **New project...**,
**Open project...** and **Close project**, which goes back to the default
project (the default project itself is not closed). New projects are made
in the folder set under **Settings → General → Folder for new projects**
(Documents/QtDMM at first).

The second button of the toolbar shows and hides the project's window;
its arrow has **New project...**, **Open project...** (its
`project.qtdmm` or a packed project), the bench entries (once there is a
device), the projects opened last (the open one ticked) and **Close
project**.

The start page has **Open project...** below *Add device* (a
`project.qtdmm` or a packed project), the benches of the open project and
the other projects opened last (up to five; a click opens one).

**Switching the project** - open, one of the last, close - closes the
bench of the project left: the question to save it comes only when it
changed, the outputs of supplies and generators go off first. Then the
project you open loads a bench of its own: the one used last in it (kept
in `project.qtdmm`), else its only one. With more benches and none used
last the start page lists them, a click loads one; without any it says
so. **New project...** with a bench running asks whether to **move** the
bench into the new project (the default), not to move it (then it closes
as above), or to cancel. The project's window does not open by itself; a
new project made with the project button shows it. A window not yet
placed goes to the right, like the readings table. The list leaves out
`project.qtdmm`, the file in which QtDMM keeps the project's name,
description and the bench used last.

With devices but none connected, the project's window stands at the
right beside the start page. The project's node in the sidebar shows once
there is a device and a project of your own.

The window title shows the project and the bench, the bench only once
it lies in the project's folder.

The project stays open until you open or close another, also across
restarts.

While a project is open, the sidebar shows **Project: name** at the top
with its benches (the one in use in bold; a double click loads one) and
**+ New project...**, which makes another project and opens it. To save
the devices and windows as they are as a new bench of the project use
**Save bench as...** (menu of the node Bench).

**Saving** a bench always puts it into the open project (its `benches`
folder); *Save bench* (Ctrl+S) of a bench whose file lies outside asks
where in the project, it never writes outside unasked.

**Move bench to project...** (in the menu of the node Bench and at Recent
benches in the sidebar, at the project button's arrow) moves the bench
into a new project, one of the last ones or another: its file goes into
that project's benches - no copy stays behind - and that project opens
with the bench running. A bench not saved yet goes along and is saved
there.

A bench belongs to the project in whose folder it lies, also in a
subfolder of it. Loading a bench that lies outside the open project -
saved anywhere, or in another project - asks: **Copy** it into the project
(the copy is loaded, the file stays where it is), **Only open** it where it
lies (*Save bench* then proposes the project), or cancel. The project
never switches by itself.

A project is a plain folder. QtDMM puts a `project.qtdmm` into a new one,
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
on works in any language; `project.qtdmm` keeps their names in each language,
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
double click. `project.qtdmm`, QtDMM's own file, is not listed.

## Exporting the project

**Export project as file...** in the menu at the project's name packs the whole
project into one file, to pass it on or to keep it. The file is
offered next to the project, `<name>.qtdmm-project`. It is a ZIP: inside,
everything lies in a folder with the project's name, so unpacking it (also
with any ZIP program) gives the project back.

The `project.qtdmm` in the file gets an `export` part: the date, the QtDMM
version and every file with its size and SHA-256 checksum, to check that
nothing went missing or changed on the way. The `project.qtdmm` in the
project itself stays as it is.

Hidden files and folders (names beginning with a dot, such as `.git`) stay
out, as does the file itself when it is saved into the project. It holds
at most 4 GB.

## Opening a packed project

**Open project...** takes a project's `project.qtdmm` or a packed project
(`.qtdmm-project`, see above): the dialog shows both. A packed one is unpacked into a
new project in the folder for new projects and opened; the status line
says where. It is named like the folder in it. When a project of that
name is there already, QtDMM asks: **Open that one**, **Unpack as copy**
(`Bench 2`) or **Cancel** - a project that is there is never written into.

Before anything is unpacked, QtDMM checks the file: every file the `export`
part of its `project.qtdmm` lists must be there with the same size and
SHA-256 checksum, and no file may lie outside its folder (a path with
`..`). If something is missing or changed, nothing is written and the
message names the files. The `export` part goes again; the project's
`project.qtdmm` is as it was before the export.

A ZIP without a `project.qtdmm` is no project: QtDMM says so and unpacks
nothing. One with `project.qtdmm` at the top, without a folder around, is
named like the ZIP file.

Projects made before 27.1 have `project.json` instead of `project.qtdmm`;
QtDMM reads it and renames it the next time it writes the file. Their
exports (`.zip`) open as well.
