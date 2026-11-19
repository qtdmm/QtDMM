// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#include "service/devicesession.h"
#include "logic/legacynodes.h"
#include "service/noderunner.h"

#include <QDateTime>

#include <algorithm>
#include <utility>

#include "core/applog.h"

#include "core/devicelibrary.h"
#include "core/sampletypes.h"
#include "device/meterconnection.h"
#include "device/metersetup.h"
#include "recording/recordingstore.h"
#include "service/metercontroller.h"
#include "service/scpiserver.h"
#include "service/servicehost.h"

DeviceSession::DeviceSession(QObject *parent)
  : QObject(parent)
  , m_host(new ServiceHost(this))
{
  installScpi();
}

DeviceSession::~DeviceSession()
{
  qDeleteAll(std::exchange(m_opened, {}));
}

void DeviceSession::setConfigDir(const QString &dir)
{
  delete m_library;
  m_library = new DeviceLibrary(dir, this);
  // renamed in My devices: the open meter takes the name
  connect(m_library, &DeviceLibrary::changed, this, [this]
  {
    for (MeterController *ctl : controllers())
      if (const std::optional<MyDevice> entry = m_library->find(ctl->deviceId()); entry && entry->name != ctl->deviceName())
        ctl->setDevice(entry->id, entry->name);
  });
}

void DeviceSession::watchRecording(MeterController *ctl)
{
  // the one that leads starts, stops and clears the others
  connect(ctl->recorder(), &RecordingStore::stateChanged, this, [this, ctl](int state)
  {
    if (ctl == lead())
      followLead(state);
    Q_EMIT recordingStateChanged();
  });
  connect(ctl->recorder(), &RecordingStore::cleared, this, [this, ctl]
  {
    if (ctl != lead() || !ctl->recorder()->isRunning())
      return;
    for (MeterController *other : controllers())
      if (other != ctl && other->recorder()->isRunning())
        other->recorder()->clear();
  });
  // its recording saved over SCPI: with those that recorded with it, on its time
  ctl->setRecordedWith([this, ctl]
  {
    QList<MeterController *> with;
    for (MeterController *other : controllers())
      if (other != ctl && other->recorder()->count() > 0 && other->recorder()->startClock() == ctl->recorder()->startClock())
        with << other;
    return with;
  });
}

MeterController *DeviceSession::lead() const
{
  // a recording keeps the one it started with
  if (m_running)
    return m_running;
  if (MeterController *ctl = controller(m_setup.trigger))
    return ctl;
  const QList<MeterController *> all = controllers();
  return all.isEmpty() ? nullptr : all.first();
}

void DeviceSession::setRecordingSetup(const RecordingStore::Setup &setup)
{
  m_setup = setup;
  applySetup();
}

void DeviceSession::applySetup()
{
  const MeterController *first = lead();
  for (MeterController *ctl : controllers())
    ctl->recorder()->applySetup(m_setup, ctl == first);
}

void DeviceSession::startRecording()
{
  if (MeterController *ctl = lead())
    ctl->recorder()->start();
}

void DeviceSession::stopRecording()
{
  if (MeterController *ctl = lead())
    ctl->recorder()->stop();
}

void DeviceSession::goLive()
{
  if (MeterController *ctl = lead())
    ctl->recorder()->live();
}

void DeviceSession::clearRecording()
{
  if (MeterController *ctl = lead())
    ctl->recorder()->clear();
}

bool DeviceSession::isRecording() const
{
  const MeterController *ctl = lead();
  return ctl && ctl->recorder()->isRunning();
}

QStringList DeviceSession::unsavedRecordings(const QString &id) const
{
  QStringList names;
  for (const MeterController *ctl : m_opened)
  {
    const RecordingStore *store = ctl->recorder();
    if ((id.isEmpty() || ctl->deviceId() == id)
        && (store->isRunning() || (store->state() == RecordingStore::View && store->dirty())))
      names << ctl->deviceName();
  }
  return names;
}

