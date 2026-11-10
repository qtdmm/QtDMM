// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/basketwidget.h"

#include "core/projectfolder.h"
#include "recording/zipfile.h"

#include <algorithm>

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMimeData>
#include <QPushButton>
#include <QSaveFile>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>

/// The files of the basket; the folders of a kind directly in it show the
/// name project.json has for them in the language of the program.
class ProjectFileModel : public QFileSystemModel
{
public:
  using QFileSystemModel::QFileSystemModel;

  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
  {
    // the symbols of the set chosen (with System the desktop's first), not
    // whatever Qt finds: a dark folder on a dark window was hard to see
    if (role == Qt::DecorationRole && index.column() == 0)
    {
      const QString name = BasketWidget::fileIconName(fileName(index), isDir(index));
      const QIcon icon = name.isEmpty() ? QIcon() : QIcon::fromTheme(name);
      if (!icon.isNull())
        return icon;
    }
    const QString shown = (role == Qt::DisplayRole || role == Qt::EditRole) ? shownName(index) : QString();
    return shown.isEmpty() ? QFileSystemModel::data(index, role) : QVariant(shown);
  }

  // F2 and Enter without a change: the name on disk stays
  bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override
  {
    const QString shown = role == Qt::EditRole ? shownName(index) : QString();
    if (!shown.isEmpty() && value.toString() == shown)
      return true;
    return QFileSystemModel::setData(index, value, role);
  }

private:
  QString shownName(const QModelIndex &index) const
  {
    if (index.column() != 0 || !isDir(index))
      return QString();
    const QString project = rootPath();
    if (filePath(index.parent()) != project)
      return QString();
    // project.json read again only when it changed
    const QDateTime changed = QFileInfo(QDir(project).filePath("project.json")).lastModified();
    if (project != m_project || changed != m_changed)
    {
      m_project = project;
      m_changed = changed;
      m_shown.clear();
      const QString language = QLocale().name().section('_', 0, 0);
      const QJsonObject folders = BasketWidget::readProject(project).value("folders").toObject();
      for (auto it = folders.begin(); it != folders.end(); ++it)
        m_shown.insert(it.key(), it.value().toObject().value(language).toString());
    }
    // not fileName(): it asks data() for the name shown
    const QString name = QFileSystemModel::data(index, FileNameRole).toString();
    const QString shown = m_shown.value(name);   // none in this language: the name on disk
    return shown == name ? QString() : shown;
  }

  mutable QString                 m_project;
  mutable QDateTime               m_changed;
  mutable QHash<QString, QString> m_shown;   ///< name on disk -> name shown
};

