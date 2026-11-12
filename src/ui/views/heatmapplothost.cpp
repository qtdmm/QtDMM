#include "ui/views/heatmapplothost.h"

#include "ui/views/heatmapplot.h"
#include "ui/views/qcpheatmapplot.h"

#include <QStackedWidget>
#include <QVBoxLayout>

HeatmapPlotHost::HeatmapPlotHost(QWidget *parent) : HeatmapPlotBackend(parent)
{
  setMinimumSize(260, 180);
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  m_stack = new QStackedWidget(this);
  layout->addWidget(m_stack);
  m_legacy = new HeatmapPlot(m_stack);
  m_qcp = new QCPHeatmapPlot(m_stack);
  m_stack->addWidget(m_legacy);
  m_stack->addWidget(m_qcp);
  for (QWidget *page : { static_cast<QWidget *>(m_legacy), static_cast<QWidget *>(m_qcp) })
  {
    page->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(page, &QWidget::customContextMenuRequested, this, [this, page](const QPoint &pos)
    {
      emit customContextMenuRequested(mapFromGlobal(page->mapToGlobal(pos)));
    });
  }
  m_stack->setCurrentWidget(m_legacy);
}

void HeatmapPlotHost::setBackend(Backend backend)
{
  m_backend = backend;
  m_stack->setCurrentWidget(backend == QCustomPlot ? static_cast<QWidget *>(m_qcp) : m_legacy);
}

void HeatmapPlotHost::setThemeColors(const QBrush &background, const QColor &grid, const QColor &labels)
{
  m_legacy->setThemeColors(background, grid, labels);
  m_qcp->setThemeColors(background, grid, labels);
}

void HeatmapPlotHost::setAxisUnits(const QString &x, const QString &y, const QString &z)
{
  m_legacy->setAxisUnits(x, y, z);
  m_qcp->setAxisUnits(x, y, z);
}

void HeatmapPlotHost::setLegend(const QString &legend)
{
  m_legacy->setLegend(legend);
  m_qcp->setLegend(legend);
}

void HeatmapPlotHost::setColorScheme(ColorMap::Scheme scheme)
{
  m_legacy->setColorScheme(scheme);
  m_qcp->setColorScheme(scheme);
}

ColorMap::Scheme HeatmapPlotHost::colorScheme() const
{
  return m_legacy->colorScheme();
}

void HeatmapPlotHost::setCellCount(int nx, int ny)
{
  m_legacy->setCellCount(nx, ny);
  m_qcp->setCellCount(nx, ny);
}

void HeatmapPlotHost::addPoint(double x, double y, double z)
{
  m_legacy->addPoint(x, y, z);
  m_qcp->addPoint(x, y, z);
}

void HeatmapPlotHost::clear()
{
  m_legacy->clear();
  m_qcp->clear();
}

void HeatmapPlotHost::setHold(bool hold)
{
  m_legacy->setHold(hold);
  m_qcp->setHold(hold);
}

bool HeatmapPlotHost::hold() const
{
  return m_legacy->hold();
}
