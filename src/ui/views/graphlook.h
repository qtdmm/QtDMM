// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QBrush>
#include <QList>
#include <QPen>
#include <QString>

#include "ui/views/plottheme.h"

/// How a graph of a recording looks: its colour variant - the screen of an
/// oscilloscope, a phosphor tube, a chart recorder's paper - turned into
/// the colours and pens a graph draws with. No drawing library: the tables
/// are those of the graph on QtCharts (GraphWidget::applyThemeColors(),
/// dataColor(), extraColor(), otherColor()), for the graph on QCustomPlot.
namespace GraphLook
{
enum Variant
{
  Neutral,         ///< follows the window design
  ScopeBlue,       ///< oscilloscope, blue screen
  PhosphorGreen,   ///< green phosphor tube: one colour, 10 x 8 divisions
  PhosphorAmber,   ///< the same in amber
  ChartRecorder,   ///< paper and ink
  ColourBlind,     ///< white paper, the Okabe-Ito hues
  HighContrast,    ///< black, white and pure hues, thicker lines
  Custom           ///< the colours of the settings page
};
QList<Variant> variants();
/// The name in the settings ("scope", "phosphor-green"); fromName() of
/// anything else is Neutral.
QString name(Variant variant);
Variant fromName(const QString &name);
/// The name in menus, translated.
QString title(Variant variant);

/// The settings page: its colours (for Custom; a data colour other than
/// the default blue goes for every variant), line, points and width.
struct Settings
{
  QColor background;
  QColor grid;
  QColor data = Qt::blue;
  int    lineMode = 1;      ///< 0 none, 1 solid, 2 dotted
  int    pointMode = 0;     ///< 0 none, 1 circle, 2 square, 3 diamond, 4 x, 5..8 the same large
  int    lineWidth = 2;
};

/// What a graph draws with.
struct Look
{
  Variant   variant = Neutral;
  PlotTheme theme;            ///< around the plot: background, grid colour, lettering, the first curve
  QBrush    plot;             ///< the plot's area; Qt::NoBrush: the background
  QPen      grid;
  QPen      minorGrid;        ///< Qt::NoPen: none (the recorder's paper has one)
  QPen      axisLine;
  QPen      centreTicks;      ///< Qt::NoPen: none (the phosphor tubes' fine ticks on the centre lines)
  bool      divisions = false;   ///< 10 x 8 divisions, the axes on whole 1-2-5 steps
  bool      oneHue = false;      ///< phosphor: the curves differ by their dashes, not their colour
  QList<QColor> valueHues;    ///< of the other values of the first curve's meter, by their id
  QList<QColor> meterHues;    ///< of the curves of other meters, in their order
  double    lineWidth = 2;
  int       lineMode = 1;
  int       pointMode = 0;
};

/// The look of @p variant on the window design @p design with the settings @p settings.
Look look(Variant variant, const PlotTheme &design, const Settings &settings);

/// Which curve of a graph: its place among the graph's values (0: the
/// first), the value it is of its meter (0: the main one), and - a curve
/// of another meter than the first curve's - which of those (0, 1, ...; -1:
/// the first curve's meter).
struct CurveKey
{
  int slot = 0;
  int valueId = 0;
  int otherMeter = -1;
};
/// The curve's colour: the first one the data colour, another value of
/// its meter a hue by its id, another meter's curve a hue by its turn (the
/// line style still says which of its values).
QColor curveColor(const Look &look, const CurveKey &key);
/// Its pen: the first curve in the settings' line and width, the others
/// thinner and dashed by the value they are of their meter (1 dashes, 2
/// dots, more dash-dots) - with @p patterns, and always where the colour
/// does not tell them apart, by a dash pattern of their place. Qt::NoPen:
/// no line.
QPen curvePen(const Look &look, const CurveKey &key, bool patterns);
}