BasketWidget::BasketWidget(QWidget *parent) :
  QWidget(parent)
{
  setWindowTitle(tr("Project"));
  setAcceptDrops(true);
  m_pages = new QStackedWidget(this);
  auto *outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->addWidget(m_pages);

  m_newAction = new QAction(tr("&New project..."), this);
  m_openAction = new QAction(tr("&Open project..."), this);
  m_closeAction = new QAction(tr("&Close project"), this);
  connect(m_newAction, &QAction::triggered, this, &BasketWidget::newSLOT);
  connect(m_openAction, &QAction::triggered, this, &BasketWidget::openSLOT);
  connect(m_closeAction, &QAction::triggered, this, [this] { setFolder(QString()); });
  m_describeAction = new QAction(tr("&Description..."), this);
  connect(m_describeAction, &QAction::triggered, this, &BasketWidget::describeSLOT);
  m_exportAction = new QAction(tr("E&xport project..."), this);
  connect(m_exportAction, &QAction::triggered, this, &BasketWidget::exportSLOT);
  m_importAction = new QAction(tr("&Import project..."), this);
  connect(m_importAction, &QAction::triggered, this, &BasketWidget::importSLOT);

  // no basket open: what it is and how to get one
  auto *none = new QWidget(m_pages);
  auto *noneLayout = new QVBoxLayout(none);
  auto *what = new QLabel(tr("A project keeps its files in one folder: its benches, recordings, "
                             "pictures, PDFs, LabPlot projects."), none);
  what->setWordWrap(true);
  what->setAlignment(Qt::AlignCenter);
  auto *create = new QPushButton(m_newAction->text(), none);
  auto *choose = new QPushButton(m_openAction->text(), none);
  auto *unpack = new QPushButton(m_importAction->text(), none);
  connect(create, &QPushButton::clicked, m_newAction, &QAction::trigger);
  connect(choose, &QPushButton::clicked, m_openAction, &QAction::trigger);
  connect(unpack, &QPushButton::clicked, m_importAction, &QAction::trigger);
  noneLayout->addStretch();
  noneLayout->addWidget(what);
  noneLayout->addWidget(create, 0, Qt::AlignCenter);
  noneLayout->addWidget(choose, 0, Qt::AlignCenter);
  noneLayout->addWidget(unpack, 0, Qt::AlignCenter);
  noneLayout->addStretch();
  m_pages->addWidget(none);

  // the basket: its folder, a bar, the files
  auto *page = new QWidget(m_pages);
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(4, 4, 4, 4);
  auto *bar = new QHBoxLayout;
  // its name; a click offers another basket
  m_title = new QToolButton(page);
  m_title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  m_title->setPopupMode(QToolButton::InstantPopup);
  m_title->setToolButtonStyle(Qt::ToolButtonTextOnly);
  m_title->setAutoRaise(true);
  auto *baskets = new QMenu(m_title);
  baskets->addAction(m_describeAction);
  baskets->addSeparator();
  baskets->addActions({ m_newAction, m_openAction });
  baskets->addSeparator();
  baskets->addActions({ m_importAction, m_exportAction });
  baskets->addSeparator();
  baskets->addAction(m_closeAction);
  m_title->setMenu(baskets);
  // icons only: the window is often a narrow column
  auto *add = new QToolButton(page);
  add->setText(tr("&Add..."));
  add->setIcon(QIcon::fromTheme("list-add", style()->standardIcon(QStyle::SP_FileDialogNewFolder)));
  add->setToolButtonStyle(Qt::ToolButtonIconOnly);
  add->setAutoRaise(true);
  add->setToolTip(tr("Copy files into the project"));
  connect(add, &QToolButton::clicked, this, &BasketWidget::addSLOT);
  auto *open = new QToolButton(page);
  open->setText(tr("Open &folder"));
  open->setIcon(QIcon::fromTheme("document-open", style()->standardIcon(QStyle::SP_DirOpenIcon)));
  open->setToolButtonStyle(Qt::ToolButtonIconOnly);
  open->setAutoRaise(true);
  open->setToolTip(tr("Show the project in the file manager"));
  connect(open, &QToolButton::clicked, this, [this]
  {
    QDir().mkpath(m_folder);
    openWithSystem(m_folder);
  });
  bar->addWidget(m_title, 1);
  bar->addWidget(add);
  bar->addWidget(open);
  layout->addLayout(bar);
  m_description = new QLabel(page);
  m_description->setWordWrap(true);
  m_description->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  layout->addWidget(m_description);
  m_empty = new QLabel(tr("Empty. Drop files here or add them with Add..."), page);
  m_empty->setAlignment(Qt::AlignCenter);
  m_empty->setWordWrap(true);
  m_empty->setEnabled(false);
  layout->addWidget(m_empty);
  m_model = new ProjectFileModel(this);
  m_model->setReadOnly(false);   // renaming in place
  m_model->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
  m_view = new QTreeView(page);
  m_view->setModel(m_model);
  // folders open in place; files and folders are dragged into folders (or
  // out to other programs), drops go to BasketWidget::dropEvent()
  m_view->setDragEnabled(true);
  m_view->setDragDropMode(QAbstractItemView::DragOnly);
  m_model->sort(3, Qt::DescendingOrder);   // newest first
  m_view->setEditTriggers(QAbstractItemView::EditKeyPressed);
  m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_view->setContextMenuPolicy(Qt::CustomContextMenu);
  // the names, newest first: the window is often a narrow column; size,
  // type and date are in the file manager
  for (int column = 1; column < m_model->columnCount(); ++column)
    m_view->setColumnHidden(column, true);
  m_view->setHeaderHidden(true);
  layout->addWidget(m_view, 1);
  connect(m_view, &QTreeView::activated, this, [this](const QModelIndex &index)
  {
    if (!m_model->isDir(index))
      Q_EMIT openRequested(pathAt(index));
  });
  connect(m_view, &QTreeView::customContextMenuRequested, this, &BasketWidget::contextMenu);
  connect(m_model, &QFileSystemModel::directoryLoaded, this, &BasketWidget::updateEmpty);
  connect(m_model, &QAbstractItemModel::rowsInserted, this, &BasketWidget::updateEmpty);
  // the project's own file is QtDMM's, not one of the person's: not listed
  auto hideProjectFile = [this]
  {
    if (m_folder.isEmpty())
      return;
    const QModelIndex file = m_model->index(QDir(m_folder).filePath("project.json"));
    if (file.isValid())
      m_view->setRowHidden(file.row(), file.parent(), true);
  };
  connect(m_model, &QFileSystemModel::directoryLoaded, this, hideProjectFile);
  connect(m_model, &QAbstractItemModel::rowsInserted, this, hideProjectFile);
  connect(m_model, &QAbstractItemModel::layoutChanged, this, hideProjectFile);
  connect(m_model, &QAbstractItemModel::rowsRemoved, this, &BasketWidget::updateEmpty);
  m_pages->addWidget(page);
  showFolder();
}

