// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>

#include <QStringList>
#include <QWidget>

class ProjectFileModel;
class QJsonObject;
class QJsonValue;
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
  /// Why the open project is left.
  enum class Leave { Open, New, Close };
  /// Asked before New, Open and Close leave the open project; false: it
  /// stays (the bench in use changed, the person cancelled).
  void        setLeaveCheck(const std::function<bool(Leave)> &check) { m_leaveCheck = check; }
  /// Copies @p files into the basket (makes the folder first), or into its
  /// folder @p dir, under names not taken yet; returns the paths they got.
  /// Into the basket itself each goes into the folder of its kind
  /// (ProjectFolder::kindOf()) while ProjectFolder::byType() is on.
  /// Not a file, or not readable: left out, an error() names it.
  QStringList addFiles(const QStringList &files, const QString &dir = QString());
  /// Makes a folder in @p dir (a folder of the basket, empty: the basket)
  /// under a name not taken yet; returns its path, empty when it failed.
  QString     newFolder(const QString &dir = QString());
  /// Moves @p paths (files or folders of the basket) into its folder @p dir;
  /// returns the new paths. One into itself or under itself stays.
  QStringList moveInto(const QStringList &paths, const QString &dir);

  /// Where newSLOT() proposes a new basket (settings: Basket/base-dir).
  void        setBaseFolder(const QString &folder);
  /// The default project (ProjectFolder::defaultProject()) ready to open:
  /// made when missing (@p make; else only one that is there is mended), a
  /// new project file when its own is missing or broken (the broken one kept
  /// beside, ".broken"). Its folder; empty when it cannot be made.
  static QString ensureDefaultProject(bool make = true);
  /// Writes project.qtdmm into @p dir unless there is one: the basket's own
  /// file for what is not a file of its own (name, description, when it was
  /// made; later what an export of the project needs). False when it fails.
  static bool writeProjectFile(const QString &dir);
  /// project.qtdmm of the basket in @p dir; empty when it has none or it is no
  /// JSON. @p ok, when given, is false when the file is there but no JSON object.
  static QJsonObject readProject(const QString &dir, bool *ok = nullptr);
  /// The name the project in @p dir is shown by: its name in project.qtdmm,
  /// else the folder's.
  static QString projectName(const QString &dir);
  /// Sets the description in project.qtdmm of @p dir (made first when missing),
  /// the other keys stay. False when it cannot be written.
  static bool setProjectDescription(const QString &dir, const QString &text);
  /// Sets @p key in project.qtdmm of @p dir (made first when missing), the
  /// other keys stay. False when it cannot be written.
  static bool setProjectValue(const QString &dir, const QString &key, const QJsonValue &value);
  /// Packs the basket in @p dir into the ZIP @p zipFile: every file under a
  /// folder named like the basket, project.qtdmm extended by "export" (date,
  /// QtDMM version, each file with size and SHA-256). Hidden files and
  /// @p zipFile itself stay out. On failure false and the reason in @p failed.
  static bool exportProject(const QString &dir, const QString &zipFile, QString *failed = nullptr);
  /// Unpacks the packed project @p zipFile (exportProject(): a ZIP, also
  /// one of before 27.1 with project.json, unpacked as project.qtdmm) into a new folder in @p baseDir: named like its top
  /// folder (zipProjectName()), "name 2" when taken. Checked before anything
  /// is written: a project.qtdmm, no path out of the folder, every file the
  /// "export" part of project.qtdmm lists there with its size and SHA-256.
  /// project.qtdmm loses that part again. The folder in @p folder; on
  /// failure false, nothing written and the reason in @p failed.
  static bool importProject(const QString &zipFile, const QString &baseDir, QString *folder,
                            QString *failed = nullptr);
  /// The folder name importProject() gives @p zipFile's project before a
  /// clash: its top folder, else the ZIP's name; empty when it is no ZIP.
  static QString zipProjectName(const QString &zipFile);
  /// The project @p file lies in: the nearest folder above it that is the
  /// open project @p open or has a project.qtdmm (subfolders count); empty
  /// when it lies in none. A bench belongs to that project.
  static QString projectOf(const QString &file, const QString &open = QString());
  /// @p name in @p dir if free, else "name (2).ext", "name (3).ext" ...
  static QString freeName(const QString &dir, const QString &name);
  /// @p name if no bench in the project @p project has it (its name, else
  /// its file's), else "name (2)", "name (3)" ... (case does not count).
  static QString freeBenchName(const QString &project, const QString &name);
  /// The symbol of a file in the project's list by its suffix ("text-csv",
  /// "image-x-generic" ...), "folder" for a folder; empty: Qt's own.
  static QString fileIconName(const QString &fileName, bool dir);

Q_SIGNALS:
  /// Another basket was opened, or none (empty).
  void        folderChanged(const QString &folder);
  /// The person made, opened or imported a project here (not a switch by
  /// loading a bench): the receiver shows the window.
  void        projectOpened();
  /// A file to open (double click, Open): the receiver loads a bench or a
  /// QtDMM recording, anything else goes to openWithSystem().
  void        openRequested(const QString &path);
  /// The button for a photo of the setup (PhotoDlg) was clicked.
  void        photoRequested();
  void        error(const QString &text);

public Q_SLOTS:
  void        addSLOT();
  /// Asks for a new folder and opens it as the basket.
  void        newSLOT();
  /// Asks for a project - its project.qtdmm (project.json before 27.1), or
  /// one packed into a .qtdmm-project (.zip before) - and opens it; a packed
  /// one is unpacked into the folder for new projects first (openZip()).
  void        openSLOT();
  /// Asks for the description of the open basket's project.
  void        describeSLOT();
  /// Asks where to and exports the open basket as a ZIP (exportProject()).
  void        exportSLOT();
  /// Opens @p path with the program the system has for it.
  void        openWithSystem(const QString &path);

protected:
  void        dragEnterEvent(QDragEnterEvent *event) override;
  void        dragMoveEvent(QDragMoveEvent *event) override;
  void        dropEvent(QDropEvent *event) override;

private:
  /// openSLOT() for a ZIP: unpacked into the folder for new projects and
  /// opened; one of the same name there: open that one, unpack a copy, or
  /// nothing. False: nothing opened.
  bool        openZip(const QString &zipFile);
  void        showFolder();
  void        contextMenu(const QPoint &pos);
  QString     pathAt(const QModelIndex &index) const;
  /// The folder a drop or a new folder at @p pos (view coordinates) goes
  /// into: the folder there, the one of the file there, else the basket.
  QString     dirAt(const QPoint &pos) const;
  void        updateEmpty();

  QString           m_folder;
  QString           m_baseFolder;
  QStackedWidget   *m_pages;
  QAction          *m_newAction;
  QAction          *m_openAction;
  QAction          *m_closeAction;
  QAction          *m_describeAction;
  QAction          *m_exportAction;
  QLabel           *m_description;   ///< of the project, shown under the name
  QToolButton      *m_title;
  QLabel           *m_empty;
  QTreeView        *m_view;
  ProjectFileModel *m_model;
  std::function<bool(Leave)> m_leaveCheck;   ///< setLeaveCheck()
};
