// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QElapsedTimer>
#include <QFrame>
#include <QPointer>
#include <QStringList>
#include <QWidget>

class DeviceSession;
class MeterController;
class QLabel;
class QToolButton;
class QVBoxLayout;

/// One logic node in the rack (an insert): a front with its green LED -
/// a click switches it on or off -, its name, kind and inputs, its values
/// small, a warning sign when it waits for an input or something does not
/// fit (why in the tooltip), and a gear for its dialog. An integral has a
/// button to set it to 0. Dragged by its front to another place.
class RackInsert : public QFrame
{
  Q_OBJECT
public:
  RackInsert(DeviceSession *session, MeterController *ctl, QWidget *parent = nullptr);

  QString     id() const;
  MeterController *controller() const { return m_ctl; }
  /// Its labels anew from the node and its last readings.
  void        refresh();

Q_SIGNALS:
  void        settingsRequested(const QString &id);
  void        removeRequested(const QString &id);
  /// The LED was clicked: on or off now.
  void        switched(const QString &id, bool on);

protected:
  void        paintEvent(QPaintEvent *event) override;
  void        mousePressEvent(QMouseEvent *event) override;
  void        mouseMoveEvent(QMouseEvent *event) override;
  void        contextMenuEvent(QContextMenuEvent *event) override;

private:
  /// The inputs as they read: "v1 Supply U · v2 Load I", "Load: value 2".
  QString     inputsText() const;

  DeviceSession   *m_session;
  QPointer<MeterController> m_ctl;
  QToolButton     *m_led;
  QLabel          *m_name;
  QLabel          *m_kind;
  QLabel          *m_values;
  QLabel          *m_warning;
  QToolButton     *m_zero = nullptr;
  QToolButton     *m_gear;
  QPoint           m_pressPos;
};

/// The rack (concept §6): a window of its own beside the main window, not
/// modal, with the logic nodes open on the bench one below the other as
/// inserts (RackInsert), in an order the user drags them into - the order
/// they are read in, not computed in. "+ Insert" adds a calculated,
/// simulated or integral value. Its order, place and whether it is open
/// are the bench's (MainWindow::storeWorkspace()).
class RackWindow : public QWidget
{
  Q_OBJECT
public:
  RackWindow(DeviceSession *session, QWidget *parent = nullptr);

  /// The inserts' order, by entry id; nodes not in it come after.
  QStringList order() const;
  void        setOrder(const QStringList &ids);
  /// Moves the insert of @p id to place @p index (what a drop does).
  void        moveInsert(const QString &id, int index);
  /// Shows the rack with the insert of @p id in view, lit up a moment.
  void        showInsert(const QString &id);
  QList<RackInsert *> inserts() const;
  /// Its background in the bench's design: the brushed metal the front
  /// panels stand on (Silver its gradient, others the area's colour).
  void        applyDesign();
  /// Open as the user left it: shown, and not closed by them - hidden
  /// with the main window when QtDMM ends it stays open (the bench's).
  bool        isOpen() const { return m_open; }

Q_SIGNALS:
  void        settingsRequested(const QString &id);
  void        removeRequested(const QString &id);
  /// "+ Insert": a new node of @p type ("formula", "generator", "integral").
  void        addRequested(const QString &type);
  void        orderChanged();

protected:
  void        dragEnterEvent(QDragEnterEvent *event) override;
  void        dragMoveEvent(QDragMoveEvent *event) override;
  void        dropEvent(QDropEvent *event) override;
  void        showEvent(QShowEvent *event) override;
  void        hideEvent(QHideEvent *event) override;
  void        closeEvent(QCloseEvent *event) override;

private:
  /// The inserts anew for the nodes open now, in the order.
  void        rebuild();
  /// The place a drop at @p y (in the list) goes to.
  int         dropIndex(int y) const;
  void        refresh();
  /// Writes a node's on/off into its entry of My devices.
  void        storeOn(const QString &id, bool on);

  DeviceSession *m_session;
  QWidget       *m_list;
  QVBoxLayout   *m_listLayout;
  QLabel        *m_empty;
  QStringList    m_order;
  QList<RackInsert *> m_inserts;
  QElapsedTimer  m_lastRefresh;
  bool           m_open = false;
};
