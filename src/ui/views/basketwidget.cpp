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
#include <QMenu>
#include <QMimeData>
#include <QPushButton>
#include <QSaveFile>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QStyle>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>

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
  m_model = new QFileSystemModel(this);
  m_model->setReadOnly(false);   // renaming in place
  m_model->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
  m_view = new QTreeView(page);
  m_view->setModel(m_model);
  m_view->setRootIsDecorated(false);
  m_view->setItemsExpandable(false);
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
  connect(m_model, &QAbstractItemModel::rowsRemoved, this, &BasketWidget::updateEmpty);
  m_pages->addWidget(page);
  showFolder();
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
  const QString dir = QFileDialog::getSaveFileName(this, tr("New project"), m_baseFolder);
  if (dir.isEmpty())
    return;
  if (!QDir().mkpath(dir) || !writeProjectFile(dir))
  {
    Q_EMIT error(tr("Project: cannot make the folder %1").arg(QDir::toNativeSeparators(dir)));
    return;
  }
  setFolder(dir);
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
}

void BasketWidget::openSLOT()
{
  const QString dir = QFileDialog::getExistingDirectory(this, tr("Open project"), m_folder);
  if (!dir.isEmpty())
    setFolder(dir);
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
  m_empty->setVisible(!there || QDir(m_folder).isEmpty());
}

QStringList BasketWidget::addFiles(const QStringList &files)
{
  QStringList added;
  if (m_folder.isEmpty() || files.isEmpty())
    return added;
  if (!QDir().mkpath(m_folder))
  {
    Q_EMIT error(tr("Project: cannot make the folder %1").arg(QDir::toNativeSeparators(m_folder)));
    return added;
  }
  for (const QString &file : files)
  {
    const QFileInfo info(file);
    // a file of the basket itself stays where it is
    if (info.absoluteDir() == QDir(m_folder))
      continue;
    const QString target = freeName(m_folder, info.fileName());
    if (!info.isFile() || !QFile::copy(file, target))
      Q_EMIT error(tr("Project: cannot copy %1").arg(QDir::toNativeSeparators(file)));
    else
      added << target;
  }
  if (!m_view->isVisible())
    showFolder();
  return added;
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
  connect(menu.addAction(tr("&Add...")), &QAction::triggered, this, &BasketWidget::addSLOT);
  menu.exec(m_view->viewport()->mapToGlobal(pos));
}

void BasketWidget::dragEnterEvent(QDragEnterEvent *event)
{
  if (!m_folder.isEmpty() && event->mimeData()->hasUrls())
    event->acceptProposedAction();
}

void BasketWidget::dropEvent(QDropEvent *event)
{
  QStringList files;
  for (const QUrl &url : event->mimeData()->urls())
    if (url.isLocalFile())
      files << url.toLocalFile();
  addFiles(files);
  event->acceptProposedAction();
}
