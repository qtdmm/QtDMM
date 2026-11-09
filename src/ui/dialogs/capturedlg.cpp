// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/dialogs/capturedlg.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollBar>
#include <QStandardPaths>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "core/projectfolder.h"
#include "core/settings.h"
#include "device/meterconnection.h"
#include "device/protocols.h"

namespace
{
const char *kFolderKey = "Capture/folder";

QTreeWidgetItem *frameItem(const CaptureLog::Frame &frame)
{
  auto *item = new QTreeWidgetItem;
  item->setText(0, QDateTime::fromMSecsSinceEpoch(frame.wall).toString("HH:mm:ss.zzz"));
  item->setText(1, QString::fromLatin1(frame.bytes.toHex(' ').toUpper()));
  const QString read = CaptureLog::readingText(frame);
  if (read.isEmpty())
  {
    item->setText(2, CaptureDlg::tr("not understood"));
    QFont italic = item->font(2);
    italic.setItalic(true);
    item->setFont(2, italic);
  }
  else
    item->setText(2, read);
  item->setToolTip(1, item->text(1));
  return item;
}

QTreeWidget *table(const QStringList &columns, QWidget *parent)
{
  auto *t = new QTreeWidget(parent);
  t->setRootIsDecorated(false);
  t->setUniformRowHeights(true);
  t->setHeaderLabels(columns);
  t->setSelectionMode(QAbstractItemView::SingleSelection);
  return t;
}
}

CaptureDlg::CaptureDlg(MeterConnection *dmm, const QString &name, Settings *settings, QWidget *parent) :
  QDialog(parent, Qt::Window),
  m_dmm(dmm),
  m_settings(settings)
{
  setObjectName("ui_captureDlg");
  setWindowTitle(tr("Capture - %1").arg(name));
  resize(900, 560);

  auto *top = new QVBoxLayout(this);
  const DmmDecoder::DMMInfo &info = dmm->dmmInfo();
  const ProtocolInfo *protocol = protocolInfo(dmm->format());
  auto *head = new QLabel(QString("%1 %2 · %3 · %4")
                            .arg(info.vendor, info.model,
                                 protocol ? QCoreApplication::translate("Protocols", protocol->description) : QString(),
                                 CaptureLog::connectionName(dmm->portType())),
                          this);
  head->setTextInteractionFlags(Qt::TextSelectableByMouse);
  top->addWidget(head);
  auto *hint = new QLabel(tr("Set the meter to what it reads wrong, enter what its display shows and keep the frame. "
                             "The export holds only the frames and what you entered, no port or address."), this);
  hint->setWordWrap(true);
  top->addWidget(hint);

  // the frames as they come
  auto *bar = new QHBoxLayout;
  top->addLayout(bar);
  bar->addWidget(new QLabel(tr("Last frames (up to %1):").arg(CaptureLog::kFrames), this), 1);
  m_pause = new QPushButton(tr("&Pause"), this);
  m_pause->setObjectName("ui_capturePause");
  m_pause->setCheckable(true);
  m_pause->setAutoDefault(false);
  m_pause->setToolTip(tr("Stop adding frames, to keep one of the list"));
  auto *clear = new QPushButton(tr("C&lear"), this);
  clear->setAutoDefault(false);
  bar->addWidget(m_pause);
  bar->addWidget(clear);
  m_frames = table({ tr("Time"), tr("Bytes"), tr("QtDMM reads") }, this);
  m_frames->setObjectName("ui_captureFrames");
  const int em = fontMetrics().averageCharWidth();
  m_frames->setColumnWidth(0, 14 * em);
  m_frames->setColumnWidth(1, 60 * em);
  top->addWidget(m_frames, 3);

  // what the meter showed, with the frame
  auto *keepBar = new QHBoxLayout;
  top->addLayout(keepBar);
  auto *shownLabel = new QLabel(tr("The meter &shows:"), this);
  m_shown = new QLineEdit(this);
  m_shown->setObjectName("ui_captureShown");
  m_shown->setPlaceholderText(tr("as on its display, e.g. -1.234 V DC AUTO HOLD"));
  shownLabel->setBuddy(m_shown);
  m_keep = new QPushButton(tr("&Keep"), this);
  m_keep->setObjectName("ui_captureKeep");
  m_keep->setEnabled(false);
  m_keep->setToolTip(tr("Keeps the frame selected, else the newest, with what the meter shows"));
  keepBar->addWidget(shownLabel);
  keepBar->addWidget(m_shown, 1);
  keepBar->addWidget(m_keep);
  m_kept = table({ tr("The meter shows"), tr("QtDMM reads"), tr("Bytes") }, this);
  m_kept->setObjectName("ui_captureKept");
  m_kept->setColumnWidth(0, 30 * em);
  m_kept->setColumnWidth(1, 30 * em);
  top->addWidget(m_kept, 2);

  auto *bottom = new QHBoxLayout;
  top->addLayout(bottom);
  auto *remove = new QPushButton(tr("&Remove"), this);
  remove->setAutoDefault(false);
  remove->setEnabled(false);
  m_allFrames = new QCheckBox(tr("Export also all &frames of the list"), this);
  m_allFrames->setObjectName("ui_captureAllFrames");
  auto *save = new QPushButton(tr("&Export..."), this);
  save->setObjectName("ui_captureExport");
  save->setAutoDefault(false);
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  bottom->addWidget(remove);
  bottom->addStretch(1);
  bottom->addWidget(m_allFrames);
  bottom->addWidget(save);
  bottom->addWidget(buttons);

  connect(dmm, &MeterConnection::frame, this, &CaptureDlg::addFrame);
  // the device removed: nothing left to capture
  connect(dmm, &QObject::destroyed, this, [this]
  {
    hide();   // no click may reach it before it goes
    deleteLater();
  });
  connect(clear, &QPushButton::clicked, this, [this]
  {
    m_log.clear();
    m_frames->clear();
  });
  connect(m_shown, &QLineEdit::textChanged, this, [this](const QString &text)
  {
    m_keep->setEnabled(!text.trimmed().isEmpty());
  });
  connect(m_shown, &QLineEdit::returnPressed, this, &CaptureDlg::keep);
  connect(m_keep, &QPushButton::clicked, this, &CaptureDlg::keep);
  connect(m_kept, &QTreeWidget::itemSelectionChanged, this, [this, remove]
  {
    remove->setEnabled(!m_kept->selectedItems().isEmpty());
  });
  connect(remove, &QPushButton::clicked, this, [this]
  {
    const int row = m_kept->indexOfTopLevelItem(m_kept->currentItem());
    m_log.removeKept(row);
    delete m_kept->takeTopLevelItem(row);
  });
  connect(save, &QPushButton::clicked, this, &CaptureDlg::exportCapture);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
}

