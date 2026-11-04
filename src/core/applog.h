// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDateTime>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QString>

/// The application's log: errors, connection states and messages of the
/// program, also Qt's (qWarning() & co. through main()'s message handler).
/// A user attaches its export to a report, so nothing secret goes in: for a
/// port use MeterConnection::deviceName(), a Bluetooth port holds its key.
///
/// It rolls: the oldest lines go beyond kMaxLines. add() may be called from
/// any thread; added() then comes in that thread, so connect it queued.
class AppLog : public QObject
{
  Q_OBJECT
public:
  enum Level { Debug, Info, Warning, Error };
  Q_ENUM(Level)

  struct Entry
  {
    qint64    number = 0;   ///< counts up, also over lines rolled out
    QDateTime time;
    QString   scope;        ///< a device's name, "app", "Graph"
    Level     level = Info;
    QString   message;      ///< one line
  };

  static constexpr int kMaxLines = 10000;

  explicit AppLog(int maxLines = kMaxLines, QObject *parent = nullptr);
  /// The log of the process.
  static AppLog *instance();

  void        add(Level level, const QString &scope, const QString &message);
  static void info(const QString &scope, const QString &message) { instance()->add(Info, scope, message); }
  static void warning(const QString &scope, const QString &message) { instance()->add(Warning, scope, message); }
  static void error(const QString &scope, const QString &message) { instance()->add(Error, scope, message); }

  /// The lines kept, oldest first.
  QList<Entry> entries() const;
  int         maxLines() const { return m_max; }

  /// "2026-11-04 09:12:33.123<tab>scope<tab>WARNING<tab>message".
  static QString line(const Entry &entry);
  /// All lines kept into @p file, after a line with the versions; false and
  /// @p error when it cannot be written.
  bool        write(const QString &file, QString *error = nullptr) const;

Q_SIGNALS:
  void        added(const AppLog::Entry &entry);

private:
  mutable QMutex m_mutex;
  QList<Entry>   m_entries;
  qint64         m_count = 0;
  int            m_max;
};
Q_DECLARE_METATYPE(AppLog::Entry)
