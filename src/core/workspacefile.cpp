// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/workspacefile.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSettings>

namespace
{
QString tr(const char *text)
{
  return QCoreApplication::translate("WorkspaceFile", text);
}

void setError(QString *error, const QString &text)
{
  if (error)
    *error = text;
}

const QLatin1String kFormat("qtdmm-workspace");

// What a window key holds: a word, yes or no, or a list ("1 2@device:a")
enum class Kind { Text, Flag, List };

// Where the window's keys go in the file: a member of "layout" or of a view
struct Place
{
  const char *key;
  const char *group;   ///< "layout" or "views/<view>"
  const char *member;
  Kind        kind;
};

const Place kPlaces[] = {
  { "Windows/arrange", "layout", "arrange", Kind::Text },
  { "Windows/layout", "layout", "tree", Kind::Text },
  { "Windows/title-bars-hidden", "layout", "titleBarsHidden", Kind::Flag },
  { "Windows/toolbar-hidden", "layout", "toolbarHidden", Kind::Flag },
  { "Windows/design", "layout", "design", Kind::Text },
  { "Display/show", "views/display", "shown", Kind::Flag },
  { "Windows/lcd-value", "views/display", "value", Kind::Text },
  { "Windows/lcd-secondary-values", "views/display", "secondaryValues", Kind::List },
  { "Windows/meter", "views/meter", "shown", Kind::Flag },
  { "Windows/meter-value", "views/meter", "value", Kind::Text },
  { "MainWindow/show-graph", "views/graph", "shown", Kind::Flag },
  { "Windows/graph-values", "views/graph", "values", Kind::List },
  // before the values had their meter: the window's ids, the main value apart
  { "Windows/graph-shown-values", "views/graph", "shownValues", Kind::List },
  { "Windows/graph-main-shown", "views/graph", "mainShown", Kind::Flag },
  { "Windows/graph-value-axes", "views/graph", "axes", Kind::List },
  { "Windows/graph-reference", "views/graph", "reference", Kind::Text },
  { "Windows/graph-minimap", "views/graph", "minimap", Kind::Flag },
  { "Windows/graph-envelope", "views/graph", "envelope", Kind::Flag },
  { "Windows/readings", "views/readings", "shown", Kind::Flag },
  { "Windows/table-hidden-values", "views/readings", "hidden", Kind::List },
  { "Windows/table-devices", "views/readings", "devices", Kind::List },
  { "Windows/table-sources", "views/readings", "sources", Kind::List },   // before 26.4: the others only
  { "Windows/poincare", "views/poincare", "shown", Kind::Flag },
  { "Windows/poincare-value", "views/poincare", "value", Kind::Text },
};

// A front panel's keys, "Windows/panel-<id>-<suffix>"
const Place kPanel[] = {
  { "-value", "", "value", Kind::Text },
  { "-folded", "", "folded", Kind::Flag },
  { "-secondary-values", "", "secondaryValues", Kind::List },
  { "-shown", "", "shown", Kind::Flag },
};

const QLatin1String kOpen("Windows/open-devices");
const QLatin1String kGeometry("Windows/geometry-");
const QLatin1String kPanelGeometry("Windows/geometry-panel-");

QJsonValue toJson(const QVariant &value, Kind kind)
{
  switch (kind)
  {
    case Kind::Flag:
      return value.toBool();
    case Kind::List:
      return QJsonArray::fromStringList(value.toString().split(' ', Qt::SkipEmptyParts));
    case Kind::Text:
      break;
  }
  return value.toString();
}

QVariant fromJson(const QJsonValue &value, Kind kind)
{
  switch (kind)
  {
    case Kind::Flag:
      return value.toBool();
    case Kind::List:
    {
      QStringList items;
      for (const QJsonValue &v : value.toArray())
        items << v.toString();
      return items.join(' ');
    }
    case Kind::Text:
      break;
  }
  return value.toString();
}

// A rule as JSON in the file (readable there), as text in the entry
QJsonValue ruleToJson(const QString &text)
{
  if (text.isEmpty())
    return QJsonValue::Undefined;
  const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
  if (doc.isArray())
    return doc.array();
  if (doc.isObject())
    return doc.object();
  return text;
}

QString ruleFromJson(const QJsonValue &value)
{
  if (value.isArray())
    return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
  if (value.isObject())
    return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
  return value.toString();
}

std::optional<WorkspaceFile> readVersion1(const QString &path, QString *error)
{
  QSettings ini(path, QSettings::IniFormat);
  if (ini.status() != QSettings::NoError || ini.value("Workspace/version").toInt() < 1)
  {
    setError(error, tr("This is no QtDMM bench file."));
    return std::nullopt;
  }
  WorkspaceFile ws;
  ws.version = ini.value("Workspace/version").toInt();
  ws.name = QFileInfo(path).completeBaseName();
  const QStringList size = ini.value("Workspace/size").toString().split(' ');
  if (size.size() == 2)
  {
    ws.width = size[0].toInt();
    ws.height = size[1].toInt();
  }
  for (const QString &key : ini.allKeys())
    if (!key.startsWith(QLatin1String("Workspace/")))
      ws.keys.insert(key, ini.value(key));
  return ws;
}
}

