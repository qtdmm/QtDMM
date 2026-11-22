// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/graphlook.h"

#include <QCoreApplication>
#include <QLinearGradient>

QList<GraphLook::Variant> GraphLook::variants()
{
  return { Neutral, ScopeBlue, PhosphorGreen, PhosphorAmber, ChartRecorder, ColourBlind, HighContrast, Custom };
}

QString GraphLook::name(Variant variant)
{
  switch (variant)
  {
    case ScopeBlue:     return "scope";
    case PhosphorGreen: return "phosphor-green";
    case PhosphorAmber: return "phosphor-amber";
    case ChartRecorder: return "recorder";
    case ColourBlind:   return "colour-blind";
    case HighContrast:  return "high-contrast";
    case Custom:        return "custom";
    default:            return "neutral";
  }
}

GraphLook::Variant GraphLook::fromName(const QString &name)
{
  for (Variant v : variants())
    if (name == GraphLook::name(v))
      return v;
  return Neutral;
}

// the texts are GraphWidget's, translated there
QString GraphLook::title(Variant variant)
{
  const char *text = "Neutral";
  switch (variant)
  {
    case ScopeBlue:     text = "Scope blue"; break;
    case PhosphorGreen: text = "Phosphor green"; break;
    case PhosphorAmber: text = "Phosphor amber"; break;
    case ChartRecorder: text = "Chart recorder"; break;
    case ColourBlind:   text = "Colour-blind safe"; break;
    case HighContrast:  text = "High contrast"; break;
    case Custom:        text = "Custom"; break;
    default: break;
  }
  return QCoreApplication::translate("GraphWidget", text);
}

static QList<QColor> colors(std::initializer_list<const char *> names)
{
  QList<QColor> out;
  for (const char *name : names)
    out << QColor(name);
  return out;
}