QString BasketWidget::fileIconName(const QString &fileName, bool dir)
{
  if (dir)
    return QStringLiteral("folder");
  const QString suffix = QFileInfo(fileName).suffix().toLower();
  static const QHash<QString, QString> names = {
    { "csv", "text-csv" }, { "qtdmm-rec", "text-csv" },
    { "xlsx", "x-office-spreadsheet" }, { "ods", "x-office-spreadsheet" },
    { "png", "image-x-generic" }, { "jpg", "image-x-generic" }, { "jpeg", "image-x-generic" },
    { "svg", "image-x-generic" }, { "bmp", "image-x-generic" },
    { "pdf", "application-pdf" },
    { "lml", "office-chart-line" },               // a LabPlot project: the analysis
    { "qtdmm-workspace", "qtdmm-arrange" },       // a bench: its arrangement
    { "txt", "text-x-generic" }, { "log", "text-x-generic" }, { "md", "text-x-generic" },
    { "yaml", "text-x-generic" }, { "json", "text-x-generic" },
  };
  return names.value(suffix);
}

QString BasketWidget::freeName(const QString &dir, const QString &name)
{
  const QDir d(dir);
  if (!QFileInfo::exists(d.filePath(name)))
    return d.filePath(name);
  const QFileInfo info(name);
  const QString base = info.completeBaseName(), suffix = info.suffix();
  for (int n = 2;; ++n)
  {
    const QString candidate = d.filePath(QString("%1 (%2)%3").arg(base).arg(n)
                                           .arg(suffix.isEmpty() ? QString() : "." + suffix));
    if (!QFileInfo::exists(candidate))
      return candidate;
  }
}

QString BasketWidget::projectOf(const QString &file, const QString &open)
{
  const QString openDir = open.isEmpty() ? QString() : QDir(open).absolutePath();
  QDir dir = QFileInfo(file).absoluteDir();
  while (true)
  {
    if (dir.absolutePath() == openDir || dir.exists("project.json"))
      return dir.absolutePath();
    if (!dir.cdUp())
      return QString();
  }
}

