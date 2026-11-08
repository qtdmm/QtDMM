# Project basket

The project basket keeps the files that belong to a bench: recordings,
pictures of the graph, data sheets as PDF, LabPlot projects, notes - any
file. Switch it on with **Project basket** in the menu (Ctrl+6); it opens
as a window next to the other views.

## A folder next to the bench

The basket is a plain folder beside the bench file, with the bench's name:

```
Power supply.qtdmm-workspace
Power supply/
    2026-11-07 ripple.csv
    ripple.png
    datasheet.pdf
```

Move or copy both together and the basket goes along. The folder can be
used in a file manager as well; the window always shows what is in it.

A basket needs a saved bench. Without one the window offers **Save
bench...** (see [Benches](connecting.md#window-layout)). The folder is made
when the first file goes in.

## Putting files in

Drop files onto the window, or choose **Add...** (the + at the top). Files are copied, the
originals stay where they are. A file is never overwritten: a second
`ripple.csv` becomes `ripple (2).csv`.

## Working with the files

| Action | What it does |
|---|---|
| Double click, **Open** | A recording of QtDMM (CSV) loads into the graph; any other file opens in the program the system has for it |
| **Open with the system** | Opens a CSV in the spreadsheet or LabPlot instead |
| **Rename** (F2) | Renames the file in the folder |
| **Move to trash** | Moves the selected files to the system's trash, never deletes them for good |
| **Open folder** (the folder at the top) | Shows the basket in the file manager |

The list shows the names, newest first.
