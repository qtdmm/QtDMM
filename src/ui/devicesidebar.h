// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>

#include <QTimer>
#include <QTreeWidget>
#include <QVariant>

#include "core/valueref.h"

class DeviceLibrary;

/// The sidebar "Devices": the node "My devices" with the entries of the
/// DeviceLibrary in their order: the cupboard. A click does nothing; one
/// connected is bold with a dot, green while its readings come in. The
/// context menu has Info and settings (also a double click), Rename (in
/// place), Connect and Remove; dragging an entry changes the order, dragged
/// onto "Open devices" it connects.
///
/// The node "Open devices" (shown as connected devices) lists the meters of
/// the process (MainWindow gives them with setOpenDevices()) with their
/// reading; the active one (the meter of the front panel used last) in bold,
/// one not connected greyed. Their context menu shows the front panel (also
/// a double click), connects or disconnects, and closes; dragged back onto
/// My devices one closes. For a meter with more than one value, a row for
/// each value with its reading; its context menu says which window shows it
/// where.
///
/// The node "Services" shows what this window offers to other programs
/// (setServices()): the SCPI server and HiSLIP, with their port or "off";
/// a double click or the context menu opens their settings, the menu also
/// copies the address.
///
/// The node "Workspaces" on top lists the workspace files used last
/// (setWorkspaces()) by their names, folded at the start and hidden while
/// there are none; at its right the name of the workspace in use
/// (setCurrentWorkspace()). A double click or Enter loads one, the context
/// menu also takes it off the list or opens its folder.
class DeviceSidebar : public QTreeWidget
{
  Q_OBJECT
public:
  /// One of the meter's values, a row below its open device (fillValues()).
  struct Value
  {
    int     id = 0;         ///< Reading::id, 0 the main value
    QString name;           ///< "Current DC (A)", "Value 2" before a reading; the row's tooltip
    QString text;           ///< its reading with the unit, "OL"
    QColor  color;          ///< its curve's colour while the graph shows it, else invalid
    QString shortName;      ///< shown in the row ("A DC"); name when empty
  };

  /// One row of the node "Open devices".
  struct OpenDevice
  {
    QString id;             ///< the entry of My devices, or MeterController::source()
    QString name;
    QString value;          ///< its reading, "OL", "" before one
    bool    active = false; ///< readings come in
    QList<Value> values;    ///< with more than one: a row each below the device
    bool    current = false; ///< the active device: the toolbar acts on it
    bool    connected = true; ///< its connection is wanted (MeterConnection::isWanted())
  };

  /// One row of the node "Services".
  /// A workspace used last.
  struct Workspace
  {
    QString file;
    QString name;           ///< empty: the file's name without the suffix
    QString tooltip;        ///< empty: the path
  };
  struct Service
  {
    QString id;             ///< "scpi", "hislip"
    QString name;           ///< "SCPI", "HiSLIP"
    QString value;          ///< at the right: the port, or "off"
    QString address;        ///< copied by the menu ("localhost:5025", a VISA address)
    QString tooltip;
    bool    running = false;
    bool    active = false; ///< a client is connected
    bool    failed = false; ///< switched on but could not start
  };

  explicit DeviceSidebar(DeviceLibrary *library, QWidget *parent = nullptr);

  /// The node "My devices".
  QTreeWidgetItem *devicesNode() const { return m_devices; }
  /// The item of the entry @p id, nullptr when there is none.
  QTreeWidgetItem *deviceItem(const QString &id) const;
  /// The ids of the entries, in the order shown.
  QStringList deviceIds() const;

  /// The connected devices. Rebuilds only when the list changed, else
  /// updates the readings; the dots of My devices follow.
  void        setOpenDevices(const QList<OpenDevice> &devices);
  /// The node "Open devices".
  QTreeWidgetItem *openNode() const { return m_open; }
  /// The item of the open device @p id, nullptr when there is none.
  QTreeWidgetItem *openItem(const QString &id) const;

  /// The rows of the meter's values below @p parent, after its first
  /// @p first children; none for one value.
  /// Their context menu is valueMenuRequested().
  void        fillValues(QTreeWidgetItem *parent, int first, const QList<Value> &values);
  /// The row of value @p id of the open device @p device (OpenDevice::id),
  /// nullptr when there is none.
  QTreeWidgetItem *valueItem(int id, const QString &device) const;
  /// The value a row of the open devices stands for: a value row, or the row
  /// of a device with one value (its main value), by "device:<id>". Invalid
  /// source "-" for none.
  ValueRef    valueOf(const QTreeWidgetItem *item) const;
  /// What a drag out of the sidebar carries for the window: a value of an
  /// open device (ValueRef::toString()) or an entry of My devices (its id).
  static constexpr const char *kValueMime = "application/x-qtdmm-value";
  static constexpr const char *kDeviceMime = "application/x-qtdmm-device";
  /// Fills the submenu "Value" of a device with one value (ValueMenu).
  void        setValueMenuFiller(std::function<void(QMenu *, const ValueRef &)> filler) { m_valueFiller = std::move(filler); }

