// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "device/transports/buffereddevice.h"

#include <QTimer>
#include <QMap>
#include <functional>

#include "device/dmmdecoder.h"
#include "core/calcexpr.h"


/// A virtual meter whose reading is a formula over the readings of other
/// meters, e.g. power from a voltage and a current meter ("u * i").
///
/// The inputs come from a function (Inputs, DeviceSession::calcInputs():
/// the meters of the process by name); every poll interval
/// the formula is evaluated and the result is offered as a sigrok-style
/// text line ("DC 6.02 W AUTO", padded to 30 bytes like SigrokDevice does),
/// so the ASCII decoder (FrameFormat::Sigrok) and everything after it work
/// unchanged. The inputs' names are the variables; a hyphen in a name can be
/// written as an underscore in the formula. The variable t is reserved: the
/// seconds since the device was opened, which together with sin(), rand()
/// and friends makes a signal generator ("QtDMM / Simulated meter" is this
/// device with a formula built from a few fields). While an input is missing,
/// invalid or older than three seconds the line carries "inf", which the
/// display shows as OL, and status() explains which input is the problem.
///
/// Up to kMaxValues values, separated by ';' ("V 12 + t; A 0.5; W v1 * v2"):
/// value n can use the ones before it as v1, v2 ...; each tick then gives a
/// line per value with sigrok-cli's channel in front ("P2: 0.500 A DC
/// AUTO"), which the decoder makes value n-1 (the main value is P1).
class CalcDevice : public BufferedDevice
{
  Q_OBJECT
public:
  /// One input: the main value of a meter.
  struct Input
  {
    double  value = 0;      ///< SI base units
    QString unit;           ///< base unit, "V"
    qint64  msecs = 0;      ///< when it arrived (ms since the epoch)
    bool    valid = false;  ///< false for overload, hold, no numeric value
  };
  /// The inputs by variable name, asked at every poll.
  using Inputs = std::function<QMap<QString, Input>()>;

  /// @param info   the "QtDMM / Calculated value" entry
  /// @param device "<unit>[/<coupling>] <formula>", e.g. "W u * i" or
  ///               "V/AC 12 + sin(2*pi*t/5)"; coupling defaults to DC.
  ///               Several values separated by ';'.
  /// @param inputs where the other meters' readings come from; none: only
  ///               formulas without variables (t aside) have a result
  /// @param parent parent object
  explicit CalcDevice(const DmmDecoder::DMMInfo &info, const QString &device,
                      Inputs inputs, QObject *parent = nullptr);


  /// Parses the formula; fails (see errorString()) when it does not parse
  /// or the unit is missing.
  bool open(OpenMode mode) override;
  void close() override;
  bool isSequential() const override { return true; }

  /// The unit given with the device string, of value @p n (0 the main one).
  QString unit(int n = 0) const { return m_values.value(n).unit; }
  /// "DC" or "AC" as given with the device string.
  QString coupling(int n = 0) const { return m_values.value(n).special; }
  /// The formula as parsed, empty before open().
  QString formula(int n = 0) const
  {
    return n < m_values.size() && m_values[n].expr ? m_values[n].expr->text() : QString();
  }
  /// How many values the device string gives.
  int valueCount() const { return int(m_values.size()); }

  /// The result as the meter would show it: scaled into [1, 1000) with an
  /// SI prefix and with as many decimals as fit the model's display count,
  /// like an autoranging meter (40000: 3.9999, 6.624, 123.46). Exposed for tests.
  static QString formatValue(double value, int counts, QString *prefix);

  /// Builds the decoder lines for the current inputs, one per value;
  /// exposed for tests.
  /// @param now    reference time for the staleness check (ms since epoch)
  ///               and for t (seconds since open())
  /// @param status receives the message status() would carry, empty when fine
  QByteArray currentLine(qint64 now, QString *status = nullptr) const;

  /// Readings older than this are treated as missing.
  static constexpr qint64 kStaleMs = 3000;
  /// Poll interval.
  static constexpr int kIntervalMs = 250;

Q_SIGNALS:
  /// Human-readable state for the status bar: which input is missing or
  /// stale, or empty when everything is fine. Emitted after the first
  /// poll and then on change only.
  void status(const QString &message);

protected:
  qint64 writeData(const char *data, qint64 len) override;

private Q_SLOTS:
  void tick();

private:
  static constexpr int kLineLength = 30;

  /// One value of the device string.
  struct Value
  {
    QString unit;
    QString special = QStringLiteral("DC");
    QString source;             ///< formula text from the device string
    std::optional<CalcExpr> expr;
  };

  DmmDecoder::DMMInfo m_dmmInfo;
  Inputs m_inputs;
  QVector<Value> m_values;
  qint64 m_openedMs = 0;        ///< when open() was called, for t
  QTimer m_timer;
  QString m_lastStatus;
  bool m_statusSent = false;    ///< the first tick always reports
};
