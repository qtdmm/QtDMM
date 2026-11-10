# Keyboard and mouse

Every toolbar button shows its shortcut in the tooltip. Shortcuts of the
main window work while it is the active window, whichever of its windows
has the focus. Ctrl+drag moves a window, also one without title bar: in
*Free* window layout anywhere, in the arranged layouts onto another window
to swap the two. There, the gaps between the windows can be dragged to
share the space differently.

## Your own keys

Settings, **Keyboard** lists the commands of the main window with their
key and the default. Select a command, click into **Key** and press the new
key; **None** takes the key away, **Default** gives the command its own key
back, **All to default** does so for all. The search field finds a command
by its name or its key. Changed keys are shown in bold.

A key another command already has is taken from it after a question. Esc,
Tab, Return and Alt with a letter or a digit stay with the window: they
move through buttons, menus and dialogs. The keys are the same for every
`--config-id` (the file `shortcuts.conf` next to the settings); another
QtDMM that runs takes over a change at once. The tables below show the
defaults.

The keys of the graph, the readings table, the settings dialog and the
handbook window are fixed.

## Main window

| Key | Action |
|---|---|
| F3 | Connect / disconnect all devices of the bench |
| Space | Record: start the recorder, or stop it when it is running |
| Ctrl+Del | Clear the recording |
| F4 | Reset the min/max memory of all devices |
| Ctrl+E | Export dialog: recording, readings table or picture, format and place |
| Ctrl+I | Import a CSV file |
| Ctrl+P | Print the graph |
| Ctrl+Shift+C | Copy the graph as an image to the clipboard |
| Ctrl+Shift+N | A note in the graph at the newest reading |
| F8 | Settings |
| Shift+F8 | Settings of the device chosen in the sidebar |
| Ctrl+F8 | Settings, Recording page |
| F9 | Show or hide the sidebar with My devices and the open devices |
| F7 | Open the menu |
| F1 | This handbook |
| Shift+F1 | Direct help — then click any control for an explanation |
| Ctrl+Q | Quit |

## Windows

| Key | Action |
|---|---|
| Ctrl+1 | Show / hide the front panels (digital displays) of all devices |
| Ctrl+2 | Show / hide the needle meter |
| Ctrl+3 | Show / hide the graph |
| Ctrl+4 | Show / hide the readings table |
| Ctrl+5 | Show / hide the Poincaré plot |
| Ctrl+6 | Show / hide the project |
| F10 | Hide / show the title bars of the windows |
| Ctrl+M | Hide / show the toolbar |
| F11 | Full screen on / off |

## Graph

The zoom shortcuts work anywhere in the main window; the plain keys
(`+`, `-`, `0`, arrows, Home, End, N) after a click into the graph.

| Input | Effect |
|---|---|
| Ctrl++ / Ctrl+- or + / - | Zoom the time axis in / out |
| Ctrl+0 or 0 | Show the whole recording |
| ← / → | Pan by a tenth of the visible window; with Shift by half |
| PgUp / PgDn | Pan by a whole window |
| Home / End | Jump to the start / end of the recording |
| Mouse wheel | Zoom the time axis |
| Middle button drag | Pan the time axis |
| Hover | Crosshair with time and value |
| Left button drag on a threshold line | Move the threshold |
| Left button drag on the reference line | Move it; it snaps to 0, the mean, minimum and maximum shown and the alarm thresholds |
| Click / Shift+click | Time cursor A / B: time and value, with both the span, the change, the mean and the integral |
| Left button drag on a time cursor | Move it |
| Shift + left button drag | Zoom on the frame: its time and the height on every axis; **Reset zoom** in the menu goes back (Esc while dragging: no zoom) |
| Esc | Remove the time cursors |
| N | A note at the newest reading |
| Click on a flag | Its title, time and text |
| Double click on a flag | Edit its title and text |
| Drag in the overview | Move the window; while live it pauses there, a click at the right end follows again |
| Drag a bracket of the overview | Widen or narrow the window |
| Right button, Shift+F10 or the menu key | Menu: connect, start/stop, Live, reset zoom, clear, note here or now, cursor A or B here, configure, copy image, colours, View (statistics, overview, min/max band, reference line, buttons), axes, time cursors, import/export; on a flag: edit note, remove mark. From the keyboard it opens where the mouse is over the graph, else in its middle |

## Readings table

While the table has the focus:

| Key | Action |
|---|---|
| Ctrl+C | Copy the selected rows (all when nothing is selected) as tab-separated text |
| Ctrl+A | Select all rows |

## Settings dialog

| Key | Action |
|---|---|
| Ctrl+PgUp / Ctrl+PgDn | Previous / next page |
| Alt+R | Reset the current page to the defaults |
| Enter / Esc | OK / Cancel |

## Handbook window

| Key | Action |
|---|---|
| Alt+← / Alt+→ | Back / forward |
| Alt+Home | Table of contents |
| Ctrl+F | Search in the page |
| Enter or F3 | Next match, Shift+Enter or Shift+F3 the previous one |
| Esc | Close |

## Sidebar

F8 opens the settings here too; F2 renames the selected device (also
through its context menu, **Rename**). Enter on a bench
loads it.
