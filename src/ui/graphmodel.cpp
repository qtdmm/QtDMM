// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/graphmodel.h"

#include <cmath>

GraphModel::GraphModel(QObject *parent) : QObject(parent)
{
}

void GraphModel::setValues(const QList<ValueRef> &values)
{
  if (values == m_values)
    return;
  m_values = values;
  Q_EMIT valuesChanged();
}

QString GraphModel::valuesSetting() const
{
  QStringList shown;
  for (const ValueRef &v : m_values)
    shown << v.toString();
  return shown.join(' ');
}

void GraphModel::setValuesSetting(const QString &text)
{
  QList<ValueRef> values;
  for (const QString &v : text.split(' ', Qt::SkipEmptyParts))
    values << ValueRef::fromString(v);
  setValues(values);
}

void GraphModel::setWindowSeconds(int seconds)
{
  seconds = qMax(1, seconds);
  if (seconds == m_windowSeconds)
    return;
  m_windowSeconds = seconds;
  Q_EMIT windowChanged(seconds);
}

void GraphModel::setFollowAll(bool on)
{
  if (on == m_followAll)
    return;
  m_followAll = on;
  Q_EMIT followAllChanged(on);
}

int GraphModel::allTarget(double recordedSeconds, bool grow, int limitSeconds)
{
  int target = int(std::ceil(grow ? recordedSeconds * 1.25 : recordedSeconds));
  target = qMax(target, 10);
  // whole minutes from one minute on: no odd window with every reading
  if (target > 60)
    target = (target + 59) / 60 * 60;
  if (limitSeconds > 0)
    target = qMin(target, limitSeconds);
  return target;
}