std::optional<MyDevice> WorkspaceFile::device(const QString &id) const
{
  for (const MyDevice &d : devices)
    if (d.id == id)
      return d;
  return std::nullopt;
}

std::optional<WorkspaceFile> WorkspaceFile::read(const QString &path, QString *error)
{
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
  {
    setError(error, tr("Cannot open the file."));
    return std::nullopt;
  }
  const QByteArray data = file.readAll();
  file.close();
  // version 1 is an ini file
  if (!data.trimmed().startsWith('{'))
    return readVersion1(path, error);

  QJsonParseError parse;
  const QJsonObject root = QJsonDocument::fromJson(data, &parse).object();
  if (parse.error != QJsonParseError::NoError || root.value("format").toString() != kFormat)
  {
    setError(error, parse.error != QJsonParseError::NoError ? tr("This is no QtDMM bench file: %1.").arg(parse.errorString())
                                                            : tr("This is no QtDMM bench file."));
    return std::nullopt;
  }
  const int minReader = root.value("minReader").toInt(root.value("version").toInt());
  if (minReader > kVersion)
  {
    setError(error, tr("This bench needs a newer QtDMM (bench file format %1).").arg(minReader));
    return std::nullopt;
  }

  WorkspaceFile ws;
  ws.version = root.value("version").toInt();
  ws.name = root.value("name").toString();
  ws.description = root.value("description").toString();
  const QJsonObject window = root.value("window").toObject();
  ws.width = window.value("width").toInt();
  ws.height = window.value("height").toInt();
  ws.windowDevice = window.value("device").toString();
  ws.thumbnail = QByteArray::fromBase64(root.value("thumbnail").toString().toLatin1());

  // the keys first as they were kept (unknown ones), the known ones over them
  const QJsonObject other = root.value("other").toObject();
  for (auto it = other.constBegin(); it != other.constEnd(); ++it)
    ws.keys.insert(it.key(), it.value().toVariant());
  const QJsonObject layout = root.value("layout").toObject();
  const QJsonObject views = root.value("views").toObject();
  for (const Place &p : kPlaces)
  {
    const QString group = QString::fromLatin1(p.group);
    const QJsonObject from = group == QLatin1String("layout") ? layout : views.value(group.mid(6)).toObject();
    if (from.contains(QLatin1String(p.member)))
      ws.keys.insert(QString::fromLatin1(p.key), fromJson(from.value(QLatin1String(p.member)), p.kind));
  }
  const QJsonObject geometry = layout.value("geometry").toObject();
  for (auto it = geometry.constBegin(); it != geometry.constEnd(); ++it)
    ws.keys.insert(kGeometry + it.key(), it.value().toString());

  QStringList beside;
  for (const QJsonValue &v : root.value("devices").toArray())
  {
    const QJsonObject o = v.toObject();
    MyDevice d;
    d.id = o.value("id").toString();
    d.name = o.value("name").toString();
    const QJsonObject keys = o.value("keys").toObject();
    for (auto it = keys.constBegin(); it != keys.constEnd(); ++it)
      d.keys.insert(it.key(), it.value().toString());
    d.alarms = ruleFromJson(o.value("alarms"));
    d.limitTest = ruleFromJson(o.value("limitTest"));
    if (d.id.isEmpty())
      continue;
    ws.devices << d;
    if (o.value("role").toString() != QLatin1String("beside"))
      continue;
    beside << d.id;
    const QJsonObject panel = o.value("panel").toObject();
    for (const Place &p : kPanel)
      if (panel.contains(QLatin1String(p.member)))
        ws.keys.insert("Windows/panel-" + d.id + QLatin1String(p.key), fromJson(panel.value(QLatin1String(p.member)), p.kind));
    if (panel.contains("geometry"))
      ws.keys.insert(kPanelGeometry + d.id, panel.value("geometry").toString());
  }
  ws.keys.insert(kOpen, beside.join(' '));
  return ws;
}