void DeviceSession::followLead(int state)
{
  MeterController *first = lead();
  RecordingStore *lead = first->recorder();
  QList<MeterController *> others = controllers();
  others.removeAll(first);
  if (state == RecordingStore::Record)
  {
    m_running = first;
    m_scope = std::exchange(m_nextScope, std::nullopt);
    // an integral counts from the recording's start
    for (MeterController *ctl : controllers())
      if (NodeRunner *runner = NodeRunner::of(ctl); runner && runner->node()->resetsOnRecording()
                                                     && (ctl == first || inScope(ctl)))
        runner->reset();
    // its start stays where it is, also when its first reading comes later:
    // the others record on it. Alone it begins with its first reading
    if (std::any_of(others.cbegin(), others.cend(), [this](const MeterController *ctl) { return inScope(ctl); }))
      lead->holdOrigin();
    for (MeterController *ctl : std::as_const(others))
      if (inScope(ctl))
        startFollower(ctl, lead->startClock(), lead->startDateTime(), lead->sampleTime());
    return;
  }
  m_scope.reset();
  m_running = nullptr;
  if (state == RecordingStore::Live)
    // live with it: a start with the pre-trigger finds their readings too
    for (MeterController *ctl : std::as_const(others))
      liveFollower(ctl);
  else
    for (MeterController *ctl : std::as_const(others))
      ctl->recorder()->stop();
}

void DeviceSession::setRecordingScope(const QStringList &sources)
{
  m_nextScope = sources;
}

bool DeviceSession::inScope(const MeterController *ctl) const
{
  return !m_scope || m_scope->contains(ctl->source());
}

void DeviceSession::liveFollower(MeterController *ctl)
{
  warnUnsaved(ctl);
  ctl->recorder()->live();
}

void DeviceSession::warnUnsaved(MeterController *ctl)
{
  const RecordingStore *store = ctl->recorder();
  if (store->state() == RecordingStore::View && store->dirty())
  {
    AppLog::warning(ctl->deviceName(), "the recording was not saved, the new one replaces it");
    Q_EMIT error(tr("%1: the recording was not saved, the new one replaces it").arg(ctl->deviceName()));
  }
}

void DeviceSession::startFollower(MeterController *ctl, qint64 t0, const QDateTime &start, int sampleTime)
{
  RecordingStore *store = ctl->recorder();
  warnUnsaved(ctl);
  if (sampleTime > 0)
    store->setSampleTime(sampleTime);
  store->startAt(t0, start);
}

void DeviceSession::onRecordingRequested(bool start)
{
  if (m_requests)
  {
    // the window's Record button: its questions (length, unsaved data) first
    QString why;
    MeterController *by = lead();
    // the recording it shows not saved: nobody could be asked about it
    if (start && by && by->recorder()->state() == RecordingStore::View && by->recorder()->dirty())
      why = tr("start ignored, the recording shown is not saved yet");
    if (start && by && (!why.isEmpty() || !by->mayStartRecording(&why)))
    {
      m_host->scpi()->pushError(-213, "Init ignored");
      AppLog::error(by->deviceName(), why);
      Q_EMIT error(tr("%1: %2").arg(by->deviceName(), why));
      return;
    }
    m_requests(start);
    return;
  }
  if (start)
    startRecording();
  else
    stopRecording();
}

QString DeviceSession::variableName(const QString &name)
{
  QString out = name.toLower();
  for (QChar &c : out)
    if (!c.isLetterOrNumber() && c != '_')
      c = '_';
  // a name begins with a letter: "34465A" is _34465a
  return !out.isEmpty() && out[0].isDigit() ? '_' + out : out;
}

QString DeviceSession::sourceOfVariable(const QString &name) const
{
  if (!m_library)
    return QString();
  for (const MyDevice &d : m_library->list())
    if (variableName(d.name) == name)
      return "device:" + d.id;
  return QString();
}

MeterController *DeviceSession::open(const QString &id, QString *error, bool waitForDevice)
{
  const std::optional<MyDevice> entry = m_library ? m_library->find(id) : std::nullopt;
  if (!entry)
  {
    if (error)
      *error = "-224,Illegal parameter value; no such device: " + id;
    return nullptr;
  }
  return open(entry->id, entry->keys, entry->name, error, waitForDevice);
}

