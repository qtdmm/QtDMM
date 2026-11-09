// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/dialogs/exportdlg.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QStandardPaths>
#include <QToolButton>
#include <QVBoxLayout>

#include "core/settings.h"
#include "recording/recordingfile.h"

// the format of a CSV with every reading: csv as well, told apart by this
static const QString kEveryReading = QStringLiteral("csv-raw");

QList<QPair<QString, QString>> ExportDlg::formats(What what)
{
  switch (what)
  {
    case Recording:
      return { { "csv", tr("CSV, on the recording's grid") }, { kEveryReading, tr("CSV, every reading") },
               { "xlsx", tr("Excel (xlsx)") }, { "ods", tr("OpenDocument (ods)") },
               { RecordingFile::kSuffix, tr("QtDMM recording, with the marks") },
               { RecordingFile::kLabPlotProjectSuffix, tr("LabPlot project, with a graph") },
               { RecordingFile::kLabPlotSuffix, tr("CSV for LabPlot, numbers only") } };
    case Table:
      return { { "csv", tr("CSV") }, { "xlsx", tr("Excel (xlsx)") }, { "ods", tr("OpenDocument (ods)") } };
    case Picture:
      return { { "png", tr("PNG image") }, { "svg", tr("SVG vector graphics") }, { "pdf", tr("PDF") },
               { "jpg", tr("JPEG image") } };
  }
  return {};
}

