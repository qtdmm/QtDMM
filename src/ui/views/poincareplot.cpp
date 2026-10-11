// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/poincareplot.h"

#include <QActionGroup>
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSvgGenerator>
#include <cmath>

#include "core/settings.h"
#include "core/siprefix.h"

namespace
{
// units that take no SI prefix on the axis
bool plainUnit(const QString &unit)
{
  return unit.isEmpty() || unit.startsWith(QChar(0x00b0)) || unit == QLatin1String("%");
}
}

PoincarePlot::PoincarePlot(QWidget *parent) :
  QWidget(parent)
{
  setMouseTracking(true);
  setMinimumSize(160, 160);
  m_repaint.setSingleShot(true);
  m_repaint.setInterval(50);   // at most 20 pictures a second
  connect(&m_repaint, &QTimer::timeout, this, [this]
  {
    if (!m_hold)
    {
      m_shown = m_series.pairs();
      m_stats = m_series.stats();
      m_unit = m_series.unit();
      m_resolution = m_series.resolution();
    }
    update();
  });
}

void PoincarePlot::setSettings(Settings *cfg)
{
  m_cfg = nullptr;   // reading, not writing back
  setCapacity(cfg->getInt("Poincare/capacity", PoincareSeries::kDefaultCapacity));
  setLag(cfg->getInt("Poincare/lag", 1));
  setEllipse(cfg->getBool("Poincare/ellipse", true));
  m_cfg = cfg;
}

void PoincarePlot::setThemeColors(const QBrush &background, const QColor &grid, const QColor &labels,
                                  const QColor &data)
{
  m_background = background;
  m_grid = grid;
  m_labels = labels;
  m_data = data;
  update();
}

void PoincarePlot::setLag(int k)
{
  m_series.setLag(k);
  if (m_cfg)
    m_cfg->setInt("Poincare/lag", m_series.lag());
  scheduleUpdate();
}

void PoincarePlot::setCapacity(int values)
{
  m_series.setCapacity(values);
  if (m_cfg)
    m_cfg->setInt("Poincare/capacity", m_series.capacity());
  scheduleUpdate();
}

void PoincarePlot::setEllipse(bool on)
{
  m_ellipse = on;
  if (m_cfg)
    m_cfg->setBool("Poincare/ellipse", on);
  update();
}

void PoincarePlot::setHold(bool on)
{
  m_hold = on;
  scheduleUpdate();
}

void PoincarePlot::addReading(const Reading &reading)
{
  if (reading.id != 0)
    return;
  m_series.feed(reading);
  scheduleUpdate();
}

void PoincarePlot::setStale(bool stale)
{
  if (stale)
    m_series.gap();
}

void PoincarePlot::clear()
{
  m_series.clear();
  m_shown.clear();
  m_stats = PoincareSeries::Stats();
  update();
  scheduleUpdate();
}

void PoincarePlot::scheduleUpdate()
{
  if (!m_repaint.isActive())
    m_repaint.start();
}

void PoincarePlot::addMenuActions(QMenu *menu)
{
  QMenu *lag = menu->addMenu(tr("&Distance k"));
  for (int k : { 1, 2, 3, 5, 10 })
  {
    QAction *a = lag->addAction(QString("&%1").arg(k));
    a->setCheckable(true);
    a->setChecked(m_series.lag() == k);
    connect(a, &QAction::triggered, this, [this, k] { setLag(k); });
  }
  QMenu *points = menu->addMenu(tr("&Points"));
  for (int n : { 100, 200, 500, 1000, 2000, 5000 })
  {
    QAction *a = points->addAction(QString::number(n));
    a->setCheckable(true);
    a->setChecked(m_series.capacity() == n);
    connect(a, &QAction::triggered, this, [this, n] { setCapacity(n); });
  }
  QAction *ellipse = menu->addAction(tr("&Ellipse SD1/SD2"));
  ellipse->setCheckable(true);
  ellipse->setChecked(m_ellipse);
  connect(ellipse, &QAction::toggled, this, &PoincarePlot::setEllipse);
  QAction *hold = menu->addAction(tr("H&old picture"));
  hold->setCheckable(true);
  hold->setChecked(m_hold);
  connect(hold, &QAction::toggled, this, &PoincarePlot::setHold);
  connect(menu->addAction(tr("&Clear")), &QAction::triggered, this, &PoincarePlot::clear);
  connect(menu->addAction(tr("&Save image...")), &QAction::triggered, this, &PoincarePlot::exportImageSLOT);
}

bool PoincarePlot::exportImageSLOT()
{
  const QString png = tr("PNG image (*.png)"), svg = tr("SVG image (*.svg)");
  QString filter = png;
  QString fn = QFileDialog::getSaveFileName(this, tr("Save image"), "poincare.png", png + ";;" + svg, &filter);
  if (fn.isEmpty())
    return false;
  if (QFileInfo(fn).suffix().isEmpty())
    fn += filter == svg ? ".svg" : ".png";
  return exportImageFile(fn);
}

