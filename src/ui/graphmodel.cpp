// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/graphmodel.h"

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