ExportDlg::ExportDlg(What what, const QString &basket, Settings *cfg, QWidget *parent) :
  QDialog(parent),
  m_cfg(cfg)
{
  setWindowTitle(tr("Export"));
  auto *form = new QFormLayout;
  m_what = new QComboBox(this);
  m_what->addItem(tr("The recording"), Recording);
  m_what->addItem(tr("The readings table"), Table);
  m_what->addItem(tr("A picture of the graph"), Picture);
  m_what->setCurrentIndex(m_what->findData(what));
  form->addRow(tr("&What:"), m_what);
  m_format = new QComboBox(this);
  form->addRow(tr("&Format:"), m_format);

  m_toBasket = new QRadioButton(tr("Into the &project"), this);
  m_toBasket->setEnabled(!basket.isEmpty());
  m_toBasket->setToolTip(basket.isEmpty() ? tr("No project is open") : QDir::toNativeSeparators(basket));
  m_toFile = new QRadioButton(tr("To a f&ile:"), this);
  m_file = new QLineEdit(this);
  m_file->setMinimumWidth(fontMetrics().horizontalAdvance('x') * 40);
  auto *browseButton = new QToolButton(this);
  browseButton->setIcon(QIcon::fromTheme("document-open"));
  browseButton->setToolTip(tr("Choose the file"));
  connect(browseButton, &QToolButton::clicked, this, &ExportDlg::browse);
  auto *fileRow = new QHBoxLayout;
  fileRow->addWidget(m_toFile);
  fileRow->addWidget(m_file, 1);
  fileRow->addWidget(browseButton);
  auto *where = new QVBoxLayout;
  where->addWidget(m_toBasket);
  where->addLayout(fileRow);
  form->addRow(tr("Where to:"), where);
  m_labPlot = new QCheckBox(tr("Then open in &LabPlot"), this);
  m_labPlot->setToolTip(tr("LabPlot (labplot.org) opens the recording as a project with a graph, for analysis: fits, "
                           "statistics, plots"));
  form->addRow(QString(), m_labPlot);

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  buttons->button(QDialogButtonBox::Ok)->setText(tr("&Export"));
  connect(buttons, &QDialogButtonBox::accepted, this, &ExportDlg::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  auto *layout = new QVBoxLayout(this);
  layout->addLayout(form);
  layout->addWidget(buttons);

  // the last choice; the view it was opened from decides what
  fillFormats();
  const int last = m_format->findData(m_cfg->getString(QString("Export/format-%1").arg(int(what))));
  if (last >= 0)
    m_format->setCurrentIndex(last);
  const bool basketLast = m_cfg->getBool("Export/to-basket", true);
  (basketLast && !basket.isEmpty() ? m_toBasket : m_toFile)->setChecked(true);
  // a project open: a file of it goes there too
  const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  const QString dir = !basket.isEmpty() ? basket : m_cfg->getString("Export/dir", documents);
  m_file->setText(QDir::toNativeSeparators(QDir(dir).filePath(tr("export"))));
  connect(m_file, &QLineEdit::textEdited, this, [this] { m_toFile->setChecked(true); });
  connect(m_what, &QComboBox::currentIndexChanged, this, &ExportDlg::fillFormats);
  // LabPlot opens its project: the one goes with the other
  m_labPlot->setChecked(what == Recording && m_cfg->getBool("Export/labplot", false)
                        && format() == QLatin1String(RecordingFile::kLabPlotProjectSuffix));
  m_labPlot->setEnabled(what == Recording);
  connect(m_what, &QComboBox::currentIndexChanged, this, [this]
  {
    m_labPlot->setEnabled(this->what() == Recording);
    if (this->what() != Recording)
      m_labPlot->setChecked(false);
  });
  connect(m_labPlot, &QCheckBox::toggled, this, [this](bool on)
  {
    if (on)
      m_format->setCurrentIndex(m_format->findData(QString(RecordingFile::kLabPlotProjectSuffix)));
  });
  connect(m_format, &QComboBox::currentIndexChanged, this, [this]
  {
    if (format() != QLatin1String(RecordingFile::kLabPlotProjectSuffix))
      m_labPlot->setChecked(false);
  });
}

void ExportDlg::fillFormats()
{
  m_format->clear();
  for (const auto &[suffix, text] : formats(what()))
    m_format->addItem(text, suffix);
}

ExportDlg::What ExportDlg::what() const
{
  return static_cast<What>(m_what->currentData().toInt());
}

QString ExportDlg::format() const
{
  const QString f = m_format->currentData().toString();
  return f == kEveryReading ? QStringLiteral("csv") : f;
}

bool ExportDlg::everyReading() const
{
  return m_format->currentData().toString() == kEveryReading;
}

bool ExportDlg::toBasket() const
{
  return m_toBasket->isChecked();
}

QString ExportDlg::file() const
{
  QString path = QDir::fromNativeSeparators(m_file->text().trimmed());
  if (path.isEmpty() || toBasket())
    return {};
  // the format decides the suffix ("x.labplot.csv" has two dots)
  if (!path.endsWith("." + format(), Qt::CaseInsensitive))
    path += "." + format();
  return path;
}

bool ExportDlg::openInLabPlot() const
{
  return m_labPlot->isChecked();
}

void ExportDlg::browse()
{
  const QString path = QFileDialog::getSaveFileName(this, tr("Export"), file().isEmpty() ? m_file->text() : file(),
                                                    QString("%1 (*.%2)").arg(m_format->currentText(), format()));
  if (path.isEmpty())
    return;
  m_file->setText(QDir::toNativeSeparators(path));
  m_toFile->setChecked(true);
}

void ExportDlg::accept()
{
  if (!toBasket() && file().isEmpty())
    return;
  if (!toBasket() && QFileInfo::exists(file())
      && QMessageBox::question(this, tr("Export"), tr("%1 exists already. Replace it?")
                                                     .arg(QDir::toNativeSeparators(file())))
           != QMessageBox::Yes)
    return;
  m_cfg->setString(QString("Export/format-%1").arg(int(what())), m_format->currentData().toString());
  m_cfg->setBool("Export/to-basket", toBasket());
  if (what() == Recording)
    m_cfg->setBool("Export/labplot", openInLabPlot());
  if (!toBasket())
    m_cfg->setString("Export/dir", QFileInfo(file()).absolutePath());
  m_cfg->save();   // set only stages it, get reads the file
  QDialog::accept();
}
