# Heatmap

The heatmap places one measured value on the X axis and another on the Y
axis. A third value sets the colour of each cell. This makes it possible to
see how a quantity changes across two other measurements, for example a
sensor reading as temperature and humidity vary.

Use **Heatmap** in the toolbar or menu to show or hide the heatmap windows.
**New...** in the menu beside the toolbar button opens another heatmap.
**Sources...** in a heatmap's right-click menu assigns the X, Y and colour
values from any connected meter. The legend above the plot names each source.
The axis labels show the units selected by the sources, with SI prefixes
chosen to fit the displayed range.

A heatmap point is added when all three values are available. Values from the
same meter are combined only when they belong to the same reading; values
from different meters are paired with the latest reading received from each
source. Overloads and readings without numbers are skipped. The plot bins the
last 5000 points into a 32 by 32 grid by default; cells with multiple points
show their average colour value. The context menu's **Color scheme** submenu
changes the palette. Its **No data** swatch shows the colour used for cells
that have not received a point.

## The context menu

- **Hold picture** - freeze the current heatmap while new points continue to
  collect; the **HOLD** mark appears in the plot. Releasing Hold displays the
  accumulated points.
- **Reset** - clear the heatmap.
- **Save image...** - save the visible heatmap as a PNG image.

Right-click **Sources...** also lets you assign different source values. Each
heatmap window keeps its own source selection.