void BasketWidget::setFolder(const QString &folder)
{
  const QString dir = folder.isEmpty() ? QString() : QDir(folder).absolutePath();
  if (dir == m_folder)
    return;
  m_folder = dir;
  ProjectFolder::set(dir);
  showFolder();
  Q_EMIT folderChanged(m_folder);
}

void BasketWidget::newSLOT()
{
  QDir().mkpath(m_baseFolder);
  // a folder to be made: folders only, no "All files (*)" [V 09.11.]
  QFileDialog dlg(this, tr("New project: the name of its folder"), m_baseFolder);
  dlg.setAcceptMode(QFileDialog::AcceptSave);
  dlg.setFileMode(QFileDialog::AnyFile);
  dlg.setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Drives);
  dlg.setNameFilter(tr("Project folder (*)"));
  dlg.setLabelText(QFileDialog::FileName, tr("Project:"));
  dlg.setLabelText(QFileDialog::Accept, tr("Make"));
  if (dlg.exec() != QDialog::Accepted || dlg.selectedFiles().isEmpty())
    return;
  const QString dir = dlg.selectedFiles().constFirst();
  if (!QDir().mkpath(dir) || !writeProjectFile(dir))
  {
    Q_EMIT error(tr("Project: cannot make the folder %1").arg(QDir::toNativeSeparators(dir)));
    return;
  }
  setFolder(dir);
  Q_EMIT projectOpened();
}

bool BasketWidget::writeProjectFile(const QString &dir)
{
  QFile file(QDir(dir).filePath("project.json"));
  if (file.exists())
    return true;
  const QJsonObject project{ { "format", 1 },
                             { "name", QFileInfo(dir).fileName() },
                             { "description", QString() },
                             { "created", QDateTime::currentDateTime().toString(Qt::ISODate) },
                             { "qtdmm", QCoreApplication::applicationVersion() } };
  return file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(project).toJson()) > 0;
}

QString BasketWidget::projectName(const QString &dir)
{
  const QString name = readProject(dir).value("name").toString();
  return name.isEmpty() ? QFileInfo(dir).fileName() : name;
}

QJsonObject BasketWidget::readProject(const QString &dir)
{
  QFile file(QDir(dir).filePath("project.json"));
  if (!file.open(QIODevice::ReadOnly))
    return {};
  return QJsonDocument::fromJson(file.readAll()).object();
}

bool BasketWidget::setProjectDescription(const QString &dir, const QString &text)
{
  if (!writeProjectFile(dir))
    return false;
  QJsonObject project = readProject(dir);
  project["description"] = text;
  QSaveFile file(QDir(dir).filePath("project.json"));
  return file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(project).toJson()) > 0 && file.commit();
}

void BasketWidget::describeSLOT()
{
  bool ok = false;
  const QString text = QInputDialog::getMultiLineText(this, tr("Project description"),
                                                      tr("What the project is about:"),
                                                      readProject(m_folder).value("description").toString(), &ok);
  if (!ok)
    return;
  if (!setProjectDescription(m_folder, text.trimmed()))
    Q_EMIT error(tr("Project: cannot write %1")
                   .arg(QDir::toNativeSeparators(QDir(m_folder).filePath("project.json"))));
  showFolder();
}

