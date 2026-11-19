// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QVariantMap>
#include <functional>
#include <optional>

class QSettings;

/// One of "My devices": a meter and its connection under a name.
struct MyDevice
{
  QString     id;      ///< fixed, the name may change
  QString     name;
  int         order = 0;
  QVariantMap keys;    ///< the meter keys that belong to it (DeviceLibrary::entryKeys())
  QString     alarms;     ///< its alarms (Alarm::listToJson()), empty when none were set
  QString     limitTest;  ///< its PASS/FAIL test (LimitTest::toJsonText()), empty when none was set

  QString model() const { return keys.value("DMM/model").toString(); }
  /// A logic node (a formula ...), not a meter: its keys are the model and
  /// the node as JSON, Node/json (LogicNode::toJsonText()).
  bool    isNode() const { return keys.contains(QStringLiteral("Node/json")); }
  /// Where it is connected, for a menu: the port, the Bluetooth address, the formula.
  QString where() const;
};

/// "My devices": the meters a user owns, each with its connection, to switch
/// between them with a click instead of filling in the meter page again.
///
/// One file for all instances, devices.conf next to their settings, one
/// section per device ([device-<id>]) with its name, its place in the list
/// and the meter keys that belong to the device (entryKeys(): the model, the
/// place and what its way of connecting needs - no values of another
/// transport), named as an instance stores them: applying an entry copies
/// them. An older file with whole snapshots is tidied up when the library
/// is made. Every change goes to the file at once (QSettings merges what
/// another instance wrote meanwhile), and a change by another instance
/// comes back as changed(). Only QtCore.
///
/// Which entry an instance uses is its key DMM/my-device (the id), not a
/// comparison of keys. Besides its meter keys an entry keeps the device's
/// rules (Alarms/list, Limits/test): they go with the device, not with a
/// window.
class DeviceLibrary : public QObject
{
  Q_OBJECT
public:
  /// The library in @p dir (Settings::configDir()).
  explicit DeviceLibrary(const QString &dir, QObject *parent = nullptr);

  QString     fileName() const { return m_file; }
  /// All entries, in their order.
  QList<MyDevice> list() const;
  std::optional<MyDevice> find(const QString &id) const;
  /// The entry at the place of @p keys (place()), whatever else differs;
  /// empty when none is. Of two entries at one USB cable type (the same
  /// VID:PID) the one with the same path.
  QString     findByPlace(const QVariantMap &keys) const;
  /// @p base, or with " (2)", " (3)", ... when an entry has that name.
  QString     uniqueName(const QString &base) const;

  /// A new entry at the end; returns its id. With @p id that one (a
  /// workspace brings back an entry gone), unless an entry has it.
  QString     add(const QString &name, const QVariantMap &keys, const QString &id = QString());
  bool        update(const QString &id, const QVariantMap &keys);
  bool        rename(const QString &id, const QString &name);
  bool        remove(const QString &id);
  /// Moves the entry to position @p index of list().
  bool        move(const QString &id, int index);
  /// A copy right after it, named "<name> (2)"; returns the new id.
  QString     duplicate(const QString &id);
  /// The device's rules: its alarms and its PASS/FAIL test as JSON
  /// (MyDevice::alarms, limitTest; empty removes them). They are not meter
  /// keys: update() and tidy() keep them.
  bool        setRules(const QString &id, const QString &alarms, const QString &limitTest);

  /// Of the meter keys @p keys those that belong to a device: model, data
  /// format, display and the place always, then what the way of connecting
  /// needs - the line settings for a serial port, an HID cable or the
  /// network, key and values for a Victron device, conn and options for
  /// sigrok, the signal of the virtual meter, unit and formula of a
  /// calculated value. A logic node keeps its model and Node/json only.
  /// A Bluetooth meter that QtDMM connects to (GATT) needs
  /// its address only.
  static QVariantMap entryKeys(const QVariantMap &keys);
  /// Whether an entry of some meter can keep @p key (entryKeys() for any way
  /// of connecting): switching to another entry drops such a key of the
  /// meter before when the new entry has none.
  static bool isEntryKey(const QString &key);
  /// Where a device is, as one string to compare: "serial <port>" (a
  /// /dev/serial/by-id name and its ttyUSB are the same place while the
  /// cable is plugged in), "hid <vid>:<pid>" (the hidraw number changes when
  /// it is plugged in again), "ble <address>", "rfc2217 <host:port>",
  /// "sigrok <conn>", "calc <formula>". Empty when there is no place - also
  /// for a simulated meter with a port left over in its keys.
  static QString place(const QVariantMap &keys);
  /// How a model connects when its name alone tells: "ble" (Victron),
  /// "blegatt", or empty for a port of its own. The model table is not in
  /// QtCore: the application sets this, before it makes a library.
  /// entryKeys() and tidy() go by it before the device string - an entry
  /// from before kept the port of the meter before as its place.
  static void setModelTransport(std::function<QString(const QString &model)> transport);

Q_SIGNALS:
  /// The list changed, here or in another instance.
  void        changed();

private:
  void        watch();
  /// Runs @p edit on the file and writes it; a write that fails (on
  /// Windows a virus scanner or indexer may hold the file a moment, the
  /// replacement then cannot take its place) is tried again a few times.
  /// QSettings may report an error on Windows although the change is in
  /// the file: @p landed gets the entries on disk (onDisk()) and says
  /// whether it is. False when it never succeeded.
  bool        write(const std::function<void(QSettings &)> &edit,
                    const std::function<bool(const QList<MyDevice> &)> &landed = {}) const;
  /// The entries of @p s, in their order.
  static QList<MyDevice> entries(QSettings &s);
  /// The entries as the file holds them, read from a copy: QSettings keeps
  /// one cache per file in a process, and after a failed write it still
  /// holds what was not written. Nothing when the file cannot be read.
  std::optional<QList<MyDevice>> onDisk() const;
  /// entryKeys() for every entry: snapshots of all meter keys from before
  /// are cut down (name, order and id stay).
  void        tidy();
  void        writeOrder(const QStringList &ids);

  QString     m_file;
  QFileSystemWatcher m_watcher;
};
