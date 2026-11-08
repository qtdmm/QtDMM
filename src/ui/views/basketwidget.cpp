// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/basketwidget.h"

#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMimeData>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>

BasketWidget::BasketWidget(QWidget *parent) :
  QWidget(parent)
{
  setWindowTitle(tr("Project basket"));
  setAcceptDrops(true);
  m_pages = new QStackedWidget(this);
  auto *outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->addWidget(m_pages);

  // without a bench: what the basket is and how to get one
  auto *none = new QWidget(m_pages);
  auto *noneLayout = new QVBoxLayout(none);
  m_noBench = new QLabel(tr("The project basket keeps the files of a bench: recordings, pictures, PDFs, "
                            "LabPlot projects. It is a folder next to the bench file.\n\n"
                            "Save the bench first."), none);
  m_noBench->setWordWrap(true);
  m_noBench->setAlignment(Qt::AlignCenter);
  auto *save = new QPushButton(tr("&Save bench..."), none);
  connect(save, &QPushButton::clicked, this, &BasketWidget::saveBenchRequested);
  noneLayout->addStretch();
  noneLayout->addWidget(m_noBench);
  noneLayout->addWidget(save, 0, Qt::AlignCenter);
  noneLayout->addStretch();
  m_pages->addWidget(none);

  // the basket: its folder, a bar, the files
  auto *page = new QWidget(m_pages);
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(4, 4, 4, 4);
  auto *bar = new QHBoxLayout;
  m_title = new QLabel(page);
  m_title->setMinimumWidth(0);
  m_title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  m_title->setTextInteractionFlags(Qt::TextSelectableByMouse);
  // icons only: the window is often a narrow column
  auto *add = new QToolButton(page);
  add->setText(tr("&Add..."));
  add->setIcon(QIcon::fromTheme("list-add", style()->standardIcon(QStyle::SP_FileDialogNewFolder)));
  add->setToolButtonStyle(Qt::ToolButtonIconOnly);
  add->setAutoRaise(true);
  add->setToolTip(tr("Copy files into the basket"));
  connect(add, &QToolButton::clicked, this, &BasketWidget::addSLOT);
  auto *open = new QToolButton(page);
  open->setText(tr("Open &folder"));
  open->setIcon(QIcon::fromTheme("document-open", style()->standardIcon(QStyle::SP_DirOpenIcon)));
  open->setToolButtonStyle(Qt::ToolButtonIconOnly);
  open->setAutoRaise(true);
  open->setToolTip(tr("Show the basket in the file manager"));
  connect(open, &QToolButton::clicked, this, [this]
  {
    QDir().mkpath(m_folder);
    openWithSystem(m_folder);
  });
  bar->addWidget(m_title, 1);
  bar->addWidget(add);
  bar->addWidget(open);
  layout->addLayout(bar);
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

QString BasketWidget::folderFor(const QString &benchFile)
{
  if (benchFile.isEmpty())
    return {};
  const QFileInfo info(benchFile);
  return info.absoluteDir().filePath(info.completeBaseName());
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

void BasketWidget::setBench(const QString &benchFile)
{
  m_folder = folderFor(benchFile);
  showFolder();
}

void BasketWidget::showFolder()
{
  m_pages->setCurrentIndex(m_folder.isEmpty() ? 0 : 1);
  if (m_folder.isEmpty())
    return;
  m_title->setText(QDir::toNativeSeparators(QFileInfo(m_folder).fileName()));
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
    Q_EMIT error(tr("Project basket: cannot make the folder %1").arg(QDir::toNativeSeparators(m_folder)));
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
      Q_EMIT error(tr("Project basket: cannot copy %1").arg(QDir::toNativeSeparators(file)));
    else
      added << target;
  }
  if (!m_view->isVisible())
    showFolder();
  return added;
}

void BasketWidget::addSLOT()
{
  addFiles(QFileDialog::getOpenFileNames(this, tr("Add to the project basket")));
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
          Q_EMIT error(tr("Project basket: cannot move %1 to the trash").arg(QDir::toNativeSeparators(p)));
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
