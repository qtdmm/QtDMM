// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QStringList>
#include <QWidget>

class DeviceSession;
class QHBoxLayout;
class QLabel;
class QTimer;
class QToolButton;

/// The small window of the logic nodes on the bench (concept §6): a front
/// with a green LED for each node open - a click switches it on or off,
/// its name and why it waits in the tooltip -, how many are on, a warning
/// count when some wait for an input (a click: the rack at the first of
/// them) and a button for the rack. The rack shows them in full.
class NodeStatus : public QWidget
{
  Q_OBJECT
public:
  /// The width/height range its content fills (MdiArranger::setContentAspect()).
  static constexpr double kMinAspect = 2.0;
  static constexpr double kMaxAspect = 5.0;

  explicit NodeStatus(DeviceSession *session, QWidget *parent = nullptr);

  /// The LEDs in the rack's order (entry ids); nodes not in it come after.
  void        setOrder(const QStringList &ids);
  /// The nodes open, in the order.
  QStringList nodes() const { return m_ids; }
  /// The nodes waiting for an input or with something that does not fit.
  QStringList waiting() const;
  /// The LEDs and counts anew from the nodes.
  void        refresh();

Q_SIGNALS:
  /// The rack, with the node of @p id in view (empty: just the rack).
  void        rackRequested(const QString &id);
  /// An LED was clicked: the node of @p id is on or off now.
  void        switched(const QString &id, bool on);

protected:
  void        paintEvent(QPaintEvent *event) override;
  void        showEvent(QShowEvent *event) override;
  void        hideEvent(QHideEvent *event) override;

private:
  /// The LEDs anew for the nodes open now.
  void        rebuild();

  DeviceSession *m_session;
  QHBoxLayout   *m_leds;
  QLabel        *m_count;
  QToolButton   *m_warning;
  QStringList    m_order;
  QStringList    m_ids;              ///< the nodes with an LED, in its order
  QHash<QString, QToolButton *> m_ledOf;
  QTimer        *m_timer;
};
