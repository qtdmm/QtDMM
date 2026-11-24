// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/rotaryknob.h"

#include <QDoubleSpinBox>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QWheelEvent>

#include <cmath>
#include <numbers>

namespace
{
// the ring's arc: from lower left over the top to lower right
constexpr double kStartDeg = 225;
constexpr double kSpanDeg = 270;
}

RotaryKnob::RotaryKnob(QWidget *parent) : QWidget(parent)
{
  setFocusPolicy(Qt::WheelFocus);
  setCursor(Qt::SizeVerCursor);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void RotaryKnob::setRange(double min, double max)
{
  m_min = qMin(min, max);
  m_max = qMax(min, max);
  m_value = qBound(m_min, m_value, m_max);
  update();
}

void RotaryKnob::setSteps(double coarse, double fine)
{
  m_coarse = coarse > 0 ? coarse : m_coarse;
  m_fine = fine > 0 ? fine : m_fine;
}

void RotaryKnob::setDecimals(int decimals)
{
  m_decimals = qBound(0, decimals, 6);
}

void RotaryKnob::setValue(double value)
{
  if (std::isnan(value))
    return;
  m_value = qBound(m_min, value, m_max);
  update();
}

void RotaryKnob::setColor(const QColor &color)
{
  m_color = color;
  update();
}

QSize RotaryKnob::sizeHint() const
{
  return QSize(52, 52);
}

QSize RotaryKnob::minimumSizeHint() const
{
  return QSize(36, 36);
}

// ---------------------------------------------------------------- turning

void RotaryKnob::step(double steps, bool fine)
{
  turnTo(m_value + steps * (fine ? m_fine : m_coarse));
}

void RotaryKnob::turnTo(double value)
{
  const double unit = std::pow(10.0, -m_decimals);
  const double v = qBound(m_min, std::round(value / unit) * unit, m_max);
  if (std::abs(v - m_value) < unit / 2)
    return;
  m_value = v;
  update();
  Q_EMIT turned(v);
}

void RotaryKnob::wheelEvent(QWheelEvent *event)
{
  // Shift turns some systems' wheel sideways
  const QPoint a = event->angleDelta();
  m_wheelRest += a.y() != 0 ? a.y() : a.x();
  const int steps = m_wheelRest / 120;
  m_wheelRest -= steps * 120;
  if (steps != 0)
    step(steps, event->modifiers() & Qt::ShiftModifier);
  event->accept();
}

void RotaryKnob::mousePressEvent(QMouseEvent *event)
{
  if (event->button() != Qt::LeftButton)
    return QWidget::mousePressEvent(event);
  m_dragging = true;
  m_dragY = event->position().toPoint().y();
}

void RotaryKnob::mouseMoveEvent(QMouseEvent *event)
{
  if (!m_dragging)
    return;
  // up turns up
  const int y = event->position().toPoint().y();
  const int steps = (m_dragY - y) / kDragPixels;
  if (steps == 0)
    return;
  m_dragY -= steps * kDragPixels;
  step(steps, event->modifiers() & Qt::ShiftModifier);
}

void RotaryKnob::mouseReleaseEvent(QMouseEvent *event)
{
  Q_UNUSED(event);
  m_dragging = false;
}

void RotaryKnob::mouseDoubleClickEvent(QMouseEvent *event)
{
  Q_UNUSED(event);
  m_dragging = false;
  edit();
}

void RotaryKnob::keyPressEvent(QKeyEvent *event)
{
  const bool fine = event->modifiers() & Qt::ShiftModifier;
  switch (event->key())
  {
    case Qt::Key_Up:
    case Qt::Key_Right:    step(1, fine); break;
    case Qt::Key_Down:
    case Qt::Key_Left:     step(-1, fine); break;
    case Qt::Key_PageUp:   step(10, fine); break;
    case Qt::Key_PageDown: step(-10, fine); break;
    case Qt::Key_Return:
    case Qt::Key_Enter:    edit(); break;
    default:               QWidget::keyPressEvent(event); return;
  }
  event->accept();
}

// ---------------------------------------------------------------- the spin box

bool RotaryKnob::isEditing() const
{
  return m_editor && m_editor->isVisible();
}

void RotaryKnob::edit()
{
  if (!isEnabled())
    return;
  if (!m_editor)
  {
    m_editor = new QDoubleSpinBox(this);
    m_editor->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_editor->setAlignment(Qt::AlignCenter);
    m_editor->installEventFilter(this);
  }
  m_editor->setDecimals(m_decimals);
  m_editor->setRange(m_min, m_max);
  m_editor->setSingleStep(m_coarse);
  m_editor->setValue(m_value);
  // across the knob, as wide as the number needs
  const int w = qMax(width(), m_editor->sizeHint().width());
  const int h = m_editor->sizeHint().height();
  m_editor->setGeometry((width() - w) / 2, (height() - h) / 2, w, h);
  m_editor->show();
  m_editor->raise();
  m_editor->setFocus();
  m_editor->selectAll();
}

void RotaryKnob::closeEditor(bool take)
{
  if (!isEditing())
    return;
  m_editor->hide();
  if (take)
  {
    m_editor->interpretText();
    turnTo(m_editor->value());
  }
  setFocus();
}

bool RotaryKnob::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == m_editor)
  {
    if (event->type() == QEvent::KeyPress)
    {
      const int key = static_cast<QKeyEvent *>(event)->key();
      if (key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Escape)
      {
        closeEditor(key != Qt::Key_Escape);
        return true;
      }
    }
    else if (event->type() == QEvent::FocusOut)
      closeEditor(false);
  }
  return QWidget::eventFilter(watched, event);
}

