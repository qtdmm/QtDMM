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
#include "device/devicetype.h"
#include "device/meterconnection.h"
#include "device/protocols.h"

namespace
{
const char *kFolderKey = "Capture/folder";

QTreeWidgetItem *frameItem(const CaptureLog::Frame &frame, FrameFormat::DataFormat format)
{
  auto *item = new QTreeWidgetItem;
  item->setText(0, QDateTime::fromMSecsSinceEpoch(frame.wall).toString("HH:mm:ss.zzz"));
  item->setText(1, QString::fromLatin1(frame.bytes.toHex(' ').toUpper()));
  const QString read = CaptureLog::readingText(frame, format);
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
  auto *hint = new QLabel(raw() ? tr("QtDMM does not know the protocol of this meter: for each frame you keep, enter "
                                     "what the meter's display shows. A new decoder is built from these pairs. "
                                     "The export holds only the frames and what you entered, no port or address.")
                                : tr("Set the meter to what it reads wrong, enter what its display shows and keep the frame. "
                                     "The export holds only the frames and what you entered, no port or address."),
                          this);
  hint->setWordWrap(true);
  top->addWidget(hint);

  // the guided steps for the kind of meter
  m_steps = CaptureSteps::forType(DeviceType::of(info));
  if (!m_steps.isEmpty())
  {
    m_guided = new QGroupBox(tr("&Guided steps"), this);
    m_guided->setObjectName("ui_captureGuided");
    m_guided->setCheckable(true);
    m_guided->setToolTip(tr("Off: keep frames by hand, without steps"));
    auto *g = new QVBoxLayout(m_guided);
    m_stepTitle = new QLabel(m_guided);
    QFont bold = m_stepTitle->font();
    bold.setBold(true);
    m_stepTitle->setFont(bold);
    m_stepText = new QLabel(m_guided);
    m_stepText->setObjectName("ui_captureStepText");
    m_stepText->setWordWrap(true);
    m_stepHint = new QLabel(m_guided);
    m_stepHint->setObjectName("ui_captureStepHint");
    m_stepHint->setWordWrap(true);
    // room for two lines of text and one of hint: a longer step does not
    // move the buttons below
    const int line = fontMetrics().lineSpacing();
    m_stepText->setMinimumHeight(2 * line);
    m_stepText->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_stepHint->setMinimumHeight(2 * line);
    m_stepHint->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    auto *stepBar = new QHBoxLayout;
    m_back = new QPushButton(tr("&Back"), m_guided);
    m_skip = new QPushButton(tr("Sk&ip"), m_guided);
    m_skip->setObjectName("ui_captureSkip");
    m_skip->setToolTip(tr("The meter has no such function, or you cannot measure it now"));
    m_sendKey = new QPushButton(m_guided);
    m_sendKey->setObjectName("ui_captureSendKey");
    for (QPushButton *b : { m_back, m_skip, m_sendKey })
      b->setAutoDefault(false);
    stepBar->addWidget(m_stepTitle, 1);
    stepBar->addWidget(m_sendKey);
    stepBar->addWidget(m_back);
    stepBar->addWidget(m_skip);
    g->addLayout(stepBar);
    g->addWidget(m_stepText);
    g->addWidget(m_stepHint);
    top->addWidget(m_guided);
    connect(m_back, &QPushButton::clicked, this, &CaptureDlg::stepBack);
    connect(m_skip, &QPushButton::clicked, this, &CaptureDlg::skipStep);
    connect(m_sendKey, &QPushButton::clicked, this, [this]
    {
      if (m_step < m_steps.size())
        m_dmm->sendKey(m_steps.at(m_step).key);
    });
  }

  // the frames as they come
  auto *bar = new QHBoxLayout;
  top->addLayout(bar);
  bar->addWidget(new QLabel(tr("Last frames (up to %1):").arg(CaptureLog::kFrames), this), 1);
  m_pause = new QPushButton(tr("&Pause"), this);
  m_pause->setObjectName("ui_capturePause");
  m_pause->setCheckable(true);
  m_pause->setAutoDefault(false);
  m_pause->setToolTip(tr("Stop adding frames, to keep one of the list"));
  // pressed it says what a click does then
  connect(m_pause, &QPushButton::toggled, this, [this](bool paused)
  {
    m_pause->setText(paused ? tr("&Continue") : tr("&Pause"));
    m_pause->setToolTip(paused ? tr("Add the frames again") : tr("Stop adding frames, to keep one of the list"));
  });
  auto *clear = new QPushButton(tr("C&lear"), this);
  clear->setAutoDefault(false);
  bar->addWidget(m_pause);
  bar->addWidget(clear);
  m_frames = table({ tr("Time"), tr("Bytes"), tr("QtDMM reads") }, this);
  m_frames->setObjectName("ui_captureFrames");
  const int em = fontMetrics().averageCharWidth();
  m_frames->setColumnWidth(0, 14 * em);
  m_frames->setColumnWidth(1, 60 * em);
  if (raw())
    m_frames->hideColumn(2);   // nothing is read
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
  m_kept = table({ tr("The meter shows"), tr("QtDMM reads"), tr("Step"), tr("Bytes") }, this);
  m_kept->setObjectName("ui_captureKept");
  m_kept->setColumnWidth(0, 30 * em);
  m_kept->setColumnWidth(1, 30 * em);
  m_kept->setColumnWidth(2, 12 * em);
  if (raw())
    m_kept->hideColumn(1);
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
  showStep();
}

bool CaptureDlg::raw() const
{
  return m_dmm->format() == FrameFormat::RawCapture;
}

bool CaptureDlg::guided() const
{
  return m_guided && m_guided->isChecked() && m_step < m_steps.size();
}

void CaptureDlg::showStep()
{
  if (!m_guided)
    return;
  m_back->setEnabled(m_step > 0);
  m_skip->setEnabled(m_step < m_steps.size());
  m_stepHint->clear();
  if (m_step >= m_steps.size())
  {
    m_stepTitle->setText(tr("All steps done"));
    m_stepText->setText(tr("Export the capture with Export... - it holds the frames kept, with what the meter showed."));
    m_sendKey->hide();
    return;
  }
  const CaptureSteps::Step &step = m_steps.at(m_step);
  m_stepTitle->setText(tr("Step %1 of %2").arg(m_step + 1).arg(m_steps.size()));
  m_stepText->setText(CaptureSteps::instruction(step));
  if (raw())
    m_stepHint->setText(tr("Then enter what the meter shows and keep the newest frame."));
  // a key QtDMM can press on the meter itself
  const bool remote = !step.key.isEmpty() && m_dmm->keys().contains(step.key);
  m_sendKey->setVisible(remote);
  if (remote)
  {
    const QString label = QCoreApplication::translate("CaptureSteps", step.keyLabel);
    m_sendKey->setText(tr("Se&nd %1").arg(label));
    m_sendKey->setToolTip(tr("Presses %1 on the meter").arg(label));
  }
}

void CaptureDlg::skipStep()
{
  if (m_step >= m_steps.size())
    return;
  if (!m_skipped.contains(m_steps.at(m_step).id))
    m_skipped << m_steps.at(m_step).id;
  m_log.setSkipped(m_skipped);
  m_proposedFrame.reset();
  // what the meter shows now is not the next step's
  m_watcher.setBaseline(m_watcher.steady() ? m_watcher.steady() : m_watcher.current());
  ++m_step;
  showStep();
}

void CaptureDlg::stepBack()
{
  if (m_step == 0)
    return;
  --m_step;
  m_skipped.removeAll(m_steps.at(m_step).id);
  m_log.setSkipped(m_skipped);
  m_proposedFrame.reset();
  m_watcher.setBaseline(std::nullopt);
  showStep();
}

// a new state held: what QtDMM reads, for the user to confirm or correct
void CaptureDlg::propose()
{
  if (!guided() || m_log.frames().isEmpty())
    return;
  const CaptureSteps::Step &step = m_steps.at(m_step);
  const CaptureSteps::State &state = *m_watcher.steady();
  if (step.expect.isSet() && !CaptureSteps::matches(step.expect, state))
    m_stepHint->setText(tr("QtDMM does not read what this step wants (%1). If the meter shows it, "
                           "keep the frame anyway: that is what the capture is for.")
                          .arg(CaptureSteps::describe(step.expect)));
  else
    m_stepHint->clear();
  m_proposedFrame = m_log.frames().last();
  const QString read = CaptureLog::readingText(*m_proposedFrame, m_dmm->format());
  const QString typed = m_shown->text();
  if (typed.isEmpty() || typed == m_proposed)
  {
    m_shown->setText(read);
    m_proposed = read;
  }
  m_shown->setFocus();
  m_shown->selectAll();
}

void CaptureDlg::addFrame(const QByteArray &bytes, qint64 wall, const std::optional<DmmDecoder::DmmResponse> &response)
{
  if (m_pause->isChecked())
    return;
  m_log.add(CaptureLog::Frame{ wall, bytes, response });
  QScrollBar *scroll = m_frames->verticalScrollBar();
  const bool atEnd = scroll->value() >= scroll->maximum() - 1;
  m_frames->addTopLevelItem(frameItem(m_log.frames().last(), m_dmm->format()));
  while (m_frames->topLevelItemCount() > m_log.frames().size())
    delete m_frames->takeTopLevelItem(0);
  if (atEnd)
    m_frames->scrollToBottom();
  if (m_guided && !raw() && m_watcher.add(CaptureSteps::stateOf(m_log.frames().last(), m_dmm->format()), wall))
    propose();
}

void CaptureDlg::keep()
{
  const QString shown = m_shown->text().trimmed();
  if (shown.isEmpty())
    return;
  const QList<QTreeWidgetItem *> selected = m_frames->selectedItems();
  const int row = selected.isEmpty() ? -1 : m_frames->indexOfTopLevelItem(selected.first());
  // in a step the frame proposed, not a newer one: the value may have moved on
  if (guided() && row < 0 && m_proposedFrame)
    m_log.keep(*m_proposedFrame, shown);
  else if (!m_log.keep(shown, row))
    return;
  if (guided())
  {
    const CaptureSteps::Step &step = m_steps.at(m_step);
    const CaptureSteps::State state = CaptureSteps::stateOf(m_log.kept().last().frame, m_dmm->format());
    m_log.setKeptStep(step.id, !raw() && step.expect.isSet() && !CaptureSteps::matches(step.expect, state));
    // the next step starts from what the meter showed in this one
    m_watcher.setBaseline(state);
    ++m_step;
    showStep();
  }
  const CaptureLog::Kept &k = m_log.kept().last();
  const QString read = CaptureLog::readingText(k.frame, m_dmm->format());
  const QString bytes = QString::fromLatin1(k.frame.bytes.toHex(' ').toUpper());
  auto *item = new QTreeWidgetItem({ k.shown, read.isEmpty() ? tr("not understood") : read, k.step, bytes });
  item->setToolTip(3, bytes);
  m_kept->addTopLevelItem(item);
  m_kept->scrollToBottom();
  m_shown->clear();
  m_proposed.clear();
  m_proposedFrame.reset();
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
