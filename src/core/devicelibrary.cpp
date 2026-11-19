// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/devicelibrary.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>
#include <QRegularExpression>
#include <QUuid>
#include <algorithm>

namespace
{
const QString kPrefix = QStringLiteral("device-");

const QString kAlarmsKey = QStringLiteral("Alarms/list");
const QString kLimitKey = QStringLiteral("Limits/test");

// the keys of an entry that are its own, not the meter's: also its rules
bool ownKey(const QString &key)
{
  return key == QLatin1String("name") || key == QLatin1String("order") || key == kAlarmsKey || key == kLimitKey;
}

std::function<QString(const QString &)> g_modelTransport;

// what entryKeys() keeps, per way of connecting; a simulated meter keeps its DMM/virtual-* keys
const QStringList kBaseKeys { "DMM/model", "DMM/data-format", "DMM/display", "DMM/number-of-values",
                              "Port settings/device" };
const QStringList kBleKeys { "Port settings/ble-address", "Port settings/ble-key", "Port settings/ble-main",
                             "Port settings/ble-second", "Port settings/ble-third", "Port settings/ble-fourth" };
const QStringList kSigrokKeys { "Port settings/sigrok-conn", "Port settings/sigrok-options" };
const QStringList kCalcKeys { "DMM/calc-unit", "DMM/calc-expression" };
const QStringList kSerialKeys { "Port settings/baud", "Port settings/bits", "Port settings/parity",
                                "Port settings/stop-bits", "DMM/rts", "DMM/dtr", "DMM/external-setup",
                                "DMM/unit-1", "DMM/unit-2", "DMM/unit-3", "DMM/unit-4" };   // the units: microcontroller only
const QLatin1String kVirtualPrefix("DMM/virtual-");
// a logic node: its model and the node as JSON (LogicNode::toJsonText())
const QLatin1String kNodeKey("Node/json");

// "SERIAL /dev/ttyUSB0" -> "serial"; a port without type is a serial port
QString deviceType(const QString &device)
{
  const QString first = device.section(' ', 0, 0).toLower();
  if (device.trimmed().isEmpty())
    return QString();
  if (!device.contains(' ') && !first.startsWith(QLatin1String("ble")))
    return QStringLiteral("serial");
  return first;
}

// "/dev/serial/by-id/usb-..." and "/dev/ttyUSB0" are one port while the
// cable is plugged in: the file the name stands for
QString portFile(const QString &port)
{
  const QString canonical = QFileInfo(port).canonicalFilePath();
  return canonical.isEmpty() ? port : canonical;
}
}

void DeviceLibrary::setModelTransport(std::function<QString(const QString &)> transport)
{
  g_modelTransport = std::move(transport);
}

QVariantMap DeviceLibrary::entryKeys(const QVariantMap &keys)
{
  if (keys.contains(kNodeKey))
    return { { "DMM/model", keys.value("DMM/model") }, { kNodeKey, keys.value(kNodeKey) } };
  QStringList wanted = kBaseKeys;
  const QString device = keys.value("Port settings/device").toString();
  const QString model = keys.value("DMM/model").toString();
  const QString byModel = g_modelTransport ? g_modelTransport(model) : QString();
  const QString type = byModel.isEmpty() ? deviceType(device) : byModel;
  if (type == QLatin1String("ble"))
    wanted << kBleKeys;
  else if (type == QLatin1String("blegatt"))
    wanted << "Port settings/ble-address";
  else if (type == QLatin1String("sigrok"))
    wanted << kSigrokKeys;
  // the simulated meter, "Virtual meter" before 26.2 (DmmDecoder::canonicalModel())
  else if (type == QLatin1String("calc")
           && (model.endsWith(QLatin1String("Simulated meter")) || model.endsWith(QLatin1String("Virtual meter"))))
  {
    for (auto it = keys.cbegin(); it != keys.cend(); ++it)
      if (it.key().startsWith(kVirtualPrefix))
        wanted << it.key();
  }
  else if (type == QLatin1String("calc"))
    wanted << kCalcKeys;
  else
    wanted << kSerialKeys;
  QVariantMap out;
  for (const QString &key : wanted)
    if (keys.contains(key))
      out.insert(key, keys.value(key));
  return out;
}

bool DeviceLibrary::isEntryKey(const QString &key)
{
  return kBaseKeys.contains(key) || kBleKeys.contains(key) || kSigrokKeys.contains(key) || kCalcKeys.contains(key)
         || kSerialKeys.contains(key) || key.startsWith(kVirtualPrefix) || key == kNodeKey;
}

