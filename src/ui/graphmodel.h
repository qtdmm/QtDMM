// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QList>
#include <QMap>
#include <QObject>
#include <QPair>
#include <QVariant>
#include <functional>

#include "core/valueref.h"
#include "ui/views/graphoutliers.h"
#include "ui/views/graphreference.h"

/// What a graph window shows, without anything that draws: its values and
/// its time window, with the texts they have in the settings and benches
/// (graph-N-...). GraphWidget shows what its model says.
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
  /// "1@device:a:right 0@device:b:left", as benches since 26.4 keep
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

  /// The overview strip below the plot, the strip of the meter's states in
  /// it, and the min/max band behind the left axis' curve.
  bool        minimapVisible() const { return m_minimap; }
  void        setMinimapVisible(bool on);
  bool        statesVisible() const { return m_states; }
  void        setStatesVisible(bool on);
  bool        envelopeVisible() const { return m_envelope; }
  void        setEnvelopeVisible(bool on);

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

  /// Readings not drawn: the rule and its text ("sigma 3", "jump 0.5 V", "limits 0 10 V", "").
  GraphOutliers::Rule outliers() const { return m_outliers; }
  void        setOutliers(const GraphOutliers::Rule &rule);
  QString     outliersSetting() const { return m_outliers.setting(); }
  void        setOutliersSetting(const QString &text) { setOutliers(GraphOutliers::Rule::fromSetting(text)); }

  /// The time the graph shows, seconds; at least 1.
  int         windowSeconds() const { return m_windowSeconds; }
  void        setWindowSeconds(int seconds);
  /// "All": the window is the whole recording and grows with it; on in a
  /// new graph.
  bool        followAll() const { return m_followAll; }
  void        setFollowAll(bool on);
  /// The window "All" takes for @p recordedSeconds of recording, seconds:
  /// at least 10, whole minutes from one minute on, at most @p limitSeconds
  /// (0: no limit). With @p grow a quarter more, so the window does not
  /// change with every reading.
  static int  allTarget(double recordedSeconds, bool grow, int limitSeconds);

  /// Its settings in a window layout or bench under @p key ("Windows/graphView-2"):
  /// @p key + "-values", "-value-axes", "-fixed-ranges", "-variant",
  /// "-patterns", "-stats", "-minimap", "-states", "-envelope",
  /// "-reference", "-reference-lines", "-crossings", "-outliers",
  /// "-window", "-all". What @p get does not have takes its default.
  using Get = std::function<QVariant(const QString &key, const QVariant &def)>;
  using Set = std::function<void(const QString &key, const QVariant &value)>;
  void        readSettings(const Get &get, const QString &key);
  void        writeSettings(const Set &set, const QString &key) const;

Q_SIGNALS:
  void        valuesChanged();
  void        valueAxesChanged();
  void        fixedRangesChanged();
  void        lookChanged();
  void        statsVisibleChanged(bool on);
  void        overviewChanged();
  void        referenceChanged();
  void        outliersChanged();
  void        windowChanged(int seconds);
  void        followAllChanged(bool on);

private:
  QList<ValueRef> m_values;
  QHash<ValueRef, int> m_valueAxes;      ///< those not on Auto
  QMap<QString, Range> m_fixedRanges;
  QString     m_variant;
  bool        m_patterns = false;
  bool        m_stats = false;
  bool        m_minimap = true;
  bool        m_states = true;
  bool        m_envelope = false;
  GraphReference::Line m_reference;
  QList<double> m_referenceLines;
  bool        m_crossings = true;
  GraphOutliers::Rule m_outliers;
  int         m_windowSeconds = kDefaultWindowSeconds;
  bool        m_followAll = true;    ///< [E 25.11.] a new graph shows all
};
