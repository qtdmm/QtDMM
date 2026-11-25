// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <array>
#include <memory>

#include "core/functiongenerator.h"
#include "logic/logicnode.h"

class SimFunctionGenerator;

/// A simulated function generator: a logic node without inputs, on a clock
/// (250 ms), that is the backend of a FunctionGenerator - its front
/// (GenFrontPanel) sets shape, frequency, amplitude, offset and switches
/// the output. "Simulated → Function generator" in the assistant. Its
/// limits are those of the generator in the OWON SDS215S (sine up to
/// 25 MHz, 5 mVpp to 3 Vpp).
///
/// - **What it gives:** QtDMM reads every 250 ms and cannot show a 1 kHz
///   sine, so a channel gives what a meter at its output would read:
///   "CH1 Vrms" (the AC part's RMS, V AC), "CH1 Vpp", "CH1 f" (Hz),
///   "CH1 mean" (the DC part, V DC) and "CH1 u" - the voltage at this
///   moment, only up to kSlowHz (then the graph shows the real curve), no
///   value above. Computed from one period of the shape, so a pulse's
///   duty cycle or one of the other shapes (sinc, gauss, stairs, ...)
///   counts as on a true RMS meter.
/// - **Which of them** is chosen per channel (setGiven()), at most
///   kMaxValues for the device as for every source [E 24.11.]; by default
///   with one channel Vrms, f, mean and u (the peak to peak is the
///   amplitude on the LCD), with two Vrms and f of each. A channel not
///   shown gives none.
/// - **Output off:** the voltages 0 V, the frequency stays. The outputs
///   always start off.
/// - **A worse generator** (Faults, all 0 by default: an ideal one): an
///   error of the frequency (ppm) and of the amplitude (%), the amplitude
///   falling towards high frequencies (first order, -3 dB at the cutoff),
///   noise on the voltages.
/// - **Stored** with its entry: channels, each one's setting, the faults,
///   the channels' settings (shown, colour, limits, steps).
class SimGenNode : public LogicNode
{
public:
  using Shape = FunctionGenerator::Shape;
  using Value = FunctionGenerator::Value;
  static constexpr const char *kType = "simgen";
  static constexpr int kDefaultPeriodMs = 250;
  static constexpr int kMaxChannels = FunctionGenerator::kMaxChannels;
  static constexpr double kSlowHz = 0.5;   ///< up to this u(t) is given
  /// What a channel can give.
  enum Quantity { Rms, PeakToPeak, Frequency, Mean, Now };
  static constexpr int kQuantities = 5;
  using Given = std::array<bool, kQuantities>;
  /// "Vrms", "Vpp", "f", "mean", "u": in the values' names.
  static QString quantityName(Quantity q);
  /// What each of @p channels gives by default.
  static Given defaultGiven(int channels);

  /// What makes it a worse generator than an ideal one.
  struct Faults
  {
    double frequencyPpm = 0;   ///< the frequency off by this
    double amplitudePct = 0;   ///< the amplitude off by this
    double cutoffHz = 0;       ///< -3 dB here (first order); 0: flat
    double noiseMv = 0;        ///< random, this much at most either way
    bool   operator==(const Faults &o) const = default;
    QJsonObject toJson() const;
    static Faults fromJson(const QJsonObject &o);
  };

  /// A channel's setting.
  struct Setting
  {
    Shape   shape = Shape::Sine;
    QString arbitrary;          ///< the shape's name when Arbitrary
    double  frequency = 1000, amplitude = 1, offset = 0, duty = 50, symmetry = 50;
    bool    operator==(const Setting &o) const = default;
  };

  SimGenNode();
  ~SimGenNode() override;

  QString type() const override { return QString::fromLatin1(kType); }
  QList<Output> outputs() const override;
  Values  evaluate(const Context &ctx) override;
  QJsonObject parameters() const override;
  void    setParameters(const QJsonObject &p) override;
  QJsonObject setUpParameters() const override;
  FunctionGenerator *functionGenerator() override;

  int     channels() const { return int(m_ch.size()); }
  /// 1..kMaxChannels; new ones start at the default setting.
  void    setChannels(int count);
  Setting setting(int ch) const;
  bool    output(int ch) const;
  /// What channel @p ch gives; more than kMaxValues in all are left out
  /// from the last channel's end.
  Given   given(int ch) const;
  void    setGiven(int ch, const Given &given);
  Faults  faults() const { return m_faults; }
  void    setFaults(const Faults &faults) { m_faults = faults; }
  /// The shapes this simulation has besides the basic ones.
  static QStringList arbitraryShapes();

  /// One period of @p shape at @p phase (0..1), spanning -1 to 1; @p duty
  /// and @p symmetry in percent (pulse, ramp).
  static double waveform(Shape shape, const QString &arbitrary, double phase, double duty = 50, double symmetry = 50);
  /// What a true RMS meter reads at an ideal output: the DC part (mean) and
  /// the RMS of the AC part, for amplitude @p vpp and @p offset.
  struct Levels
  {
    double mean = 0, rms = 0;
  };
  static Levels levels(const Setting &s);

private:
  friend class SimFunctionGenerator;
  struct Channel
  {
    Setting set;
    bool    out = false;
    Given   given {};
  };
  bool    valid(int ch) const { return ch >= 0 && ch < channels(); }

  QList<Channel> m_ch;
  Faults  m_faults;
  std::unique_ptr<SimFunctionGenerator> m_generator;
};
