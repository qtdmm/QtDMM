// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/flatpakupdate.h"

#ifdef QTDMM_WITH_DBUS
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QDir>
#include <QFile>

namespace
{
const char *const kService = "org.freedesktop.portal.Flatpak";
const char *const kPath = "/org/freedesktop/portal/Flatpak";
const char *const kMonitor = "org.freedesktop.portal.Flatpak.UpdateMonitor";

enum Status : uint { Running = 0, Empty = 1, Done = 2, Error = 3 };
enum SpawnFlags : uint { LatestVersion = 2 };
}   // namespace
#endif

FlatpakUpdate::FlatpakUpdate(QObject *parent) : QObject(parent) {}

bool FlatpakUpdate::supported()
{
#ifdef QTDMM_WITH_DBUS
  return true;
#else
  return false;
#endif
}

void FlatpakUpdate::update()
{
#ifdef QTDMM_WITH_DBUS
  QDBusConnection bus = QDBusConnection::sessionBus();
  if (m_monitor.isEmpty())
  {
    QDBusMessage create = QDBusMessage::createMethodCall(kService, kPath, kService, "CreateUpdateMonitor");
    create << QVariantMap();
    const QDBusReply<QDBusObjectPath> reply = bus.call(create);
    if (!reply.isValid())
    {
      Q_EMIT failed(reply.error().message());
      return;
    }
    m_monitor = reply.value().path();
    bus.connect(kService, m_monitor, kMonitor, "Progress", this, SLOT(onProgress(QVariantMap)));
  }
  QDBusMessage call = QDBusMessage::createMethodCall(kService, m_monitor, kMonitor, "Update");
  call << QString() << QVariantMap();
  const QDBusMessage reply = bus.call(call);
  if (reply.type() == QDBusMessage::ErrorMessage)
    Q_EMIT failed(reply.errorMessage());
#else
  Q_EMIT failed(tr("Built without D-Bus: no Flatpak portal"));
#endif
}

void FlatpakUpdate::onProgress(const QVariantMap &info)
{
#ifdef QTDMM_WITH_DBUS
  switch (info.value("status").toUInt())
  {
  case Running: Q_EMIT progress(int(info.value("progress").toUInt())); break;
  case Empty: Q_EMIT upToDate(); break;
  case Done: Q_EMIT done(); break;
  default: Q_EMIT failed(info.value("error_message", info.value("error")).toString()); break;
  }
#else
  Q_UNUSED(info)
#endif
}

bool FlatpakUpdate::restart()
{
#ifdef QTDMM_WITH_DBUS
  using Fds = QMap<uint, QDBusUnixFileDescriptor>;
  using Envs = QMap<QString, QString>;
  qDBusRegisterMetaType<Fds>();
  qDBusRegisterMetaType<Envs>();
  // Spawn(ay cwd, aay argv, a{uh} fds, a{ss} envs, u flags, a{sv} options):
  // byte strings ending in a zero byte
  auto bytes = [](const QString &s) { return QFile::encodeName(s) + '\0'; };
  QDBusMessage call = QDBusMessage::createMethodCall(kService, kPath, kService, "Spawn");
  call << bytes(QDir::currentPath()) << QVariant::fromValue(QByteArrayList{ bytes("qtdmm") }) << QVariant::fromValue(Fds())
       << QVariant::fromValue(Envs()) << uint(LatestVersion) << QVariantMap();
  return QDBusConnection::sessionBus().call(call).type() != QDBusMessage::ErrorMessage;
#else
  return false;
#endif
}
