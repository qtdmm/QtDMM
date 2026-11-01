//======================================================================
// File:		graphwidget_theme.cpp (from dmmgraph.cpp)
// Author:	Matthias Toussaint
// Created:	Tue Apr 10 17:45:35 CEST 2001
//----------------------------------------------------------------------
// This file is part of QtDMM.
//
// QtDMM is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// QtDMM is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with QtDMM.  If not, see <http://www.gnu.org/licenses/>.
//----------------------------------------------------------------------
// Copyright (c) 2001 Matthias Toussaint
//======================================================================
//
// GraphWidget, part: colours, colour variants and line styles.
// The class is in graphwidget.h, its other parts in graphwidget*.cpp.

#include <QtGui>
#include <QtWidgets>
#include <QPen>

#include "ui/views/graphwidget.h"
#include "ui/views/graphflags.h"
#include "ui/views/graphminimap.h"

void GraphWidget::updateSeriesAppearance()
{
  rebuildExtras();   // their colours and line width
  m_dataLine->setPen(QPen(dataColor(), m_lineWidth, penStyle(m_lineMode), Qt::RoundCap, Qt::RoundJoin));
  m_dataLine->setVisible(m_lineMode != NoLine && m_mainShown);
  updateMinimapValue();
  // the band: the curve's colour, faint
  QColor band = dataColor();
  band.setAlpha(60);
  m_envelope->setBrush(band);

  QScatterSeries::MarkerShape shape = QScatterSeries::MarkerShapeCircle;
  int size = 5;   // small enough to show the curve between them

  switch (m_pointMode)
  {
    case NoPoint:                                                                     break;
    case Circle:      case LargeCircle:  shape = QScatterSeries::MarkerShapeCircle;    break;
    case Square:      case LargeSquare:  shape = QScatterSeries::MarkerShapeRectangle; break;
    // Qt Charts has no "X"/cross marker; approximate both Diamond and X with a
    // rotated square, the closest built-in shape available.
    case Diamond:     case LargeDiamond:
    case X:           case LargeX:       shape = QScatterSeries::MarkerShapeRotatedRectangle; break;
  }
  if (m_pointMode == LargeCircle || m_pointMode == LargeSquare ||
      m_pointMode == LargeDiamond || m_pointMode == LargeX)
    size = 8;

  m_dataPoints->setMarkerShape(shape);
  m_dataPoints->setMarkerSize(size);
  m_dataPoints->setColor(dataColor());
  // Qt Charts rims the markers in white: dense points turn into a white band
  m_dataPoints->setBorderColor(dataColor());
  m_dataPoints->setVisible(m_pointMode != NoPoint && m_mainShown);

  // phosphor: one colour, the integration curve dashed when it is a solid line
  const Qt::PenStyle intStyle = phosphor() && m_intLineMode == Solid ? Qt::DashLine : penStyle(m_intLineMode);
  m_intLine->setPen(QPen(intColor(), m_intLineWidth, intStyle, Qt::RoundCap, Qt::RoundJoin));
  // the main value's integral is on its axis: hidden with it
  const bool integral = m_showIntegration && (m_intValueId != 0 || m_mainShown);
  m_intLine->setVisible(integral && m_intLineMode != NoLine);

  QScatterSeries::MarkerShape intShape = QScatterSeries::MarkerShapeCircle;
  int intSize = 5;

  switch (m_intPointMode)
  {
    case NoPoint:                                                                        break;
    case Circle:      case LargeCircle:  intShape = QScatterSeries::MarkerShapeCircle;    break;
    case Square:      case LargeSquare:  intShape = QScatterSeries::MarkerShapeRectangle; break;
    case Diamond:     case LargeDiamond:
    case X:           case LargeX:       intShape = QScatterSeries::MarkerShapeRotatedRectangle; break;
  }
  if (m_intPointMode == LargeCircle || m_intPointMode == LargeSquare ||
      m_intPointMode == LargeDiamond || m_intPointMode == LargeX)
    intSize = 8;

  m_intPoints->setMarkerShape(intShape);
  m_intPoints->setMarkerSize(intSize);
  m_intPoints->setColor(intColor());
  m_intPoints->setBorderColor(intColor());
  m_intPoints->setVisible(integral && m_intPointMode != NoPoint);
}

