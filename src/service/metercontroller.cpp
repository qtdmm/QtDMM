// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "service/metercontroller.h"

#include <cmath>
#include <limits>

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHostInfo>
#include <QProcess>
#include <QTimer>
#include <QRegularExpression>
#include <QStandardPaths>

#include "device/meterconnection.h"
#include "device/metersetup.h"
#include "ui/engnumbervalidator.h"
#include "recording/recordingstore.h"
#include "service/scpiserver.h"
#include "service/servicehost.h"
#include "core/siprefix.h"



MeterController::MeterController(QObject *parent, ServiceHost *host)
  : QObject(parent)
  , m_dmm(new MeterConnection(this))
  , m_alarms(new AlarmManager(this))
  , m_recorder(new RecordingStore(this))
  , m_scpiDevice(new ScpiDevice(this))
  , m_host(host ? host : new ServiceHost(this))
  , m_external(new QProcess(this))
{
  qRegisterMetaType<Reading>();
  m_host->scpi()->addDevice(m_scpiDevice);
  connect(m_host, &ServiceHost::statusChanged, this, &MeterController::scpiStatusChanged);
  connect(m_host, &ServiceHost::error, this, &MeterController::error);
  updateScpiName();
  connect(m_dmm, &MeterConnection::response, this, &MeterController::responseSLOT);
  connect(m_dmm, &MeterConnection::error, this, &MeterController::error);
  // SCPI's "connected" (INPut?, STAT:QUES? bit 2) is the link itself: lost
  // and reconnecting (a Bluetooth meter switched off) is not connected
  connect(m_dmm, &MeterConnection::linkStateChanged, this, [this](MeterConnection::LinkState state)
  {
    m_scpiDevice->setConnected(state == MeterConnection::LinkState::Connected);
  });

  // the recorder keeps every reading of the main value, and a gap where it
  // went stale
  connect(this, &MeterController::reading, m_recorder, &RecordingStore::setReading);
  // in its base unit, as the values come (the graph sets the same)
  connect(this, &MeterController::unitChanged, m_recorder,
          [this](const QString &unit) { m_recorder->setUnit(SiPrefix::split(unit).baseUnit); });
  connect(this, &MeterController::staleChanged, m_recorder, &RecordingStore::setStale);
  connect(m_recorder, &RecordingStore::runningChanged, this, &MeterController::setRecording);
  // another function at the meter ended the recording: say so, and mark
  // where in the graph
  connect(m_recorder, &RecordingStore::functionChanged, this, [this](const QString &from, const QString &to)
  {
    Q_EMIT error(tr("Recording stopped: the meter measures %2 now, not %1").arg(from, to));
    Q_EMIT markRequested(QColor(0xff, 0x8c, 0x00), tr("Function changed"), true, false);
  });

  connect(m_alarms, &AlarmManager::raised, this, &MeterController::onAlarmRaised);
  connect(m_alarms, &AlarmManager::cleared, this, [this](int index, const Alarm &alarm)
  {
    Q_EMIT info(tr("%1 cleared").arg(Alarm::title(alarm.name)));
    Q_EMIT alarmCleared(index, alarm);
    updateBanner();
  });

  // What a SCPI command starts in the UI runs after the command: a dialog it
  // opens (the length of a recording, a port without access) must not spin an
  // event loop inside the server, where the client may go away meanwhile. The
  // checks stay at once, so the error queue answers the next command; they
  // count what is checked but not run yet, and the run checks once more.
  connect(m_scpiDevice, &ScpiDevice::startRecording, this, [this]
  {
    QString why;
    if (m_startPending)
      why = tr("already starting, the recording goes on");
    if (!m_startPending && mayStartRecording(&why))
    {
      m_startPending = true;
      QMetaObject::invokeMethod(this, [this]
      {
        QString why;
        if (mayStartRecording(&why))
          Q_EMIT recordingRequested(true);   // may ask the length: pending until it returns
        else
          Q_EMIT info(tr("SCPI: %1").arg(why));
        m_startPending = false;
      }, Qt::QueuedConnection);
    }
    else
    {
      m_host->scpi()->pushError(-213, "Init ignored");
      Q_EMIT info(tr("SCPI: %1").arg(why));
    }
  });
  connect(m_scpiDevice, &ScpiDevice::stopRecording, this, [this]
  {
    QMetaObject::invokeMethod(this, [this] { Q_EMIT recordingRequested(false); }, Qt::QueuedConnection);
  });
  connect(m_scpiDevice, &ScpiDevice::connectRequested, this, [this](bool on)
  {
    // INPut OFF also ends a connection that is lost and being reopened
    if (on == m_connectPending.value_or(m_dmm->isWanted()))
      return;
    m_connectPending = on;
    QMetaObject::invokeMethod(this, [this, on]
    {
      if (on != m_dmm->isWanted())
        Q_EMIT connectRequested(on);   // may show a dialog: pending until it returns
      if (m_connectPending == on)
        m_connectPending.reset();
    }, Qt::QueuedConnection);
  });
  connect(m_scpiDevice, &ScpiDevice::resetMinMaxRequested, this, &MeterController::resetMinMax);
  connect(m_scpiDevice, &ScpiDevice::acknowledgeAlarmsRequested, this, &MeterController::acknowledgeAlarms);
  connect(m_scpiDevice, &ScpiDevice::markRequested, this, [this](const QString &text)
  {
    Q_EMIT markRequested(QColor(0x1e, 0x90, 0xff), text, true, true);
  });
  installScpiSources();

  // an alarm's program that had the port to itself has ended: the meter again
  connect(m_external, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus)
  {
    Q_EMIT info(tr("%1 ended with exit code %2").arg(m_external->program()).arg(exitCode));
    if (m_reopenAfter)
    {
      m_reopenAfter = false;
      Q_EMIT portReleased(false);
    }
  });

  m_clock.start();
  startTimer(100);   // the stale check and the alarms' time base
}