bool PoincarePlot::exportImageFile(const QString &fileName, QSize size)
{
  if (size.isEmpty())
    size = this->size();
  if (QFileInfo(fileName).suffix().compare("svg", Qt::CaseInsensitive) == 0)
  {
    QSvgGenerator svg;
    svg.setFileName(fileName);
    svg.setSize(size);
    svg.setViewBox(QRect(QPoint(0, 0), size));
    svg.setTitle(tr("QtDMM Poincaré plot"));
    QPainter p;
    if (!p.begin(&svg))
      return false;
    render(p, QRect(QPoint(0, 0), size));
    return p.end();
  }
  QImage image(size, QImage::Format_ARGB32);
  image.fill(Qt::transparent);
  QPainter p(&image);
  render(p, QRect(QPoint(0, 0), size));
  p.end();
  return image.save(fileName);
}

QRect PoincarePlot::plotRect(const QRect &area) const
{
  const QFontMetrics fm(font());
  const int left = fm.horizontalAdvance("-000.000") + fm.height() + 10;
  const int bottom = 2 * fm.height() + 8;
  const int top = fm.height() / 2 + 4, right = fm.horizontalAdvance("000") / 2 + 6;
  const int side = qMax(20, qMin(area.width() - left - right, area.height() - top - bottom));
  const int x = area.left() + left + (area.width() - left - right - side) / 2;
  const int y = area.top() + top + (area.height() - top - bottom - side) / 2;
  return QRect(x, y, side, side);
}

void PoincarePlot::paintEvent(QPaintEvent *)
{
  QPainter p(this);
  render(p, rect());
}

