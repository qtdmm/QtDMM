#include "ui/views/qcpheatmapplot.h"

#include "ui/views/plotaxis.h"

#include "3rdparty/qcustomplot/qcustomplot.h"

#include <QCoreApplication>
#include <QVBoxLayout>

#include <cmath>

namespace
{
constexpr int kMaxSamples = 5000;
constexpr int kMaxGridDimension = 256;
}

QCPHeatmapPlot::QCPHeatmapPlot(QWidget *parent) : HeatmapPlotBackend(parent)
{
  setMinimumSize(260, 180);
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  m_plot = new QCustomPlot(this);
  layout->addWidget(m_plot);
  m_plot->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_plot, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos)
  {
    emit customContextMenuRequested(mapFromGlobal(m_plot->mapToGlobal(pos)));
  });

  m_plot->plotLayout()->insertRow(0);
  m_plot->plotLayout()->insertColumn(1);
  m_title = new QCPTextElement(m_plot, QString(), font());
  m_plot->plotLayout()->addElement(0, 0, m_title);

  m_map = new QCPColorMap(m_plot->xAxis, m_plot->yAxis);
  m_map->data()->setSize(m_nx, m_ny);
  m_map->setInterpolate(false);
  m_map->setTightBoundary(false);
  m_map->data()->setRange(QCPRange(0, 1), QCPRange(0, 1));
  m_map->setDataRange(QCPRange(0, 1));

  m_scale = new QCPColorScale(m_plot);
  m_scale->setMinimumMargins(QMargins(0, 0, 0, 0));
  m_plot->plotLayout()->addElement(1, 1, m_scale);
  m_scale->setType(QCPAxis::atRight);
  m_map->setColorScale(m_scale);
  auto *axisMargins = new QCPMarginGroup(m_plot);
  m_plot->axisRect()->setMarginGroup(QCP::msTop | QCP::msBottom, axisMargins);
  m_scale->axis()->axisRect()->setMarginGroup(QCP::msTop | QCP::msBottom, axisMargins);

  m_noDataText = new QCPItemText(m_plot);
  m_noDataText->position->setType(QCPItemPosition::ptAxisRectRatio);
  m_noDataText->position->setCoords(0.5, 0.5);
  m_noDataText->setPositionAlignment(Qt::AlignCenter);
  m_noDataText->setText(QCoreApplication::translate("HeatmapPlot", "No heatmap data yet"));

  m_holdText = new QCPItemText(m_plot);
  m_holdText->position->setType(QCPItemPosition::ptAxisRectRatio);
  m_holdText->position->setCoords(0.98, 0.02);
  m_holdText->setPositionAlignment(Qt::AlignRight | Qt::AlignTop);
  m_holdText->setText(QStringLiteral("HOLD"));
  m_holdText->setVisible(false);

  applyGradient();
  setAxisUnits({}, {}, {});
  setLegend({});
  m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void QCPHeatmapPlot::setThemeColors(const QBrush &background, const QColor &grid, const QColor &labels)
{
  m_grid = grid;
  m_labels = labels;
  const QBrush bg = background.style() == Qt::NoBrush ? QBrush(palette().base()) : background;
  m_plot->setBackground(bg);
  m_plot->axisRect()->setBackground(Qt::NoBrush);
  for (QCPAxis *axis : { m_plot->xAxis, m_plot->yAxis, m_scale->axis() })
  {
    axis->setBasePen(QPen(grid));
    axis->setTickPen(QPen(grid));
    axis->setSubTickPen(QPen(grid));
    axis->setTickLabelColor(labels);
    axis->setLabelColor(labels);
    axis->grid()->setPen(QPen(grid, 1, Qt::DotLine));
    axis->grid()->setSubGridPen(QPen(grid, 1, Qt::DotLine));
  }
  m_title->setTextColor(labels);
  m_noDataText->setColor(labels);
  m_holdText->setColor(labels);
  m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void QCPHeatmapPlot::setAxisUnits(const QString &x, const QString &y, const QString &z)
{
  m_xUnit = x;
  m_yUnit = y;
  m_zUnit = z;
  refreshMap();
}

void QCPHeatmapPlot::setLegend(const QString &legend)
{
  m_legend = legend;
  m_title->setText(legend);
  m_title->setTextColor(m_labels.isValid() ? m_labels : palette().text().color());
  m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void QCPHeatmapPlot::setColorScheme(ColorMap::Scheme scheme)
{
  if (m_scheme == scheme)
    return;
  m_scheme = scheme;
  applyGradient();
  refreshMap();
}

void QCPHeatmapPlot::setCellCount(int nx, int ny)
{
  m_nx = qBound(2, nx, kMaxGridDimension);
  m_ny = qBound(2, ny, kMaxGridDimension);
  refreshMap();
}

void QCPHeatmapPlot::addPoint(double x, double y, double z)
{
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
    return;
  if (m_samples.size() < kMaxSamples)
    m_samples.append({x, y, z});
  else
  {
    m_samples[m_sampleHead] = {x, y, z};
    m_sampleHead = (m_sampleHead + 1) % kMaxSamples;
  }
  if (!m_hold)
    refreshMap();
}

void QCPHeatmapPlot::clear()
{
  m_samples.clear();
  m_shown.clear();
  m_sampleHead = 0;
  refreshMap();
}

void QCPHeatmapPlot::setHold(bool hold)
{
  if (m_hold == hold)
    return;
  m_hold = hold;
  if (hold)
    m_shown = m_samples;
  else
    m_shown.clear();
  m_holdText->setVisible(hold);
  refreshMap();
}

void QCPHeatmapPlot::applyGradient()
{
  QCPColorGradient gradient;
  if (m_scheme == ColorMap::Ironbow)
  {
    gradient.setColorStopAt(0.00, QColor(0, 0, 0));
    gradient.setColorStopAt(0.17, QColor(32, 0, 120));
    gradient.setColorStopAt(0.38, QColor(140, 20, 140));
    gradient.setColorStopAt(0.55, QColor(210, 50, 60));
    gradient.setColorStopAt(0.72, QColor(250, 130, 10));
    gradient.setColorStopAt(0.88, QColor(255, 220, 60));
    gradient.setColorStopAt(1.00, QColor(255, 255, 230));
  }
  else
  {
    for (int i = 0; i <= 16; ++i)
    {
      const double t = double(i) / 16;
      gradient.setColorStopAt(t, ColorMap::color(m_scheme, t));
    }
  }
  gradient.setLevelCount(256);
  gradient.setNanHandling(QCPColorGradient::nhNanColor);
  gradient.setNanColor(ColorMap::noDataColor(m_scheme));
  m_map->setGradient(gradient);
  m_scale->setGradient(gradient);
}

void QCPHeatmapPlot::refreshMap()
{
  const QVector<Sample> &shown = m_hold ? m_shown : m_samples;
  m_holdText->setVisible(m_hold);
  m_noDataText->setVisible(shown.isEmpty());
  if (shown.isEmpty())
  {
    const PlotAxis::Range xAxis = PlotAxis::range(0, 0, m_xUnit);
    const PlotAxis::Range yAxis = PlotAxis::range(0, 0, m_yUnit);
    const PlotAxis::Range zAxis = PlotAxis::range(0, 0, m_zUnit);
    m_map->data()->setSize(m_nx, m_ny);
    m_map->data()->setRange(QCPRange(0, 1), QCPRange(0, 1));
    m_map->setDataRange(QCPRange(0, 1));
    for (int y = 0; y < m_ny; ++y)
      for (int x = 0; x < m_nx; ++x)
        m_map->data()->setCell(x, y, qQNaN());
    m_plot->xAxis->setRange(0, 1);
    m_plot->yAxis->setRange(0, 1);
    m_scale->axis()->setRange(0, 1);
    m_plot->xAxis->setLabel(xAxis.title);
    m_plot->yAxis->setLabel(yAxis.title);
    m_scale->setLabel(zAxis.title);
    m_plot->replot(QCustomPlot::rpQueuedReplot);
    return;
  }

  double xmin = shown.first().x, xmax = xmin;
  double ymin = shown.first().y, ymax = ymin;
  double zmin = shown.first().z, zmax = zmin;
  for (const Sample &sample : shown)
  {
    xmin = qMin(xmin, sample.x);
    xmax = qMax(xmax, sample.x);
    ymin = qMin(ymin, sample.y);
    ymax = qMax(ymax, sample.y);
    zmin = qMin(zmin, sample.z);
    zmax = qMax(zmax, sample.z);
  }

  const PlotAxis::Range xAxis = PlotAxis::range(xmin, xmax, m_xUnit);
  const PlotAxis::Range yAxis = PlotAxis::range(ymin, ymax, m_yUnit);
  const PlotAxis::Range zAxis = PlotAxis::range(zmin, zmax, m_zUnit);
  const QCPRange xRange(xAxis.minimum / xAxis.factor, xAxis.maximum / xAxis.factor);
  const QCPRange yRange(yAxis.minimum / yAxis.factor, yAxis.maximum / yAxis.factor);
  const QCPRange zRange(zAxis.minimum / zAxis.factor, zAxis.maximum / zAxis.factor);
  m_map->data()->setSize(m_nx, m_ny);
  m_map->data()->setRange(xRange, yRange);
  m_map->setDataRange(zRange);
  m_plot->xAxis->setRange(xRange);
  m_plot->yAxis->setRange(yRange);
  m_scale->axis()->setRange(zRange);
  m_plot->xAxis->setLabel(xAxis.title);
  m_plot->yAxis->setLabel(yAxis.title);
  m_scale->setLabel(zAxis.title);

  const int cellCount = m_nx * m_ny;
  QVector<double> sums(cellCount, 0.0);
  QVector<int> counts(cellCount, 0);
  const double dx = xmax - xmin;
  const double dy = ymax - ymin;
  for (const Sample &sample : shown)
  {
    const int ix = dx == 0 ? m_nx / 2 : qBound(0, int((sample.x - xmin) / dx * m_nx), m_nx - 1);
    const int iy = dy == 0 ? m_ny / 2 : qBound(0, int((sample.y - ymin) / dy * m_ny), m_ny - 1);
    const int i = iy * m_nx + ix;
    sums[i] += sample.z / zAxis.factor;
    ++counts[i];
  }
  for (int y = 0; y < m_ny; ++y)
    for (int x = 0; x < m_nx; ++x)
    {
      const int i = y * m_nx + x;
      m_map->data()->setCell(x, y, counts[i] ? sums[i] / counts[i] : qQNaN());
    }

  m_plot->replot(QCustomPlot::rpQueuedReplot);
}
