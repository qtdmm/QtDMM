// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>

#include "device/capturelog.h"
#include "device/capturesteps.h"

class QCheckBox;
class QGroupBox;
class QLabel;
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
///
/// For a kind of meter with guided steps (CaptureSteps) the window walks
/// through them: each says what to set the meter to, and once the meter
/// shows a new state for a second (CaptureWatcher) it proposes what QtDMM
/// reads as what the meter shows; Keep takes it and goes to the next step.
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
  /// The guided step shown (CaptureSteps::forType()); steps().size() when
  /// all are done.
  int         step() const { return m_step; }
  const QList<CaptureSteps::Step> &steps() const { return m_steps; }
  /// Writes yaml() to @p file; false with @p error on failure.
  bool        write(const QString &file, QString *error) const;

private:
  void        addFrame(const QByteArray &bytes, qint64 wall, const std::optional<DmmDecoder::DmmResponse> &response);
  void        keep();
  void        exportCapture();
  bool        guided() const;
  void        showStep();
  void        skipStep();
  void        stepBack();
  void        propose();

  MeterConnection *m_dmm;
  Settings    *m_settings;
  CaptureLog   m_log;
  QTreeWidget *m_frames;
  QTreeWidget *m_kept;
  QLineEdit   *m_shown;
  QPushButton *m_keep;
  QPushButton *m_pause;
  QCheckBox   *m_allFrames;

  QList<CaptureSteps::Step> m_steps;
  int          m_step = 0;
  QStringList  m_skipped;
  CaptureWatcher m_watcher;
  QString      m_proposed;   ///< the text put into "The meter shows:", replaced by the next one
  std::optional<CaptureLog::Frame> m_proposedFrame;   ///< the frame it was read from, the one Keep takes
  QGroupBox   *m_guided = nullptr;
  QLabel      *m_stepTitle = nullptr;
  QLabel      *m_stepText = nullptr;
  QLabel      *m_stepHint = nullptr;
  QPushButton *m_back = nullptr;
  QPushButton *m_skip = nullptr;
  QPushButton *m_sendKey = nullptr;
};
