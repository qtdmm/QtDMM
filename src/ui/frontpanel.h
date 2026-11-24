// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>

#include <QPointer>
#include <QWidget>

#include "core/limittest.h"
#include "core/valueref.h"

class ControlBar;
class DeviceSession;
class FoldButton;
class PanelGear;
class NodeBar;
class LcdWidget;
class MeterController;
class PsuFrontPanel;
class ValueFeed;

/// One meter's front: its LCD, the keys below it (ControlBar, for meters
/// QtDMM can press keys of; NodeBar for a logic node: its status LED and
/// its own keys), the fold button and the gear for its settings in the
/// LCD's name plate (the gear) and under the keys (the handle).
///
/// The panel shows the meter of value() (ValueFeed): its readings, min/max
/// and staleness on the LCD, its key states on the keys; a click on a key
/// goes to that meter. Each meter open in the process has one (MainWindow).
/// The LCD takes the colour of its meter's raised alarm, under it
/// the PASS/FAIL test's green or red, and the verdict on the value it shows.
///
/// A logic node that is a power supply (LogicNode::powerSupply(), the
/// simulated one) shows its own front instead of LCD and keys
/// (PsuFrontPanel): its value k is block k, the verdict on it in that
/// block; colours, alarm colour, staleness, gear and fold as here.
class FrontPanel : public QWidget
{
  Q_OBJECT
public:
  explicit FrontPanel(DeviceSession *session, QWidget *parent = nullptr);

  LcdWidget  *display() const { return m_display; }
  /// The value on the LCD's big line, of the meter that has it; another
  /// meter (or none) clears the LCD.
  void        setValue(const ValueRef &value);
  ValueRef    value() const;
  /// The LCD's small line (LcdWidget::setSecondaries()); std::nullopt: the
  /// first values after the big one.
  void        setSecondaries(const std::optional<QList<int>> &ids);
  QList<int>  secondaries() const;
  /// Whether the small line was chosen (else it is not saved).
  bool        secondariesChosen() const { return m_secondariesChosen; }
  /// The meter shown, nullptr while it is not open.
  MeterController *controller() const;
  /// The keys folded away (the fold button, "Hide controls").
  void        setFolded(bool folded);
  bool        isFolded() const;
  /// The meter has keys QtDMM can press (ControlBar::supported()).
  bool        keysSupported() const { return m_keysSupported; }
  /// The keys are shown: the meter has some and they are not folded.
  bool        keysShown() const;
  /// Pixels the keys take below the LCD, 0 while they are not shown.
  int         keysHeight() const;
  /// Reads the meter's keys anew (after its settings were applied).
  void        updateKeys();
  /// An alarm's colour blinks (Alarms/blink).
  void        setBlink(bool blink);
  /// The power supply's front while the meter shown is one, else nullptr.
  PsuFrontPanel *supplyPanel() const { return m_psu; }
  /// The LCD's colours (display()'s) onto the supply's LCD too.
  void        syncLook();
  /// The width/height range of the LCD shown (the supply's has its own).
  double      minAspect() const;
  double      maxAspect() const;

Q_SIGNALS:
  /// Folded or unfolded by the user (the fold button).
  void        foldToggled(bool folded);
  /// The keys appeared or went: the panel's height for a width changed.
  void        keysChanged();
  /// A key did not reach the meter (not connected yet).
  void        keyFailed();
  /// The gear: the settings of the meter shown (its id in My devices).
  void        settingsRequested(const QString &id);
  /// The LCD's context menu, at @p global (the supply's LCD's as well).
  void        menuRequested(const QPoint &global);

protected:
  /// Keeps the fold button in the LCD's top right corner.
  bool        eventFilter(QObject *watched, QEvent *event) override;

private:
  /// The meter now shown: its keys, the LCD afresh, the connection.
  void        attach(MeterController *ctl);
  /// The keys enabled while the meter's connection is wanted.
  void        updateConnected();
  /// The verdict of the limit test on value @p id: on the LCD for the value
  /// it shows big, its colour under an alarm's.
  void        showVerdict(int id, LimitTest::Verdict verdict, double deviation);
  /// The limit test's count in the display's tooltip, while the test is on.
  void        showLimitCount(const LimitTest::Count &count);
  /// The alarm's colour (blinking if wanted) or the limit test's on the LCD.
  void        showAlert();
  /// The supply's front for the meter now, or none: what is shown.
  void        followSupply();

  ValueFeed  *m_feed;
  LcdWidget  *m_display;
  ControlBar *m_controls;
  FoldButton *m_fold;
  NodeBar    *m_nodeBar;
  PanelGear  *m_gear;
  PsuFrontPanel *m_psu = nullptr;
  /// The bar under the LCD now: the meter's keys or the node's.
  QWidget    *bar() const;
  /// The gear at the right end of the LCD's name plate.
  void        placeCorner();
  bool        m_keysSupported = false;
  bool        m_secondariesChosen = false;
  QPointer<MeterController> m_ctl;
  QMetaObject::Connection m_link;   ///< the meter's connection state, for the keys
  QList<QMetaObject::Connection> m_rules;   ///< the meter's alarm colour and limit test
  QColor      m_alertColor;   ///< the raised alarm's colour, invalid when none
  QColor      m_limitColor;   ///< the limit test's green or red, invalid when none
  bool        m_blink = false;
  bool        m_phase = true;   ///< blinking: the colour shown now
  class QTimer *m_blinkTimer;
};