bool BasketWidget::exportProject(const QString &dir, const QString &zipFile, QString *failed)
{
  auto fail = [failed](const QString &why)
  {
    if (failed)
      *failed = why;
    return false;
  };
  const QDir root(dir);
  if (!root.exists())
    return fail(tr("there is no folder %1").arg(QDir::toNativeSeparators(dir)));
  const QString top = QFileInfo(root.absolutePath()).fileName();
  const QString target = QFileInfo(zipFile).absoluteFilePath();
  // sorted: the same basket gives the same archive
  QStringList files;
  QDirIterator it(root.absolutePath(), QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
  while (it.hasNext())
  {
    const QString path = it.next();
    const QString name = root.relativeFilePath(path);
    // hidden folders (.git, a file manager's) and the archive itself stay out
    if (path == target || name == "project.json" || name.startsWith('.') || name.contains("/."))
      continue;
    files << name;
  }
  files.sort();

  // already packed: stored as they are, deflating them again gains nothing
  static const QStringList packed{ "zip", "qtdmm-rec", "xlsx", "ods", "odt", "docx", "png", "jpg", "jpeg",
                                   "pdf", "gz", "xz", "bz2", "7z", "mp4", "webm" };
  ZipWriter zip;
  QJsonArray manifest;
  for (const QString &name : files)
  {
    QFile file(root.filePath(name));
    if (!file.open(QIODevice::ReadOnly))
      return fail(tr("cannot read %1").arg(QDir::toNativeSeparators(file.fileName())));
    const QByteArray data = file.readAll();
    const QFileInfo info(file);
    if (!zip.add((top + '/' + name).toUtf8(), data, packed.contains(info.suffix().toLower()), info.lastModified()))
      return fail(tr("the project is too large for a ZIP file (4 GB)"));
    manifest.append(QJsonObject{ { "path", name },
                                 { "size", double(data.size()) },
                                 { "sha256", QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()) } });
  }
  QJsonObject project = readProject(dir);
  if (!project.contains("name"))
    project["name"] = top;
  if (!project.contains("format"))
    project["format"] = 1;
  project["export"] = QJsonObject{ { "date", QDateTime::currentDateTime().toString(Qt::ISODate) },
                                   { "qtdmm", QCoreApplication::applicationVersion() },
                                   { "files", manifest } };
  if (!zip.add((top + "/project.json").toUtf8(), QJsonDocument(project).toJson(), false, QDateTime::currentDateTime()))
    return fail(tr("the project is too large for a ZIP file (4 GB)"));

  QSaveFile out(zipFile);
  if (!out.open(QIODevice::WriteOnly) || out.write(zip.finish()) < 0 || !out.commit())
    return fail(tr("cannot write %1").arg(QDir::toNativeSeparators(zipFile)));
  return true;
}

void BasketWidget::exportSLOT()
{
  if (m_folder.isEmpty())
    return;
  // next to the basket, not in it
  const QFileInfo folder(m_folder);
  const QString zipFile = QFileDialog::getSaveFileName(this, tr("Export project"),
                                                       folder.absoluteDir().filePath(folder.fileName() + ".zip"),
                                                       tr("ZIP archives (*.zip)"));
  if (zipFile.isEmpty())
    return;
  QApplication::setOverrideCursor(Qt::WaitCursor);
  QString why;
  const bool ok = exportProject(m_folder, zipFile, &why);
  QApplication::restoreOverrideCursor();
  if (!ok)
    Q_EMIT error(tr("Export project: %1").arg(why));
}

