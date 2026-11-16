// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/qcpgraphview.h"

#include <QFileInfo>
#include <QScopedValueRollback>

#include "recording/recordingfile.h"
#include "recording/recordingstore.h"

#include "service/devicesession.h"
#include "service/metercontroller.h"
#include "ui/graphmodel.h"
#include "ui/views/qcpgraph.h"

QcpGraphView::QcpGraphView(DeviceSession *session, QcpGraph *graph, const QString &key, QObject *parent)
  : QObject(parent), m_session(session), m_graph(graph), m_model(new GraphModel(this)), m_key(key)
{
  // window and "All" both ways: the model is what the bench keeps, the
  // graph changes them itself (zoom, time buttons, "All" growing)
  m_graph->setWindowSeconds(m_model->windowSeconds());
  m_graph->setFollowAll(m_model->followAll());
  connect(m_model, &GraphModel::windowChanged, m_graph, &QcpGraph::setWindowSeconds);
  connect(m_model, &GraphModel::followAllChanged, m_graph, &QcpGraph::setFollowAll);
  connect(m_graph, &QcpGraph::windowChanged, m_model, &GraphModel::setWindowSeconds);
  connect(m_graph, &QcpGraph::followAllChanged, m_model, &GraphModel::setFollowAll);
  m_graph->setStatsVisible(m_model->statsVisible());
  connect(m_model, &GraphModel::statsVisibleChanged, m_graph, &QcpGraph::setStatsVisible);
  connect(m_graph, &QcpGraph::statsVisibleChanged, m_model, &GraphModel::setStatsVisible);
  for (const MeterController *ctl : m_session->controllers())
    watch(ctl);
  connect(m_session, &DeviceSession::opened, this, &QcpGraphView::watch);
  // the values' stores are the meters', their names the legend's
  m_graph->setSources([this](const QString &source) -> RecordingStore *
  {
    MeterController *ctl = m_session->controller(source);
    return ctl && ctl != m_closing ? ctl->recorder() : nullptr;
  },
  [this](const QString &source)
  {
    const MeterController *ctl = m_session->controller(source);
    return ctl ? ctl->deviceName() : QString();
  });
  m_graph->setValues(m_model->values());
  m_graph->setValueAxes(m_model->valueAxes());
  m_graph->setFixedRanges(m_model->fixedRanges());
  connect(m_model, &GraphModel::valuesChanged, this, [this] { m_graph->setValues(m_model->values()); });
  connect(m_model, &GraphModel::valueAxesChanged, this, [this] { m_graph->setValueAxes(m_model->valueAxes()); });
  connect(m_model, &GraphModel::fixedRangesChanged, this, [this] { m_graph->setFixedRanges(m_model->fixedRanges()); });
  connect(m_model, &GraphModel::lookChanged, this, &QcpGraphView::applyLook);
  connect(m_session, &DeviceSession::opened, this, &QcpGraphView::followBase);
  // a store goes with its meter: let go of it before
  connect(m_session, &DeviceSession::closing, this, [this](MeterController *ctl)
  {
    const QScopedValueRollback<MeterController *> going(m_closing, ctl);
    followBase();
  });
}

void QcpGraphView::watch(const MeterController *ctl)
{
  connect(ctl->recorder(), &RecordingStore::stateChanged, this, [this](RecordingStore::State state)
  {
    if (m_showsFile && state != RecordingStore::View)
      closeFile();
  });
}

bool QcpGraphView::loadFile(const QString &path, QString *error)
{
  const std::optional<Recording> rec = RecordingFile::read(path, error);
  if (!rec)
    return false;
  if (!m_fileStore)
    m_fileStore = new RecordingStore(this);
  // the columns of the file's own meter; those of others come with several values
  m_fileStore->load(RecordingFile::devicePart(*rec, rec->device));
  m_showsFile = true;
  m_graph->setStore(m_fileStore);
  m_graph->setSourceName(QFileInfo(path).fileName());
  return true;
}

void QcpGraphView::closeFile()
{
  if (!m_showsFile)
    return;
  m_showsFile = false;
  m_graph->setSourceName(QString());
  m_graph->setStore(nullptr);
}

void QcpGraphView::followBase()
{
  m_graph->sourcesChanged();
}

void QcpGraphView::setDefaultVariant(const QString &name)
{
  m_defaultVariant = name;
  applyLook();
}

void QcpGraphView::applyLook()
{
  m_graph->setVariant(GraphLook::fromName(m_model->variant().isEmpty() ? m_defaultVariant : m_model->variant()));
  m_graph->setPatternedLines(m_model->patternedLines());
}
