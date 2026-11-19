// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QObject>
#include <QTimer>
#include <memory>
#include <optional>

#include "core/reading.h"
#include "logic/logicnode.h"

class DeviceSession;
class MeterController;

/// Runs a LogicNode for its MeterController (Kind::Node) in a DeviceSession:
/// takes the readings of its inputs from the session's sources - devices
/// and other nodes, open now or later -, computes when the leading input
/// brings a new one (or on the node's clock), and delivers the outputs as
/// the node's readings, at the leading input's time.
///
/// It runs while its controller is wanted (connectMeter(true)). An input
/// that is not open, has no reading yet or one older than the node's
/// maximum age is none: the node decides what it gives then (a formula a
/// gap), and the controller's state says which input it waits for.
/// A child of the controller.
class NodeRunner : public QObject
{
  Q_OBJECT
public:
  NodeRunner(DeviceSession *session, MeterController *ctl, std::unique_ptr<LogicNode> node);
  ~NodeRunner() override;

  LogicNode   *node() const { return m_node.get(); }
  /// The runner of a node's controller, null for a device.
  static NodeRunner *of(const MeterController *ctl);
  /// Switches the node on or off (its LED): off gives its bypass().
  void        setOn(bool on);
  /// Clears its state (LogicNode::reset()).
  void        reset();

private:
  /// The inputs' sources again: after one opened or closed, or a start.
  void        bind();
  void        onReading(int input, const Reading &r);
  /// Computes with the inputs as they are at @p t (core clock) / @p wall.
  void        compute(qint64 t, qint64 wall);
  void        deliver(const LogicNode::Values &values, qint64 t, qint64 wall);
  /// Starts or stops with the controller's wish.
  void        follow();

  DeviceSession   *m_session;
  MeterController *m_ctl;
  std::unique_ptr<LogicNode> m_node;
  QList<std::optional<Reading>> m_last;   ///< per input
  QList<QMetaObject::Connection> m_links;
  QTimer           m_clock;
  bool             m_running = false;
};
