// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/dialogs/photocamera.h"

#include "ui/dialogs/photodlg.h"

#include <QBuffer>
#include <QComboBox>
#include <QHBoxLayout>
#include <QImageWriter>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#ifdef QTDMM_WITH_MULTIMEDIA
#include <QApplication>
#include <QCamera>
#include <QImageCapture>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QPermissions>
#include <QVideoFrame>
#include <QVideoSink>
#endif

PhotoCamera::PhotoCamera(QWidget *parent) : QWidget(parent)
{
  auto *layout = new QVBoxLayout(this);
  auto *row = new QHBoxLayout;
  auto *label = new QLabel(tr("&Device:"), this);
  m_choice = new QComboBox(this);
  m_choice->setObjectName("ui_photoCamera");
  label->setBuddy(m_choice);
  row->addWidget(label);
  row->addWidget(m_choice, 1);
  m_take = new QPushButton(QIcon::fromTheme("camera-photo"), tr("&Take photo"), this);
  m_take->setObjectName("ui_photoTake");
  m_take->setEnabled(false);
  row->addWidget(m_take);
  layout->addLayout(row);
  m_live = new PhotoView(this);
  m_live->setObjectName("ui_photoLive");
  m_live->setMinimumSize(320, 180);
  layout->addWidget(m_live, 1);
  m_state = new QLabel(this);
  m_state->setWordWrap(true);
  layout->addWidget(m_state);
  connect(m_take, &QPushButton::clicked, this, &PhotoCamera::take);
#ifdef QTDMM_WITH_MULTIMEDIA
  for (const QCameraDevice &device : QMediaDevices::videoInputs())
    m_choice->addItem(device.description(), device.id());
  if (const int def = m_choice->findData(QMediaDevices::defaultVideoInput().id()); def >= 0)
    m_choice->setCurrentIndex(def);
  connect(m_choice, &QComboBox::currentIndexChanged, this, [this]
  {
    if (m_wanted)
      start();
  });
  m_session = new QMediaCaptureSession(this);
  m_capture = new QImageCapture(this);
  m_capture->setQuality(QImageCapture::VeryHighQuality);
  m_session->setImageCapture(m_capture);
  m_sink = new QVideoSink(this);
  m_session->setVideoSink(m_sink);
  // the live picture: some 15 a second are enough to aim
  connect(m_sink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame)
  {
    if (m_shown.isValid() && m_shown.elapsed() < 66)
      return;
    m_shown.start();
    m_live->setImage(frame.toImage());
  });
  connect(m_capture, &QImageCapture::readyForCaptureChanged, m_take, &QWidget::setEnabled);
  connect(m_capture, &QImageCapture::imageCaptured, this, [this](int, const QImage &image)
  {
    QByteArray jpeg;
    QBuffer buffer(&jpeg);
    QImageWriter writer(&buffer, "jpg");
    writer.setQuality(92);
    if (buffer.open(QIODevice::WriteOnly) && writer.write(image))
      Q_EMIT photoTaken(jpeg);
    else
      setState(tr("The photo could not be stored: %1").arg(writer.errorString()));
  });
  connect(m_capture, &QImageCapture::errorOccurred, this, [this](int, QImageCapture::Error, const QString &text)
  {
    setState(tr("The camera took no photo: %1").arg(text));
  });
#endif
  if (m_choice->count() == 0)
  {
    m_choice->setEnabled(false);
    m_live->setText(tr("No camera found."));
  }
}

PhotoCamera::~PhotoCamera()
{
  stop();
}

bool PhotoCamera::supported()
{
#ifdef QTDMM_WITH_MULTIMEDIA
  return true;
#else
  return false;
#endif
}

int PhotoCamera::cameraCount() const
{
  return m_choice->count();
}

void PhotoCamera::setActive(bool on)
{
  m_wanted = on;
  if (on)
    start();
  else
    stop();
}

bool PhotoCamera::isActive() const
{
#ifdef QTDMM_WITH_MULTIMEDIA
  return m_camera && m_camera->isActive();
#else
  return false;
#endif
}

void PhotoCamera::start()
{
#ifdef QTDMM_WITH_MULTIMEDIA
  stop();
  if (m_choice->count() == 0)
    return;
  // the system may ask whether QtDMM may use the camera (macOS); then once more
  const QCameraPermission permission;
  switch (qApp->checkPermission(permission))
  {
  case Qt::PermissionStatus::Undetermined:
    setState(tr("Waiting for the permission to use the camera..."));
    qApp->requestPermission(permission, this, [this](const QPermission &)
    {
      if (m_wanted)
        start();
    });
    return;
  case Qt::PermissionStatus::Denied:
    setState(tr("QtDMM may not use the camera: allow it in the system settings (privacy, camera)."));
    return;
  case Qt::PermissionStatus::Granted:
    break;
  }
  const QList<QCameraDevice> devices = QMediaDevices::videoInputs();
  const QByteArray id = m_choice->currentData().toByteArray();
  QCameraDevice device = QMediaDevices::defaultVideoInput();
  for (const QCameraDevice &d : devices)
    if (d.id() == id)
      device = d;
  m_camera = new QCamera(device, this);
  connect(m_camera, &QCamera::errorOccurred, this, [this](QCamera::Error, const QString &text)
  {
    setState(tr("The camera does not start: %1").arg(text));
  });
  m_session->setCamera(m_camera);
  setState(QString());
  m_camera->start();
#endif
}

void PhotoCamera::stop()
{
#ifdef QTDMM_WITH_MULTIMEDIA
  if (!m_camera)
    return;
  m_camera->stop();
  m_session->setCamera(nullptr);
  delete m_camera;
  m_camera = nullptr;
  m_take->setEnabled(false);
#endif
}

void PhotoCamera::take()
{
#ifdef QTDMM_WITH_MULTIMEDIA
  if (m_capture->isReadyForCapture())
    m_capture->capture();
#endif
}

void PhotoCamera::setState(const QString &text)
{
  m_state->setText(text);
}