void CaptureDlg::addFrame(const QByteArray &bytes, qint64 wall, const std::optional<DmmDecoder::DmmResponse> &response)
{
  if (m_pause->isChecked())
    return;
  m_log.add(CaptureLog::Frame{ wall, bytes, response });
  QScrollBar *scroll = m_frames->verticalScrollBar();
  const bool atEnd = scroll->value() >= scroll->maximum() - 1;
  m_frames->addTopLevelItem(frameItem(m_log.frames().last()));
  while (m_frames->topLevelItemCount() > m_log.frames().size())
    delete m_frames->takeTopLevelItem(0);
  if (atEnd)
    m_frames->scrollToBottom();
}

void CaptureDlg::keep()
{
  const QString shown = m_shown->text().trimmed();
  if (shown.isEmpty())
    return;
  const QList<QTreeWidgetItem *> selected = m_frames->selectedItems();
  const int row = selected.isEmpty() ? -1 : m_frames->indexOfTopLevelItem(selected.first());
  if (!m_log.keep(shown, row))
    return;
  const CaptureLog::Kept &k = m_log.kept().last();
  const QString read = CaptureLog::readingText(k.frame);
  const QString bytes = QString::fromLatin1(k.frame.bytes.toHex(' ').toUpper());
  auto *item = new QTreeWidgetItem({ k.shown, read.isEmpty() ? tr("not understood") : read, bytes });
  item->setToolTip(2, bytes);
  m_kept->addTopLevelItem(item);
  m_kept->scrollToBottom();
  m_shown->clear();
}

QString CaptureDlg::yaml() const
{
  const DmmDecoder::DMMInfo &info = m_dmm->dmmInfo();
  CaptureLog::Header header;
  header.vendor = info.vendor;
  header.model = info.model;
  header.protocol = m_dmm->format();
  header.connection = m_dmm->portType();
  header.framesPerReading = info.framesPerReading;
  header.version = QCoreApplication::applicationVersion();
  header.date = QDateTime::currentDateTime();
  return m_log.yaml(header, m_allFrames->isChecked());
}

bool CaptureDlg::write(const QString &file, QString *error) const
{
  QSaveFile out(file);
  if (!out.open(QIODevice::WriteOnly) || out.write(yaml().toUtf8()) < 0 || !out.commit())
  {
    if (error)
      *error = out.errorString();
    return false;
  }
  return true;
}

void CaptureDlg::exportCapture()
{
  if (m_log.kept().isEmpty() && !m_allFrames->isChecked()
      && QMessageBox::question(this, tr("Export capture"),
                               tr("No frame is kept with what the meter showed. Export anyway?"))
           != QMessageBox::Yes)
    return;
  const DmmDecoder::DMMInfo &info = m_dmm->dmmInfo();
  const QString name = CaptureLog::fileName(info.vendor, info.model, QDateTime::currentDateTime());
  // the folder chosen last time; else the project's documents, else Documents
  const QString remembered = m_settings ? m_settings->getString(kFolderKey) : QString();
  const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  ProjectFolder::Proposal where(QDir(documents).filePath(name), ProjectFolder::Documents);
  const QString start = QFileInfo(remembered).isDir() ? QDir(remembered).filePath(name) : where.path();
  const QString file = QFileDialog::getSaveFileName(this, tr("Export capture"), start, tr("Capture (*.yaml)"));
  if (file.isEmpty())
    return;
  QString error;
  if (!write(file, &error))
  {
    QMessageBox::warning(this, tr("Export capture"),
                         tr("Could not write %1: %2").arg(QDir::toNativeSeparators(file), error));
    return;
  }
  where.keep(file);
  if (m_settings)
  {
    m_settings->setString(kFolderKey, QFileInfo(file).absolutePath());
    m_settings->save();   // set only stages it, get reads the file
  }
}
