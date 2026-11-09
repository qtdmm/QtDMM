// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>

#include "device/capturelog.h"

class QCheckBox;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class MeterConnection;
class Settings;

/// The frames of one meter in a window beside the others, for a report of
/// what QtDMM reads wrong: the last ones with their bytes and what QtDMM read,
/// and those kept with what the meter showed then ("The meter shows:").
/// Exported as YAML (CaptureLog) to a folder chosen in the dialog, which is
/// remembered (Capture/folder).
class CaptureDlg : public QDialog
{
  Q_OBJECT
public:
  /// Shows the frames of @p dmm from now on; @p name is the device's name in
  /// the title. @p settings keeps the export folder (may be null).
  CaptureDlg(MeterConnection *dmm, const QString &name, Settings *settings, QWidget *parent = nullptr);

  const CaptureLog &log() const { return m_log; }
  /// The YAML export writes (the head from the connection's meter).
  QString     yaml() const;
  /// Writes yaml() to @p file; false with @p error on failure.
  bool        write(const QString &file, QString *error) const;

private:
  void        addFrame(const QByteArray &bytes, qint64 wall, const std::optional<DmmDecoder::DmmResponse> &response);
  void        keep();
  void        exportCapture();

  MeterConnection *m_dmm;
  Settings    *m_settings;
  CaptureLog   m_log;
  QTreeWidget *m_frames;
  QTreeWidget *m_kept;
  QLineEdit   *m_shown;
  QPushButton *m_keep;
  QPushButton *m_pause;
  QCheckBox   *m_allFrames;
};