// The colour of a value of the meter (its id, not its place: a value keeps
// it when another goes): the next hues of the categorical order after the
// main curve's blue, stepped for a dark background; the variants with a
// curve colour of their own bring theirs. Phosphor keeps its one hue and
// tells the curves apart by dashes (extraPen()).
QColor GraphWidget::extraColor(int id) const
{
  const int slot = qMax(0, id - 1) % 3;
  switch (m_variant)
  {
    case ScopeBlue:     return QColor(QList<const char *>{ "#ff8a5c", "#7dff8a", "#d9a6ff" }[slot]);
    case PhosphorGreen: return QColor::fromHsv(135, 190, 220);
    case PhosphorAmber: return QColor::fromHsv(38, 230, 220);
    case ChartRecorder: return QColor(QList<const char *>{ "#2e7d32", "#7b3fa0", "#b8860b" }[slot]);
    default: break;
  }
  const bool dark = m_xLabelColor.isValid() && m_xLabelColor.lightness() > 128;
  static const char *light[] = { "#eb6834", "#1baf7a", "#6250d6" };
  static const char *darkSteps[] = { "#d95926", "#199e70", "#9085e9" };
  return QColor(dark ? darkSteps[slot] : light[slot]);
}

void GraphWidget::setColors(const QColor &bg, const QColor &grid,
                         const QColor &data, const QColor &cursor,
                         const QColor &start,
                         const QColor &integration, const QColor &intThreshold)
{
  m_bgColor           = bg;
  m_gridColor         = grid;
  m_dataColor         = data;
  m_cursorColor       = cursor;
  m_startColor        = start;
  m_intColor          = integration;
  m_intThresholdColor = intThreshold;

  applyThemeColors();
  updateSeriesAppearance();
}

void GraphWidget::setThemeColors(const QBrush &background, const QColor &grid, const QColor &labels,
                              const QColor &data)
{
  m_themeBackground = background;
  m_themeGrid = grid;
  m_themeLabels = labels;
  m_themeData = data;
  applyThemeColors();
  updateSeriesAppearance();
}

// The curve colour: a colour chosen in the settings stays in every
// variant; the default blue is replaced by the variant's (or the design's).
QColor GraphWidget::dataColor() const
{
  if (m_variant == Custom || m_dataColor != QColor(Qt::blue))
    return m_dataColor;
  switch (m_variant)
  {
    case ScopeBlue:     return QColor("#ffe14d");
    case PhosphorGreen: return QColor::fromHsv(135, 190, 255);
    case PhosphorAmber: return QColor::fromHsv(38, 230, 255);
    case ChartRecorder: return QColor("#1f4e9c");
    default:            return m_themeData.isValid() ? m_themeData : m_dataColor;
  }
}

// The same for the integration curve (default dark blue); phosphor keeps
// one colour and tells the curves apart by brightness and dashes.
QColor GraphWidget::intColor() const
{
  if (m_variant == Custom || m_intColor != QColor(Qt::darkBlue))
    return m_intColor;
  switch (m_variant)
  {
    case ScopeBlue:     return QColor("#4de8ff");
    case PhosphorGreen: return QColor::fromHsv(135, 190, 190);
    case PhosphorAmber: return QColor::fromHsv(38, 230, 190);
    case ChartRecorder: return QColor("#c0282d");
    default:            return m_themeData.isValid() ? m_themeData.lighter(140) : m_intColor;
  }
}

void GraphWidget::setColorVariant(ColorVariant defaultVariant, int variant)
{
  m_defaultVariant = defaultVariant;
  m_variantOverride = variant;
  m_variant = variant >= 0 ? static_cast<ColorVariant>(variant) : defaultVariant;
  applyThemeColors();
  updateSeriesAppearance();
}

QString GraphWidget::variantTitle(ColorVariant variant)
{
  switch (variant)
  {
    case ScopeBlue:     return tr("Scope blue");
    case PhosphorGreen: return tr("Phosphor green");
    case PhosphorAmber: return tr("Phosphor amber");
    case ChartRecorder: return tr("Chart recorder");
    case Custom:        return tr("Custom");
    default:            return tr("Neutral");
  }
}

QString GraphWidget::variantName(ColorVariant variant)
{
  switch (variant)
  {
    case ScopeBlue:     return "scope";
    case PhosphorGreen: return "phosphor-green";
    case PhosphorAmber: return "phosphor-amber";
    case ChartRecorder: return "recorder";
    case Custom:        return "custom";
    default:            return "neutral";
  }
}

GraphWidget::ColorVariant GraphWidget::variantFromName(const QString &name)
{
  for (ColorVariant v : { ScopeBlue, PhosphorGreen, PhosphorAmber, ChartRecorder, Custom })
    if (name == variantName(v))
      return v;
  return Neutral;
}

