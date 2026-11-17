// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/graphmodel.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

// the words of the first three axes, as the graph on QtCharts writes them
static const char *const kAxisWords[] = { "left", "right", "third" };

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

void GraphModel::setValueAxis(const ValueRef &value, int axis)
{
  if (axis == valueAxis(value))
    return;
  if (axis < 0)
    m_valueAxes.remove(value);
  else
    m_valueAxes.insert(value, axis);
  Q_EMIT valueAxesChanged();
}

QString GraphModel::valueAxesSetting() const
{
  // in the order of the values, then those of values not shown now
  QList<ValueRef> order = m_values;
  QList<ValueRef> others = m_valueAxes.keys();
  std::sort(others.begin(), others.end(), [](const ValueRef &a, const ValueRef &b)
  { return a.source != b.source ? a.source < b.source : a.id < b.id; });
  for (const ValueRef &v : std::as_const(others))
    if (!order.contains(v))
      order << v;
  QStringList parts;
  for (const ValueRef &v : std::as_const(order))
  {
    const int axis = valueAxis(v);
    if (axis < 0)
      continue;
    parts << v.toString() + ':' + (axis < 3 ? QString::fromLatin1(kAxisWords[axis]) : QString("axis%1").arg(axis + 1));
  }
  return parts.join(' ');
}

void GraphModel::setValueAxesSetting(const QString &text)
{
  QHash<ValueRef, int> axes;
  for (const QString &part : text.split(' ', Qt::SkipEmptyParts))
  {
    // the source has colons of its own: the axis is after the last one
    const qsizetype at = part.lastIndexOf(':');
    if (at <= 0)
      continue;
    const QString word = part.mid(at + 1);
    int axis = -1;
    for (int i = 0; i < 3; ++i)
      if (word == QLatin1String(kAxisWords[i]))
        axis = i;
    if (axis < 0 && word.startsWith(QLatin1String("axis")))
      axis = word.mid(4).toInt() - 1;
    if (axis >= 0)
      axes.insert(ValueRef::fromString(part.left(at)), axis);
  }
  if (axes == m_valueAxes)
    return;
  m_valueAxes = axes;
  Q_EMIT valueAxesChanged();
}

void GraphModel::setFixedRange(const QString &unit, double min, double max)
{
  if (unit.isEmpty() || !(max > min) || m_fixedRanges.value(unit, Range(0, 0)) == Range(min, max))
    return;
  m_fixedRanges.insert(unit, { min, max });
  Q_EMIT fixedRangesChanged();
}

void GraphModel::clearFixedRange(const QString &unit)
{
  if (m_fixedRanges.remove(unit))
    Q_EMIT fixedRangesChanged();
}

QString GraphModel::fixedRangesSetting() const
{
  QJsonObject o;
  for (auto it = m_fixedRanges.cbegin(); it != m_fixedRanges.cend(); ++it)
    o.insert(it.key(), QJsonArray{ it->first, it->second });
  return o.isEmpty() ? QString() : QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

void GraphModel::setFixedRangesSetting(const QString &text)
{
  QMap<QString, Range> ranges;
  const QJsonObject o = QJsonDocument::fromJson(text.toUtf8()).object();
  for (auto it = o.begin(); it != o.end(); ++it)
  {
    const QJsonArray r = it.value().toArray();
    if (r.size() == 2 && r[1].toDouble() > r[0].toDouble())
      ranges.insert(it.key(), { r[0].toDouble(), r[1].toDouble() });
  }
  if (ranges == m_fixedRanges)
    return;
  m_fixedRanges = ranges;
  Q_EMIT fixedRangesChanged();
}

void GraphModel::setVariant(const QString &name)
{
  if (name == m_variant)
    return;
  m_variant = name;
  Q_EMIT lookChanged();
}

void GraphModel::setPatternedLines(bool on)
{
  if (on == m_patterns)
    return;
  m_patterns = on;
  Q_EMIT lookChanged();
}

void GraphModel::setStatsVisible(bool on)
{
  if (on == m_stats)
    return;
  m_stats = on;
  Q_EMIT statsVisibleChanged(on);
}

void GraphModel::setMinimapVisible(bool on)
{
  if (on == m_minimap)
    return;
  m_minimap = on;
  Q_EMIT overviewChanged();
}

void GraphModel::setStatesVisible(bool on)
{
  if (on == m_states)
    return;
  m_states = on;
  Q_EMIT overviewChanged();
}

void GraphModel::setEnvelopeVisible(bool on)
{
  if (on == m_envelope)
    return;
  m_envelope = on;
  Q_EMIT overviewChanged();
}

void GraphModel::setOutliers(const GraphOutliers::Rule &rule)
{
  if (rule == m_outliers)
    return;
  m_outliers = rule;
  Q_EMIT outliersChanged();
}

void GraphModel::setReference(const GraphReference::Line &line)
{
  if (line == m_reference)
    return;
  m_reference = line;
  Q_EMIT referenceChanged();
}

void GraphModel::setReferenceLines(const QList<double> &values)
{
  const QList<double> lines = GraphReference::lines(values);
  if (lines == m_referenceLines)
    return;
  m_referenceLines = lines;
  Q_EMIT referenceChanged();
}

void GraphModel::setCrossingsVisible(bool on)
{
  if (on == m_crossings)
    return;
  m_crossings = on;
  Q_EMIT referenceChanged();
}

// The order matters: the axes and fixed ranges name the values
void GraphModel::readSettings(const Get &get, const QString &key)
{
  setValuesSetting(get(key + "-values", QString()).toString());
  setValueAxesSetting(get(key + "-value-axes", QString()).toString());
  setFixedRangesSetting(get(key + "-fixed-ranges", QString()).toString());
  setVariant(get(key + "-variant", QString()).toString());
  setPatternedLines(get(key + "-patterns", false).toBool());
  setStatsVisible(get(key + "-stats", false).toBool());
  setMinimapVisible(get(key + "-minimap", true).toBool());
  setStatesVisible(get(key + "-states", true).toBool());
  setEnvelopeVisible(get(key + "-envelope", false).toBool());
  setReferenceSetting(get(key + "-reference", QString()).toString());
  setReferenceLinesSetting(get(key + "-reference-lines", QString()).toString());
  setCrossingsVisible(get(key + "-crossings", true).toBool());
  setOutliersSetting(get(key + "-outliers", QString()).toString());
  if (const int seconds = get(key + "-window", 0).toInt(); seconds > 0)
    setWindowSeconds(seconds);
  setFollowAll(get(key + "-all", false).toBool());
}

void GraphModel::writeSettings(const Set &set, const QString &key) const
{
  set(key + "-values", valuesSetting());
  set(key + "-value-axes", valueAxesSetting());
  set(key + "-fixed-ranges", fixedRangesSetting());
  set(key + "-variant", variant());
  set(key + "-patterns", patternedLines());
  set(key + "-stats", statsVisible());
  set(key + "-minimap", minimapVisible());
  set(key + "-states", statesVisible());
  set(key + "-envelope", envelopeVisible());
  set(key + "-reference", referenceSetting());
  set(key + "-reference-lines", referenceLinesSetting());
  set(key + "-crossings", crossingsVisible());
  set(key + "-outliers", outliersSetting());
  set(key + "-window", windowSeconds());
  set(key + "-all", followAll());
}
