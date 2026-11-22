// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QElapsedTimer>
#include <QWidget>

class PhotoView;
class QCamera;
class QComboBox;
class QImageCapture;
class QLabel;
class QMediaCaptureSession;
class QPushButton;
class QVideoSink;

/// The camera source of "Photo of the setup" (PhotoDlg): a camera of the
/// computer (a webcam), its live picture and a button that takes a photo.
/// Qt Multimedia (QCamera, QImageCapture); the live picture comes through a
/// QVideoSink into a PhotoView, so Qt Multimedia Widgets is not needed.
/// Without QTDMM_WITH_MULTIMEDIA supported() is false and the dialog leaves
/// the tab out.
///
/// The camera runs only while setActive(true): while the dialog shows its
/// tab. Where the system asks for the camera (macOS), it asks then.
class PhotoCamera : public QWidget
{
  Q_OBJECT
public:
  explicit PhotoCamera(QWidget *parent = nullptr);
  ~PhotoCamera() override;

  /// Built with Qt Multimedia.
  static bool supported();
  /// How many cameras the system has.
  int         cameraCount() const;
  /// Starts (true) or stops the camera chosen.
  void        setActive(bool on);
  bool        isActive() const;

Q_SIGNALS:
  /// A photo taken, as JPEG.
  void        photoTaken(const QByteArray &jpeg);

private:
  void        start();
  void        stop();
  void        take();
  void        setState(const QString &text);

  QComboBox   *m_choice = nullptr;
  PhotoView   *m_live = nullptr;
  QPushButton *m_take = nullptr;
  QLabel      *m_state = nullptr;
  bool         m_wanted = false;     ///< setActive(true), the camera may still be asked for
  QMediaCaptureSession *m_session = nullptr;
  QCamera     *m_camera = nullptr;
  QImageCapture *m_capture = nullptr;
  QVideoSink  *m_sink = nullptr;
  QElapsedTimer m_shown;           ///< since the live picture was last shown
};