// Background, grid and lettering. Neutral follows the window design (on
// System the palette), Custom takes the settings page, the others bring
// their own; the scope-like ones also switch to 10 x 8 divisions.
void GraphWidget::applyThemeColors()
{
  QBrush background, plot(Qt::NoBrush);
  QPen grid, minor, line;
  QColor labels;
  int minors = 0;
  auto vgrad = [](const QColor &top, const QColor &bottom)
  {
    QLinearGradient g(0, 0, 0, 1);
    g.setCoordinateMode(QGradient::ObjectBoundingMode);
    g.setColorAt(0, top);
    g.setColorAt(1, bottom);
    return QBrush(g);
  };
  switch (m_variant)
  {
    case ScopeBlue:
      background = QColor("#0b1633");
      plot = vgrad(QColor("#16306a"), QColor("#081338"));
      grid = QPen(QColor(90, 130, 200, 80), 1);   // solid and faint: dashes are the curves'
      line = QPen(QColor("#5a78b0"));
      labels = QColor("#b9c9e8");
      break;
    case PhosphorGreen:
    case PhosphorAmber:
    {
      const bool green = m_variant == PhosphorGreen;
      background = green ? QColor("#0a0d0a") : QColor("#0d0b08");
      plot = green ? QColor("#060d08") : QColor("#0e0a04");
      grid = QPen(green ? QColor(60, 150, 85, 110) : QColor(190, 120, 30, 110), 1);
      line = grid;
      labels = green ? QColor("#5fbf78") : QColor("#d99a2b");
      m_centreTicks->setPen(QPen(green ? QColor(80, 190, 110, 150) : QColor(220, 150, 50, 150), 1));
      break;
    }
    case ChartRecorder:
      background = QColor("#f4eedb");
      plot = QColor("#fbf6e6");
      grid = QPen(QColor(214, 140, 130, 190), 1);
      minor = QPen(QColor(226, 176, 166, 110), 0.6);
      line = QPen(QColor(190, 120, 110));
      labels = QColor("#5a4636");
      minors = 4;   // millimetre-ish paper
      break;
    case Custom:
      background = m_bgColor;
      grid = QPen(m_gridColor);
      line = QPen(m_defaultAxisLine);
      // lettering that reads on the chosen background
      labels = m_bgColor.lightness() < 128 ? QColor(220, 220, 220) : QColor(30, 30, 30);
      break;
    case Neutral:
      if (m_themeBackground.style() != Qt::NoBrush)
      {
        background = m_themeBackground;
        grid = QPen(m_themeGrid);
        labels = m_themeLabels;
      }
      else
      {
        // the System design: the palette, as the table and the dialogs
        const QPalette pal = palette();
        background = pal.base();
        grid = QPen(pal.color(QPalette::Mid));
        labels = pal.color(QPalette::Text);
      }
      line = QPen(labels);
      break;
  }
  m_chart->setBackgroundBrush(background);
  m_minimap->setColors(plot.style() != Qt::NoBrush ? plot : background, labels);
  // the reference line: the lettering, half strong - above the grid, below
  // the curve
  m_referenceLine->setPen(QPen(QColor(labels.red(), labels.green(), labels.blue(), 150), 1));
  m_referenceLabel->setBrush(labels);
  // the scrollbar in the graph's colours: a dark graph gets no light strip
  {
    QPalette pal = scrollbar->palette();
    for (QPalette::ColorRole role : { QPalette::Window, QPalette::Base, QPalette::Button })
      pal.setBrush(role, background);
    pal.setColor(QPalette::WindowText, labels);
    pal.setColor(QPalette::ButtonText, labels);
    scrollbar->setPalette(pal);
    scrollbar->setAutoFillBackground(true);
  }
  // the time buttons in the lettering colour, the chosen one framed
  if (m_timeBar)
    m_timeBar->setStyleSheet(QString("QToolButton { color: %1; border: 1px solid transparent; border-radius: 2px;"
                                     " padding: 0px 4px; background: transparent; }"
                                     " QToolButton:checked { border-color: %1; }"
                                     " QToolButton:hover { border-color: %2; }")
                               .arg(labels.name(), QColor(labels.red(), labels.green(), labels.blue(), 110).name(QColor::HexArgb)));
  if (m_cursorLabel)
    m_cursorLabel->setStyleSheet(QString("QLabel { color: %1; background: transparent; }").arg(labels.name()));
  m_stateColor = labels;
  updateStateLabel();
  m_chart->setPlotAreaBackgroundBrush(plot);
  m_chart->setPlotAreaBackgroundVisible(plot.style() != Qt::NoBrush);
  for (SideAxis &side : m_right)
  {
    side.axis->setLabelsBrush(labels);
    side.axis->setLinePen(line);
  }
  updateRightAxis();   // the titles: the colours of their curves
  for (QValueAxis *axis : { m_xAxis, m_yAxis })
  {
    axis->setGridLinePen(grid);
    axis->setMinorTickCount(minors);
    axis->setMinorGridLineVisible(minors > 0);
    if (minors > 0)
      axis->setMinorGridLinePen(minor);
    axis->setLabelsBrush(labels);
    axis->setTitleBrush(labels);
    axis->setLinePen(line);
  }
  // 10 x 8 divisions for the scope-like variants, Qt's default otherwise;
  // their round steps need no forced decimals. The x ticks are time steps
  // (updateXAxisRange()), labelled by updateXLabels(): Qt's own x labels
  // only keep the room for them.
  m_yAxis->setTickCount(divisions() ? 9 : 5);
  m_xAxis->setLabelFormat("%.4g");
  m_yAxis->setLabelFormat("%.4g");   // invisible, only keeps the room
  m_xLabelColor = labels;
  m_xAxis->setLabelsBrush(Qt::transparent);
  m_yAxis->setLabelsBrush(Qt::transparent);
  updateYLabels();
  m_yTitle->setBrush(labels);

  // cursor and threshold lines: a colour chosen in the settings stays,
  // the defaults follow the variant (a black cursor on phosphor is lost)
  const bool own = m_variant == Custom;
  auto pick = [own](const QColor &setting, Qt::GlobalColor def, const QColor &variant)
  {
    return own || setting != QColor(def) || !variant.isValid() ? setting : variant;
  };
  QColor start;
  switch (m_variant)
  {
    case ScopeBlue:     start = QColor("#ff5fd2"); break;
    case PhosphorGreen: start = QColor::fromHsv(135, 190, 160); break;
    case PhosphorAmber: start = QColor::fromHsv(38, 230, 160); break;
    case ChartRecorder: start = QColor("#2e7d32"); break;
    default: break;
  }
  const Qt::PenStyle lineStyle = phosphor() ? Qt::DashLine : Qt::SolidLine;
  const QColor cursor = pick(m_cursorColor, Qt::black, labels);
  m_crosshairVLine->setPen(QPen(cursor));
  m_crosshairHLine->setPen(QPen(cursor));
  for (int i = 0; i < 2; ++i)
  {
    m_cursorLine[i]->setPen(QPen(cursor, 1, Qt::DashLine));
    m_cursorLevel[i]->setPen(QPen(cursor, 1, Qt::DashLine));
    m_cursorName[i]->setColor(cursor);
  }
  if (m_cursorInfo)
  {
    const QColor bg = background.color().isValid() ? background.color() : palette().color(QPalette::Base);
    m_cursorInfo->setStyleSheet(QString("QLabel { color: %1; background: %2; border: 1px solid %3; padding: 2px 4px; }")
                                  .arg(labels.name(), QColor(bg.red(), bg.green(), bg.blue(), 220).name(QColor::HexArgb),
                                       QColor(labels.red(), labels.green(), labels.blue(), 110).name(QColor::HexArgb)));
  }
  m_triggerLine->setPen(QPen(pick(m_startColor, Qt::magenta, start), 1, lineStyle));
  // dotted: by default it takes the integration curve's colour and must not
  // pass for a curve (phosphor dashes the curve)
  m_integrationLine->setPen(QPen(pick(m_intThresholdColor, Qt::darkBlue, intColor()), 1, Qt::DotLine));
  updateXAxisRange();
  setYRange(m_scaleMin, m_scaleMax);
  updateThresholdLinePositions();
}

void GraphWidget::setLineStyle(int lineMode, int pointMode, int intLineMode, int intPointMode)
{
  const bool points = pointsShown(), intPoints = intPointsShown();
  m_lineMode = static_cast<LineMode>(lineMode);
  m_pointMode = static_cast<PointMode>(pointMode);
  m_intLineMode = static_cast<LineMode>(intLineMode);
  m_intPointMode = static_cast<PointMode>(intPointMode);

  // hidden points are not kept up to date
  if (points != pointsShown() || intPoints != intPointsShown())
    rebuildSeries();
  updateSeriesAppearance();
}

void GraphWidget::setLine(int d, int i)
{
  m_lineWidth    = d;
  m_intLineWidth = i;

  updateSeriesAppearance();
}

Qt::PenStyle GraphWidget::penStyle(LineMode mode)
{
  switch (mode)
  {
    case NoLine:
      return Qt::NoPen;
    case Solid:
      return Qt::SolidLine;
    case Dot:
      return Qt::DotLine;
  }
  return Qt::SolidLine;
}
