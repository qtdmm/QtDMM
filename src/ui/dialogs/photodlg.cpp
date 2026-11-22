// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/dialogs/photodlg.h"

#include "core/photostore.h"

#include <QDialogButtonBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QListWidget>
#include <QMimeData>
#include <QPushButton>
#include <QScreen>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace
{
// the files among @p mime's that are pictures, by suffix: read when taken in
QStringList pictureFiles(const QMimeData *mime)
{
  QStringList files;
  if (!mime || !mime->hasUrls())
    return files;
  const QList<QByteArray> formats = QImageReader::supportedImageFormats();
  for (const QUrl &url : mime->urls())
    if (url.isLocalFile() && formats.contains(QFileInfo(url.toLocalFile()).suffix().toLower().toLatin1()))
      files << url.toLocalFile();
  return files;
}
}   // namespace

PhotoView::PhotoView(QWidget *parent) : QLabel(parent)
{
  setAlignment(Qt::AlignCenter);
  setMinimumSize(160, 120);
  setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
}

void PhotoView::setImage(const QImage &image)
{
  m_image = image;
  rescale();
}

void PhotoView::resizeEvent(QResizeEvent *event)
{
  QLabel::resizeEvent(event);
  rescale();
}

void PhotoView::rescale()
{
  if (m_image.isNull())
    return;   // its text stays ("No photo yet")
  const qreal dpr = devicePixelRatioF();
  QPixmap pixmap = QPixmap::fromImage(m_image.scaled(size() * dpr, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  pixmap.setDevicePixelRatio(dpr);
  setPixmap(pixmap);
}

bool PhotoView::showPhoto(const QString &path, QWidget *parent)
{
  const QImage image = PhotoStore::load(path);
  if (image.isNull())
    return false;
  auto *dlg = new QDialog(parent);
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  dlg->setWindowTitle(QFileInfo(path).fileName());
  auto *layout = new QVBoxLayout(dlg);
  layout->setContentsMargins(0, 0, 0, 0);
  auto *view = new PhotoView(dlg);
  view->setImage(image);
  layout->addWidget(view);
  // as large as the picture, at most most of the screen
  const QScreen *screen = parent ? parent->screen() : QGuiApplication::primaryScreen();
  const QSize room = screen ? screen->availableSize() * 0.8 : QSize(1024, 768);
  dlg->resize(image.size().scaled(room, Qt::KeepAspectRatio).boundedTo(image.size()).expandedTo(QSize(320, 240)));
  dlg->show();
  return true;
}

PhotoDlg::PhotoDlg(const QString &project, QWidget *parent) : QDialog(parent), m_project(project)
{
  setWindowTitle(tr("Photo of the setup"));
  setAcceptDrops(true);
  auto *layout = new QVBoxLayout(this);
  auto *hint = new QLabel(tr("The photos go into the project's folder \"photos\", each with a note in the graph. "
                             "Add a picture from a file or drop it here."),
                          this);
  hint->setWordWrap(true);
  layout->addWidget(hint);
  m_preview = new PhotoView(this);
  m_preview->setObjectName("ui_photoPreview");
  m_preview->setMinimumSize(360, 270);
  m_preview->setText(tr("No photo yet"));
  m_preview->setEnabled(false);
  layout->addWidget(m_preview, 1);
  // the photos of this time, the newest first; a click shows it above
  m_strip = new QListWidget(this);
  m_strip->setObjectName("ui_photoStrip");
  m_strip->setViewMode(QListView::IconMode);
  m_strip->setFlow(QListView::LeftToRight);
  m_strip->setWrapping(false);
  m_strip->setIconSize(QSize(96, 72));
  m_strip->setFixedHeight(110);
  m_strip->setMovement(QListView::Static);
  m_strip->hide();
  layout->addWidget(m_strip);
  connect(m_strip, &QListWidget::currentItemChanged, this, [this](const QListWidgetItem *item)
  {
    if (item)
      m_preview->setImage(PhotoStore::load(item->data(Qt::UserRole).toString()));
  });
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  QPushButton *file = buttons->addButton(tr("Add &file..."), QDialogButtonBox::ActionRole);
  file->setObjectName("ui_photoFile");
  file->setIcon(QIcon::fromTheme("document-open"));
  connect(file, &QPushButton::clicked, this, &PhotoDlg::chooseFiles);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttons);
  resize(560, 520);
}

int PhotoDlg::photoCount() const
{
  return m_strip->count();
}

bool PhotoDlg::addFile(const QString &file)
{
  const QDateTime when = QDateTime::currentDateTime();
  QString why;
  const QString path = PhotoStore::addFile(m_project, file, when, &why);
  if (path.isEmpty())
  {
    Q_EMIT error(why);
    return false;
  }
  present(path, PhotoStore::load(path));
  Q_EMIT photoAdded(path, when);
  return true;
}

void PhotoDlg::chooseFiles()
{
  QStringList patterns;
  for (const QByteArray &format : QImageReader::supportedImageFormats())
    patterns << "*." + QString::fromLatin1(format);
  const QStringList files = QFileDialog::getOpenFileNames(this, tr("Add photos"),
                                                          QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
                                                          tr("Pictures (%1)").arg(patterns.join(' ')));
  for (const QString &f : files)
    addFile(f);
}

void PhotoDlg::present(const QString &path, const QImage &image)
{
  m_preview->setEnabled(true);
  m_preview->setText(QString());
  auto *item = new QListWidgetItem(QIcon(QPixmap::fromImage(image.scaled(m_strip->iconSize(), Qt::KeepAspectRatio,
                                                                         Qt::SmoothTransformation))),
                                   QString());
  item->setData(Qt::UserRole, path);
  item->setToolTip(QFileInfo(path).fileName());
  m_strip->insertItem(0, item);
  m_strip->show();
  m_strip->setCurrentItem(item);   // shows it above
}

void PhotoDlg::dragEnterEvent(QDragEnterEvent *event)
{
  if (!pictureFiles(event->mimeData()).isEmpty())
    event->acceptProposedAction();
}

void PhotoDlg::dropEvent(QDropEvent *event)
{
  const QStringList files = pictureFiles(event->mimeData());
  if (files.isEmpty())
    return;
  event->acceptProposedAction();
  for (const QString &f : files)
    addFile(f);
}
