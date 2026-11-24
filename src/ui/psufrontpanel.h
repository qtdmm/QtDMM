// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QPointer>
#include <QWidget>

class FoldButton;
class PanelGear;
class PowerSupply;
class PsuLcdWidget;
class QGridLayout;
class QLabel;
class QTimer;
class QToolButton;
class RotaryKnob;

/// A power supply's front (PowerSupply): its LCD (PsuLcdWidget) with a
/// block per channel shown, below it per channel a knob for the voltage and
/// the OUT key with its LED, both behind the channel's coloured name. One
/// or two channels sit in a row under their blocks, three or four as 2×2,
/// as on the LCD; with more than one, ALL OUT below them at the right. The gear
/// in the LCD's name plate and the handle that folds the controls away, as
/// on the meters' fronts (FrontPanel).
///
/// Turning shows the setpoint on the LCD at once and sends it at most
/// every kSendInterval ms (the last one always); until the device read it
/// back the knob shows what was sent, a request the device did not take
/// lights ERR on the block and says why in its tooltip. The knob follows
/// the setpoint read back when nobody turns it. A channel whose setpoint
/// comes from a logic node (EXT) has its knob locked. A channel not shown
/// in the settings is not on the front at all.
class PsuFrontPanel : public QWidget
{
  Q_OBJECT
public:
  static constexpr int kSendInterval = 100;   ///< ms

  explicit PsuFrontPanel(PowerSupply *supply, QWidget *parent = nullptr);

  PsuLcdWidget *display() const { return m_display; }
  PowerSupply  *supply() const { return m_supply; }
  /// The device's name on the LCD's bezel.
  void          setCaption(const QString &caption);
  /// The knob, the OUT key of device channel @p ch; nullptr when not shown.
  RotaryKnob   *knob(int ch) const;
  QToolButton  *outputKey(int ch) const;
  /// ALL OUT; nullptr with one channel shown.
  QToolButton  *allOffKey() const { return m_allOff; }
  /// The controls folded away (the handle).
  void          setFolded(bool folded);
  bool          isFolded() const;
  /// Pixels the controls and the handle take below the LCD.
  int           controlsHeight() const;

Q_SIGNALS:
  /// The gear: the supply's settings.
  void          settingsRequested();
  void          foldToggled(bool folded);

protected:
  bool          eventFilter(QObject *watched, QEvent *event) override;

private:
  /// The LCD's channels and the controls anew (channels shown, colours, limits).
  void          rebuild();
  /// Channel @p ch's state on the LCD, knob and key.
  void          showState(int ch);
  /// The knob turned: on the LCD now, to the device soon.
  void          turned(int ch, double value);
  void          sendPending();
  void          placeCorner();
  /// Asks before an output goes on, where the supply wants it (real devices).
  bool          mayTurnOn();

  struct Strip
  {
    int          ch = -1;   ///< the device's channel
    QWidget     *box = nullptr;
    RotaryKnob  *knob = nullptr;
    QToolButton *out = nullptr;
    bool         turning = false;   ///< turned since the last answer: the knob is the user's
    double       pending = 0;       ///< to send
    bool         hasPending = false;
  };

  QPointer<PowerSupply> m_supply;
  PsuLcdWidget *m_display;
  QWidget      *m_controls;
  QGridLayout  *m_grid;
  QToolButton  *m_allOff = nullptr;
  FoldButton   *m_fold;
  PanelGear    *m_gear;
  QTimer       *m_send;
  QList<Strip>  m_strips;
};