bool WorkspaceFile::write(const QString &path, QString *error) const
{
  QVariantMap rest = keys;
  QJsonObject layout;
  QJsonObject views;
  for (const Place &p : kPlaces)
  {
    const QString key = QString::fromLatin1(p.key);
    if (!rest.contains(key))
      continue;
    const QJsonValue value = toJson(rest.take(key), p.kind);
    const QString group = QString::fromLatin1(p.group);
    if (group == QLatin1String("layout"))
      layout.insert(QLatin1String(p.member), value);
    else
    {
      QJsonObject view = views.value(group.mid(6)).toObject();
      view.insert(QLatin1String(p.member), value);
      views.insert(group.mid(6), view);
    }
  }

  // the devices: the window's, then those beside with their front panels
  const QStringList beside = rest.take(kOpen).toString().split(' ', Qt::SkipEmptyParts);
  QJsonArray deviceArray;
  auto addDevice = [&](const QString &id, const QString &role)
  {
    QJsonObject o{ { "id", id }, { "role", role } };
    if (const std::optional<MyDevice> d = device(id))
    {
      o.insert("name", d->name);
      QJsonObject k;
      // a file is passed on: no secret of the device in it (a Victron's key
      // stays in My devices; one added from the file needs it entered again)
      for (auto it = d->keys.cbegin(); it != d->keys.cend(); ++it)
        if (it.key() != QLatin1String("Port settings/ble-key"))
          k.insert(it.key(), it.value().toString());
      o.insert("keys", k);
      const QJsonValue alarms = ruleToJson(d->alarms);
      if (!alarms.isUndefined())
        o.insert("alarms", alarms);
      const QJsonValue limit = ruleToJson(d->limitTest);
      if (!limit.isUndefined())
        o.insert("limitTest", limit);
    }
    if (role == QLatin1String("beside"))
    {
      QJsonObject panel;
      for (const Place &p : kPanel)
      {
        const QString key = "Windows/panel-" + id + QLatin1String(p.key);
        if (rest.contains(key))
          panel.insert(QLatin1String(p.member), toJson(rest.take(key), p.kind));
      }
      if (rest.contains(kPanelGeometry + id))
        panel.insert("geometry", rest.take(kPanelGeometry + id).toString());
      o.insert("panel", panel);
    }
    deviceArray << o;
  };
  if (!windowDevice.isEmpty())
    addDevice(windowDevice, "window");
  for (const QString &id : beside)
    if (id != windowDevice)
      addDevice(id, "beside");

  QJsonObject geometry;
  for (const QString &key : rest.keys())
    if (key.startsWith(kGeometry) && !key.startsWith(kPanelGeometry))
      geometry.insert(key.mid(kGeometry.size()), rest.take(key).toString());
  if (!geometry.isEmpty())
    layout.insert("geometry", geometry);

  QJsonObject window{ { "width", width }, { "height", height } };
  if (!windowDevice.isEmpty())
    window.insert("device", windowDevice);
  QJsonObject root{ { "format", kFormat }, { "version", kVersion }, { "minReader", kVersion },
                    { "createdWith", QCoreApplication::applicationVersion() }, { "name", name },
                    { "description", description }, { "window", window }, { "devices", deviceArray },
                    { "layout", layout }, { "views", views } };
  if (!rest.isEmpty())
    root.insert("other", QJsonObject::fromVariantMap(rest));
  if (!thumbnail.isEmpty())
    root.insert("thumbnail", QString::fromLatin1(thumbnail.toBase64()));

  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0 || !file.commit())
  {
    setError(error, tr("Cannot write the file."));
    return false;
  }
  return true;
}