MeterController::~MeterController() = default;

void MeterController::setModel(const QString &model)
{
  m_model = model;
  m_dmm->setName(model);
  m_scpiDevice->setModel(model);
}

void MeterController::setAlarms(const QList<Alarm> &alarms)
{
  const bool other = Alarm::listToJson(alarms) != Alarm::listToJson(m_alarms->alarms());
  m_alarms->setAlarms(alarms);
  updateBanner();
  if (other)
    Q_EMIT alarmsChanged(alarms);
}

void MeterController::setLimitTest(const LimitTest &test)
{
  m_limitTest = test;
  Q_EMIT limitTestChanged(m_limitTest);
  m_verdict = LimitTest::NoVerdict;
  Q_EMIT limitJudged(m_limitTest.value.id, m_verdict, std::nan(""));
  resetLimitCount();
}

void MeterController::resetLimitCount()
{
  m_limitCount = LimitTest::Count();
  Q_EMIT limitCountChanged(m_limitCount);
}

void MeterController::configure(const MeterSetup &setup)
{
  m_dmm->setDmmInfo(setup.info);
  m_dmm->setDevice(setup.device);
  m_dmm->setFormat(setup.format);
  m_dmm->setFramesPerReading(setup.info.framesPerReading);
  setDisplayCounts(setup.info.display);
}

bool MeterController::connectMeter(bool on)
{
  bool ok = true;
  m_stale.reset();   // a new connection has its own rhythm
  if (on)
    ok = m_dmm->open();
  else
    m_dmm->close();
  // a "no readings" alarm watches a connected meter, so its clock starts here
  m_alarms->setConnected(m_dmm->isOpen(), QDateTime::currentMSecsSinceEpoch());
  if (!m_dmm->isOpen() && m_verdict != LimitTest::NoVerdict)
  {
    m_verdict = LimitTest::NoVerdict;
    Q_EMIT limitJudged(m_limitTest.value.id, m_verdict, std::nan(""));
  }
  return ok;
}

bool MeterController::pressKey(const QString &key)
{
  return m_dmm->sendKey(key);
}

void MeterController::setRecording(bool on)
{
  m_scpiDevice->setRecording(on);
}

void MeterController::timerEvent(QTimerEvent *)
{
  // a value older than the meter's rhythm allows is no value any more
  // (kern_spezifikation §4.3): the recorder gets a gap, not the old value
  const qint64 t = m_clock.elapsed();
  m_recorder->setStaleAfter(int(m_stale.maxAgeMs()));
  const bool stale = m_stale.stale(t);
  if (stale != m_staleShown)
  {
    m_staleShown = stale;
    Q_EMIT staleChanged(stale);
  }
  m_alarms->tick(QDateTime::currentMSecsSinceEpoch());
}

void MeterController::responseSLOT(const DmmDecoder::DmmResponse &response)
{
  // the one place the decoders' strings are read is ReadingAdapter
  m_adapter.setFormat(m_dmm->format());
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  const QString from = source();
  for (const PortSample &ps : m_adapter.adapt(response, now))
  {
    Reading rd = ReadingAdapter::reading(ps);
    rd.source = from;
    publish(rd);
  }
}

