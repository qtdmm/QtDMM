// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>
#include <optional>

#include "core/sampletypes.h"
#include "device/capturelog.h"
#include "device/devicetype.h"

/// The guided steps of a capture (CaptureDlg): what to set the meter to, one
/// after the other, for the kind of meter (DeviceType). After dmm-tools'
/// `capture`, much simplified: a step moves on with what the meter shows,
/// not with a keypress, but QtDMM only proposes what to keep - what the
/// meter showed is typed in (or confirmed) by the user.
namespace CaptureSteps
{
/// What a step wants the meter to show; only a hint, since reading it wrong
/// may be the very fault the capture is for.
struct Expect
{
  Quantity quantity = Quantity::Unknown;   ///< Unknown: any
  quint32  defining = 0;     ///< SampleFlag AC, DC, Diode that must be set
  quint32  anyFlag = 0;      ///< one of these SampleFlags must be set (Hold, Relative, Max|Min)
  bool     overload = false; ///< the meter shows OL
  bool     negative = false;
  bool     manualRange = false;

  bool isSet() const;
};

struct Step
{
  QString id;            ///< "dc_v_neg", in the export
  const char *instruction = nullptr;   ///< untranslated, context "CaptureSteps"
  QString key;           ///< a key of DmmDecoder::keyNames() the step presses, or empty
  const char *keyLabel = nullptr;      ///< "HOLD", untranslated
  Expect  expect;
};

/// The steps for meters of @p type; empty for a kind with none (the capture
/// is then kept by hand, as without steps).
QList<Step> forType(DeviceType::Type type);
/// The step's instruction, translated.
QString instruction(const Step &step);

/// The state of a frame without its digits: what the meter is set to.
/// Two frames of the same function, flags and range compare equal however
/// the value wanders.
struct State
{
  bool     understood = false;
  int      length = 0;        ///< bytes, for a frame not understood
  Quantity quantity = Quantity::Unknown;
  quint32  defining = 0;
  quint32  flags = 0;         ///< the meter's own annunciators (Hold, Relative, Max, Min, Autorange, ...)
  QString  unit;
  QString  prefix;
  int      decimals = 0;      ///< digits after the point: the range
  bool     negative = false;
  Quality  quality = Quality::Valid;
  int      values = 0;        ///< values with a number in the frame (a second probe)

  bool operator==(const State &o) const;
  bool operator!=(const State &o) const { return !(*this == o); }
};
/// The state of @p frame, read as @p format reads it.
State stateOf(const CaptureLog::Frame &frame, FrameFormat::DataFormat format);
/// True when @p state is what @p expect wants.
bool matches(const Expect &expect, const State &state);
/// What @p expect wants in words ("Voltage DC, negative"), translated.
QString describe(const Expect &expect);
}

/// Watches the states of the frames for one that is new and stays: the
/// moment to propose keeping a frame. New means other than the baseline,
/// the state the step started from (the one kept or skipped before).
class CaptureWatcher
{
public:
  /// How long a state must hold to count, in ms of the frames' wall time.
  static constexpr qint64 kStable = 1000;

  /// The state the step started from; none: any steady state is new.
  void        setBaseline(std::optional<CaptureSteps::State> state);
  /// Takes the next frame's state; true once when a new state has held for
  /// kStable.
  bool        add(const CaptureSteps::State &state, qint64 wall);
  /// The state that has held for kStable, if any.
  const std::optional<CaptureSteps::State> &steady() const { return m_steady; }
  /// The newest state.
  const std::optional<CaptureSteps::State> &current() const { return m_current; }

private:
  std::optional<CaptureSteps::State> m_baseline;
  std::optional<CaptureSteps::State> m_current;
  std::optional<CaptureSteps::State> m_steady;
  qint64      m_since = 0;
  bool        m_proposed = false;
};
