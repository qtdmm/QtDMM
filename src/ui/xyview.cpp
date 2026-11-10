// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/xyview.h"

#include "service/metercontroller.h"
#include "service/valuefeed.h"
#include "ui/views/xyplot.h"

#include <cmath>

XYView::XYView(DeviceSession *session, XYPlot *plot, QObject *parent) :
  QObject(parent),
  m_plot(plot),
  m_xFeed(new ValueFeed(session, this)),
  m_yFeed(new ValueFeed(session, this))
{
  connect(m_xFeed, &ValueFeed::reading, this, &XYView::feedX);
  connect(m_yFeed, &ValueFeed::reading, this, &XYView::feedY);
  connect(m_xFeed, &ValueFeed::controllerChanged, plot, &XYPlot::clear);
  connect(m_yFeed, &ValueFeed::controllerChanged, plot, &XYPlot::clear);
  connect(m_xFeed, &ValueFeed::captionChanged, this, [this](const QString &caption)
  {
    m_xSource = caption;
    updateLegend();
  });
  connect(m_yFeed, &ValueFeed::captionChanged, this, [this](const QString &caption)
  {
    m_ySource = caption;
    updateLegend();
  });
}

void XYView::setValues(const ValueRef &x, const ValueRef &y)
{
  m_x = x;
  m_y = y;
  m_lastX = m_lastY = qQNaN();
  m_lastXT = m_lastYT = m_plottedT = -1;
  m_xName.clear();
  m_yName.clear();
  m_plot->clear();
  m_plot->setAxisCaptions(tr("X axis"), tr("Y axis"));
  m_xFeed->setValue(x);
  m_yFeed->setValue(y);
  updateLegend();
}

void XYView::updateLegend()
{
  const auto label = [](const QString &axis, const QString &source, const QString &name, const ValueRef &value)
  {
    QString text = source;
    if (!name.isEmpty())
      text += text.isEmpty() ? name : QString(" · %1").arg(name);
    if (text.isEmpty() && !value.source.isEmpty())
      text = value.source;
    return QString("%1: %2").arg(axis, text.isEmpty() ? QStringLiteral("—") : text);
  };
  m_plot->setLegend(QString("%1     %2").arg(label(tr("X"), m_xSource, m_xName, m_x),
                                           label(tr("Y"), m_ySource, m_yName, m_y)));
}

void XYView::feedX(const Reading &reading)
{
  if (reading.id != m_x.id)
    return;
  if (reading.noNumber() || reading.overload || !std::isfinite(reading.value))
  {
    m_lastX = qQNaN();
    return;
  }
  m_lastX = reading.value;
  m_xName = reading.name;
  updateLegend();
  m_lastXT = reading.t;
  if (std::isfinite(m_lastY) && (m_x.source != m_y.source || (m_lastXT == m_lastYT && m_plottedT != m_lastXT)))
  {
    m_plot->addPoint(m_lastX, m_lastY);
    m_plottedT = m_lastXT;
  }
}

void XYView::feedY(const Reading &reading)
{
  if (reading.id != m_y.id)
    return;
  if (reading.noNumber() || reading.overload || !std::isfinite(reading.value))
  {
    m_lastY = qQNaN();
    return;
  }
  m_lastY = reading.value;
  m_yName = reading.name;
  updateLegend();
  m_lastYT = reading.t;
  if (std::isfinite(m_lastX) && (m_x.source != m_y.source || (m_lastXT == m_lastYT && m_plottedT != m_lastYT)))
  {
    m_plot->addPoint(m_lastX, m_lastY);
    m_plottedT = m_lastYT;
  }
}
