// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/histogramview.h"

#include "service/metercontroller.h"
#include "service/valuefeed.h"
#include "ui/views/histogramplot.h"

HistogramView::HistogramView(DeviceSession *session, HistogramPlot *plot, QObject *parent) :
  QObject(parent),
  m_plot(plot),
  m_feed(new ValueFeed(session, this))
{
  connect(m_feed, &ValueFeed::reading, this, [this](const Reading &reading)
  {
    m_plot->addReading(reading);
    if (reading.id == m_value.id && !reading.name.isEmpty())
    {
      m_valueName = reading.name;
      updateCaption();
    }
  });
  connect(m_feed, &ValueFeed::controllerChanged, plot, &HistogramPlot::clear);
  connect(m_feed, &ValueFeed::captionChanged, this, [this](const QString &caption)
  {
    m_sourceCaption = caption;
    updateCaption();
  });
  connect(m_feed, &ValueFeed::noticeChanged, plot, &HistogramPlot::setNotice);
}

void HistogramView::setValue(const ValueRef &value)
{
  m_value = value;
  m_valueName.clear();
  updateCaption();
  m_feed->setValue(value);
  m_plot->setValueId(value.id);
}

void HistogramView::updateCaption()
{
  QString caption = m_sourceCaption;
  if (!m_valueName.isEmpty())
    caption += caption.isEmpty() ? m_valueName : QString(" · %1").arg(m_valueName);
  m_plot->setCaption(caption);
}
