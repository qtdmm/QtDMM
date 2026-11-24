// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QPointer>

#include <array>

#include "core/functiongenerator.h"
#include "ui/outputfront.h"

class GenLcdWidget;
class QHBoxLayout;
class QLabel;
class QTimer;
class QToolButton;
class RotaryKnob;

/// A function generator's front (FunctionGenerator): its LCD
/// (GenLcdWidget) with a block per channel shown, below it per channel
/// - behind the channel's coloured name - a key for each basic shape
///   (sine, square, ramp, pulse; the device's other shapes in the menu of
///   the key after them, when it has any) and the OUT key with its LED,
/// - the knobs: frequency (1-2-5 steps, fine with Shift, a double click
///   reads "1kHz"), amplitude (Vpp), offset (V), and the duty cycle of a
///   pulse or the symmetry of a ramp; a knob a shape has not is hidden.
/// With more than one channel, ALL OUT below them at the right. Gear and
/// handle come from OutputFront.
///
/// As on the power supply's front: turning shows the value on the LCD at
/// once (SET blinks) and sends it at most every kSendInterval ms (the
/// last one always); the knob follows the value read back when nobody
/// turns it; a request the device did not take lights ERR and says why
/// in the LCD's tooltip; a channel set from outside (EXT) has its keys
/// and knobs locked. The knobs' ranges follow the generator's limits
/// (the offset leaves room for the amplitude and the other way round).
class GenFrontPanel : public OutputFront
{
  Q_OBJECT
public:
  using Shape = FunctionGenerator::Shape;
  using Value = FunctionGenerator::Value;
  static constexpr int kSendInterval = 100;   ///< ms

  explicit GenFrontPanel(FunctionGenerator *generator, QWidget *parent = nullptr);

  GenLcdWidget *display() const { return m_display; }
  FunctionGenerator *generator() const { return m_generator; }
  /// The knob of @p v, the key of @p shape (Arbitrary: the menu's), the
  /// OUT key of device channel @p ch; nullptr when not shown.
  RotaryKnob   *knob(int ch, Value v) const;
  QToolButton  *shapeKey(int ch, Shape shape) const;
  QToolButton  *outputKey(int ch) const;
  /// ALL OUT; nullptr with one channel shown.
  QToolButton  *allOffKey() const { return m_allOff; }

private:
  struct Strip
  {
    int          ch = -1;   ///< the device's channel
    QWidget     *box = nullptr;
    std::array<QToolButton *, FunctionGenerator::kBasicShapes + 1> shape {};   ///< + the menu's key
    std::array<RotaryKnob *, FunctionGenerator::kValues> knob {};
    std::array<QWidget *, FunctionGenerator::kValues> knobBox {};   ///< the knob with its caption
    QToolButton *out = nullptr;
    std::array<bool, FunctionGenerator::kValues> turning {};    ///< the knob is the user's until the answer
    std::array<double, FunctionGenerator::kValues> pending {};
    std::array<bool, FunctionGenerator::kValues> hasPending {};
  };

  void          rebuild();
  /// The controls of @p s, in column @p column under its block.
  void          buildStrip(Strip &s, int column);
  void          showState(int ch);
  Strip        *strip(int ch);
  const Strip  *strip(int ch) const;
  /// The knob of @p v turned: on the LCD now, to the device soon.
  void          turned(int ch, Value v, double value);
  void          sendPending();

  QPointer<FunctionGenerator> m_generator;
  GenLcdWidget *m_display;
  QHBoxLayout  *m_row;
  QToolButton  *m_allOff = nullptr;
  QTimer       *m_send;
  QList<Strip>  m_strips;
};