GraphLook::Look GraphLook::look(Variant variant, const PlotTheme &design, const Settings &settings)
{
  Look l;
  l.variant = variant;
  l.lineMode = settings.lineMode;
  l.pointMode = settings.pointMode;
  l.lineWidth = variant == HighContrast ? settings.lineWidth * 1.5 : settings.lineWidth;
  l.divisions = variant == ScopeBlue || variant == PhosphorGreen || variant == PhosphorAmber || variant == ChartRecorder;
  l.oneHue = variant == PhosphorGreen || variant == PhosphorAmber;
  auto vgrad = [](const QColor &top, const QColor &bottom)
  {
    QLinearGradient g(0, 0, 0, 1);
    g.setCoordinateMode(QGradient::ObjectBoundingMode);
    g.setColorAt(0, top);
    g.setColorAt(1, bottom);
    return QBrush(g);
  };
  QBrush background;
  QColor labels, data;
  l.plot = QBrush(Qt::NoBrush);
  l.minorGrid = QPen(Qt::NoPen);
  l.centreTicks = QPen(Qt::NoPen);
  switch (variant)
  {
    case ScopeBlue:
      background = QColor("#0b1633");
      l.plot = vgrad(QColor("#16306a"), QColor("#081338"));
      l.grid = QPen(QColor(90, 130, 200, 80), 1);   // solid and faint: dashes are the curves'
      l.axisLine = QPen(QColor("#5a78b0"));
      labels = QColor("#b9c9e8");
      data = QColor("#ffe14d");
      // a meter's other values, then other meters' curves
      l.valueHues = colors({ "#ff8a5c", "#7dff8a", "#d9a6ff" });
      l.meterHues = colors({ "#5ce1e6", "#ff7eb6", "#e8e8e8", "#c6ff5c" });
      break;
    case PhosphorGreen:
    case PhosphorAmber:
    {
      const bool green = variant == PhosphorGreen;
      background = green ? QColor("#0a0d0a") : QColor("#0d0b08");
      l.plot = green ? QColor("#060d08") : QColor("#0e0a04");
      l.grid = QPen(green ? QColor(60, 150, 85, 110) : QColor(190, 120, 30, 110), 1);
      l.axisLine = l.grid;
      labels = green ? QColor("#5fbf78") : QColor("#d99a2b");
      l.centreTicks = QPen(green ? QColor(80, 190, 110, 150) : QColor(220, 150, 50, 150), 1);
      data = green ? QColor::fromHsv(135, 190, 255) : QColor::fromHsv(38, 230, 255);
      l.valueHues = { green ? QColor::fromHsv(135, 190, 220) : QColor::fromHsv(38, 230, 220) };
      l.meterHues = l.valueHues;
      break;
    }
    case ChartRecorder:
      background = QColor("#f4eedb");
      l.plot = QColor("#fbf6e6");
      l.grid = QPen(QColor(214, 140, 130, 190), 1);
      l.minorGrid = QPen(QColor(226, 176, 166, 110), 0.6);   // millimetre-ish paper
      l.axisLine = QPen(QColor(190, 120, 110));
      labels = QColor("#5a4636");
      data = QColor("#1f4e9c");
      l.valueHues = colors({ "#2e7d32", "#7b3fa0", "#b8860b" });
      l.meterHues = colors({ "#1565c0", "#ad1457", "#00838f", "#6d4c41" });
      break;
    case ColourBlind:
      background = QColor(Qt::white);
      l.grid = QPen(QColor("#c8c8c8"), 1);
      l.axisLine = QPen(QColor("#1a1a1a"));
      labels = QColor("#1a1a1a");
      data = QColor("#0072b2");
      l.valueHues = colors({ "#d55e00", "#009e73", "#e69f00" });
      l.meterHues = colors({ "#cc79a7", "#56b4e9", "#000000" });
      break;
    case HighContrast:
      // the grid solid and plain grey: dashes are the curves'
      background = QColor(Qt::black);
      l.grid = QPen(QColor("#9a9a9a"), 1);
      l.axisLine = QPen(QColor(Qt::white), 2);
      labels = QColor(Qt::white);
      data = QColor(Qt::white);
      l.valueHues = colors({ "#ffff00", "#ff00ff", "#00ff00" });
      l.meterHues = colors({ "#ff8000", "#80b0ff", "#ff6060", "#c0c0c0" });
      break;
    case Custom:
      background = settings.background;
      l.grid = QPen(settings.grid);
      // lettering that reads on the chosen background
      labels = settings.background.lightness() < 128 ? QColor(220, 220, 220) : QColor(30, 30, 30);
      l.axisLine = QPen(labels);
      data = settings.data;
      break;
    case Neutral:
      background = design.background;
      l.grid = QPen(design.grid);
      labels = design.labels;
      l.axisLine = QPen(labels);
      data = design.data.isValid() ? design.data : design.labels;
      break;
  }
  if (l.valueHues.isEmpty())
  {
    // Neutral and Custom: the hues of the categorical order after blue, stepped for a dark background
    const bool dark = labels.isValid() && labels.lightness() > 128;
    l.valueHues = dark ? colors({ "#d95926", "#199e70", "#9085e9" }) : colors({ "#eb6834", "#1baf7a", "#6250d6" });
    l.meterHues = dark ? colors({ "#5b8def", "#e0568f", "#26a69a", "#b39060" })
                       : colors({ "#2f6fde", "#c2185b", "#00897b", "#8a6d3b" });
  }
  // a data colour chosen in the settings goes for every variant
  if (variant == Custom || settings.data != QColor(Qt::blue))
    data = settings.data;
  l.theme = PlotTheme{ background, l.grid.color(), labels, data };
  l.cursor = settings.cursor != QColor(Qt::black) ? settings.cursor : labels;
  // the start threshold per variant
  QColor trigger = Qt::magenta;
  switch (variant)
  {
    case ScopeBlue:     trigger = QColor("#ff5fd2"); break;
    case PhosphorGreen: trigger = QColor::fromHsv(135, 190, 160); break;
    case PhosphorAmber: trigger = QColor::fromHsv(38, 230, 160); break;
    case ChartRecorder: trigger = QColor("#2e7d32"); break;
    case ColourBlind:   trigger = QColor("#cc79a7"); break;
    case HighContrast:  trigger = QColor("#ff8000"); break;
    default: break;
  }
  l.trigger = settings.trigger != QColor(Qt::magenta) ? settings.trigger : trigger;
  return l;
}

QColor GraphLook::curveColor(const Look &look, const CurveKey &key)
{
  if (key.slot <= 0)
    return look.theme.data;
  if (key.otherMeter >= 0)
    return look.meterHues.at(key.otherMeter % look.meterHues.size());
  return look.valueHues.at(qMax(0, key.valueId - 1) % look.valueHues.size());
}

QPen GraphLook::curvePen(const Look &look, const CurveKey &key, bool patterns)
{
  if (look.lineMode == 0)
    return QPen(Qt::NoPen);
  if (key.slot <= 0)
    return QPen(curveColor(look, key), look.lineWidth, look.lineMode == 2 ? Qt::DotLine : Qt::SolidLine, Qt::RoundCap,
                Qt::RoundJoin);
  const int id = key.valueId;
  const Qt::PenStyle style = id <= 0 ? Qt::SolidLine : id == 1 ? Qt::DashLine : id == 2 ? Qt::DotLine : Qt::DashDotLine;
  QPen pen(curveColor(look, key), qMax(1.0, look.lineWidth * 0.75), style, Qt::RoundCap, Qt::RoundJoin);
  if (patterns || look.oneHue)
  {
    static const QList<QList<qreal>> dashes = {
      { 10, 4 },                 // long dashes
      { 6, 3, 1, 3, 1, 3 },      // dash dot dot
      { 1, 4 },                  // sparse dots
      { 12, 3, 4, 3 },           // long short
    };
    pen.setDashPattern(dashes[(key.slot - 1) % dashes.size()]);
  }
  return pen;
}
