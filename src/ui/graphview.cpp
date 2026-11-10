// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/graphview.h"

#include <QScopedValueRollback>

#include "core/alarm.h"
#include "device/dmmdecoder.h"
#include "recording/recordingstore.h"
#include "service/devicesession.h"
#include "service/metercontroller.h"
#include "ui/views/graphwidget.h"

GraphView::GraphView(DeviceSession *session, GraphWidget *graph, const QString &key, QObject *parent)
  : QObject(parent), m_session(session), m_graph(graph), m_key(key)
{
  // the graph shows the store of the meter of its first value, the values
  // of other meters from their stores; without a meter an empty one
  m_emptyStore = new RecordingStore(graph);   // outlives this view with its graph
  m_emptyStore->live();
  m_graph->setStore(m_emptyStore);
  // the recording is the session's: the graph's store may follow another meter's
  m_graph->setControl({ [this] { m_session->startRecording(); leaveFile(); }, [this] { m_session->stopRecording(); },
                        [this] { m_session->goLive(); leaveFile(); },
                        [this] { m_session->clearRecording(); leaveFile(); } });
  m_graph->setOtherStores([this](const QString &source) -> RecordingStore *
  {
    MeterController *ctl = m_session->controller(source);
    return ctl ? ctl->recorder() : nullptr;
  },
  [this](const QString &source)
  {
    const MeterController *ctl = m_session->controller(source);
    return ctl ? ctl->deviceName() : QString();
  });
  // the vendors, for the legend to leave out when it gets narrow
  QStringList vendors;
  for (const DmmDecoder::DMMInfo &info : DmmDecoder::getDeviceConfigurations())
    if (!info.vendor.isEmpty() && !vendors.contains(info.vendor))
      vendors << info.vendor;
  m_graph->setVendorNames(vendors);
  connect(m_session, &DeviceSession::opened, m_graph, &GraphWidget::otherStoresChanged);
  connect(m_session, &DeviceSession::closed, m_graph, &GraphWidget::otherStoresChanged);
  // the graph's store: of the meter of its first value while it shows none
  // of the meter before; that meter closed, of the next value's or empty
  connect(m_graph, &GraphWidget::valuesChanged, this, &GraphView::followBase);
  connect(m_session, &DeviceSession::opened, this, &GraphView::followBase);
  connect(m_session, &DeviceSession::closing, this, [this](MeterController *ctl)
  {
    if (ctl != m_base)
      return;
    // still in the session while it closes: not the next base
    const QScopedValueRollback<MeterController *> going(m_closing, ctl);
    MeterController *next = nullptr;
    for (const ValueRef &v : m_graph->values())
      if (MeterController *other = m_session->controller(v); other && other != ctl)
      {
        next = other;
        break;
      }
    setBase(next);
  });
  // the graph's band: the test of the meter whose value is on its left axis
  connect(m_graph, &GraphWidget::leftValueChanged, this, [this](const ValueRef &value)
  {
    disconnect(m_limitLink);
    if (const MeterController *ctl = value.source.isEmpty() ? nullptr : m_session->controller(value.source))
      m_limitLink = connect(ctl, &MeterController::limitTestChanged, this, &GraphView::applyLimitBand);
    applyLimitBand();
  });
}

void GraphView::followBase()
{
  const QList<ValueRef> values = m_graph->values();
  MeterController *base = m_base.data();
  if (values.isEmpty() || std::any_of(values.cbegin(), values.cend(), [this](const ValueRef &v) { return m_graph->isBase(v); }))
    return;
  // the first value whose meter is open: one not connected (yet) is none
  for (const ValueRef &v : values)
    if (MeterController *next = m_session->controller(v); next && next != m_closing)
    {
      if (next != base)
        setBase(next);
      return;
    }
}

void GraphView::leaveFile()
{
  // a file loaded without a device lies in the graph's own store and is no
  // value of any meter: the graph starts afresh, as an empty one
  if (m_graph->store() != m_emptyStore || !m_graph->showsFile())
    return;
  m_emptyStore->live();
  m_graph->setValues({});
  if (!m_session->controllers().isEmpty())
    m_graph->setValues({ ValueRef(0, m_session->controllers().first()->source()) });
}

void GraphView::setBase(MeterController *ctl)
{
  disconnect(m_unit);
  disconnect(m_alarms);
  m_base = ctl;
  m_graph->setBase(ctl ? ctl->recorder() : m_emptyStore, ctl ? ctl->source() : QString());
  if (ctl)
  {
    m_unit = connect(ctl, &MeterController::unitChanged, m_graph, &GraphWidget::setUnit);
    m_alarms = connect(ctl, &MeterController::alarmsChanged, this, &GraphView::applyAlarmThresholds);
  }
  applyAlarmThresholds();
}

void GraphView::applyAlarmThresholds()
{
  // the reference line snaps to the thresholds of the alarms on the main value
  QList<double> thresholds;
  if (m_base)
    for (const Alarm &a : m_base->alarms()->alarms())
    {
      if (!a.enabled || a.value.id != 0 || a.condition == Alarm::Overload || a.condition == Alarm::NoReadings)
        continue;
      thresholds << a.a;
      if (a.condition == Alarm::Outside || a.condition == Alarm::Inside)
        thresholds << a.b;
    }
  m_graph->setAlarmThresholds(thresholds);
}

void GraphView::applyLimitBand()
{
  const ValueRef ref = m_graph->leftValue();
  const MeterController *ctl = ref.id < 0 ? nullptr : m_session->controller(ref.source);
  if (ctl && ctl->limitTest().enabled
      && (ctl->limitTest().value.source.isEmpty() || ctl->limitTest().value.source == ctl->source()))
  {
    const LimitTest &test = ctl->limitTest();
    m_graph->setLimits(ValueRef(test.value.id, ref.source), test.low(), test.high());
  }
  else
    m_graph->setLimits(-1);
}

QPointer<MeterController> GraphView::recordingLead() const
{
  // the session's recording: every meter that recorded with all its values,
  // as QTDMM:RECord:SAVE writes it; a file viewed or Live: what the graph has
  QPointer<MeterController> lead = m_session->lead();
  if (m_graph->showsFile() || !lead || lead->recorder()->state() != RecordingStore::View
      || lead->recorder()->count() <= 0)
    return nullptr;
  return lead;
}
