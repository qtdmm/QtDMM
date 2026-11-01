// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QVariantMap>
#include <optional>

#include "core/devicelibrary.h"

/// A workspace file (*.qtdmm-workspace): the measuring set-up of a window -
/// its device and the devices open beside it, the views with their values,
/// the layout - under a name, with a description and a picture.
///
/// Version 2 is JSON: the window's size and device, the devices (a copy of
/// their entry in My devices, their front panels), the layout, the views
/// and the picture (base64). Version 1, an ini file with the window's
/// keys, is still read. Towards the window it is the keys of
/// MainWindow::storeWorkspace()/applyWorkspace() (keys(), "Windows/..."),
/// which the file sorts into its members; keys it does not know are kept
/// under "other". Only QtCore.
class WorkspaceFile
{
public:
  static constexpr int kVersion = 2;

  QString     name;
  QString     description;
  int         width = 0;      ///< the main window's size, 0: none
  int         height = 0;
  /// The window's device of a file before 26.4 (its id in My devices),
  /// connected first; empty: none.
  QString     windowDevice;
  /// Copies of the devices' entries in My devices, those connected
  /// ("Windows/open-devices") in their order; without secrets (ble-key).
  QList<MyDevice> devices;
  /// The window's keys (storeWorkspace()).
  QVariantMap keys;
  QByteArray  thumbnail;      ///< a picture (JPEG or PNG); empty: none
  int         version = kVersion;   ///< the file's, after read()

  /// Reads @p path, version 2 or 1; nothing and why in @p error
  /// (translated) when it is no workspace or needs a newer QtDMM.
  static std::optional<WorkspaceFile> read(const QString &path, QString *error = nullptr);
  /// Writes version 2 to @p path; false and why in @p error.
  bool        write(const QString &path, QString *error = nullptr) const;

  /// The entry @p id among devices(), or nothing.
  std::optional<MyDevice> device(const QString &id) const;
};