QString DeviceLibrary::place(const QVariantMap &keys)
{
  const QString device = keys.value("Port settings/device").toString().trimmed();
  const QString type = deviceType(device);
  const QString rest = device.contains(' ') ? device.section(' ', 1).trimmed() : device;
  // the simulated and calculated meters are nowhere; entries from before
  // 26.2 kept the port of the meter before ("serial ..." of the UT61E)
  if (keys.value("DMM/model").toString().startsWith(QLatin1String("QtDMM ")) && type != QLatin1String("calc"))
    return QString();
  if (type == QLatin1String("serial"))
    return "serial " + portFile(rest);
  if (type == QLatin1String("hid") || type == QLatin1String("hidholtek"))
    return "hid " + rest.section(' ', 0, 0).toLower();
  if (type == QLatin1String("ble") || type == QLatin1String("blegatt") || type.isEmpty())
  {
    // the address in the device string, else the one of a find
    QString address = rest.section(' ', 0, 0);
    if (type.isEmpty() || address.isEmpty())
      address = keys.value("Port settings/ble-address").toString().section(' ', 0, 0);
    return address.isEmpty() ? QString() : "ble " + address.toUpper();
  }
  if (type == QLatin1String("sigrok"))
    return "sigrok " + keys.value("Port settings/sigrok-conn", rest).toString();
  return type + ' ' + rest;
}

QString DeviceLibrary::findByPlace(const QVariantMap &keys) const
{
  const QString where = place(keys);
  if (where.isEmpty())
    return QString();
  QStringList same;
  for (const MyDevice &d : list())
    if (place(d.keys) == where)
    {
      // two cables of one type: the path tells them apart
      if (d.keys.value("Port settings/device") == keys.value("Port settings/device"))
        return d.id;
      same << d.id;
    }
  return same.isEmpty() ? QString() : same.first();
}

void DeviceLibrary::tidy()
{
  if (!QFileInfo::exists(m_file))
    return;
  for (const MyDevice &d : list())
  {
    QVariantMap keys = entryKeys(d.keys);
    // a Bluetooth meter whose place is still the port of the meter before
    const QString byModel = g_modelTransport ? g_modelTransport(d.model()) : QString();
    const QString address = keys.value("Port settings/ble-address").toString().section(' ', 0, 0);
    if (!byModel.isEmpty() && !address.isEmpty()
        && deviceType(keys.value("Port settings/device").toString()) != byModel)
      keys.insert("Port settings/device", byModel + ' ' + address);
    if (keys == d.keys)
      continue;
    write([&](QSettings &s)
    {
      s.beginGroup(kPrefix + d.id);
      for (auto it = d.keys.cbegin(); it != d.keys.cend(); ++it)
        if (!keys.contains(it.key()))
          s.remove(it.key());
      for (auto it = keys.cbegin(); it != keys.cend(); ++it)
        s.setValue(it.key(), it.value());
      s.endGroup();
    });
  }
}

QString MyDevice::where() const
{
  // a node: its formulas
  if (keys.contains(kNodeKey))
  {
    QStringList formulas;
    const QJsonObject node = QJsonDocument::fromJson(keys.value(kNodeKey).toString().toUtf8()).object();
    for (const QJsonValue &v : node["parameters"].toObject()["outputs"].toArray())
      formulas << v.toObject()["formula"].toString();
    return formulas.join("; ");
  }
  const QString device = keys.value("Port settings/device").toString();
  const QString type = device.section(' ', 0, 0).toUpper();
  if (type == QLatin1String("BLE"))
    return keys.value("Port settings/ble-address").toString();
  if (type == QLatin1String("CALC"))
    return device.section(' ', 1);
  if (type == QLatin1String("SIGROK"))
    return keys.value("Port settings/sigrok-conn").toString();
  return device.section(' ', 1);
}

DeviceLibrary::DeviceLibrary(const QString &dir, QObject *parent) :
  QObject(parent),
  m_file(QDir(dir).filePath("devices.conf"))
{
  connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this]
  {
    watch();   // a file written anew is a new file to the watcher
    Q_EMIT changed();
  });
  connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this]
  {
    // the file came into being
    if (!m_watcher.files().contains(m_file) && QFileInfo::exists(m_file))
    {
      watch();
      Q_EMIT changed();
    }
  });
  tidy();
  watch();
}

void DeviceLibrary::watch()
{
  const QString dir = QFileInfo(m_file).absolutePath();
  if (QFileInfo::exists(dir) && !m_watcher.directories().contains(dir))
    m_watcher.addPath(dir);
  if (QFileInfo::exists(m_file) && !m_watcher.files().contains(m_file))
    m_watcher.addPath(m_file);
}