void MeterController::setInstanceId(const QString &id)
{
  m_instanceId = id;
  updateScpiName();
}

void MeterController::setDevice(const QString &id, const QString &name)
{
  m_deviceId = id;
  m_deviceName = name;
  updateScpiName();
}

void MeterController::updateScpiName()
{
  m_scpiDevice->setName(m_deviceName.isEmpty() ? m_instanceId : m_deviceName);
}

QString MeterController::source() const
{
  return m_deviceId.isEmpty() ? "instance:" + m_instanceId : "device:" + m_deviceId;
}

void MeterController::publish(const Reading &in)
{
  // the limit test's verdict goes with the reading as flags, so the
  // recording and the readings table keep it
  Reading rd = in;
  const bool judged = m_limitTest.enabled && m_limitTest.value.matches(rd) && !(rd.id > 0 && rd.text.isEmpty());
  if (judged)
  {
    m_verdict = rd.noNumber() ? LimitTest::NoVerdict : m_limitTest.judge(rd.value, rd.overload, rd.text.trimmed().startsWith('-'));
    rd.flags = (rd.flags & ~quint32(SampleFlag::Limit)) | LimitTest::flags(m_verdict);
    if (m_verdict != LimitTest::NoVerdict)
    {
      m_limitCount.add(m_verdict);
      Q_EMIT limitCountChanged(m_limitCount);
    }
  }
  const double dval = rd.value;
  const QString &val = rd.text;
  const QString &unit = rd.unit;
  const bool hold = rd.hold;
  const int id = rd.id;
  const qint64 now = rd.msecs;
  const bool overload = rd.overload;
  const QString &baseUnit = rd.baseUnit;

  // the meter's secondary display is off (an empty second value): the views
  // clear it, but it is no reading - not logged, not published
  if (id > 0 && val.isEmpty())
  {
    Q_EMIT reading(rd);
    return;
  }

  // min/max follow their value (setMinMaxValue()); a held display is the
  // old reading again, so it does not count
  bool newMin = false;
  bool newMax = false;
  if (id == 0 && !hold)
  {
    // the unit of the value (prefix and base unit), which is the display's
    // but for Victron's time to go: shown in h from an hour on, its values min
    const QString valueUnit = rd.prefix + baseUnit;
    if (m_lastUnit != valueUnit)
      Q_EMIT unitChanged(valueUnit);
    m_lastUnit = valueUnit;
  }
  bool valueMinMaxNew = false;
  if (!hold && id >= 0 && id < kMaxValues)
  {
    const MinMaxMemory::Result mm = m_valueMinMax[size_t(id)].feed(rd);
    valueMinMaxNew = mm.reset || mm.newMin || mm.newMax;
  }
  if (id == m_minMaxId && !hold)
  {
    // kept across a range change (mV -> V), afresh on another port
    const MinMaxMemory::Result mm = m_minMax.feed(rd);
    if (mm.reset)
      Q_EMIT minMaxReset();
    newMin = mm.newMin;
    newMax = mm.newMax;
  }

  Q_EMIT reading(rd);
  // a fault word ("FUSE"): said once when it comes, a mark where it began
  const QString fault = rd.fault() ? val.trimmed() : QString();
  if (fault != m_valueFault.value(id))
  {
    if (!fault.isEmpty())
    {
      Q_EMIT error(tr("The meter shows %1").arg(fault));
      Q_EMIT markRequested(QColor(0xd3, 0x2f, 0x2f), fault, true, true);
    }
    m_valueFault[id] = fault;
  }
  if (newMax)
    Q_EMIT maximumChanged(m_minMax.maximum(), val, unit);
  if (newMin)
    Q_EMIT minimumChanged(m_minMax.minimum(), val, unit);
  if (valueMinMaxNew)
    emitValueMinMax(id);

  m_lastReadings[id] = rd;
  if (id == 0)
  {
    m_lastMain = rd;
    m_stale.arrived(m_clock.elapsed());
    m_overload = overload;
    if (m_baseUnit != baseUnit)
    {
      m_baseUnit = baseUnit;
      Q_EMIT baseUnitChanged(baseUnit);
    }
  }

  // the alarms on this value
  if (!baseUnit.isEmpty())
    m_valueUnits[id] = baseUnit;
  m_valueOverload[id] = overload;
  m_alarms->feed(dval, overload, now, id, rd.source);

  // the limit test's verdict (judged above) to the views
  if (judged)
    Q_EMIT limitJudged(id, m_verdict, overload || m_verdict == LimitTest::NoVerdict ? std::nan("") : m_limitTest.deviation(dval));

  ScpiServer::Reading r;
  r.value = dval;
  r.unit = baseUnit;
  r.port = rd.port;
  r.range = rd.range;
  r.hold = hold;
  r.overload = overload;
  r.valid = !rd.noNumber();   // no number shown: no measurement (NaN)
  r.msecs = now;
  m_scpiDevice->setReading(id, r);
}

