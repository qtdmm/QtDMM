//======================================================================
// File:		graphwidget_export.cpp (from dmmgraph.cpp)
// Author:	Matthias Toussaint
// Created:	Tue Apr 10 17:45:35 CEST 2001
//----------------------------------------------------------------------
// This file is part of QtDMM.
//
// QtDMM is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// QtDMM is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with QtDMM.  If not, see <http://www.gnu.org/licenses/>.
//----------------------------------------------------------------------
// Copyright (c) 2001 Matthias Toussaint
//======================================================================
//
// GraphWidget, part: printing, CSV export and import, and the graph as an image.
// The class is in graphwidget.h, its other parts in graphwidget*.cpp.

#include <QtGui>
#include <QtWidgets>
#include <QSvgGenerator>
#include <QPdfWriter>

#include "ui/views/graphwidget.h"
#include "core/settings.h"
#include "recording/recordingfile.h"

void GraphWidget::print(QPrinter *prt, const QString &title, const QString &comment)
{
  if (!title.isEmpty())
    prt->setDocName(title);
  else
    prt->setDocName(tr("QtDMM: %1").arg(QDateTime::currentDateTime().toString()));

  prt->setCreator("QtDMM: (c) 2001 Matthias Toussaint");
  prt->setPrintProgram("QtDMM: (c) 2001 Matthias Toussaint");

  QPainter p;
  p.begin(prt);

  int w = prt->width();
  int h = prt->height();

  p.setFont(QFont("Helvetica", 16));

  QRect tRect = p.boundingRect(0, 0, w, h, Qt::AlignTop | Qt::AlignHCenter | Qt::TextWordWrap, title);

  p.drawText(tRect, Qt::AlignTop | Qt::AlignHCenter | Qt::TextWordWrap, title);

  p.setFont(QFont("Helvetica", 10));

  QFontMetrics fm = p.fontMetrics();
  int maxWidth = qMax(fm.horizontalAdvance(tr("Sampling start:")),
                      fm.horizontalAdvance(tr("Sampling resolution:")));
  int tHeight = fm.height();

  p.drawText(0, tRect.height() + 10, maxWidth, tHeight, Qt::AlignLeft | Qt::AlignVCenter,
             tr("Sampling start:"));
  p.drawText(maxWidth + 10, tRect.height() + 10, w - maxWidth - 10, tHeight, Qt::AlignLeft | Qt::AlignVCenter,
             m_store->startDateTime().toString());

  p.drawText(0, tRect.height() + 10 + tHeight, maxWidth, tHeight, Qt::AlignLeft | Qt::AlignVCenter,
             tr("Sampling resolution:"));
  p.drawText(maxWidth + 10, tRect.height() + 10 + tHeight,
             w - maxWidth - 10, tHeight, Qt::AlignLeft | Qt::AlignVCenter,
             tr("%1 Seconds").arg(m_store->sampleTime() / 10.0));

  QRect cRect = p.boundingRect(0, 0, w, h, Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, comment);

  p.drawText(0, tRect.height() + 20 + 2 * tHeight, w, cRect.height(),
             Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, comment);

  h -= tRect.height() + 30 + 2 * tHeight + cRect.height();

  QRectF chartRect(0, tRect.height() + 30 + 2 * tHeight + cRect.height(), w, h);
  m_chartView->render(&p, chartRect);

  p.end();
}

bool GraphWidget::exportDataSLOT()
{
  // the export goes by the graph's meter: without its readings nothing to write
  if (m_store->count() <= 0)
  {
    Q_EMIT error(tr("Nothing to export: the graph's meter has no readings; show a value of the meter that recorded"));
    return false;
  }
  bool raw = false;
  const QString fn = askExportFile(&raw);
  return !fn.isEmpty() && exportCsvFile(fn, raw);
}

