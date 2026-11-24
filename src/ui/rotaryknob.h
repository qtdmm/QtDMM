// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QWidget>

class QDoubleSpinBox;
class SiSpinBox;

/// A knob to set a value, as on a power supply's front: the mouse wheel
/// and dragging up or down turn it in coarse steps, with Shift in fine
/// ones; the arrow keys too (Page Up/Down ten steps). A double click opens
/// a small spin box on the knob: Enter takes the value, Esc (or a click
/// elsewhere) leaves it as it was.
///
/// The ring around it shows where the value is in its range, in the
/// colour given (the channel's). turned() comes for what the user does
/// only, setValue() is silent: the setpoint read back from the device.
///
/// In decades (setDecades(), a generator's frequency) a step goes along
/// 1-2-5 (1 kHz, 2 kHz, 5 kHz, 10 kHz), a fine step by 1 in the third
/// digit (1.00 kHz to 1.01 kHz), the ring is logarithmic and the spin box
/// reads SI prefixes ("1k", "2.5 MHz", "100m"); values are rounded to
/// seven digits instead of decimals.
class RotaryKnob : public QWidget
{
  Q_OBJECT
public:
  explicit RotaryKnob(QWidget *parent = nullptr);

  void   setRange(double min, double max);
  double minimum() const { return m_min; }
  double maximum() const { return m_max; }
  /// Per wheel step, per kDragPixels dragged; @p fine with Shift.
  void   setSteps(double coarse, double fine);
  double coarseStep() const { return m_coarse; }
  double fineStep() const { return m_fine; }
  /// The value's resolution (digits after the point), also the spin box's.
  void   setDecimals(int decimals);
  /// Steps in 1-2-5 decades, see the class.
  void   setDecades(bool decades);
  bool   decades() const { return m_decades; }
  /// The unit the spin box shows and reads ("V", "Hz").
  void   setUnit(const QString &unit);
  /// Without turned().
  void   setValue(double value);
  double value() const { return m_value; }
  void   setColor(const QColor &color);
  /// The spin box is open.
  bool   isEditing() const;
  /// Opens the spin box (as a double click).
  void   edit();

  /// Pixels dragged per step.
  static constexpr int kDragPixels = 6;

  QSize  sizeHint() const override;
  QSize  minimumSizeHint() const override;

Q_SIGNALS:
  /// The user turned it to @p value (or typed it).
  void   turned(double value);

protected:
  void   paintEvent(QPaintEvent *) override;
  void   wheelEvent(QWheelEvent *event) override;
  void   mousePressEvent(QMouseEvent *event) override;
  void   mouseMoveEvent(QMouseEvent *event) override;
  void   mouseReleaseEvent(QMouseEvent *event) override;
  void   mouseDoubleClickEvent(QMouseEvent *event) override;
  void   keyPressEvent(QKeyEvent *event) override;
  bool   eventFilter(QObject *watched, QEvent *event) override;

private:
  /// Turns by @p steps coarse or fine steps; turned() when the value moved.
  void   step(double steps, bool fine);
  /// The value @p steps 1-2-5 steps (or fine steps) from @p value.
  static double decadeStep(double value, int steps, bool fine);
  /// Where @p value is in the range, 0..1 (logarithmic in decades).
  double share(double value) const;
  /// Sets @p value clamped and rounded; turned() when it moved.
  void   turnTo(double value);
  void   closeEditor(bool take);

  double m_min = 0, m_max = 30;
  double m_coarse = 0.1, m_fine = 0.01;
  int    m_decimals = 2;
  double m_value = 0;
  QColor m_color = QColor(0xe0, 0xa0, 0x00);
  int    m_dragY = 0;
  bool   m_dragging = false;
  int    m_wheelRest = 0;   ///< angle not turned yet (high-resolution wheels)
  bool   m_decades = false;
  QString m_unit;
  SiSpinBox *m_editor = nullptr;
};
