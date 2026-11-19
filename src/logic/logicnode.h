// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <functional>
#include <optional>

#include "core/reading.h"
#include "core/sampletypes.h"
#include "core/valueref.h"

/// A logic node: a source of values computed from other values (concept
/// tmp/redesign/logikknoten_konzept.md §5-§8). It is a source like a device
/// - its outputs are readings of its own source, recorded, shown, exported
/// and served like a meter's -, but has no port, protocol or keys.
///
/// This class is the common form; there is no general node, only its
/// specialisations (formula, integral, generator ...). It knows no session:
/// NodeRunner gives it the inputs and takes its outputs.
///
/// - **Inputs:** any values of any source (ValueRef), none for a generator.
///   The leading input triggers a computation when a new reading of it
///   comes; the others take their last reading if it is not older than
///   maxAgeMs(). A node with a period computes on a clock instead.
/// - **Outputs:** up to kMaxValues, each a value or (later) an event.
/// - **On/off:** off is the bypass, which each specialisation defines
///   (bypass(): a filter passes its input on, a formula gives nothing).
/// - **State:** a node may keep state (an integral); reset() clears it.
class LogicNode
{
public:
  /// What an output delivers: a value, as a meter's, or an event (a
  /// switching relay, a comparison) - the common form has it, 27.1 uses
  /// values only.
  enum class OutputKind { Value, Event };
  struct Output
  {
    QString    name;   ///< shown in the views; empty: named by what it measures
    QString    unit;   ///< with prefix as the decoders spell it ("W", "mA", "Ohm"); empty: none
    OutputKind kind = OutputKind::Value;
  };
  /// The inputs as they are when the node computes: the reading of each
  /// input, or none when it has none or one older than maxAgeMs().
  struct Context
  {
    QList<std::optional<Reading>> inputs;
    qint64 t = 0;      ///< the time of the computation on the core clock (Sample::now()): the leading input's, or the clock's
    qint64 wall = 0;   ///< the same as ms since the epoch
  };
  /// The outputs of one computation: a number for each, or none (a gap);
  /// fewer than outputs(): the rest none.
  using Values = QList<std::optional<double>>;

  virtual ~LogicNode() = default;

  /// The kind, as stored ("formula", "integral", "generator").
  virtual QString type() const = 0;
  /// The outputs it has now (they may follow its parameters).
  virtual QList<Output> outputs() const = 0;
  /// Computes the outputs from @p ctx; may change the node's state.
  virtual Values evaluate(const Context &ctx) = 0;
  /// What it gives while switched off: by default nothing (a gap).
  virtual Values bypass(const Context &ctx) { Q_UNUSED(ctx); return {}; }
  /// Clears its state (an integral back to 0); nothing by default.
  virtual void reset() {}
  /// Its own parameters (formula, unit, ...), stored with the entry; the
  /// common ones (inputs, leading, max age, period, on) are the runner's.
  virtual QJsonObject parameters() const { return {}; }
  virtual void setParameters(const QJsonObject &p) { Q_UNUSED(p); }

  /// @name The common form
  /// @{
  QList<ValueRef> inputs() const { return m_inputs; }
  void        setInputs(const QList<ValueRef> &inputs) { m_inputs = inputs; }
  /// The input whose new readings trigger a computation (the first by default).
  int         leadingInput() const { return m_inputs.isEmpty() ? -1 : qBound(0, m_leading, int(m_inputs.size()) - 1); }
  void        setLeadingInput(int index) { m_leading = index; }
  /// A reading of another input older than this, relative to the leading
  /// one's, is no reading (3 s as the calculated value had it).
  int         maxAgeMs() const { return m_maxAgeMs; }
  void        setMaxAgeMs(int ms) { m_maxAgeMs = qMax(1, ms); }
  /// A clock instead of the leading input (a generator): 0 = none.
  int         periodMs() const { return m_periodMs; }
  void        setPeriodMs(int ms) { m_periodMs = qMax(0, ms); }
  /// On (computes) or off (bypass()); the green LED of its face.
  bool        isOn() const { return m_on; }
  void        setOn(bool on) { m_on = on; }
  /// The common form and the node's parameters as one object, as stored.
  QJsonObject toJson() const;
  /// Reads toJson(): the common form, then setParameters().
  void        fromJson(const QJsonObject &o);
  /// @}

  /// Would a node with @p source taking @p inputs close a loop? @p inputsOf
  /// gives the inputs of any other node's source (empty for a device). A
  /// node may not take its own values either.
  static bool makesLoop(const QString &source, const QList<ValueRef> &inputs,
                        const std::function<QList<ValueRef>(const QString &source)> &inputsOf);

private:
  QList<ValueRef> m_inputs;
  int  m_leading = 0;
  int  m_maxAgeMs = 3000;
  int  m_periodMs = 0;
  bool m_on = true;
};
