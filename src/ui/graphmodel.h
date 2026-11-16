// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QList>
#include <QMap>
#include <QObject>
#include <QPair>

#include "core/valueref.h"
#include "ui/views/graphreference.h"

/// What a graph window shows, without anything that draws: its values and
/// its time window, with the texts they have in the settings and benches
/// (the formats of the graph on QtCharts, under keys of its own). The graph
/// on QCustomPlot shows what its model says; it grows with that graph.
class GraphModel : public QObject
{
  Q_OBJECT
public:
  static constexpr int kDefaultWindowSeconds = 600;

  explicit GraphModel(QObject *parent = nullptr);

  /// The values drawn, the first one on the left axis.
  QList<ValueRef> values() const { return m_values; }
  void        setValues(const QList<ValueRef> &values);
  /// "0@device:a 1@device:b", as the settings keep the values.
  QString     valuesSetting() const;
  void        setValuesSetting(const QString &text);

  /// The axis chosen for a value: kAxisAuto, or the place of the axis - 0
  /// the left one, 1 the first on the right, 2 the next one out, and so on
  /// (how many a graph draws is the graph's). Auto: the first unit on the
  /// left, each further unit on the next axis, a unit that has an axis on it.
  static constexpr int kAxisAuto = -1;
  int         valueAxis(const ValueRef &value) const { return m_valueAxes.value(value, kAxisAuto); }
  QHash<ValueRef, int> valueAxes() const { return m_valueAxes; }
  void        setValueAxis(const ValueRef &value, int axis);
  /// "1@device:a:right 0@device:b:left", as the graph on QtCharts keeps
  /// them: left, right, third, then "axis4", "axis5"; Auto is left out.
  QString     valueAxesSetting() const;
  void        setValueAxesSetting(const QString &text);

  /// Fixed ranges of the axes by base unit ("Hz" 49.5 .. 50.5): the axis of
  /// that unit shows them; a unit without one fits its values.
  using Range = QPair<double, double>;
  QMap<QString, Range> fixedRanges() const { return m_fixedRanges; }
  void        setFixedRange(const QString &unit, double min, double max);
  void        clearFixedRange(const QString &unit);
  /// {"Hz":[49.5,50.5]}, empty without any.
  QString     fixedRangesSetting() const;
  void        setFixedRangesSetting(const QString &text);

  /// The colour variant of this graph by its name ("scope"); empty: the settings'.
  QString     variant() const { return m_variant; }
  void        setVariant(const QString &name);
  /// Each curve after the first with a dash pattern of its own.
  bool        patternedLines() const { return m_patterns; }
  void        setPatternedLines(bool on);

  /// The statistics column next to the plot.
  bool        statsVisible() const { return m_stats; }
  void        setStatsVisible(bool on);

  /// The reference line (GraphReference::setting(): "", "mean", "off", ...),
  /// the lines added to it ("1.5 -2", base unit of the left axis) and
  /// whether the crossings of the curve with them are marked and counted.
  GraphReference::Line reference() const { return m_reference; }
  void        setReference(const GraphReference::Line &line);
  QString     referenceSetting() const { return GraphReference::setting(m_reference); }
  void        setReferenceSetting(const QString &text) { setReference(GraphReference::fromSetting(text)); }
  QList<double> referenceLines() const { return m_referenceLines; }
  void        setReferenceLines(const QList<double> &values);
  QString     referenceLinesSetting() const { return GraphReference::linesSetting(m_referenceLines); }
  void        setReferenceLinesSetting(const QString &text) { setReferenceLines(GraphReference::linesFromSetting(text)); }
  bool        crossingsVisible() const { return m_crossings; }
  void        setCrossingsVisible(bool on);

  /// The time the graph shows, seconds; at least 1.
  int         windowSeconds() const { return m_windowSeconds; }
  void        setWindowSeconds(int seconds);
  /// "All": the window is the whole recording and grows with it.
  bool        followAll() const { return m_followAll; }
  void        setFollowAll(bool on);
  /// The window "All" takes for @p recordedSeconds of recording, seconds:
  /// at least 10, whole minutes from one minute on, at most @p limitSeconds
  /// (0: no limit). With @p grow a quarter more, so the window does not
  /// change with every reading.
  static int  allTarget(double recordedSeconds, bool grow, int limitSeconds);

Q_SIGNALS:
  void        valuesChanged();
  void        valueAxesChanged();
  void        fixedRangesChanged();
  void        lookChanged();
  void        statsVisibleChanged(bool on);
  void        referenceChanged();
  void        windowChanged(int seconds);
  void        followAllChanged(bool on);

private:
  QList<ValueRef> m_values;
  QHash<ValueRef, int> m_valueAxes;      ///< those not on Auto
  QMap<QString, Range> m_fixedRanges;
  QString     m_variant;
  bool        m_patterns = false;
  bool        m_stats = false;
  GraphReference::Line m_reference;
  QList<double> m_referenceLines;
  bool        m_crossings = true;
  int         m_windowSeconds = kDefaultWindowSeconds;
  bool        m_followAll = false;
};
