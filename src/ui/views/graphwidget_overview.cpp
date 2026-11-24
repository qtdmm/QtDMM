// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// GraphWidget: the overview strip below the plot, the strip of the meter's
// states at its bottom, and the curves drawn as bands - the dense ones and
// the min/max band.
#include "ui/views/graphwidget.h"

#include "recording/recordingstore.h"
#include "ui/views/graphband.h"
#include "ui/views/graphminimap.h"
#include "ui/views/graphstates.h"
#include "ui/views/graphstatestrip.h"
#include "ui/views/graphtimebar.h"
#include "ui/views/qcp.h"

#include <QBoxLayout>
#include <QScrollBar>

namespace
{
/// Curves as bands (GraphBand): each from the lowest to the highest value
/// of its pixel columns, where the axes are when it is drawn.
class Bands : public QCPAbstractItem
{
public:
  struct Entry
  {
    Entry() = default;   ///< declared with the moves below, so spelled out
    /// The fields in the order of the members.
    Entry(int curveIndex, int sourceIndex, QCPAxis *valueAxis, double unitFactor, int columnCount, const QBrush &brush,
          const QPen &outline)
      : curve(curveIndex), source(sourceIndex), axis(valueAxis), factor(unitFactor), columns(columnCount), fill(brush), pen(outline)
    {
    }
    int      curve = 0;      ///< -1: the min/max band
    int      source = 0;     ///< the curve whose points it draws
    QCPAxis *axis = nullptr;
    double   factor = 1;     ///< base unit per unit of the axis
    int      columns = 0;    ///< the columns either side that widen it
    QBrush   fill;
    QPen     pen;
    // the moves and copies of the compiler, marked noexcept (SonarCloud S5018)
    Entry(const Entry &) = default;
    Entry &operator=(const Entry &) = default;
    Entry(Entry &&) noexcept = default;
    Entry &operator=(Entry &&) noexcept = default;
  };

  explicit Bands(QCustomPlot *plot) : QCPAbstractItem(plot) {}
  double selectTest(const QPointF &, bool, QVariant *) const override { return -1; }

  QList<Entry> entries;
  /// The points of a curve, nullptr for none. Not held in the entries: a
  /// copy shared with the curve would be copied whole by its next reading.
  std::function<const QList<QPointF> *(int)> pointsOf;

protected:
  /// A band of columnBand() - its upper edge, then the lower one backwards -
  /// in pieces of a few columns. Whole, a jagged band of a curve that swings
  /// costs the raster engine more than linearly in its columns (the stroke
  /// above all: 70 ms against 4.4 ms for 800 columns that swing over most
  /// of the height); the pieces fill the same pixels.
  static void drawBand(QPainter *painter, const QPolygonF &band, const QBrush &fill, const QPen &pen)
  {
    constexpr qsizetype kPiece = 16;
    const qsizetype n = (band.size() - 1) / 2;   // columns; the last point closes the polygon
    if (n < 2)
      return;
    auto upper = [&](qsizetype c) { return band.at(c); };
    auto lower = [&](qsizetype c) { return band.at(2 * n - 1 - c); };
    QPolygonF piece, edge;
    for (qsizetype a = 0; a + 1 < n; a += kPiece)
    {
      const qsizetype b = qMin(n - 1, a + kPiece);
      piece.clear();
      for (qsizetype c = a; c <= b; ++c)
        piece << upper(c);
      for (qsizetype c = b; c >= a; --c)
        piece << lower(c);
      painter->setPen(Qt::NoPen);
      painter->setBrush(fill);
      painter->drawPolygon(piece);
      if (pen.style() == Qt::NoPen)
        continue;
      painter->setPen(pen);
      painter->setBrush(Qt::NoBrush);
      edge.clear();
      for (qsizetype c = a; c <= b; ++c)
        edge << upper(c);
      painter->drawPolyline(edge);
      edge.clear();
      for (qsizetype c = a; c <= b; ++c)
        edge << lower(c);
      painter->drawPolyline(edge);
    }
    if (pen.style() != Qt::NoPen)
    {
      // its ends
      painter->drawLine(upper(0), lower(0));
      painter->drawLine(upper(n - 1), lower(n - 1));
    }
  }

  void draw(QCPPainter *painter) override
  {
    const QRectF rect = parentPlot()->axisRect()->rect();
    const QCPRange time = parentPlot()->xAxis->range();
    painter->setAntialiasing(false);
    for (const Entry &e : std::as_const(entries))
    {
      const QList<QPointF> *points = pointsOf(e.source);
      if (!points)
        continue;
      const QCPRange values = e.axis->range();
      for (const QPolygonF &outline : GraphBand::columnBand(*points, time.lower, time.upper, values.lower * e.factor,
                                                            values.upper * e.factor, rect, e.columns))
        drawBand(painter, outline, e.fill, e.pen);
    }
  }
};
}

