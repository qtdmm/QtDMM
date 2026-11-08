// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QStringList>
#include <QWidget>

class QFileSystemModel;
class QModelIndex;
class QAction;
class QLabel;
class QToolButton;
class QStackedWidget;
class QTreeView;

/// The project basket: the files of a project (its benches, recordings,
/// pictures, PDFs, LabPlot projects, anything), as a window.
///
/// The basket is a plain folder the user makes or opens; one project has
/// one basket and may have several benches in it. The window shows what is
/// in the folder, so it can be used in a file manager as well. It stays
/// open until another is opened, also when a bench is loaded from
/// elsewhere. Files come in as copies, never moved, never overwritten.
class BasketWidget : public QWidget
{
  Q_OBJECT

public:
  explicit BasketWidget(QWidget *parent = nullptr);

  /// Opens the basket in @p folder; empty: none.
  void        setFolder(const QString &folder);
  /// The basket's folder, empty while none is open.
  QString     folder() const { return m_folder; }
  /// Copies @p files into the basket (makes the folder first) under names
  /// not taken yet; returns the paths they got. Not a file, or not
  /// readable: left out, an error() names it.
  QStringList addFiles(const QStringList &files);

  /// Where newSLOT() proposes a new basket (settings: Basket/base-dir).
  void        setBaseFolder(const QString &folder) { m_baseFolder = folder; }
  /// Writes project.json into @p dir unless there is one: the basket's own
  /// file for what is not a file of its own (name, description, when it was
  /// made; later what an export of the project needs). False when it fails.
  static bool writeProjectFile(const QString &dir);
  /// @p name in @p dir if free, else "name (2).ext", "name (3).ext" ...
  static QString freeName(const QString &dir, const QString &name);

Q_SIGNALS:
  /// Another basket was opened, or none (empty).
  void        folderChanged(const QString &folder);
  /// A file to open (double click, Open): the receiver loads a bench or a
  /// QtDMM recording, anything else goes to openWithSystem().
  void        openRequested(const QString &path);
  void        error(const QString &text);

public Q_SLOTS:
  void        addSLOT();
  /// Asks for a new folder and opens it as the basket.
  void        newSLOT();
  /// Asks for a folder and opens it as the basket.
  void        openSLOT();
  /// Opens @p path with the program the system has for it.
  void        openWithSystem(const QString &path);

protected:
  void        dragEnterEvent(QDragEnterEvent *event) override;
  void        dropEvent(QDropEvent *event) override;

private:
  void        showFolder();
  void        contextMenu(const QPoint &pos);
  QString     pathAt(const QModelIndex &index) const;
  void        updateEmpty();

  QString           m_folder;
  QString           m_baseFolder;
  QStackedWidget   *m_pages;
  QAction          *m_newAction;
  QAction          *m_openAction;
  QAction          *m_closeAction;
  QToolButton      *m_title;
  QLabel           *m_empty;
  QTreeView        *m_view;
  QFileSystemModel *m_model;
};