QString GraphWidget::askExportFile(bool *raw)
{
  QFileInfo fileInfo(m_cfg->getString("QtDMM/LastUsesPath"));
  QStringList validSuffixes = { "csv", "xlsx", "ods" };
  QString fnSuffix = validSuffixes.contains(fileInfo.suffix()) ? fileInfo.suffix() : "csv";
  QString fn = fileInfo.baseName().isEmpty() ? "untitled." + fnSuffix : fileInfo.absolutePath() + "/untitled." + fnSuffix;
  const QString csvFilter = tr("CSV (*.csv)"), xlsxFilter = tr("Excel (*.xlsx)"), odsFilter = tr("OpenDocument (*.ods)");
  // every reading at its time instead of the sample time's grid
  const QString rawFilter = tr("CSV, every reading (*.csv)");
  QString filter = fnSuffix == "xlsx" ? xlsxFilter : fnSuffix == "ods" ? odsFilter : csvFilter;
  fn = QFileDialog::getSaveFileName(this, tr("Export data"), fn,
                                    csvFilter + ";;" + xlsxFilter + ";;" + odsFilter + ";;" + rawFilter, &filter);

  if (fn.isNull())
    return QString();
  // the chosen filter decides when no suffix was typed
  if (QFileInfo(fn).suffix().isEmpty())
    fn += filter == xlsxFilter ? ".xlsx" : filter == odsFilter ? ".ods" : ".csv";
  *raw = filter == rawFilter;
  m_cfg->setString("QtDMM/LastUsesPath", QDir().absoluteFilePath(fn));
  return fn;
}

bool GraphWidget::exportCsvFile(const QString &fileName, bool raw)
{
  if (m_store->count() <= 0)
    return false;

  m_cfg->setString("QtDMM/LastUsesPath", QDir().absoluteFilePath(fileName));

  // the other values the graph shows (right-click menu, Values), a column
  // group each
  QList<int> extras;
  for (int i = 0; i < m_store->extraCount(); ++i)
    if (m_shownValues.contains(m_store->extra(i).id))
      extras << i;
  // and the values of other meters it shows, a column group each
  QList<RecordingStore::Foreign> foreign;
  for (const OtherCurve &c : m_otherCurves)
    if (c.store)
      foreign << RecordingStore::Foreign{ c.store, c.value.id, otherName(c.value.source) };
  QString err;
  // every column after its meter, this one's too
  const QString device = !m_sourceFile.isEmpty() ? m_sourceDevice
                         : m_baseSource.isEmpty() ? QString() : otherName(m_baseSource);
  if (!m_store->write(fileName, &err, raw, extras, foreign, device))
  {
    Q_EMIT error(err);
    return false;
  }
  return true;
}

void GraphWidget::importDataSLOT()
{
  // unsaved data: InstanceWidget::importSLOT() asks first
  QString fn = QFileDialog::getOpenFileName(this, tr("Import data"), m_cfg->getString("QtDMM/LastUsesPath", tr("CSV (*.csv);;All files (*)")));

  if (!fn.isNull())
    importCsvFile(fn);
}

bool GraphWidget::importCsvFile(const QString &fileName)
{
  m_cfg->setString("QtDMM/LastUsesPath", QDir().absoluteFilePath(fileName));

  QString err;
  const std::optional<Recording> rec = RecordingFile::read(fileName, &err);
  if (!rec)
  {
    Q_EMIT error(err);
    return false;
  }

  setUnit(rec->unit);
  m_store->setSampleTime(rec->sampleTimeTenths);
  const int cnt = int(rec->values.size());
  if (cnt > 1)
    Q_EMIT sampleTime(m_store->sampleTime());

  m_scaleMin =  1e40;
  m_scaleMax = -1e40;

  m_sourceFile = QFileInfo(fileName).fileName();
  m_sourceDevice = rec->device;
  // this meter's columns into the store, another meter's into one of its own
  m_store->load(RecordingFile::devicePart(*rec, rec->device));
  // all its values shown, so a new export writes them again
  for (int i = 0; i < m_store->extraCount(); ++i)
    setValueShown(m_store->extra(i).id, true);
  setFileStores(*rec);
  Q_EMIT valuesChanged();
  updateStateLabel();

  setScale(true, true, 0, 0);
  // the graph shows the import by itself
  rebuildSeries();
  updateScrollRange();
  scrollbar->setValue(0);
  updateXAxisRange();
  updateTimeButtons();

  Q_EMIT error(fileName);
  update();
  return true;
}

void GraphWidget::copyImageSLOT()
{
  QGuiApplication::clipboard()->setPixmap(m_chartView->grab());
}

