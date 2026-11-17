// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// QcpGraph: pictures of the graph - a file (SVG, PDF, PNG, JPEG), the
// clipboard, a printed page.

#include "ui/views/qcpgraph.h"

#include "recording/recordingstore.h"
#include "ui/views/graphlegend.h"
#include "ui/views/graphstatestrip.h"
#include "ui/views/graphtimebar.h"
#include "ui/views/qcp.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QPageSize>
#include <QPdfWriter>
#include <QPicture>
#include <QPrinter>
#include <QSvgGenerator>

namespace
{
// a text the graph on QtCharts has translated already
QString word(const char *text)
{
  return QCoreApplication::translate("GraphWidget", text);
}
}

QSize QcpGraph::pictureSize() const
{
  const GraphLegend *legend = m_timeBar->legend();
  const int above = legend->isVisibleTo(m_timeBar) ? legend->height() : 0;
  return QSize(qMax(1, m_plot->width()), qMax(1, above + m_plot->height()));
}

void QcpGraph::paintPicture(QCPPainter *painter, const QRectF &target)
{
  // the crosshair follows the mouse: it does not belong in the picture
  hideCrosshair();
  const QSize natural = pictureSize();
  const GraphLegend *legend = m_timeBar->legend();
  const int above = natural.height() - m_plot->height();
  const double k = qMin(target.width() / natural.width(), target.height() / natural.height());
  painter->save();
  painter->translate(target.topLeft());
  painter->scale(k, k);
  painter->fillRect(QRect(QPoint(0, 0), natural), m_look.theme.background.style() == Qt::NoBrush ? m_plot->palette().base()
                                                                                                 : m_look.theme.background);
  if (above > 0)
    const_cast<GraphLegend *>(legend)->render(painter, QPoint(0, 0), QRegion(), QWidget::DrawChildren);
  painter->translate(0, above);
  // the plot as QCustomPlot draws it, vectors where the device keeps them
  m_plot->toPainter(painter, m_plot->width(), m_plot->height());
  // the states' strip is a widget over the plot
  if (m_stateStrip && m_stateStrip->isVisibleTo(this))
    m_stateStrip->render(painter, m_stateStrip->pos(), QRegion(), QWidget::DrawChildren);
  painter->restore();
}

// The picture recorded once, at the screen's resolution as the widget lays
// it out; played into any device - a file, the clipboard, a page - it keeps
// that layout, scaled, its texts too
QPicture QcpGraph::picture()
{
  QPicture picture;
  QCPPainter p(&picture);
  p.setMode(QCPPainter::pmVectorized);
  p.setMode(QCPPainter::pmNoCaching);
  paintPicture(&p, QRectF(QPointF(0, 0), QSizeF(pictureSize())));
  p.end();
  return picture;
}

// @p picture into @p painter's @p target, its proportions kept
static void play(QPainter &painter, const QPicture &picture, const QSize &size, const QRectF &target)
{
  // drawPicture() already scales from the picture's resolution to the device's
  const double kx = double(picture.logicalDpiX()) / painter.device()->logicalDpiX();
  const double ky = double(picture.logicalDpiY()) / painter.device()->logicalDpiY();
  const double k = qMin(target.width() / size.width(), target.height() / size.height());
  painter.save();
  painter.translate(target.topLeft());
  painter.scale(k * kx, k * ky);
  painter.drawPicture(0, 0, picture);
  painter.restore();
}

QImage QcpGraph::pictureImage()
{
  const QSize size = pictureSize();
  QImage image(size, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::white);
  QPainter p(&image);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::TextAntialiasing);
  play(p, picture(), size, QRectF(QPointF(0, 0), QSizeF(size)));
  return image;
}

bool QcpGraph::exportImageFile(const QString &fileName)
{
  const QSize size = pictureSize();
  const QString suffix = QFileInfo(fileName).suffix().toLower();
  if (suffix == "svg")
  {
    QSvgGenerator svg;
    svg.setFileName(fileName);
    svg.setSize(size);
    svg.setViewBox(QRect(QPoint(0, 0), size));
    svg.setTitle(m_store && m_store->startDateTime().isValid()
                 ? word("QtDMM recording, %1").arg(m_store->startDateTime().toString(Qt::ISODate)) : word("QtDMM graph"));
    QPainter p;
    if (!p.begin(&svg))
      return false;
    play(p, picture(), size, QRectF(QPointF(0, 0), QSizeF(size)));
    return p.end();
  }
  if (suffix == "pdf")
  {
    QPdfWriter pdf(fileName);
    pdf.setTitle(word("QtDMM graph"));
    pdf.setCreator("QtDMM");
    // the page takes the graph's proportions, so nothing is stretched
    pdf.setPageSize(QPageSize(QSizeF(size.width(), size.height()), QPageSize::Point, QString(), QPageSize::ExactMatch));
    pdf.setPageMargins(QMarginsF(0, 0, 0, 0));
    QPainter p;
    if (!p.begin(&pdf))
      return false;
    play(p, picture(), size, QRectF(0, 0, pdf.width(), pdf.height()));
    return p.end();
  }
  return pictureImage().save(fileName);
}

void QcpGraph::copyImage()
{
  QGuiApplication::clipboard()->setImage(pictureImage());
}

// The page of the graph on QtCharts: the title, when the recording started
// and its sample time, the comment, the picture in the rest of the page
void QcpGraph::print(QPrinter *printer, const QString &title, const QString &comment)
{
  printer->setDocName(!title.isEmpty() ? title : word("QtDMM: %1").arg(QDateTime::currentDateTime().toString()));
  printer->setCreator("QtDMM");
  QPainter p;
  if (!p.begin(printer))
    return;
  const int w = printer->width();
  const int h = printer->height();
  p.setFont(QFont("Helvetica", 16));
  const QRect tRect = p.boundingRect(0, 0, w, h, Qt::AlignTop | Qt::AlignHCenter | Qt::TextWordWrap, title);
  p.drawText(tRect, Qt::AlignTop | Qt::AlignHCenter | Qt::TextWordWrap, title);
  p.setFont(QFont("Helvetica", 10));
  const QFontMetrics fm = p.fontMetrics();
  const QString start = word("Sampling start:"), resolution = word("Sampling resolution:");
  const int label = qMax(fm.horizontalAdvance(start), fm.horizontalAdvance(resolution));
  const int line = fm.height();
  const int top = tRect.height() + 10;
  p.drawText(0, top, label, line, Qt::AlignLeft | Qt::AlignVCenter, start);
  p.drawText(label + 10, top, w - label - 10, line, Qt::AlignLeft | Qt::AlignVCenter,
             m_store ? m_store->startDateTime().toString() : QString());
  p.drawText(0, top + line, label, line, Qt::AlignLeft | Qt::AlignVCenter, resolution);
  p.drawText(label + 10, top + line, w - label - 10, line, Qt::AlignLeft | Qt::AlignVCenter,
             word("%1 Seconds").arg(m_store ? m_store->sampleTime() / 10.0 : 0.0));
  const QRect cRect = p.boundingRect(0, 0, w, h, Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, comment);
  p.drawText(0, top + 10 + 2 * line, w, cRect.height(), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, comment);
  const int below = top + 20 + 2 * line + cRect.height();
  play(p, picture(), pictureSize(), QRectF(0, below, w, h - below));
  p.end();
}