namespace
{
constexpr int kAttempts = 6;   ///< reading/writing devices.conf, 20 ... 320 ms apart

std::optional<MyDevice> findIn(const QList<MyDevice> &entries, const QString &id)
{
  for (const MyDevice &d : entries)
    if (d.id == id)
      return d;
  return std::nullopt;
}
}

bool DeviceLibrary::write(const std::function<void(QSettings &)> &edit,
                          const std::function<bool(const QList<MyDevice> &)> &landed) const
{
  for (int attempt = 0; attempt < kAttempts; ++attempt)
  {
    if (attempt > 0)
      QThread::msleep(20u << (attempt - 1));
    {
      QSettings s(m_file, QSettings::IniFormat);
      edit(s);
      s.sync();
      if (s.status() == QSettings::NoError)
        return true;
    }
    // an error, and still in the file (Windows CI: the entry of a second
    // instance was there, add() said it failed) - the file itself, not
    // the cache of this process (review R9-04)
    if (landed)
    {
      const std::optional<QList<MyDevice>> disk = onDisk();
      if (disk && landed(*disk))
        return true;
    }
  }
  return false;
}

QList<MyDevice> DeviceLibrary::list() const
{
  // the file is replaced on every write; an attempt that cannot read it is
  // tried again (see write())
  for (int attempt = 1; attempt < kAttempts; ++attempt)
  {
    QSettings probe(m_file, QSettings::IniFormat);
    if (probe.status() == QSettings::NoError)
      break;
    QThread::msleep(20u << (attempt - 1));
  }
  QSettings s(m_file, QSettings::IniFormat);
  return entries(s);
}

std::optional<QList<MyDevice>> DeviceLibrary::onDisk() const
{
  QFile file(m_file);
  QTemporaryDir dir;
  if (!dir.isValid() || !file.open(QIODevice::ReadOnly))
    return std::nullopt;
  const QByteArray bytes = file.readAll();
  // a path of its own: a cache of its own
  QFile copy(dir.filePath(QStringLiteral("devices.conf")));
  if (!copy.open(QIODevice::WriteOnly) || copy.write(bytes) != bytes.size())
    return std::nullopt;
  copy.close();
  QSettings s(copy.fileName(), QSettings::IniFormat);
  if (s.status() != QSettings::NoError)
    return std::nullopt;
  return entries(s);
}

QList<MyDevice> DeviceLibrary::entries(QSettings &s)
{
  QList<MyDevice> out;
  for (const QString &group : s.childGroups())
  {
    if (!group.startsWith(kPrefix))
      continue;
    s.beginGroup(group);
    MyDevice d;
    d.id = group.mid(kPrefix.size());
    d.name = s.value("name").toString();
    d.order = s.value("order", 0).toInt();
    d.alarms = s.value(kAlarmsKey).toString();
    d.limitTest = s.value(kLimitKey).toString();
    for (const QString &key : s.allKeys())
      if (!ownKey(key))
        d.keys.insert(key, s.value(key));
    s.endGroup();
    out << d;
  }
  std::stable_sort(out.begin(), out.end(), [](const MyDevice &a, const MyDevice &b)
  {
    return a.order != b.order ? a.order < b.order : a.name.localeAwareCompare(b.name) < 0;
  });
  return out;
}

std::optional<MyDevice> DeviceLibrary::find(const QString &id) const
{
  for (const MyDevice &d : list())
    if (d.id == id)
      return d;
  return std::nullopt;
}

QString DeviceLibrary::uniqueName(const QString &base) const
{
  QStringList names;
  for (const MyDevice &d : list())
    names << d.name;
  QString name = base.trimmed().isEmpty() ? tr("Meter") : base.trimmed();
  const QString stem = name;
  for (int n = 2; names.contains(name); ++n)
    name = QString("%1 (%2)").arg(stem).arg(n);
  return name;
}

QString DeviceLibrary::add(const QString &name, const QVariantMap &keys, const QString &wanted)
{
  const QList<MyDevice> before = list();
  // an id from a file: only what a group name in devices.conf takes
  static const QRegularExpression plain(QStringLiteral("^[A-Za-z0-9_-]{1,32}$"));
  const bool take = plain.match(wanted).hasMatch() && !find(wanted);
  const QString id = take ? wanted : QUuid::createUuid().toString(QUuid::Id128).left(8);
  const QVariantMap own = entryKeys(keys);
  const bool ok = write([&](QSettings &s)
  {
    s.beginGroup(kPrefix + id);
    s.setValue("name", name);
    s.setValue("order", before.isEmpty() ? 1 : before.last().order + 1);
    for (auto it = own.cbegin(); it != own.cend(); ++it)
      s.setValue(it.key(), it.value());
    s.endGroup();
  }, [&](const QList<MyDevice> &disk) { const std::optional<MyDevice> d = findIn(disk, id); return d && d->name == name; });
  watch();
  Q_EMIT changed();
  return ok ? id : QString();
}

