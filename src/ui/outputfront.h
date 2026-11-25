// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

class FoldButton;
class LcdPanel;
class PanelGear;

/// What the fronts of devices with outputs share (PsuFrontPanel,
/// GenFrontPanel): their LCD on top with the gear in its name plate, the
/// controls below it, the handle that folds the controls away (as on the
/// meters' fronts, FrontPanel), and the LED of an OUT key. A subclass
/// creates its LCD, hands it to setDisplay() and fills controls().
class OutputFront : public QWidget
{
  Q_OBJECT
public:
  /// The device's name on the LCD's bezel.
  void    setCaption(const QString &caption);
  /// The controls folded away (the handle).
  void    setFolded(bool folded);
  bool    isFolded() const;
  /// Pixels the controls and the handle take below the LCD.
  int     controlsHeight() const;
  /// An OUT key's LED: green while the output is on.
  static QIcon led(bool on);
  /// The LCD on top (the subclass's, as an LcdPanel).
  LcdPanel *lcd() const { return m_lcd; }

Q_SIGNALS:
  /// The gear: the device's settings.
  void    settingsRequested();
  void    foldToggled(bool folded);

protected:
  explicit OutputFront(QWidget *parent = nullptr);
  /// The LCD on top (a child of this front); once, in the constructor.
  void    setDisplay(LcdPanel *display);
  /// The widget the controls go into, below the LCD.
  QWidget *controls() const { return m_controls; }
  bool    eventFilter(QObject *watched, QEvent *event) override;
  void    resizeEvent(QResizeEvent *event) override;

private:
  void    placeCorner();

  LcdPanel   *m_lcd = nullptr;
  QWidget    *m_controls;
  FoldButton *m_fold;
  PanelGear  *m_gear = nullptr;
};
