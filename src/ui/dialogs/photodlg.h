// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDateTime>
#include <QDialog>
#include <QImage>
#include <QLabel>

class QListWidget;

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
/// (photoAdded()). For now from a file, chosen or dropped onto the dialog;
/// the phone and a camera come as further sources.
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
  /// How many photos were added while the dialog is open.
  int         photoCount() const;

Q_SIGNALS:
  /// A photo saved at @p path (full path), taken at @p when.
  void        photoAdded(const QString &path, const QDateTime &when);
  void        error(const QString &text);

protected:
  void        dragEnterEvent(QDragEnterEvent *event) override;
  void        dropEvent(QDropEvent *event) override;

private:
  void        chooseFiles();
  void        present(const QString &path, const QImage &image);

  QString      m_project;
  PhotoView   *m_preview;
  QListWidget *m_strip;
};