// ---------------------------------------------------------------- painting

void RotaryKnob::paintEvent(QPaintEvent *)
{
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  const double side = qMin(width(), height());
  const QPointF c(width() / 2.0, height() / 2.0);
  const double ring = side * 0.08;
  const double r = side / 2 - ring * 1.3;
  if (!isEnabled())
    p.setOpacity(0.45);

  // the ring: the range as a groove, the value in the channel's colour
  const QRectF arc(c.x() - r - ring * 0.7, c.y() - r - ring * 0.7, 2 * (r + ring * 0.7), 2 * (r + ring * 0.7));
  const double share = m_max > m_min ? (m_value - m_min) / (m_max - m_min) : 0;
  p.setPen(QPen(palette().color(QPalette::Mid), ring, Qt::SolidLine, Qt::RoundCap));
  p.drawArc(arc, int(kStartDeg * 16), int(-kSpanDeg * 16));
  p.setPen(QPen(m_color, ring, Qt::SolidLine, Qt::RoundCap));
  p.drawArc(arc, int(kStartDeg * 16), int(-kSpanDeg * share * 16));

  // the knob: dark metal, lit from the upper left
  QRadialGradient g(c - QPointF(r * 0.35, r * 0.35), r * 1.6);
  g.setColorAt(0, QColor(0x70, 0x72, 0x76));
  g.setColorAt(1, QColor(0x1c, 0x1d, 0x20));
  p.setPen(QPen(QColor(0x10, 0x10, 0x12), qMax(1.0, side * 0.02)));
  p.setBrush(g);
  p.drawEllipse(c, r, r);
  // the mark where it points
  const double a = (kStartDeg - kSpanDeg * share) * std::numbers::pi / 180;
  const QPointF dir(std::cos(a), -std::sin(a));
  p.setPen(QPen(QColor(0xf0, 0xf0, 0xf0), qMax(1.5, side * 0.05), Qt::SolidLine, Qt::RoundCap));
  p.drawLine(c + dir * r * 0.35, c + dir * r * 0.82);
  if (hasFocus())
  {
    p.setPen(QPen(palette().color(QPalette::Highlight), 1, Qt::DotLine));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(c, r + ring * 1.25, r + ring * 1.25);
  }
}