  /// The services of this window, in this order.
  void        setServices(const QList<Service> &services);
  /// The node "Services".
  QTreeWidgetItem *servicesNode() const { return m_services; }

  /// The workspaces used last, newest first; the node is hidden without
  /// any. A file that is gone is grey and loads no more.
  void        setWorkspaces(const QList<Workspace> &workspaces);
  /// The name of the workspace in use, at the right of the node; empty: none.
  void        setCurrentWorkspace(const QString &name, const QString &tooltip = QString());
  /// The node "Workspaces".
  QTreeWidgetItem *workspacesNode() const { return m_workspaces; }

  /// Shown at the start: as the user left it (@p stored, invalid when never
  /// set), else with two devices or more.
  static bool shownAtStart(const QVariant &stored, int devices);
  /// The entry @p id is connected.
  bool        isConnected(const QString &id) const;
  /// The assistant added an entry: the sidebar opens with the second device.
  static bool opensAfterAdd(int devicesBefore, int devicesAfter);

Q_SIGNALS:
  /// Info and settings of the entry @p id: its double click, its menu.
  void        settingsRequested(const QString &id);
  /// Connect the entry @p id beside the devices connected (its menu,
  /// DeviceSession::open()).
  void        openBesideRequested(const QString &id);
  /// Close the open device @p id (OpenDevice::id).
  void        closeRequested(const QString &id);
  /// Show the front panel of the open device @p id (OpenDevice::id).
  void        frontPanelRequested(const QString &id);
  /// Connect (@p on) or disconnect the open device @p id.
  void        connectRequested(const QString &id, bool on);
  /// The alarms of the open device @p id.
  void        alarmsRequested(const QString &id);
  /// The PASS/FAIL test's limits of the open device @p id.
  void        limitsRequested(const QString &id);
  /// The row "+ Add device..." at the end of My devices.
  void        addDeviceRequested();
  /// The settings of the services (Settings, SCPI server).
  void        serviceSettingsRequested();
  /// The context menu of the row of value @p id (MainWindow fills it).
  void        valueMenuRequested(const ValueRef &value, const QPoint &globalPos);
  /// Load the workspace file @p path.
  void        workspaceRequested(const QString &path);
  /// Take @p path off the list of workspaces used last.
  void        removeWorkspaceRequested(const QString &path);

public Q_SLOTS:
  /// The entries anew from the library.
  void        fill();

protected:
  QMimeData  *mimeData(const QList<QTreeWidgetItem *> &items) const override;
  void        dragMoveEvent(QDragMoveEvent *event) override;
  void        dropEvent(QDropEvent *event) override;
  void        contextMenuEvent(QContextMenuEvent *event) override;
  void        keyPressEvent(QKeyEvent *event) override;
  void        changeEvent(QEvent *event) override;

private:
  void        updateMarks();
  /// The node (My devices, Open devices, ...) @p item is in, or is.
  static QTreeWidgetItem *nodeOf(QTreeWidgetItem *item);
  QString     idOf(const QTreeWidgetItem *item) const;
  /// The last row of My devices, which adds one.
  QTreeWidgetItem *m_addItem = nullptr;
  /// The id of an open device's row, else empty.
  QString     openOf(const QTreeWidgetItem *item) const;
  void        openMenu(QTreeWidgetItem *item, const QPoint &pos);
  void        serviceMenu(QTreeWidgetItem *item, const QPoint &pos);
  void        workspaceMenu(QTreeWidgetItem *item, const QPoint &pos);
  /// The file of a workspace row that is still there, else empty.
  QString     workspaceOf(const QTreeWidgetItem *item) const;

  DeviceLibrary   *m_library = nullptr;
  QTreeWidgetItem *m_workspaces = nullptr;
  QTreeWidgetItem *m_devices = nullptr;
  QTreeWidgetItem *m_open = nullptr;
  QTreeWidgetItem *m_services = nullptr;
  QList<OpenDevice> m_openList;   ///< as last set: coloured anew with another design
  std::function<void(QMenu *, const ValueRef &)> m_valueFiller;   ///< setValueMenuFiller()
  QList<Service>   m_serviceList;
  QList<Workspace> m_workspaceList;
  bool             m_filling = false;
};