void GraphWidget::setupOverview()
{
  // before the curves on their layer: under them
  auto *bands = new Bands(m_plot);
  bands->setLayer(QStringLiteral("main"));
  bands->setSelectable(false);
  bands->pointsOf = [this](int i) { return i >= 0 && i < m_curves.size() ? &m_curves.at(i).points : nullptr; };
  m_bands = bands;

  m_minimap = new GraphMinimap(this);
  m_minimap->setFixedHeight(GraphMinimap::kHeight);
  layout()->addWidget(m_minimap);
  connect(m_minimap, &GraphMinimap::moveRequested, this, [this](qint64 start) { showFrom(start + m_minimapShift); });
  connect(m_minimap, &GraphMinimap::endRequested, this, &GraphWidget::followEnd);
  connect(m_minimap, &GraphMinimap::widthRequested, this, [this](qint64 start, qint64 width, bool atEnd)
  {
    setWindowSeconds(qMax(1, qRound(width / 1000.0)));
    if (atEnd)
      followEnd();
    else
      showFrom(start + m_minimapShift);
  });
  m_minimap->setVisible(m_minimapOn);
  m_scrollBar->setVisible(!m_minimapOn);

  m_stateStrip = new GraphStateStrip(m_plot);
  m_stateStrip->setObjectName("ui_graphStates");
  m_stateStrip->setProvider([this]
  {
    QList<GraphStateStrip::Span> out;
    const double from = m_plot->xAxis->range().lower * 1000, width = m_plot->xAxis->range().size() * 1000;
    if (width <= 0 || leftCurve() < 0)
      return out;
    for (const GraphStates::Span &s : m_statesCache.spans(statsCurve(leftCurve()), qRound64(from), qRound64(from + width)))
      out << GraphStateStrip::Span{ GraphStateStrip::Kind(s.kind), s.label, qBound(0.0, (s.from - from) / width, 1.0),
                                    qBound(0.0, (s.to - from) / width, 1.0), s.pattern };
    return out;
  });
  // the axes' rectangle is known when the plot was laid out
  connect(m_plot, &QCustomPlot::afterReplot, this, &GraphWidget::placeStates);
  connect(m_timeBar, &GraphTimeBar::overviewToggled, this, &GraphWidget::setMinimapVisible);
  connect(m_timeBar, &GraphTimeBar::envelopeToggled, this, &GraphWidget::setEnvelopeVisible);
  m_timeBar->setOverviewChecked(m_minimapOn);
  m_timeBar->setEnvelopeChecked(m_envelopeOn);
}

void GraphWidget::setMinimapVisible(bool on)
{
  if (on == m_minimapOn)
    return;
  m_minimapOn = on;
  m_minimap->setVisible(on);
  m_scrollBar->setVisible(!on);
  m_timeBar->setOverviewChecked(on);
  updateOverview();
  Q_EMIT overviewChanged();
}

void GraphWidget::setStatesVisible(bool on)
{
  if (on == m_statesOn)
    return;
  m_statesOn = on;
  placeStates();
  Q_EMIT overviewChanged();
}

void GraphWidget::setEnvelopeVisible(bool on)
{
  if (on == m_envelopeOn)
    return;
  m_envelopeOn = on;
  m_timeBar->setEnvelopeChecked(on);
  updateOverview();
  m_plot->replot(QCustomPlot::rpQueuedReplot);
  Q_EMIT overviewChanged();
}

void GraphWidget::placeStates()
{
  const QRect rect = m_plot->axisRect()->rect();
  m_stateStrip->setGeometry(rect.left(), rect.bottom() + 1 - GraphStateStrip::fullHeight(), rect.width(), GraphStateStrip::fullHeight());
  m_stateStrip->setVisible(m_statesOn);
  m_stateStrip->update();   // also its states, when the window stands still
}

// The overview draws the value of the left axis from the recording of its
// meter, on that recording's time; the bands are the curves too dense for
// a line and, behind the left axis' curve, its min/max band.
void GraphWidget::updateOverview()
{
  const int left = leftCurve();
  RecordingStore *store = left < 0 ? m_store.data() : m_curves.at(left).store.data();
  m_minimapShift = left < 0 ? 0 : qRound64(m_curves.at(left).shift * 1000);
  if (m_minimapOn)
  {
    m_minimap->setStore(store);
    m_minimap->setValue(left < 0 || m_ownStore ? 0 : m_curves.at(left).value.id);
    if (left >= 0)
      m_minimap->setCurveColor(colorOf(left));
    m_minimap->setView(windowStart() - m_minimapShift, qRound64(shownSeconds() * 1000));
  }

  auto *bands = static_cast<Bands *>(m_bands);
  bands->entries.clear();
  if (m_envelopeOn && left >= 0)
  {
    // the curve's colour, faint, no outline
    QColor fill = colorOf(left);
    fill.setAlpha(60);
    const Axis &axis = m_axes.first();
    bands->entries << Bands::Entry{ -1, left, axis.axis, axis.factor, GraphBand::kEnvelopeColumns, fill, QPen(Qt::NoPen) };
  }
  for (int i = 0; i < m_curves.size(); ++i)
  {
    const Curve &c = m_curves.at(i);
    if (!c.banded || c.axis < 0)
      continue;
    QColor fill = colorOf(i);
    fill.setAlpha(64);
    // the outline as the line would be, never dashed: it is the edge of an area
    QPen pen(colorOf(i), c.pen.style() == Qt::NoPen ? 1 : c.pen.widthF());
    const Axis &axis = m_axes.at(c.axis);
    bands->entries << Bands::Entry{ i, i, axis.axis, axis.factor, GraphBand::kDenseColumns, fill, pen };
  }
}

QString GraphWidget::statesText() const
{
  const double from = m_plot->xAxis->range().lower * 1000, to = m_plot->xAxis->range().upper * 1000;
  return leftCurve() < 0 ? QString() : GraphStates::text(GraphStates::spans(statsCurve(leftCurve()), qRound64(from), qRound64(to)));
}

bool GraphWidget::curveBanded(int i) const
{
  return i >= 0 && i < m_curves.size() && m_curves.at(i).banded;
}

bool GraphWidget::bandAt(int i, qint64 t, double *lo, double *hi) const
{
  const auto *bands = static_cast<const Bands *>(m_bands);
  for (const Bands::Entry &e : bands->entries)
    if (const QList<QPointF> *points = bands->pointsOf(e.source); e.curve == i && points)
      return GraphBand::columnAt(*points, m_plot->xAxis->range().lower, m_plot->xAxis->range().upper, m_plot->axisRect()->rect(),
                                 e.columns, t / 1000.0, lo, hi);
  return false;
}
