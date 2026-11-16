// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// QcpGraph: the look - colour variant, pens and markers of the curves, the
// colours of the axes.
#include "ui/views/qcpgraph.h"

#include "recording/recordingstore.h"
#include "ui/views/graphtimebar.h"
#include "ui/views/qcp.h"

#include <QScrollBar>
#include <cmath>

namespace
{
/// The fine ticks on the centre lines of a tube's graticule: five per division.
class CentreTicks : public QCPAbstractItem
{
public:
  explicit CentreTicks(QCustomPlot *plot) : QCPAbstractItem(plot) {}
  void setPen(const QPen &pen) { m_pen = pen; }
  double selectTest(const QPointF &, bool, QVariant *) const override { return -1; }

protected:
  void draw(QCPPainter *painter) override
  {
    if (m_pen.style() == Qt::NoPen)
      return;
    const QRectF r = parentPlot()->axisRect()->rect();
    const double cx = r.center().x(), cy = r.center().y();
    painter->setPen(m_pen);
    for (int i = 1; i < 50; ++i)
      if (const double x = r.left() + r.width() * i / 50; i % 5)
        painter->drawLine(QLineF(x, cy - 3, x, cy + 3));
    for (int i = 1; i < 40; ++i)
      if (const double y = r.top() + r.height() * i / 40; i % 5)
        painter->drawLine(QLineF(cx - 3, y, cx + 3, y));
  }

private:
  QPen m_pen{ Qt::NoPen };
};
}

void QcpGraph::setupTheme()
{
  auto *ticks = new CentreTicks(m_plot);
  ticks->setLayer(QStringLiteral("grid"));
  ticks->setSelectable(false);
  m_centreTicks = ticks;
  // lines wider than a pixel stroke by stroke: a tenth of the time and
  // less, the joins a little rougher
  m_plot->setPlottingHint(QCP::phFastPolylines, true);
  applyLook();
}

void QcpGraph::setTheme(const PlotTheme &theme)
{
  m_design = theme;
  applyLook();
}

void QcpGraph::setVariant(GraphLook::Variant variant)
{
  if (variant == m_variant)
    return;
  m_variant = variant;
  applyLook();
}

void QcpGraph::setLookSettings(const GraphLook::Settings &settings)
{
  m_lookSettings = settings;
  applyLook();
}

