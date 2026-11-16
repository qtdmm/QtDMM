// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/qcpgraphview.h"

#include <QScopedValueRollback>

#include "service/devicesession.h"
#include "service/metercontroller.h"
#include "ui/graphmodel.h"
#include "ui/views/qcpgraph.h"

QcpGraphView::QcpGraphView(DeviceSession *session, QcpGraph *graph, const QString &key, QObject *parent)
  : QObject(parent), m_session(session), m_graph(graph), m_model(new GraphModel(this)), m_key(key)
{
  m_graph->setWindowSeconds(m_model->windowSeconds());
  connect(m_model, &GraphModel::windowChanged, m_graph, &QcpGraph::setWindowSeconds);
  connect(m_model, &GraphModel::valuesChanged, this, &QcpGraphView::followBase);
  connect(m_session, &DeviceSession::opened, this, &QcpGraphView::followBase);
  // the store goes with its meter: let go of it before
  connect(m_session, &DeviceSession::closing, this, [this](MeterController *ctl)
  {
    if (ctl != m_base)
      return;
    const QScopedValueRollback<MeterController *> going(m_closing, ctl);
    followBase();
  });
}

void QcpGraphView::followBase()
{
  const QList<ValueRef> values = m_model->values();
  MeterController *base = values.isEmpty() ? nullptr : m_session->controller(values.first());
  if (base == m_closing)
    base = nullptr;
  m_base = base;
  m_graph->setStore(base ? base->recorder() : nullptr);
}