void PoincarePlot::render(QPainter &p, const QRect &area)
{
  const QPalette pal = palette();
  const QBrush background = m_background.style() != Qt::NoBrush ? m_background : pal.base();
  const QColor grid = m_grid.isValid() ? m_grid : pal.color(QPalette::Mid);
  const QColor labels = m_labels.isValid() ? m_labels : pal.color(QPalette::Text);
  const QColor data = m_data.isValid() ? m_data : pal.color(QPalette::Highlight);
  p.setRenderHint(QPainter::Antialiasing);
  p.fillRect(area, background);
  p.setFont(font());
  const QFontMetrics fm(font());
  const QRect plot = plotRect(area);
  m_plot = plot;

  if (m_shown.isEmpty())
  {
    p.setPen(labels);
    p.drawText(area, Qt::AlignCenter, m_series.valueCount() > 0 ? tr("Waiting for the next reading")
                                                                : tr("No readings yet"));
    return;
  }

  // one scale for both axes: what the points span, with some room
  double lo = m_shown.first().x, hi = lo;
  for (const PoincareSeries::Pair &q : m_shown)
  {
    lo = qMin(lo, qMin(q.x, q.y));
    hi = qMax(hi, qMax(q.x, q.y));
  }
  const PoincareSeries::Axis axis = PoincareSeries::axis(lo, hi, m_resolution);
  lo = axis.lo;
  hi = axis.hi;
  const double step = axis.step;
  m_lo = lo;
  m_hi = hi;
  const double scale = plot.width() / (hi - lo);
  auto px = [&](double v) { return plot.left() + (v - lo) * scale; };
  auto py = [&](double v) { return plot.bottom() - (v - lo) * scale; };

  // the axis labels with one SI prefix for all of them
  QString prefix;
  if (!plainUnit(m_unit))
    SiPrefix::scale(qMax(std::abs(lo), std::abs(hi)), &prefix);
  const double factor = SiPrefix::factor(prefix);
  const int decimals = qMax(0, int(-std::floor(std::log10(step / factor) + 1e-9)));
  const QString unit = prefix + m_unit;

  // grid and labels
  p.setPen(QPen(grid, 1));
  for (double v = lo; v <= hi + step * 1e-9; v += step)
  {
    p.drawLine(QPointF(px(v), plot.top()), QPointF(px(v), plot.bottom()));
    p.drawLine(QPointF(plot.left(), py(v)), QPointF(plot.right(), py(v)));
  }
  p.setPen(labels);
  for (double v = lo; v <= hi + step * 1e-9; v += step)
  {
    // no "-0" from the sum of the steps
    const QString text = QString::number(std::abs(v) < step * 1e-6 ? 0.0 : v / factor, 'f', decimals);
    const int w = fm.horizontalAdvance(text);
    if (v + step <= hi + step * 1e-9)   // the last x label would sit under the corner
      p.drawText(QPointF(px(v) - w / 2.0, plot.bottom() + fm.ascent() + 3), text);
    p.drawText(QPointF(plot.left() - w - 4, py(v) + fm.ascent() / 2.0 - 1), text);
  }
  const QString unitText = unit.isEmpty() ? QString() : QString(" [%1]").arg(unit);
  const QString xTitle = tr("x(n)") + unitText;
  const QString yTitle = tr("x(n+%1)").arg(m_series.lag()) + unitText;
  p.drawText(QPointF(plot.center().x() - fm.horizontalAdvance(xTitle) / 2.0, plot.bottom() + 2 * fm.height() + 4),
             xTitle);
  p.save();
  p.translate(area.left() + fm.ascent() + 1, plot.center().y() + fm.horizontalAdvance(yTitle) / 2.0);
  p.rotate(-90);
  p.drawText(QPointF(0, 0), yTitle);
  p.restore();
  p.setPen(QPen(labels, 1));
  p.setBrush(Qt::NoBrush);
  p.drawRect(plot);

  p.save();
  p.setClipRect(plot);
  // the diagonal: x(n+k) = x(n)
  p.setPen(QPen(labels, 1, Qt::DashLine));
  p.drawLine(QPointF(px(lo), py(lo)), QPointF(px(hi), py(hi)));

  // the cloud, oldest first, fading towards the past
  const int n = int(m_shown.size());
  const double r = qMax(2.0, plot.width() / 160.0);
  p.setPen(Qt::NoPen);
  for (const PoincareSeries::Pair &q : m_shown)
  {
    QColor c = data;
    c.setAlphaF(n > 1 ? 0.15 + 0.85 * (1.0 - double(q.age) / (n - 1)) : 1.0);
    p.setBrush(c);
    p.drawEllipse(QPointF(px(q.x), py(q.y)), r, r);
  }
  const PoincareSeries::Pair &newest = m_shown.last();
  p.setBrush(data);
  p.setPen(QPen(labels, 1.5));
  p.drawEllipse(QPointF(px(newest.x), py(newest.y)), r * 1.8, r * 1.8);

  // the ellipse: SD2 along the diagonal, SD1 across it
  if (m_ellipse && m_stats.count > 1 && std::isfinite(m_stats.sd1) && std::isfinite(m_stats.sd2))
  {
    p.setPen(QPen(labels, 1.5));
    p.setBrush(Qt::NoBrush);
    p.translate(px(m_stats.meanX), py(m_stats.meanY));
    p.rotate(-45);
    p.drawEllipse(QPointF(0, 0), m_stats.sd2 * scale, m_stats.sd1 * scale);
  }
  p.restore();

  // the numbers in the corner
  auto valueText = [&](double v)
  {
    if (plainUnit(m_unit))
      return QString("%1 %2").arg(QString::number(v, 'g', 4), m_unit).trimmed();
    QString pre;
    const double scaled = SiPrefix::scale(v, &pre);
    return QString("%1 %2%3").arg(QString::number(scaled, 'g', 4), pre, m_unit);
  };
  QStringList lines;
  if (std::isfinite(m_stats.sd1))
    lines << tr("SD1 %1").arg(valueText(m_stats.sd1)) << tr("SD2 %1").arg(valueText(m_stats.sd2));
  lines << tr("n = %1, k = %2").arg(m_stats.count).arg(m_series.lag());
  if (m_hold)
    lines << tr("HOLD");
  int y = plot.top() + fm.ascent() + 4;
  p.setPen(labels);
  for (const QString &line : lines)
  {
    p.drawText(QPointF(plot.left() + 6, y), line);
    y += fm.height();
  }

  // the crosshair: x(n) and x(n+k) under the mouse
  if (m_mouseIn && plot.contains(m_mouse) && area == rect())
  {
    p.setPen(QPen(labels, 1, Qt::DotLine));
    p.drawLine(QPointF(m_mouse.x(), plot.top()), QPointF(m_mouse.x(), plot.bottom()));
    p.drawLine(QPointF(plot.left(), m_mouse.y()), QPointF(plot.right(), m_mouse.y()));
    const double vx = lo + (m_mouse.x() - plot.left()) / scale, vy = lo + (plot.bottom() - m_mouse.y()) / scale;
    const QString text = QString("%1   %2").arg(valueText(vx), valueText(vy));
    p.setPen(labels);
    p.drawText(QPointF(plot.right() - fm.horizontalAdvance(text) - 6, plot.top() + fm.ascent() + 4), text);
  }
}

void PoincarePlot::mouseMoveEvent(QMouseEvent *ev)
{
  m_mouse = ev->position().toPoint();
  m_mouseIn = true;
  update();
}

void PoincarePlot::leaveEvent(QEvent *)
{
  m_mouseIn = false;
  update();
}
