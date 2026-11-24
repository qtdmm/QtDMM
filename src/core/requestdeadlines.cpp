// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/requestdeadlines.h"

#include <QDateTime>
#include <QTimer>

namespace
{
qint64 now()
{
  return QDateTime::currentMSecsSinceEpoch();
}
}

RequestDeadlines::RequestDeadlines(QObject *parent) : QObject(parent), m_timer(new QTimer(this))
{
  m_timer->setSingleShot(true);
  connect(m_timer, &QTimer::timeout, this, &RequestDeadlines::check);
}

void RequestDeadlines::resize(int count)
{
  m_deadline.resize(qMax(0, count));
}

void RequestDeadlines::start(int ch)
{
  if (ch < 0 || ch >= int(m_deadline.size()))
    return;
  m_deadline[ch] = now() + m_timeout;
  // the earliest deadline wakes the timer
  if (!m_timer->isActive() || m_timer->remainingTime() > m_timeout)
    m_timer->start(m_timeout);
}

void RequestDeadlines::clear(int ch)
{
  if (ch >= 0 && ch < int(m_deadline.size()))
    m_deadline[ch] = 0;
}

bool RequestDeadlines::pending(int ch) const
{
  return ch >= 0 && ch < int(m_deadline.size()) && m_deadline[ch] != 0;
}

void RequestDeadlines::check()
{
  const qint64 t = now();
  qint64 next = 0;
  QList<int> expiredNow;
  for (int ch = 0; ch < int(m_deadline.size()); ++ch)
  {
    if (m_deadline[ch] == 0)
      continue;
    if (m_deadline[ch] <= t)
    {
      m_deadline[ch] = 0;
      expiredNow << ch;
    }
    else
      next = next == 0 ? m_deadline[ch] : qMin(next, m_deadline[ch]);
  }
  if (next > 0)
    m_timer->start(int(next - t));
  for (int ch : expiredNow)
    Q_EMIT expired(ch);
}