bool MeterController::mayStartRecording(QString *why) const
{
  QString reason;
  if (m_recorder->state() == RecordingStore::Record)
    reason = tr("already recording, the recording goes on");
  else if (m_recorder->state() == RecordingStore::View && m_recorder->dirty() && !m_followsWindow)
    reason = tr("start ignored, the recording shown is not saved yet");
  if (why)
    *why = reason;
  return reason.isEmpty();
}

namespace
{
// a file name only, and one on every system: letters, digits, " _.()-";
// no drive or stream (':'), no device name of Windows, no other format
bool plainFileName(const QString &name)
{
  static const QRegularExpression chars(R"(^[\w .()-]+$)", QRegularExpression::UseUnicodePropertiesOption);
  static const QRegularExpression device(R"(^(CON|PRN|AUX|NUL|COM\d|LPT\d)$)", QRegularExpression::CaseInsensitiveOption);
  const QString suffix = QFileInfo(name).suffix().toLower();
  return chars.match(name).hasMatch() && !name.startsWith('.') && !name.endsWith('.') && !name.endsWith(' ')
         && !device.match(name.section('.', 0, 0).trimmed()).hasMatch()
         && (suffix.isEmpty() || suffix == "csv" || suffix == "xlsx" || suffix == "ods");
}
}

QString MeterController::saveRecording(const QString &name, bool raw)
{
  // a client on the network names a file, no place: only into the folder
  // for recordings, and nothing there is overwritten
  if (!plainFileName(name))
    return "-257,File name error; a file name only, .csv, .xlsx or .ods: " + name;
  if (m_recorder->state() == RecordingStore::Record)
    return "-221,Settings conflict; recording, stop it first";
  if (m_recorder->state() != RecordingStore::View || m_recorder->count() == 0)
    return "-221,Settings conflict; nothing recorded";
  const QString dir = m_saveDir.isEmpty()
                        ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/QtDMM"
                        : m_saveDir;
  const QString path = dir + '/' + (QFileInfo(name).suffix().isEmpty() ? name + ".csv" : name);
  if (QFileInfo::exists(path))
    return "-257,File name error; exists already: " + QFileInfo(path).fileName();
  QString error;
  if (!QDir().mkpath(dir) || !writeRecording(path, raw, &error))
    return "-250,Mass storage error; " + (error.isEmpty() ? dir : error);
  Q_EMIT info(tr("SCPI: recording saved as %1").arg(QDir::toNativeSeparators(path)));
  return {};
}

bool MeterController::writeRecording(const QString &path, bool raw, QString *error)
{
  QList<int> extras;
  for (int i = 0; i < m_recorder->extraCount(); ++i)
    extras << i;
  // all values of the meters that recorded with it
  QList<RecordingStore::Foreign> foreign;
  for (MeterController *other : m_recordedWith ? m_recordedWith() : QList<MeterController *>())
  {
    RecordingStore *store = other->recorder();
    QList<int> ids{ 0 };
    for (int i = 0; i < store->extraCount(); ++i)
      if (!ids.contains(store->extra(i).id))
        ids << store->extra(i).id;
    for (int id : std::as_const(ids))
      foreign << RecordingStore::Foreign{ store, id, other->deviceName() };
  }
  return m_recorder->write(path, error, raw, extras, foreign, m_deviceName);
}

QString MeterController::valueName(int id) const
{
  const Reading r = lastReading(id);
  if (r.msecs <= 0)
    return QString();
  return r.name.isEmpty() ? RecordingStore::describe(r.port, r.baseUnit) : r.name;
}

int MeterController::valueCount() const
{
  const int seen = m_lastReadings.isEmpty() ? 0 : m_lastReadings.lastKey() + 1;
  return qBound(1, qMax(seen, m_dmm->dmmInfo().valueCount()), kMaxValues);
}

void MeterController::setValueCount(int count)
{
  m_scpiDevice->setChannelCount(count);
}

