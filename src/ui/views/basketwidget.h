// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QStringList>
#include <QWidget>

class QFileSystemModel;
class QJsonObject;
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
  /// New, Open and Close of the menu, for the toolbar's project button.
  QAction    *newAction() const { return m_newAction; }
  QAction    *openAction() const { return m_openAction; }
  QAction    *closeAction() const { return m_closeAction; }
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
  /// project.json of the basket in @p dir; empty when it has none or it is no JSON.
  static QJsonObject readProject(const QString &dir);
  /// Sets the description in project.json of @p dir (made first when missing),
  /// the other keys stay. False when it cannot be written.
  static bool setProjectDescription(const QString &dir, const QString &text);
  /// Packs the basket in @p dir into the ZIP @p zipFile: every file under a
  /// folder named like the basket, project.json extended by "export" (date,
  /// QtDMM version, each file with size and SHA-256). Hidden files and
  /// @p zipFile itself stay out. On failure false and the reason in @p failed.
  static bool exportProject(const QString &dir, const QString &zipFile, QString *failed = nullptr);
  /// Unpacks the project ZIP @p zipFile (exportProject(), or any ZIP) into a
  /// new folder in @p baseDir: named like its top folder, else like the ZIP,
  /// "name (2)" when taken. Checked before anything is written: no path
  /// out of the folder, every file the "export" part of project.json lists
  /// there with its size and SHA-256. project.json loses that part again.
  /// The folder in @p folder; on failure false, nothing written and the
  /// reason in @p failed.
  static bool importProject(const QString &zipFile, const QString &baseDir, QString *folder,
                            QString *failed = nullptr);
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
  /// Asks for the description of the open basket's project.
  void        describeSLOT();
  /// Asks where to and exports the open basket as a ZIP (exportProject()).
  void        exportSLOT();
  /// Asks for a project ZIP, unpacks it into the folder for new baskets
  /// (importProject()) and opens it.
  void        importSLOT();
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
  QAction          *m_describeAction;
  QAction          *m_exportAction;
  QAction          *m_importAction;
  QLabel           *m_description;   ///< of the project, shown under the name
  QToolButton      *m_title;
  QLabel           *m_empty;
  QTreeView        *m_view;
  QFileSystemModel *m_model;
};
