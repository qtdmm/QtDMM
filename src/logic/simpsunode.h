// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <memory>

#include "logic/logicnode.h"

class PowerSupply;
class SimPowerSupply;

/// A simulated power supply: a logic node without inputs, on a clock as the
/// generator (250 ms), that is the backend of a PowerSupply - its front
/// (PsuFrontPanel) sets the voltage and switches the outputs. "Simulated →
/// Power supply" in the assistant. Stage 1: voltage only, no current, no
/// load, no CV/CC, no protection; no SCPI.
///
/// - **Channels:** 1 to kMaxChannels (dual: 2), each 0..kMaxVoltage V in
///   10 mV. Each channel shown (ChannelSettings::shown) gives its measured
///   voltage as an ordinary value, "CH1 U" in V: graph, table, limit test,
///   other nodes as usual. A channel not shown gives none and is not touched.
/// - **Output off → 0 V**, after the fall time. The outputs always start
///   off, whatever was set before.
/// - **A worse supply** (Faults, all 0 by default: an ideal one): rise time
///   when switched on and fall time when switched off (or set higher /
///   lower), an overshoot in percent of the step after a rise, ripple and
///   noise, a drift since switched on. The clock of 250 ms cannot show a
///   100 Hz hum: the ripple is honestly a band, random within ±Vpp/2, not a
///   sine.
/// - **Stored** with its entry (parameters()): channels, setpoints, the
///   channels' settings (shown, colour, limits, steps), the faults.
class SimPsuNode : public LogicNode
{
public:
  static constexpr const char *kType = "simpsu";
  static constexpr int kDefaultPeriodMs = 250;
  static constexpr int kMaxChannels = 4;
  static constexpr double kMaxVoltage = 30;
  static constexpr int kDecimals = 2;

  /// What makes it a worse supply than an ideal one.
  struct Faults
  {
    double riseMs = 0;         ///< 0 V to the setpoint when switched on (and per step up)
    double fallMs = 0;         ///< to 0 V when switched off (and per step down)
    double overshoot = 0;      ///< % of a step up, decaying after the rise
    double rippleMvpp = 0;     ///< a band of this width around the voltage
    double noiseMv = 0;        ///< random, this much at most either way
    double driftMvPerMin = 0;  ///< since the output went on
    bool   operator==(const Faults &o) const = default;
    QJsonObject toJson() const;
    static Faults fromJson(const QJsonObject &o);
  };

  SimPsuNode();
  ~SimPsuNode() override;

  QString type() const override { return QString::fromLatin1(kType); }
  QList<Output> outputs() const override;
  Values  evaluate(const Context &ctx) override;
  void    reset() override;
  QJsonObject parameters() const override;
  void    setParameters(const QJsonObject &p) override;
  PowerSupply *powerSupply() override;

  int     channels() const { return int(m_ch.size()); }
  /// 1..kMaxChannels; new ones start at 0 V.
  void    setChannels(int count);
  double  setpoint(int ch) const;
  void    setSetpoint(int ch, double u);
  bool    output(int ch) const;
  Faults  faults() const { return m_faults; }
  void    setFaults(const Faults &faults) { m_faults = faults; }
  /// The shown channels' measured voltages as last computed (for the tests).
  double  voltage(int ch) const;

private:
  friend class SimPowerSupply;
  struct Channel
  {
    double setpoint = 0;
    bool   out = false;
    double level = 0;          ///< the voltage without ripple, noise, drift
    // the step under way: from, to, when it started, how long it takes
    double from = 0, to = 0;
    qint64 since = -1;
    double duration = 0;
    bool   rising = false;
    qint64 onSince = -1;       ///< when the output went on (drift)
    double shown = 0;          ///< the last value given
  };
  /// The voltage of channel @p c at @p t (ms, core clock).
  double  levelAt(Channel &c, qint64 t) const;
  /// The target of @p c changed (setpoint, output) at @p t: a new step.
  void    retarget(Channel &c, qint64 t);
  void    switchOutput(int ch, bool on);

  QList<Channel> m_ch;
  Faults  m_faults;
  qint64  m_now = 0;   ///< the time of the last computation
  std::unique_ptr<SimPowerSupply> m_supply;
};