void MeterController::setMinMaxValue(int id)
{
  if (id == m_minMaxId)
    return;
  m_minMaxId = id;
  // the display's memory follows another value; those of the values go on
  m_minMax.clear();
  Q_EMIT minMaxReset();
}

void MeterController::resetMinMax()
{
  m_minMax.clear();
  for (MinMaxMemory &m : m_valueMinMax)
    m.clear();
  Q_EMIT minMaxReset();
  for (int id = 0; id < kMaxValues; ++id)
    emitValueMinMax(id);
  resetLimitCount();   // the counter goes with min/max
}

void MeterController::emitValueMinMax(int id)
{
  const MinMaxMemory &m = valueMinMax(id);
  const double nan = std::numeric_limits<double>::quiet_NaN();
  Q_EMIT valueMinMaxChanged(id, m.isEmpty() ? nan : m.minimum(), m.isEmpty() ? nan : m.maximum());
}

void MeterController::forgetUnit()
{
  m_lastUnit.clear();
  m_baseUnit.clear();
  m_valueUnits.clear();
  m_scpiDevice->clearReadings();   // CHANnel:CATalog? and UNIT? (@4) must not keep the previous meter
  Q_EMIT unitChanged(QString());
  Q_EMIT baseUnitChanged(QString());
}

// ---------------------------------------------------------------- alarms

QString MeterController::valueUnit(int id) const
{
  return id == 0 ? m_baseUnit : m_valueUnits.value(id);
}

void MeterController::onAlarmRaised(int index, const Alarm &alarm, double value)
{
  const QString unit = valueUnit(alarm.value.id);
  const bool overload = m_valueOverload.value(alarm.value.id);
  const QString shown = overload ? QStringLiteral("OL") : EngNumberValidator::engValue(value) + unit;
  const QString text = alarm.message.isEmpty() ? alarm.describe(unit) : alarm.message;
  Q_EMIT error(tr("%1: %2 (%3)").arg(Alarm::title(alarm.name), text, shown));

  QString why;
  if (alarm.recorder == Alarm::RecorderStart && mayStartRecording(&why))
    Q_EMIT recordingRequested(true);
  else if (alarm.recorder == Alarm::RecorderStart)
    Q_EMIT info(tr("%1: %2").arg(Alarm::title(alarm.name), why));
  else if (alarm.recorder == Alarm::RecorderStop)
    Q_EMIT recordingRequested(false);
  if (alarm.markGraph || alarm.markTable)
    Q_EMIT markRequested(alarm.color, Alarm::title(alarm.name), alarm.markGraph, alarm.markTable);
  if (!alarm.command.isEmpty())
  {
    QString cmd = alarm.command;
    cmd.replace("%v", EngNumberValidator::engText(value)).replace("%u", unit).replace("%n", alarm.name);
    QStringList args = QProcess::splitCommand(cmd);
    if (!args.isEmpty())
    {
      const QString program = args.takeFirst();
      if (!alarm.disconnect)
      {
        if (!QProcess::startDetached(program, args))
          Q_EMIT error(tr("%1: could not run %2").arg(Alarm::title(alarm.name), program));
      }
      else if (m_externalPending || m_external->state() != QProcess::NotRunning)
        Q_EMIT error(tr("%1: %2 is still running").arg(Alarm::title(alarm.name), m_external->program()));
      else
      {
        // "Disconnect first": the program has the port to itself, the meter
        // comes back when it has ended. Not here: the alarm raised while the
        // reader is still on the frame of the port that would close
        const QString title = Alarm::title(alarm.name);
        m_externalPending = true;   // a second alarm in the meantime waits for it
        m_external->setProgram(program);
        QTimer::singleShot(0, this, [this, title, program, args]
        {
          m_externalPending = false;
          m_reopenAfter = m_dmm->isOpen();
          if (m_reopenAfter)
          {
            // the recording goes on: no readings while the program runs
            m_recorder->setStale(true);
            Q_EMIT portReleased(true);
          }
          m_external->start(program, args);
          if (!m_external->waitForStarted(3000))
          {
            Q_EMIT error(tr("%1: could not run %2").arg(title, program));
            if (m_reopenAfter)
            {
              m_reopenAfter = false;
              Q_EMIT portReleased(false);
            }
          }
        });
      }
    }
  }
  Q_EMIT alarmRaised(index, alarm, shown, text);
  updateBanner();
}

void MeterController::acknowledgeAlarms()
{
  m_alarms->acknowledgeAll();
  updateBanner();
}