MeterController *DeviceSession::open(const QString &id, const QVariantMap &keys, const QString &name, QString *error,
                                     bool waitForDevice)
{
  if (controller("device:" + id))
  {
    if (error)
      *error = "-221,Settings conflict; open already: " + name;
    return nullptr;
  }
  // a logic node of My devices: opened as a node; a simulated meter or a
  // calculated value of before as one too (LegacyNodes)
  if (keys.contains(QStringLiteral("Node/json")))
  {
    std::unique_ptr<LogicNode> node = LogicNode::fromJsonText(keys.value(QStringLiteral("Node/json")).toString());
    if (!node)
    {
      if (error)
        *error = "-221,Settings conflict; unknown kind of calculated value: " + name;
      return nullptr;
    }
    return openNode(id, name, std::move(node), error);
  }
  if (LegacyNodes::isLegacy(keys))
  {
    QString why;
    std::unique_ptr<LogicNode> node = LegacyNodes::fromKeys(keys, [this](const QString &var) { return sourceOfVariable(var); }, &why);
    if (!node)
    {
      if (error)
        *error = "-221,Settings conflict; " + why + ": " + name;
      return nullptr;
    }
    return openNode(id, name, std::move(node), error);
  }
  const MeterSetup setup = MeterSetup::fromKeys(keys, m_sigrokExe);
  auto *ctl = new MeterController(this, m_host);
  ctl->setModel(setup.knownModel ? setup.info.name : QStringLiteral("Manual"));
  ctl->setValueCount(setup.valueCount);
  ctl->configure(setup);
  return adopt(ctl, id, name, error, waitForDevice);
}

MeterController *DeviceSession::openNode(const QString &id, const QString &name, std::unique_ptr<LogicNode> node,
                                         QString *error)
{
  const QString source = "device:" + id;
  if (controller(source))
  {
    if (error)
      *error = "-221,Settings conflict; open already: " + name;
    return nullptr;
  }
  if (!node || LogicNode::makesLoop(source, node->inputs(), [this](const QString &s) { return nodeInputs(s); }))
  {
    if (error)
      *error = "-221,Settings conflict; its inputs close a loop: " + name;
    return nullptr;
  }
  auto *ctl = new MeterController(this, m_host, MeterController::Kind::Node);
  // a node's "model": QtDMM and its kind, as many values as outputs
  DmmDecoder::DMMInfo info;
  info.vendor = QStringLiteral("QtDMM");
  info.model = node->type().left(1).toUpper() + node->type().mid(1);   // "Formula", "Generator"
  info.name = info.vendor + ' ' + info.model;
  info.channels = qBound(1, int(node->outputs().size()), kMaxValues);
  info.display = 60000;
  ctl->setNodeInfo(info);
  ctl->setModel(info.name);
  ctl->setValueCount(info.channels);
  new NodeRunner(this, ctl, std::move(node));
  return adopt(ctl, id, name, error, false);
}

QList<ValueRef> DeviceSession::nodeInputs(const QString &source) const
{
  const NodeRunner *runner = NodeRunner::of(controller(source));
  return runner ? runner->node()->inputs() : QList<ValueRef>();
}

