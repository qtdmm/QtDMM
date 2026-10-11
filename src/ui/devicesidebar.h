// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QTreeWidget>
#include <QVariant>

class DeviceLibrary;

/// The sidebar "Devices": the node "My devices" with the entries of the
/// DeviceLibrary in their order. A click switches to an entry, the one in
/// use is bold with the state of its readings; where a device is connected
/// shows in its tooltip. The context menu has Settings, Rename (in place),
/// Open in a new window and Remove; dragging an entry changes the order.
///
/// The node "Instances" lists the instances (MainWindow gives them with
/// setInstances()) with their reading and, below each, its device; this
/// window's own is bold. A click brings a running one to the front or starts
/// a stopped one; a stopped one can be renamed and deleted.
class DeviceSidebar : public QTreeWidget
{
  Q_OBJECT
public:
  /// One row of the node "Instances".
  struct Instance
  {
    QString id;
    QString device;         ///< the device below it ("" = none)
    QString value;          ///< its reading, "OL", or "stopped"
    bool    running = false;
    bool    active = false; ///< readings come in
  };

  explicit DeviceSidebar(DeviceLibrary *library, QWidget *parent = nullptr);

  /// The entry this window uses (empty: none) and whether its readings come in.
  void        setCurrent(const QString &id, bool active);
  /// The node "My devices".
  QTreeWidgetItem *devicesNode() const { return m_devices; }
  /// The item of the entry @p id, nullptr when there is none.
  QTreeWidgetItem *deviceItem(const QString &id) const;
  /// The ids of the entries, in the order shown.
  QStringList deviceIds() const;

  /// The instances, @p own is this window's. Rebuilds only when the list
  /// changed, else updates the values; nothing while a name is edited.
  void        setInstances(const QList<Instance> &instances, const QString &own);
  /// The node "Instances".
  QTreeWidgetItem *instancesNode() const { return m_instances; }
  /// The item of the instance @p id, nullptr when there is none.
  QTreeWidgetItem *instanceItem(const QString &id) const;

  /// Shown at the start: as the user left it (@p stored, invalid when never
  /// set), else with two devices or more; a window without a meter shows it
  /// whenever there are devices to choose from.
  static bool shownAtStart(const QVariant &stored, int devices, bool configured);
  /// The assistant added an entry: the sidebar opens with the second device.
  static bool opensAfterAdd(int devicesBefore, int devicesAfter);

Q_SIGNALS:
  void        switchRequested(const QString &id);
  void        settingsRequested(const QString &id);
  void        newWindowRequested(const QString &id);
  /// Bring the instance @p id to the front, or start it.
  void        instanceRequested(const QString &id);
  void        renameInstanceRequested(const QString &from, const QString &to);
  void        deleteInstanceRequested(const QString &id);
  /// The row "+ Add device..." at the end of My devices.
  void        addDeviceRequested();

public Q_SLOTS:
  /// The entries anew from the library.
  void        fill();

protected:
  void        dropEvent(QDropEvent *event) override;
  void        contextMenuEvent(QContextMenuEvent *event) override;
  void        keyPressEvent(QKeyEvent *event) override;

private:
  void        updateMarks();
  QString     idOf(const QTreeWidgetItem *item) const;
  /// The last row of My devices, which adds one.
  QTreeWidgetItem *m_addItem = nullptr;
  /// The instance of an instance item or of the device below it.
  QString     instanceOf(const QTreeWidgetItem *item) const;
  /// Whether the instance of @p item (its row or its device row) runs.
  bool        instanceRunning(const QTreeWidgetItem *item) const;
  void        instanceMenu(QTreeWidgetItem *item, const QPoint &pos);

  DeviceLibrary   *m_library = nullptr;
  QTreeWidgetItem *m_devices = nullptr;
  QTreeWidgetItem *m_instances = nullptr;
  QString          m_ownInstance;
  QString          m_selectInstance;   ///< renamed: selected again under this name
  QString          m_current;
  bool             m_active = false;
  bool             m_filling = false;
};
