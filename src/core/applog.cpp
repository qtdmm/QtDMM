// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/applog.h"

#include <QCoreApplication>
#include <QFile>
#include <QMutexLocker>
#include <QSysInfo>
#include <QTextStream>

AppLog::AppLog(int maxLines, QObject *parent) :
  QObject(parent),
  m_max(qMax(1, maxLines))
{
  qRegisterMetaType<AppLog::Entry>();
}

AppLog *AppLog::instance()
{
  static AppLog log;
  return &log;
}

void AppLog::add(Level level, const QString &scope, const QString &message)
{
  Entry entry;
  entry.time = QDateTime::currentDateTime();
  entry.scope = scope;
  entry.level = level;
  // a line each: several lines (a hint with its commands) in one
  entry.message = QString(message).replace('\n', QStringLiteral(" · "));
  {
    QMutexLocker lock(&m_mutex);
    entry.number = ++m_count;
    m_entries << entry;
    if (m_entries.size() > m_max)
      m_entries.removeFirst();
  }
  Q_EMIT added(entry);
}

QList<AppLog::Entry> AppLog::entries() const
{
  QMutexLocker lock(&m_mutex);
  return m_entries;
}

QString AppLog::line(const Entry &entry)
{
  static const char *const levels[] = { "DEBUG", "INFO", "WARNING", "ERROR" };
  return QStringLiteral("%1\t%2\t%3\t%4").arg(entry.time.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz")), entry.scope,
                                              QLatin1String(levels[entry.level]), entry.message);
}

bool AppLog::write(const QString &file, QString *error) const
{
  QFile out(file);
  if (!out.open(QIODevice::WriteOnly | QIODevice::Text))
  {
    if (error)
      *error = out.errorString();
    return false;
  }
  QTextStream stream(&out);
  stream << "# QtDMM " << QCoreApplication::applicationVersion() << ", Qt " << qVersion() << ", "
         << QSysInfo::prettyProductName() << '\n';
  for (const Entry &entry : entries())
    stream << line(entry) << '\n';
  stream.flush();
  if (out.error() != QFile::NoError)
  {
    if (error)
      *error = out.errorString();
    return false;
  }
  return true;
}