MeterController *DeviceSession::adopt(MeterController *ctl, const QString &id, const QString &name, QString *error,
                                      bool waitForDevice)
{
  ctl->setInstanceId(m_instanceId);
  ctl->setDevice(id, name);
  ctl->setFollowsWindow(true);
  // its messages to the status bar: a lost connection, a recording stopped
  connect(ctl, &MeterController::error, this, [this, ctl](const QString &text)
  {
    // a connection's message is in the log with its state
    if (text != ctl->errorString())
      AppLog::error(ctl->deviceName(), text);
    Q_EMIT this->error(tr("%1: %2").arg(ctl->deviceName(), text));
  });
  connect(ctl, &MeterController::info, this, [this, ctl](const QString &text)
  {
    AppLog::info(ctl->deviceName(), text);
    Q_EMIT info(tr("%1: %2").arg(ctl->deviceName(), text));
  });
  connect(ctl, &MeterController::linkStateChanged, this, [ctl](MeterConnection::LinkState state, const QString &detail)
  {
    using State = MeterConnection::LinkState;
    AppLog::instance()->add(state == State::Error ? AppLog::Error : state == State::Timeout ? AppLog::Warning : AppLog::Info,
                            ctl->deviceName(), detail);
  });
  // no window to ask: INPut ON|OFF goes to the connection
  connect(ctl, &MeterController::connectRequested, ctl, [ctl](bool on) { ctl->connectMeter(on); });
  connect(ctl, &MeterController::recordingRequested, this, &DeviceSession::onRecordingRequested);
  // an alarm's program with "Disconnect first" has the port for a while
  connect(ctl, &MeterController::portReleased, ctl, [ctl](bool released) { ctl->connectMeter(!released); });
  applyRules(ctl, id);
  if (!ctl->connectMeter(true, waitForDevice))
  {
    // SCPI in English, the reason in the status bar in the user's language
    if (error)
      *error = "-240,Hardware error; " + name + ": does not connect";
    Q_EMIT this->error(tr("%1: %2").arg(name, ctl->errorString()));
    if (!waitForDevice)
    {
      delete ctl;
      return nullptr;
    }
    // it stays, not connected, and connects when the device comes
  }
  MeterController *leading = lead();   // before it is one of them
  m_opened << ctl;
  watchRecording(ctl);
  applySetup();
  // opened while the session records: it joins, on the same time base
  if (leading && leading->recorder()->isRunning() && inScope(ctl))
  {
    RecordingStore *store = leading->recorder();
    store->holdOrigin();
    startFollower(ctl, store->startClock(), store->startDateTime(), store->sampleTime());
  }
  else if (!leading || leading->recorder()->state() == RecordingStore::Live)
    ctl->recorder()->live();
  Q_EMIT opened(ctl);
  return ctl;
}

void DeviceSession::applyRules(MeterController *ctl, const QString &id)
{
  const std::optional<MyDevice> entry = m_library ? m_library->find(id) : std::nullopt;
  ctl->setAlarms(Alarm::listFromJson(entry ? entry->alarms : QString()));
  ctl->setLimitTest(LimitTest::fromJsonText(entry ? entry->limitTest : QString()));
  // a change, from the window or a SCPI client, goes to the entry
  auto store = [this, ctl, id]
  {
    if (m_library)
      m_library->setRules(id, ctl->alarms()->alarms().isEmpty() ? QString() : Alarm::listToJson(ctl->alarms()->alarms()),
                          ctl->limitTest().toJsonText());
  };
  connect(ctl, &MeterController::alarmsChanged, this, store);
  connect(ctl, &MeterController::limitTestChanged, this, store);
}

bool DeviceSession::close(const QString &id, QString *error)
{
  const QString source = "device:" + id;
  for (MeterController *ctl : std::as_const(m_opened))
    if (ctl->source() == source)
    {
      // the one that leads a recording ends it for all
      if (ctl == m_running)
        ctl->recorder()->stop();
      Q_EMIT closing(ctl);
      m_opened.removeOne(ctl);
      ctl->connectMeter(false);
      delete ctl;
      applySetup();
      Q_EMIT closed(source);
      Q_EMIT recordingStateChanged();
      return true;
    }
  if (error)
    *error = "-224,Illegal parameter value; not open: " + id;
  return false;
}

QList<MeterController *> DeviceSession::controllers() const
{
  return m_opened;
}

MeterController *DeviceSession::controller(const QString &source) const
{
  for (MeterController *ctl : controllers())
    if (ctl->source() == source)
      return ctl;
  return nullptr;
}

MeterController *DeviceSession::controller(const ValueRef &value) const
{
  return value.source.isEmpty() ? nullptr : controller(value.source);
}

void DeviceSession::installScpi()
{
  ScpiServer::DeviceControl control;
  control.catalog = [this]
  {
    QList<ScpiServer::DeviceEntry> out;
    if (m_library)
      for (const MyDevice &d : m_library->list())
        out << ScpiServer::DeviceEntry { d.id, d.name, controller("device:" + d.id) != nullptr };
    return out;
  };
  control.open = [this](const QString &id)
  {
    QString error;
    open(id, &error);
    return error;
  };
  control.close = [this](const QString &id)
  {
    QString error;
    close(id, &error);
    return error;
  };
  m_host->scpi()->setDeviceControl(control);
}