// One line per raised alarm (acknowledged ones are silent), on the colour
// of the first one.
void MeterController::updateBanner()
{
  QStringList lines;
  QColor color;
  const QList<Alarm> &list = m_alarms->alarms();
  for (int i = 0; i < list.size(); ++i)
  {
    if (m_alarms->state(i) != AlarmManager::Raised || !list[i].banner)
      continue;
    const Alarm &al = list[i];
    lines << QString("%1: %2").arg(al.name, al.message.isEmpty() ? al.describe(valueUnit(al.value.id)) : al.message);
    if (!color.isValid())
      color = al.color;
  }
  Q_EMIT alarmBannerChanged(lines.join('\n'), color);

  // the display: the newest raised alarm that colours it, acknowledged or not
  const QColor alert = m_alarms->displayColor();
  if (alert != m_alertColor)
  {
    m_alertColor = alert;
    Q_EMIT alertColorChanged(alert);
  }
}

// ---------------------------------------------------------------- SCPI

// What the QTDMM: and CALCulate queries read: the min/max memory, the
// recording, the alarms and the meter's keys
void MeterController::installScpiSources()
{
  ScpiServer::Sources s;
  auto statsOf = [](const MinMaxMemory &m)
  {
    ScpiServer::Stats st;
    if (!m.isEmpty())
    {
      st.count = m.count();
      st.min = m.minimum();
      st.max = m.maximum();
      st.mean = m.mean();
    }
    return st;
  };
  s.stats = [this, statsOf] { return statsOf(m_minMax); };
  s.channelStats = [this, statsOf](int channel) { return statsOf(valueMinMax(channel - 1)); };
  s.clearChannelStats = [this](int channel)
  {
    if (channel >= 1 && channel <= kMaxValues)
    {
      m_valueMinMax[size_t(channel - 1)].clear();
      emitValueMinMax(channel - 1);
    }
  };
  s.recording = [this]
  {
    ScpiServer::RecordingState r;
    r.count = m_recorder->count();
    switch (m_recorder->state())
    {
      case RecordingStore::Live:   r.mode = ScpiServer::RecordingState::Live; break;
      case RecordingStore::Record: r.mode = ScpiServer::RecordingState::Record; break;
      default: r.mode = r.count ? ScpiServer::RecordingState::View : ScpiServer::RecordingState::Idle;
    }
    r.seconds = m_recorder->duration() / 1000.0;
    r.unit = m_recorder->unit();
    return r;
  };
  s.saveRecording = [this](const QString &name, bool raw) { return saveRecording(name, raw); };
  s.recordingData = [this](int count, int channel)
  {
    // channel 2 and on: the series of that value used last, as the graph
    // and the export have it
    const RecordingSeries *series = channel <= 1 ? &m_recorder->series() : nullptr;
    for (int i = m_recorder->extraCount() - 1; i >= 0 && !series; --i)
      if (m_recorder->extra(i).id == channel - 1)
        series = &m_recorder->extra(i);
    QList<ScpiServer::Point> points;
    if (!series)
      return points;
    for (int i = std::max(0, series->count() - count); i < series->count(); ++i)
    {
      const RawPoint &p = series->at(i);
      points.append({ p.t / 1000.0, p.value, p.quality == Quality::Overload });
    }
    return points;
  };
  s.alarms = [this]
  {
    QList<ScpiServer::AlarmState> alarms;
    const QList<Alarm> &list = m_alarms->alarms();
    for (int i = 0; i < list.size(); ++i)
      alarms.append({ list[i].name, ScpiServer::AlarmState::Level(m_alarms->state(i)) });
    return alarms;
  };
  s.keys = [this] { return m_dmm->keys(); };
  s.pressKey = [this](const QString &key) { return pressKey(key); };
  s.limitTest = [this] { return m_limitTest; };
  s.setLimitTest = [this](const LimitTest &test) { setLimitTest(test); };
  s.verdict = [this] { return m_verdict; };
  s.limitCount = [this] { return m_limitCount; };
  s.clearLimitCount = [this] { resetLimitCount(); };
  m_scpiDevice->setSources(std::move(s));
}

void MeterController::applyScpi(const ScpiConfig &config)
{
  m_host->apply(config, m_instanceId, m_model);
}

QList<ServiceState> MeterController::services() const
{
  return m_host->services();
}

void MeterController::setScreenshotSource(std::function<QByteArray(const QByteArray &)> source)
{
  m_host->setScreenshotSource(std::move(source));
}
