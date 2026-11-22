// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/dialogs/photodlg.h"

#include "core/photostore.h"
#include "service/photoserver.h"
#include "ui/dialogs/photocamera.h"
#include "ui/qrcodeimage.h"

#include <QComboBox>
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
#include <QHBoxLayout>
#include <QScreen>
#include <QStandardPaths>
#include <QTabWidget>
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
  auto *hint = new QLabel(tr("The photos go into the project's folder \"photos\", each with a note in the graph."), this);
  hint->setWordWrap(true);
  layout->addWidget(hint);
  // where they come from: the phone through a QR code, or a file
  m_sources = new QTabWidget(this);
  m_sources->setObjectName("ui_photoSources");
  auto *phone = new QWidget(m_sources);
  auto *phoneLayout = new QHBoxLayout(phone);
  m_qr = new QLabel(phone);
  m_qr->setObjectName("ui_photoQr");
  m_qr->setFixedSize(180, 180);
  m_qr->setAlignment(Qt::AlignCenter);
  m_qr->setScaledContents(true);
  phoneLayout->addWidget(m_qr, 0, Qt::AlignTop);
  auto *phoneText = new QVBoxLayout;
  auto *scan = new QLabel(tr("Scan the code with the phone's camera and open the link. "
                             "Its page takes photos and sends each one here at once."),
                          phone);
  scan->setWordWrap(true);
  phoneText->addWidget(scan);
  auto *netRow = new QHBoxLayout;
  auto *netLabel = new QLabel(tr("&Network:"), phone);
  m_address = new QComboBox(phone);
  m_address->setObjectName("ui_photoAddress");
  m_address->setToolTip(tr("The address of this computer the phone opens: one in the network the phone is in"));
  netLabel->setBuddy(m_address);
  netRow->addWidget(netLabel);
  netRow->addWidget(m_address, 1);
  phoneText->addLayout(netRow);
  m_url = new QLabel(phone);
  m_url->setObjectName("ui_photoUrl");
  m_url->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_url->setWordWrap(true);
  phoneText->addWidget(m_url);
  m_phoneState = new QLabel(phone);
  m_phoneState->setObjectName("ui_photoState");
  m_phoneState->setWordWrap(true);
  phoneText->addWidget(m_phoneState);
  auto *net = new QLabel(tr("Phone and computer must be in the same network; a guest WLAN often keeps them apart. "
                            "A firewall may ask whether QtDMM may accept connections."),
                         phone);
  net->setWordWrap(true);
  net->setEnabled(false);
  phoneText->addWidget(net);
  phoneText->addStretch();
  phoneLayout->addLayout(phoneText, 1);
  m_sources->addTab(phone, QIcon::fromTheme("smartphone"), tr("&Phone"));
  m_phoneTab = phone;
  // a camera of the computer, with Qt Multimedia
  if (PhotoCamera::supported())
  {
    m_camera = new PhotoCamera(m_sources);
    m_sources->addTab(m_camera, QIcon::fromTheme("camera-web"), tr("&Webcam"));
    connect(m_camera, &PhotoCamera::photoTaken, this, [this](const QByteArray &jpeg) { addData(jpeg); });
  }
  auto *fromFile = new QWidget(m_sources);
  auto *fileLayout = new QVBoxLayout(fromFile);
  auto *fileHint = new QLabel(tr("Add a picture from a file, or drop pictures onto this dialog."), fromFile);
  fileHint->setWordWrap(true);
  fileLayout->addWidget(fileHint);
  auto *file = new QPushButton(QIcon::fromTheme("document-open"), tr("&Add file..."), fromFile);
  file->setObjectName("ui_photoFile");
  connect(file, &QPushButton::clicked, this, &PhotoDlg::chooseFiles);
  fileLayout->addWidget(file, 0, Qt::AlignLeft);
  fileLayout->addStretch();
  m_sources->addTab(fromFile, QIcon::fromTheme("document-open"), tr("From &file"));
  layout->addWidget(m_sources);

  // the server for the phone: only while its tab shows
  m_server = new PhotoServer(this);
  for (const PhotoServer::Address &a : PhotoServer::addresses())
    m_address->addItem(QString("%1 (%2)").arg(a.address, a.card), a.address);
  connect(m_address, &QComboBox::currentIndexChanged, this, &PhotoDlg::updatePhone);
  connect(m_sources, &QTabWidget::currentChanged, this, &PhotoDlg::updatePhone);
  connect(m_sources, &QTabWidget::currentChanged, this, &PhotoDlg::updateCamera);
  connect(m_server, &PhotoServer::visited, this, [this] { m_phoneState->setText(tr("The phone is connected.")); });
  connect(m_server, &PhotoServer::received, this, [this](const QByteArray &data) { addData(data); });

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
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttons);
  resize(620, 680);
}

int PhotoDlg::photoCount() const
{
  return m_strip->count();
}

bool PhotoDlg::addFile(const QString &file)
{
  const QDateTime when = QDateTime::currentDateTime();
  QString why;
  return taken(PhotoStore::addFile(m_project, file, when, &why), when, why);
}

bool PhotoDlg::addData(const QByteArray &data)
{
  const QDateTime when = QDateTime::currentDateTime();
  QString why;
  return taken(PhotoStore::save(m_project, data, when, &why), when, why);
}

bool PhotoDlg::taken(const QString &path, const QDateTime &when, const QString &why)
{
  if (path.isEmpty())
  {
    Q_EMIT error(why);
    return false;
  }
  present(path, PhotoStore::load(path));
  Q_EMIT photoAdded(path, when);
  return true;
}

void PhotoDlg::updatePhone()
{
  const bool on = isVisible() && m_sources->currentWidget() == m_phoneTab && m_address->count() > 0;
  if (!on)
  {
    m_server->stop();
    m_qr->clear();
    m_url->clear();
    m_phoneState->setText(m_address->count() == 0 ? tr("No network found: connect the computer to the phone's network.")
                                                   : QString());
    return;
  }
  if (!m_server->start(QHostAddress(m_address->currentData().toString())))
  {
    m_qr->clear();
    m_url->clear();
    m_phoneState->setText(tr("Cannot wait for the phone: %1").arg(m_server->error()));
    return;
  }
  const QString url = m_server->url().toString();
  m_qr->setPixmap(QPixmap::fromImage(qrCodeImage(url)));
  m_url->setText(url);
  m_phoneState->setText(tr("Waiting for the phone..."));
}

void PhotoDlg::updateCamera()
{
  if (m_camera)
    m_camera->setActive(isVisible() && m_sources->currentWidget() == m_camera);
}

void PhotoDlg::showEvent(QShowEvent *event)
{
  QDialog::showEvent(event);
  updatePhone();
  updateCamera();
}

void PhotoDlg::hideEvent(QHideEvent *event)
{
  QDialog::hideEvent(event);
  updatePhone();   // not visible any more: the server stops
  updateCamera();  // and the camera
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
