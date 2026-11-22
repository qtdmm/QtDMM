// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDateTime>
#include <QDialog>
#include <QImage>
#include <QLabel>

class PhotoServer;
class QComboBox;
class QListWidget;
class QTabWidget;

/// A picture scaled to the label's size, its proportions kept.
class PhotoView : public QLabel
{
  Q_OBJECT
public:
  explicit PhotoView(QWidget *parent = nullptr);
  void        setImage(const QImage &image);
  QImage      image() const { return m_image; }
  /// A window with the photo @p path (PhotoStore::load()); false when it
  /// cannot be read.
  static bool showPhoto(const QString &path, QWidget *parent);

protected:
  void        resizeEvent(QResizeEvent *event) override;

private:
  void        rescale();
  QImage      m_image;
};

/// "Photo of the setup": pictures into the open project's photos
/// (PhotoStore), each with a note in the graph at the time it came
/// (photoAdded()). From the phone - it opens a page of PhotoServer through
/// the QR code shown, the server runs only while that tab shows - or from a
/// file, chosen or dropped onto the dialog.
class PhotoDlg : public QDialog
{
  Q_OBJECT
public:
  /// Into the project @p project (BasketWidget::folder()).
  explicit PhotoDlg(const QString &project, QWidget *parent = nullptr);

  QString     project() const { return m_project; }
  /// Takes the picture @p file in: saved, shown, photoAdded(). False when
  /// it is none or was not written (error()).
  bool        addFile(const QString &file);
  /// Takes a picture in as addFile(), from its bytes (the phone's).
  bool        addData(const QByteArray &data);
  /// The server for the phone (tests).
  PhotoServer *server() const { return m_server; }
  /// How many photos were added while the dialog is open.
  int         photoCount() const;

Q_SIGNALS:
  /// A photo saved at @p path (full path), taken at @p when.
  void        photoAdded(const QString &path, const QDateTime &when);
  void        error(const QString &text);

protected:
  void        showEvent(QShowEvent *event) override;
  void        hideEvent(QHideEvent *event) override;
  void        dragEnterEvent(QDragEnterEvent *event) override;
  void        dropEvent(QDropEvent *event) override;

private:
  void        chooseFiles();
  void        present(const QString &path, const QImage &image);
  /// A photo saved at @p path (empty: not, @p why says why).
  bool        taken(const QString &path, const QDateTime &when, const QString &why);
  /// Starts the server and shows its code while the phone's tab shows, stops it otherwise.
  void        updatePhone();

  QString      m_project;
  PhotoView   *m_preview;
  QListWidget *m_strip;
  QTabWidget  *m_sources;
  QLabel      *m_qr;
  QComboBox   *m_address;
  QLabel      *m_url;
  QLabel      *m_phoneState;
  PhotoServer *m_server;
};