bool BasketWidget::importProject(const QString &zipFile, const QString &baseDir, QString *folder, QString *failed)
{
  auto fail = [failed](const QString &why)
  {
    if (failed)
      *failed = why;
    return false;
  };
  QFile in(zipFile);
  if (!in.open(QIODevice::ReadOnly))
    return fail(tr("cannot read %1").arg(QDir::toNativeSeparators(zipFile)));
  // what exportProject() writes at most
  constexpr qint64 kMaxTotal = qint64(4) << 30;
  std::optional<QList<ZipFileEntry>> files = zipFiles(in.readAll(), kMaxTotal);
  in.close();
  if (!files)
    return fail(tr("%1 is no ZIP file, is damaged or unpacks to more than 4 GB")
                  .arg(QDir::toNativeSeparators(zipFile)));
  if (files->isEmpty())
    return fail(tr("%1 is empty").arg(QDir::toNativeSeparators(zipFile)));

  // no path out of the folder: none absolute, none with "..", no drive
  static const QRegularExpression drive(QStringLiteral("^[A-Za-z]:"));
  for (const ZipFileEntry &f : std::as_const(*files))
  {
    const QStringList parts = f.name.split('/');
    if (f.name.startsWith('/') || f.name.contains('\\') || f.name.contains(drive) || parts.contains("..")
        || parts.contains("."))
      return fail(tr("%1 has a file outside its folder: %2").arg(QDir::toNativeSeparators(zipFile), f.name));
  }
  // one folder at the top (as exportProject() packs it): the basket's name
  QString top = files->first().name.section('/', 0, 0);
  for (const ZipFileEntry &f : std::as_const(*files))
    if (!f.name.startsWith(top + '/'))
      top.clear();
  if (top.isEmpty())
    top = QFileInfo(zipFile).completeBaseName();
  else
    for (ZipFileEntry &f : *files)
      f.name = f.name.mid(top.size() + 1);

  QJsonObject project;
  qsizetype projectIndex = -1;
  for (qsizetype i = 0; i < files->size(); ++i)
    if (files->at(i).name == QLatin1String("project.json"))
    {
      project = QJsonDocument::fromJson(files->at(i).data).object();
      projectIndex = i;
    }
  // what the export listed is there, unchanged
  QStringList wrong;
  for (const QJsonValue &v : project.value("export").toObject().value("files").toArray())
  {
    const QJsonObject listed = v.toObject();
    const QString path = listed.value("path").toString();
    auto it = std::find_if(files->cbegin(), files->cend(), [&path](const ZipFileEntry &f) { return f.name == path; });
    if (it == files->cend() || it->data.size() != qint64(listed.value("size").toDouble())
        || QCryptographicHash::hash(it->data, QCryptographicHash::Sha256).toHex() != listed.value("sha256").toString().toLatin1())
      wrong << path;
  }
  if (!wrong.isEmpty())
    return fail(tr("missing or changed since the export: %1").arg(wrong.join(", ")));
  project.remove("export");

  // a folder of its own, beside any of the same name
  QString target = QDir(baseDir).filePath(top);
  for (int n = 2; QFileInfo::exists(target); ++n)
    target = QDir(baseDir).filePath(QString("%1 (%2)").arg(top).arg(n));
  if (!QDir().mkpath(target))
    return fail(tr("cannot make the folder %1").arg(QDir::toNativeSeparators(target)));
  const QDir dir(target);
  for (qsizetype i = 0; i < files->size(); ++i)
  {
    const ZipFileEntry &f = files->at(i);
    QFile out(dir.filePath(f.name));
    if (!QDir().mkpath(QFileInfo(out).absolutePath()) || !out.open(QIODevice::WriteOnly)
        || out.write(i == projectIndex ? QJsonDocument(project).toJson() : f.data) < 0 || !out.flush())
    {
      QDir(target).removeRecursively();   // made just now, nothing of the user's in it
      return fail(tr("cannot write %1").arg(QDir::toNativeSeparators(out.fileName())));
    }
  }
  writeProjectFile(target);   // a ZIP of any files: it is a basket now
  if (folder)
    *folder = target;
  return true;
}

void BasketWidget::importSLOT()
{
  const QString zipFile = QFileDialog::getOpenFileName(this, tr("Import project"), m_baseFolder,
                                                       tr("ZIP archives (*.zip)"));
  if (zipFile.isEmpty())
    return;
  QDir().mkpath(m_baseFolder);
  QApplication::setOverrideCursor(Qt::WaitCursor);
  QString folder, why;
  const bool ok = importProject(zipFile, m_baseFolder, &folder, &why);
  QApplication::restoreOverrideCursor();
  if (!ok)
  {
    Q_EMIT error(tr("Import project: %1").arg(why));
    return;
  }
  setFolder(folder);
  Q_EMIT projectOpened();
}

void BasketWidget::openSLOT()
{
  const QString dir = QFileDialog::getExistingDirectory(this, tr("Open project"), m_folder);
  if (dir.isEmpty())
    return;
  setFolder(dir);
  Q_EMIT projectOpened();
}

