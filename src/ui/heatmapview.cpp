// heatmapview.cpp
#include "ui/heatmapview.h"

#include "service/metercontroller.h"
#include "service/valuefeed.h"
#include "ui/views/heatmapplotbackend.h"

#include <cmath>

HeatmapView::HeatmapView(DeviceSession *session, HeatmapPlotBackend *plot, QObject *parent) :
  QObject(parent), m_plot(plot)
{
  for (int a = 0; a < 3; ++a)
  {
    m_feed[a] = new ValueFeed(session, this);
    connect(m_feed[a], &ValueFeed::reading, this, [this, a](const Reading &r) { feed(a, r); });
    connect(m_feed[a], &ValueFeed::controllerChanged, this, [this, a]
    {
      m_units[a].clear();
      m_last.fill(qQNaN());
      m_lastT.fill(-1);
      m_plottedT = -1;
      m_plot->clear();
      updateAxisUnits();
    });
    connect(m_feed[a], &ValueFeed::captionChanged, this, [this, a](const QString &c)
    {
      m_source[a] = c;
      updateLegend();
    });
  }
}

void HeatmapView::setValues(const ValueRef &x, const ValueRef &y, const ValueRef &z)
{
  m_ref = {x, y, z};
  m_last = {qQNaN(), qQNaN(), qQNaN()};
  m_lastT = {-1, -1, -1};
  m_plottedT = -1;
  for (auto &n : m_name) n.clear();
  m_units.fill(QString());
  m_plot->clear();
  updateAxisUnits();
  for (int a = 0; a < 3; ++a)
    m_feed[a]->setValue(m_ref[a]);
  updateLegend();
}

void HeatmapView::updateAxisUnits()
{
  m_plot->setAxisUnits(m_units[X], m_units[Y], m_units[Z]);
}

void HeatmapView::updateLegend()
{
  const char *axisName[3] = {"X", "Y", "Z"};
  QStringList parts;
  for (int a = 0; a < 3; ++a)
  {
    QString text = m_source[a];
    const QString valueName = m_name[a].isEmpty() ? tr("Value %1").arg(m_ref[a].id + 1) : m_name[a];
    if (!valueName.isEmpty())
      text += text.isEmpty() ? valueName : QString(" · %1").arg(valueName);
    if (text.isEmpty() && !m_ref[a].source.isEmpty())
      text = m_ref[a].source;
    parts << QString("%1: %2").arg(QLatin1String(axisName[a]), text.isEmpty() ? QStringLiteral("—") : text);
  }
  m_plot->setLegend(parts.join(QStringLiteral("     ")));
}

void HeatmapView::feed(int axis, const Reading &reading)
{
  if (reading.id != m_ref[axis].id)
    return;
  if (m_units[axis] != reading.baseUnit)
  {
    if (!m_units[axis].isEmpty())
    {
      m_last.fill(qQNaN());
      m_lastT.fill(-1);
      m_plottedT = -1;
      m_plot->clear();
    }
    m_units[axis] = reading.baseUnit;
    updateAxisUnits();
  }
  if (reading.noNumber() || reading.overload || !std::isfinite(reading.value))
  {
    m_last[axis] = qQNaN();
    return;
  }
  m_last[axis] = reading.value;
  m_name[axis] = reading.name;
  m_lastT[axis] = reading.t;
  updateLegend();
  tryEmit(axis);
}

void HeatmapView::tryEmit(int axis)
{
  for (double v : m_last)
    if (!std::isfinite(v))
      return;

  // Values from the same meter must belong to the same sample (same timestamp)
  // and are plotted once. Values from different meters are combined as-is,
  // triggered by each new reading, like in XYView.
  bool sameSource = (m_ref[X].source == m_ref[Y].source) && (m_ref[Y].source == m_ref[Z].source);
  if (sameSource)
  {
    if (!(m_lastT[X] == m_lastT[Y] && m_lastT[Y] == m_lastT[Z]) || m_plottedT == m_lastT[axis])
      return;
    m_plottedT = m_lastT[axis];
  }
  m_plot->addPoint(m_last[X], m_last[Y], m_last[Z]);
}