void QcpGraph::setPatternedLines(bool on)
{
  if (on == m_patterns)
    return;
  m_patterns = on;
  styleCurves();
  updateLegend();
  m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void QcpGraph::applyLook()
{
  m_look = GraphLook::look(m_variant, m_design, m_lookSettings);
  const PlotTheme &theme = m_look.theme;
  // the rows above and below the plot on its background, the scrollbar too:
  // a dark graph gets no light strip
  QPalette p = palette();
  const QBrush background = theme.background.style() == Qt::NoBrush ? p.base() : theme.background;
  p.setBrush(QPalette::Window, background);
  setPalette(p);
  setAutoFillBackground(true);
  QPalette bar = m_scrollBar->palette();
  for (QPalette::ColorRole role : { QPalette::Window, QPalette::Base, QPalette::Button })
    bar.setBrush(role, background);
  bar.setColor(QPalette::WindowText, theme.labels);
  bar.setColor(QPalette::ButtonText, theme.labels);
  m_scrollBar->setPalette(bar);
  m_scrollBar->setAutoFillBackground(true);
  m_timeBar->setTextColor(theme.labels);

  // the base knows the time and the left axis: the others too
  QcpPlotBase::setTheme(theme);
  QList<QCPAxis *> axes{ m_plot->xAxis };
  for (const Axis &a : std::as_const(m_axes))
    axes << a.axis;
  theme.applyTo(m_plot, axes);
  m_plot->axisRect()->setBackground(m_look.plot);
  const bool minor = m_look.minorGrid.style() != Qt::NoPen;
  for (QCPAxis *axis : std::as_const(axes))
  {
    axis->setBasePen(m_look.axisLine);
    axis->setTickPen(m_look.axisLine);
    axis->setSubTickPen(m_look.axisLine);
    axis->grid()->setPen(m_look.grid);
    axis->grid()->setZeroLinePen(m_look.grid);
    axis->grid()->setSubGridVisible(minor);
    axis->grid()->setSubGridPen(minor ? m_look.minorGrid : m_look.grid);
  }
  static_cast<CentreTicks *>(m_centreTicks)->setPen(m_look.centreTicks);
  styleCurves();
  updateLegend();
  // other divisions, another width: the axes anew
  redraw();
  m_plot->replot(QCustomPlot::rpQueuedReplot);
}

// By the value's place among the values, so a curve keeps its colour and
// its dashes while another meter is closed. The meter of the first value is
// the graph's own: its other values by their id, other meters' in their turn.
GraphLook::CurveKey QcpGraph::keyOf(int curve) const
{
  GraphLook::CurveKey key;
  if (m_ownStore || m_values.isEmpty())
    return key;
  const ValueRef value = m_curves.value(curve).value;
  key.slot = qMax(0, int(m_values.indexOf(value)));
  key.valueId = value.id;
  if (value.source != m_values.first().source)
  {
    key.otherMeter = 0;
    for (int i = 0; i < key.slot; ++i)
      key.otherMeter += m_values.at(i).source != m_values.first().source;
  }
  return key;
}

QColor QcpGraph::colorOf(int curve) const
{
  return GraphLook::curveColor(m_look, keyOf(curve));
}

void QcpGraph::styleCurves()
{
  QList<int> onAxis(m_axes.size(), 0), single(m_axes.size(), -1);
  for (int i = 0; i < m_curves.size(); ++i)
  {
    Curve &c = m_curves[i];
    c.pen = GraphLook::curvePen(m_look, keyOf(i), m_patterns);
    c.graph->setLineStyle(c.pen.style() == Qt::NoPen ? QCPGraph::lsNone : QCPGraph::lsLine);
    c.graph->setPen(c.pen.style() == Qt::NoPen ? QPen(colorOf(i)) : c.pen);
    if (c.axis >= 0)
    {
      onAxis[c.axis]++;
      single[c.axis] = i;
    }
  }
  // an axis with one curve alone says so by its colour
  for (int i = 0; i < m_axes.size(); ++i)
  {
    const QColor color = onAxis.at(i) == 1 && m_curves.size() > 1 ? colorOf(single.at(i)) : m_look.theme.labels;
    m_axes.at(i).axis->setTickLabelColor(color);
    m_axes.at(i).axis->setLabelColor(color);
  }
}

void QcpGraph::styleDensity(const Curve &curve, int shown)
{
  const int width = qMax(1, m_plot->axisRect()->width());
  // more than a point per pixel: a band of strokes
  const bool dense = shown > width;
  // a dashed line wider than a pixel is not smoothed either: that takes
  // fifteen times as long, too long for a live picture
  const bool dashed = curve.pen.style() != Qt::SolidLine && curve.pen.style() != Qt::NoPen && curve.pen.widthF() > 1;
  curve.graph->setAntialiased(!dense && !dashed);
  if (curve.pen.style() != Qt::NoPen)
  {
    QPen pen = curve.pen;
    if (dense)
      pen.setStyle(Qt::SolidLine);
    if (pen != curve.graph->pen())
      curve.graph->setPen(pen);
  }
  // markers while one can tell them apart
  QCPScatterStyle::ScatterShape shape = QCPScatterStyle::ssNone;
  if (shown * 6 <= width)
    switch ((m_look.pointMode - 1) % 4 + 1)
    {
      case 1: shape = QCPScatterStyle::ssDisc; break;
      case 2: shape = QCPScatterStyle::ssSquare; break;
      case 3: shape = QCPScatterStyle::ssDiamond; break;
      case 4: shape = QCPScatterStyle::ssCross; break;
      default: break;
    }
  if (m_look.pointMode <= 0)
    shape = QCPScatterStyle::ssNone;
  const QColor color = curve.graph->pen().color();
  curve.graph->setScatterStyle(shape == QCPScatterStyle::ssNone ? QCPScatterStyle(QCPScatterStyle::ssNone)
                                                                : QCPScatterStyle(shape, color, color, m_look.pointMode > 4 ? 8 : 5));
}

// 8 divisions of a whole 1-2-5 step that take lo .. hi in.
void QcpGraph::divide(double &lo, double &hi, double &step)
{
  const double span = qMax(hi - lo, 1e-12);
  const double magnitude = std::pow(10.0, std::floor(std::log10(span / 8)));
  step = magnitude;
  for (int i = 0; i < 12; ++i)
  {
    const double low = std::floor(lo / step) * step;
    if (low + 8 * step >= hi - step * 1e-9)
    {
      lo = low;
      hi = low + 8 * step;
      return;
    }
    // 1, 2, 5, 10, ...
    const double mantissa = step / std::pow(10.0, std::floor(std::log10(step) + 1e-9));
    step *= mantissa < 1.5 ? 2 : mantissa < 3.5 ? 2.5 : 2;
  }
  lo = std::floor(lo / step) * step;
  hi = lo + 8 * step;
}

QColor QcpGraph::curveColor(int i) const
{
  return i >= 0 && i < m_curves.size() ? colorOf(i) : QColor();
}

QPen QcpGraph::curvePen(int i) const
{
  if (i < 0 || i >= m_curves.size())
    return QPen(Qt::NoPen);
  return m_curves.at(i).pen;
}

bool QcpGraph::curveMarkers(int i) const
{
  return i >= 0 && i < m_curves.size() && m_curves.at(i).graph->scatterStyle().shape() != QCPScatterStyle::ssNone;
}

QColor QcpGraph::axisColor(int i) const
{
  return i >= 0 && i < m_axes.size() ? m_axes.at(i).axis->tickLabelColor() : QColor();
}

int QcpGraph::axisTickCount(int i) const
{
  return i >= 0 && i < m_axes.size() ? int(m_axes.at(i).axis->tickVector().size()) : 0;
}