void BasketWidget::showFolder()
{
  m_pages->setCurrentIndex(m_folder.isEmpty() ? 0 : 1);
  if (m_folder.isEmpty())
    return;
  m_title->setText(QFileInfo(m_folder).fileName());
  const QString description = readProject(m_folder).value("description").toString();
  m_description->setText(description);
  m_description->setVisible(!description.isEmpty());
  m_title->setToolTip(QDir::toNativeSeparators(m_folder));
  // a folder that is not there yet shows nothing, not the whole disk
  const bool there = QFileInfo(m_folder).isDir();
  m_model->setRootPath(there ? m_folder : QString());
  m_view->setRootIndex(there ? m_model->index(m_folder) : QModelIndex());
  m_view->setVisible(there);
  updateEmpty();
}

void BasketWidget::updateEmpty()
{
  const bool there = !m_folder.isEmpty() && QFileInfo(m_folder).isDir();
  // project.json alone is empty to the person: it is not listed
  const QStringList entries = there ? QDir(m_folder).entryList(QDir::AllEntries | QDir::NoDotAndDotDot)
                                    : QStringList();
  m_empty->setVisible(entries.isEmpty() || entries == QStringList { "project.json" });
}

QStringList BasketWidget::addFiles(const QStringList &files, const QString &dir)
{
  QStringList added;
  if (m_folder.isEmpty() || files.isEmpty())
    return added;
  // into the project itself: each into the folder of its kind
  const bool sort = dir.isEmpty() || QDir(dir) == QDir(m_folder);
  if (!QDir().mkpath(sort ? m_folder : dir))
  {
    Q_EMIT error(tr("Project: cannot make the folder %1").arg(QDir::toNativeSeparators(sort ? m_folder : dir)));
    return added;
  }
  for (const QString &file : files)
  {
    const QFileInfo info(file);
    const QString into = !sort ? dir : info.isFile() ? ProjectFolder::dirFor(m_folder, ProjectFolder::kindOf(file))
                                                     : m_folder;
    // a file already there stays as it is
    if (info.absoluteDir() == QDir(into))
      continue;
    const QString target = freeName(into, info.fileName());
    if (!info.isFile() || !QFile::copy(file, target))
      Q_EMIT error(tr("Project: cannot copy %1").arg(QDir::toNativeSeparators(file)));
    else
      added << target;
  }
  if (!m_view->isVisible())
    showFolder();
  return added;
}

QString BasketWidget::newFolder(const QString &dir)
{
  if (m_folder.isEmpty())
    return QString();
  const QString path = freeName(dir.isEmpty() ? m_folder : dir, tr("New folder"));
  if (!QDir().mkpath(path))
  {
    Q_EMIT error(tr("Project: cannot make the folder %1").arg(QDir::toNativeSeparators(path)));
    return QString();
  }
  return path;
}

QStringList BasketWidget::moveInto(const QStringList &paths, const QString &dir)
{
  QStringList moved;
  const QString into = QDir(dir).absolutePath();
  for (const QString &path : paths)
  {
    const QFileInfo info(path);
    const QString from = info.absoluteFilePath();
    // not into itself or a folder under itself, nor where it is
    if (into == from || into.startsWith(from + '/') || info.absolutePath() == into || !info.exists())
      continue;
    const QString target = freeName(into, info.fileName());
    if (QFile::rename(from, target))
      moved << target;
    else
      Q_EMIT error(tr("Project: cannot move %1").arg(QDir::toNativeSeparators(from)));
  }
  return moved;
}

void BasketWidget::addSLOT()
{
  addFiles(QFileDialog::getOpenFileNames(this, tr("Add to the project")));
}

void BasketWidget::openWithSystem(const QString &path)
{
  if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
    Q_EMIT error(tr("No program opens %1").arg(QDir::toNativeSeparators(path)));
}

