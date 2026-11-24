// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QObject>

class QTimer;

/// When a request sent to a device has to be answered, per channel: the
/// device (PowerSupply, FunctionGenerator) starts one with each request
/// and clears it with the answer; one that runs out emits expired() for
/// its channel. One timer for all channels, woken by the earliest.
class RequestDeadlines : public QObject
{
  Q_OBJECT
public:
  explicit RequestDeadlines(QObject *parent = nullptr);

  /// Channels 0..@p count - 1; new ones have none.
  void    resize(int count);
  /// Channel @p ch has to be answered within timeout() from now.
  void    start(int ch);
  void    clear(int ch);
  bool    pending(int ch) const;
  int     timeout() const { return m_timeout; }
  void    setTimeout(int ms) { m_timeout = ms; }

Q_SIGNALS:
  /// No answer for channel @p ch in time; its deadline is cleared.
  void    expired(int ch);

private:
  void    check();

  QList<qint64> m_deadline;   ///< ms since the epoch, 0 none
  int     m_timeout = 2000;
  QTimer *m_timer;
};
