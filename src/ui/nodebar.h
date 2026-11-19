// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QPointer>
#include <QWidget>

class MeterController;
class QHBoxLayout;
class QLabel;
class QToolButton;

/// A logic node's controls under its display, where a meter has its keys
/// (ControlBar): the status LED on the left - green while it computes,
/// yellow while it waits for an input or holds, dark while disconnected,
/// why in its tooltip -, then the node's own keys, switches and LEDs
/// (LogicNode::controls()): an integral's 0 and HOLD. A click goes to the
/// node (NodeRunner::press()).
class NodeBar : public QWidget
{
  Q_OBJECT
public:
  explicit NodeBar(QWidget *parent = nullptr);

  /// The node shown (its controls anew); nullptr or a device: none.
  void        setController(MeterController *ctl);
  /// The LEDs and switches anew from the node.
  void        refresh();
  /// The status LED's colour now (for the tests): green, yellow or dark.
  QColor      statusColor() const { return m_color; }

  /// An LED of @p color as a pixmap of @p size pixels (logical).
  static QPixmap led(const QColor &color, int size);

private:
  QPointer<MeterController> m_ctl;
  QHBoxLayout *m_row;
  QLabel      *m_status;
  QHash<QString, QWidget *> m_controls;   ///< by the node's id
  QList<QMetaObject::Connection> m_links;
  QColor       m_color;
};