bool GraphWidget::exportImageSLOT()
{
  QFileInfo fileInfo(m_cfg->getString("QtDMM/LastImagePath"));
  const QStringList suffixes = { "svg", "pdf", "png", "jpg" };
  const QString suffix = suffixes.contains(fileInfo.suffix().toLower()) ? fileInfo.suffix().toLower() : "svg";
  const QString name = fileInfo.baseName().isEmpty() ? QString("untitled")
                                                     : fileInfo.absolutePath() + "/" + fileInfo.baseName();
  const QString svgFilter = tr("Scalable vector graphics (*.svg)"), pdfFilter = tr("PDF (*.pdf)"),
                pngFilter = tr("PNG image (*.png)"), jpgFilter = tr("JPEG image (*.jpg)");
  QString filter = suffix == "pdf" ? pdfFilter : suffix == "png" ? pngFilter
                 : suffix == "jpg" ? jpgFilter : svgFilter;
  QString fn = QFileDialog::getSaveFileName(this, tr("Export image"), name + "." + suffix,
                                            svgFilter + ";;" + pdfFilter + ";;" + pngFilter + ";;" + jpgFilter,
                                            &filter);
  if (fn.isNull())
    return false;
  // the dialog leaves the name alone when the user typed one: follow the
  // chosen filter, so picking "PDF" and typing "plot" does write a PDF
  const QString chosen = filter == pdfFilter ? "pdf" : filter == pngFilter ? "png"
                       : filter == jpgFilter ? "jpg" : "svg";
  if (!suffixes.contains(QFileInfo(fn).suffix().toLower()))
    fn += "." + chosen;

  m_cfg->setString("QtDMM/LastImagePath", QDir().absoluteFilePath(fn));
  return exportImageFile(fn);
}

bool GraphWidget::exportImageFile(const QString &fileName, QSize size)
{
  // the crosshair follows the mouse and would be baked into the picture at
  // whatever point the user right-clicked
  const bool crosshairShown = m_crosshairVLine->isVisible() || m_crosshairHLine->isVisible();
  if (crosshairShown)
  {
    m_crosshairVLine->hide();
    m_crosshairHLine->hide();
  }
  const bool ok = writeImage(fileName, size);
  if (crosshairShown)
  {
    m_crosshairVLine->show();
    m_crosshairHLine->show();
  }
  return ok;
}

bool GraphWidget::writeImage(const QString &fileName, QSize size)
{
  if (size.isEmpty())
    size = m_chartView->size();
  if (size.isEmpty())
    size = QSize(1024, 640);
  const QString suffix = QFileInfo(fileName).suffix().toLower();

  if (suffix == "svg")
  {
    QSvgGenerator svg;
    svg.setFileName(fileName);
    svg.setSize(size);
    svg.setViewBox(QRect(QPoint(0, 0), size));
    svg.setTitle(m_store->startDateTime().isValid()
                 ? tr("QtDMM recording, %1").arg(m_store->startDateTime().toString(Qt::ISODate))
                 : tr("QtDMM graph"));
    svg.setDescription(tr("%1 readings, unit %2").arg(m_store->count()).arg(m_store->unit()));
    QPainter p;
    if (!p.begin(&svg))
    {
      Q_EMIT error(tr("Could not write %1").arg(fileName));
      return false;
    }
    // into the requested size, as PDF and PNG: at the view's own size the
    // drawing sat in a corner of a larger viewBox
    m_chartView->render(&p, QRectF(QPointF(0, 0), QSizeF(size)));
    p.end();
  }
  else if (suffix == "pdf")
  {
    QPdfWriter pdf(fileName);
    pdf.setTitle(tr("QtDMM graph"));
    pdf.setCreator("QtDMM");
    // the page takes the graph's proportions, so nothing is stretched
    pdf.setPageSize(QPageSize(QSizeF(size.width(), size.height()), QPageSize::Point,
                              QString(), QPageSize::ExactMatch));
    pdf.setPageMargins(QMarginsF(0, 0, 0, 0));
    QPainter p;
    if (!p.begin(&pdf))
    {
      Q_EMIT error(tr("Could not write %1").arg(fileName));
      return false;
    }
    m_chartView->render(&p, QRectF(QPointF(0, 0), QSizeF(pdf.width(), pdf.height())));
    p.end();
  }
  else
  {
    QPixmap pixmap(size);
    pixmap.fill(Qt::white);
    QPainter p(&pixmap);
    m_chartView->render(&p, QRectF(QPointF(0, 0), QSizeF(size)));
    p.end();
    if (!pixmap.save(fileName))
    {
      Q_EMIT error(tr("Could not write %1").arg(fileName));
      return false;
    }
  }
  Q_EMIT info(tr("Graph written to %1").arg(QFileInfo(fileName).fileName()));
  return true;
}