QString BasketWidget::pathAt(const QModelIndex &index) const
{
  return m_model->filePath(index);
}

void BasketWidget::contextMenu(const QPoint &pos)
{
  const QModelIndex index = m_view->indexAt(pos);
  QMenu menu(this);
  if (index.isValid())
  {
    const QString path = pathAt(index);
    connect(menu.addAction(tr("&Open")), &QAction::triggered, this, [this, path]
    {
      if (QFileInfo(path).isDir())
        openWithSystem(path);
      else
        Q_EMIT openRequested(path);
    });
    connect(menu.addAction(tr("Open &with the system")), &QAction::triggered, this,
            [this, path] { openWithSystem(path); });
    connect(menu.addAction(tr("&Rename")), &QAction::triggered, this, [this, index] { m_view->edit(index); });
    menu.addSeparator();
    connect(menu.addAction(tr("Move to &trash")), &QAction::triggered, this, [this]
    {
      // the rows selected; a right click selects the row it is on
      QStringList paths;
      for (const QModelIndex &row : m_view->selectionModel()->selectedRows())
        paths << pathAt(row);
      for (const QString &p : std::as_const(paths))
        if (!QFile::moveToTrash(p))
          Q_EMIT error(tr("Project: cannot move %1 to the trash").arg(QDir::toNativeSeparators(p)));
    });
    menu.addSeparator();
  }
  // in the folder clicked on, beside the file clicked on, else at the top;
  // named at once
  const QString dir = dirAt(pos);
  connect(menu.addAction(QIcon::fromTheme("folder-new"), tr("New &folder")), &QAction::triggered, this, [this, dir]
  {
    const QString path = newFolder(dir);
    if (path.isEmpty())
      return;
    m_view->expand(m_model->index(dir));
    // the model sees the folder a moment later
    QTimer::singleShot(200, this, [this, path]
    {
      const QModelIndex index = m_model->index(path);
      if (index.isValid())
      {
        m_view->setCurrentIndex(index);
        m_view->edit(index);
      }
    });
  });
  connect(menu.addAction(tr("&Add...")), &QAction::triggered, this,
          [this, dir] { addFiles(QFileDialog::getOpenFileNames(this, tr("Add to the project")), dir); });
  menu.exec(m_view->viewport()->mapToGlobal(pos));
}

void BasketWidget::dragEnterEvent(QDragEnterEvent *event)
{
  if (!m_folder.isEmpty() && event->mimeData()->hasUrls())
    event->acceptProposedAction();
}

void BasketWidget::dragMoveEvent(QDragMoveEvent *event)
{
  // the folder it would go into, selected while over it
  const QPoint pos = m_view->viewport()->mapFrom(this, event->position().toPoint());
  const QModelIndex index = m_view->indexAt(pos);
  if (index.isValid() && m_model->isDir(index))
    m_view->setCurrentIndex(index);
  if (event->source() == m_view)
  {
    event->setDropAction(Qt::MoveAction);
    event->accept();
  }
  else
    event->acceptProposedAction();
}

QString BasketWidget::dirAt(const QPoint &pos) const
{
  const QModelIndex index = m_view->indexAt(pos);
  if (!index.isValid())
    return m_folder;
  const QString path = pathAt(index);
  return m_model->isDir(index) ? path : QFileInfo(path).absolutePath();
}

void BasketWidget::dropEvent(QDropEvent *event)
{
  QStringList files;
  for (const QUrl &url : event->mimeData()->urls())
    if (url.isLocalFile())
      files << url.toLocalFile();
  const QString dir = dirAt(m_view->viewport()->mapFrom(this, event->position().toPoint()));
  // from the list itself: moved into the folder; from elsewhere: copied
  if (event->source() == m_view)
  {
    moveInto(files, dir);
    event->setDropAction(Qt::MoveAction);
    event->accept();
    return;
  }
  addFiles(files, dir);
  event->acceptProposedAction();
}
