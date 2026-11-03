// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/poincareview.h"

#include "service/metercontroller.h"
#include "service/valuefeed.h"
#include "ui/views/poincareplot.h"

PoincareView::PoincareView(DeviceSession *session, PoincarePlot *plot, QObject *parent) :
  QObject(parent),
  m_plot(plot),
  m_feed(new ValueFeed(session, this))
{
  connect(m_feed, &ValueFeed::reading, plot, &PoincarePlot::addReading);
  connect(m_feed, &ValueFeed::staleChanged, plot, &PoincarePlot::setStale);
  connect(m_feed, &ValueFeed::controllerChanged, plot, &PoincarePlot::clear);
  connect(m_feed, &ValueFeed::captionChanged, plot, &PoincarePlot::setCaption);
  connect(m_feed, &ValueFeed::noticeChanged, plot, &PoincarePlot::setNotice);
}

void PoincareView::setValue(const ValueRef &value)
{
  m_value = value;
  m_feed->setValue(value);
  m_plot->setValueId(value.id);
}