bool DeviceLibrary::setRules(const QString &id, const QString &alarms, const QString &limitTest)
{
  const std::optional<MyDevice> d = find(id);
  if (!d)
    return false;
  if (d->alarms == alarms && d->limitTest == limitTest)
    return true;
  const bool ok = write([&](QSettings &s)
  {
    s.beginGroup(kPrefix + id);
    const std::pair<QString, QString> rules[] = { { kAlarmsKey, alarms }, { kLimitKey, limitTest } };
    for (const auto &[key, json] : rules)
    {
      if (json.isEmpty())
        s.remove(key);
      else
        s.setValue(key, json);
    }
    s.endGroup();
  }, [&](const QList<MyDevice> &disk)
  {
    const std::optional<MyDevice> e = findIn(disk, id);
    return e && e->alarms == alarms && e->limitTest == limitTest;
  });
  Q_EMIT changed();
  return ok;
}

bool DeviceLibrary::update(const QString &id, const QVariantMap &keys)
{
  if (!find(id))
    return false;
  const QVariantMap own = entryKeys(keys);
  const bool ok = write([&](QSettings &s)
  {
    s.beginGroup(kPrefix + id);
    for (const QString &key : s.allKeys())
      if (!ownKey(key))
        s.remove(key);
    for (auto it = own.cbegin(); it != own.cend(); ++it)
      s.setValue(it.key(), it.value());
    s.endGroup();
  }, [&](const QList<MyDevice> &disk)
  {
    const std::optional<MyDevice> d = findIn(disk, id);
    if (!d || d->keys.size() != own.size())
      return false;
    for (auto it = own.cbegin(); it != own.cend(); ++it)
      if (d->keys.value(it.key()).toString() != it.value().toString())
        return false;
    return true;
  });
  Q_EMIT changed();
  return ok;
}

bool DeviceLibrary::rename(const QString &id, const QString &name)
{
  if (!find(id) || name.trimmed().isEmpty())
    return false;
  const bool ok = write([&](QSettings &s) { s.setValue(kPrefix + id + "/name", name.trimmed()); },
                        [&](const QList<MyDevice> &disk) { const std::optional<MyDevice> d = findIn(disk, id); return d && d->name == name.trimmed(); });
  Q_EMIT changed();
  return ok;
}

bool DeviceLibrary::remove(const QString &id)
{
  if (!find(id))
    return false;
  const bool ok = write([&](QSettings &s) { s.remove(kPrefix + id); },
                        [&](const QList<MyDevice> &disk) { return !findIn(disk, id); });
  Q_EMIT changed();
  return ok;
}

void DeviceLibrary::writeOrder(const QStringList &ids)
{
  write([&](QSettings &s)
  {
    for (int i = 0; i < ids.size(); ++i)
      s.setValue(kPrefix + ids[i] + "/order", i + 1);
  }, [&](const QList<MyDevice> &disk)
  {
    QStringList now;
    for (const MyDevice &d : disk)
      now << d.id;
    return now == ids;
  });
}

bool DeviceLibrary::move(const QString &id, int index)
{
  QStringList ids;
  for (const MyDevice &d : list())
    ids << d.id;
  const int from = int(ids.indexOf(id));
  if (from < 0)
    return false;
  ids.move(from, qBound(0, index, int(ids.size()) - 1));
  writeOrder(ids);
  Q_EMIT changed();
  return true;
}

QString DeviceLibrary::duplicate(const QString &id)
{
  const std::optional<MyDevice> d = find(id);
  if (!d)
    return QString();
  const QString copy = add(uniqueName(d->name), d->keys);
  if (!copy.isEmpty() && (!d->alarms.isEmpty() || !d->limitTest.isEmpty()))
    setRules(copy, d->alarms, d->limitTest);
  // right after the original
  QStringList ids;
  for (const MyDevice &e : list())
    if (e.id != copy)
      ids << e.id;
  ids.insert(ids.indexOf(id) + 1, copy);
  writeOrder(ids);
  Q_EMIT changed();
  return copy;
}
