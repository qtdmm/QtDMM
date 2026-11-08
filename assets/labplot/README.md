# LabPlot project templates

Parts of a LabPlot project (`.lml`, LabPlot 2.12, xmlVersion 16) that
`LabPlotProject` (src/recording/labplotproject.cpp) fills in: a plot area
with its four axes (`plot.xml`), an xy curve (`curve.xml`) and a legend
(`legend.xml`). They are LabPlot's own default look, saved by LabPlot and
cut apart; `${NAME}` and the like are filled in, the values XML escaped.

LabPlot reads every attribute it misses as 0, not as its default, so the
parts keep all of them. A new LabPlot version reads these (older xmlVersion)
as it reads older projects.
