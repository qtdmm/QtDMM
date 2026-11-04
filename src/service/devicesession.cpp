// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#include "service/devicesession.h"

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

CalcDevice::Inputs DeviceSession::calcInputs() const
{
  // a calculated meter may outlive the session by a deleteLater()
  return [self = QPointer<const DeviceSession>(this)]
  {
    QMap<QString, CalcDevice::Input> out;
    if (!self)
      return out;
    for (const MeterController *ctl : self->controllers())
    {
      const Reading &r = ctl->lastReading();
      if (ctl->deviceName().isEmpty() || r.msecs == 0)
        continue;
      out.insert(variableName(ctl->deviceName()), { r.value, r.baseUnit, r.msecs, !r.hold && !r.overload && !r.noNumber() });
    }
    return out;
  };
}

MeterController *DeviceSession::open(const QString &id, QString *error)
{
  const std::optional<MyDevice> entry = m_library ? m_library->find(id) : std::nullopt;
  if (!entry)
  {
    if (error)
      *error = "-224,Illegal parameter value; no such device: " + id;
    return nullptr;
  }
  return open(entry->id, entry->keys, entry->name, error);
}

MeterController *DeviceSession::open(const QString &id, const QVariantMap &keys, const QString &name, QString *error)
{
  if (controller("device:" + id))
  {
    if (error)
      *error = "-221,Settings conflict; open already: " + name;
    return nullptr;
  }
  const MeterSetup setup = MeterSetup::fromKeys(keys, m_sigrokExe);
  auto *ctl = new MeterController(this, m_host);
  ctl->setInstanceId(m_instanceId);
  ctl->setDevice(id, name);
  ctl->setModel(setup.knownModel ? setup.info.name : QStringLiteral("Manual"));
  ctl->setValueCount(setup.valueCount);
  ctl->setFollowsWindow(true);
  ctl->configure(setup);
  ctl->dmm()->setCalcInputs(calcInputs());
  // its messages to the status bar: a lost connection, a recording stopped
  connect(ctl, &MeterController::error, this, [this, ctl](const QString &text)
  {
    // a connection's message is in the log with its state
    if (text != ctl->dmm()->errorString())
      AppLog::error(ctl->deviceName(), text);
    Q_EMIT this->error(tr("%1: %2").arg(ctl->deviceName(), text));
  });
  connect(ctl, &MeterController::info, this, [this, ctl](const QString &text)
  {
    AppLog::info(ctl->deviceName(), text);
    Q_EMIT info(tr("%1: %2").arg(ctl->deviceName(), text));
  });
  connect(ctl->dmm(), &MeterConnection::linkStateChanged, this, [ctl](MeterConnection::LinkState state, const QString &detail)
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
  if (!ctl->connectMeter(true))
  {
    // SCPI in English, the reason in the status bar in the user's language
    if (error)
      *error = "-240,Hardware error; " + name + ": does not connect";
    Q_EMIT this->error(tr("%1: %2").arg(name, ctl->dmm()->errorString()));
    delete ctl;
    return nullptr;
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
