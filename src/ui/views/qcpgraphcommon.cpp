// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views/qcpgraphcommon.h"

#include "ui/views/qcp.h"

#include <QHelpEvent>
#include <QToolTip>

QcpGraphCommon::QcpGraphCommon(QWidget *parent, const QString &noData) : QcpPlotBase(parent)
{
  // no stamp: room for the axes with their labels and titles, the title
  // above, and a plot one can read - in lines of text, so it grows with
  // the font (and the screen's scale)
  const int line = QFontMetrics(font()).height();
  setMinimumSize(qMax(300, 20 * line), qMax(220, 13 * line));

  m_plot->plotLayout()->insertRow(0);
  m_titleElement = new QCPTextElement(m_plot, QString(), font());
  m_plot->plotLayout()->addElement(0, 0, m_titleElement);

  m_noData = new QCPItemText(m_plot);
  m_noData->position->setType(QCPItemPosition::ptAxisRectRatio);
  m_noData->position->setCoords(0.5, 0.5);
  m_noData->setPositionAlignment(Qt::AlignCenter);
  m_noData->setText(noData);

  m_holdItem = new QCPItemText(m_plot);
  m_holdItem->position->setType(QCPItemPosition::ptAxisRectRatio);
  m_holdItem->position->setCoords(0.98, 0.02);
  m_holdItem->setPositionAlignment(Qt::AlignRight | Qt::AlignTop);
  m_holdItem->setText(QStringLiteral("HOLD"));
  m_holdItem->setVisible(false);
}

void QcpGraphCommon::setNoDataText(const QString &text)
{
  m_noData->setText(text);
  m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void QcpGraphCommon::setNoDataVisible(bool visible)
{
  m_noData->setVisible(visible);
  setCrosshairEnabled(!visible);
}

void QcpGraphCommon::setTitle(const QString &title)
{
  setTitles(title, title);
}

void QcpGraphCommon::setTitles(const QString &title, const QString &shortTitle)
{
  if (title == this->title() && shortTitle == m_shortTitle)
    return;
  GraphCommon::setTitle(title);
  m_shortTitle = shortTitle;
  fitTitle();
}

void QcpGraphCommon::fitTitle()
{
  const QFontMetrics fm(m_titleElement->font());
  const int room = width() - fm.averageCharWidth() * 2;
  QString text = title();
  if (fm.horizontalAdvance(text) > room)
    text = fm.elidedText(m_shortTitle, Qt::ElideRight, room);
  if (text != m_titleElement->text())
  {
    m_titleElement->setText(text);
    m_plot->replot(QCustomPlot::rpQueuedReplot);
  }
}

void QcpGraphCommon::resizeEvent(QResizeEvent *event)
{
  QcpPlotBase::resizeEvent(event);
  fitTitle();
}

bool QcpGraphCommon::eventFilter(QObject *watched, QEvent *event)
{
  // the full title over a shortened one
  if (watched == m_plot && event->type() == QEvent::ToolTip && m_titleElement->text() != title()
      && m_titleElement->outerRect().contains(static_cast<QHelpEvent *>(event)->pos()))
  {
    QToolTip::showText(static_cast<QHelpEvent *>(event)->globalPos(), title(), m_plot);
    return true;
  }
  return QcpPlotBase::eventFilter(watched, event);
}

void QcpGraphCommon::setTheme(const PlotTheme &theme)
{
  m_titleElement->setTextColor(theme.labels);
  for (QCPItemText *text : { m_noData, m_holdItem })
    text->setColor(theme.labels);
  QcpPlotBase::setTheme(theme);
}

void QcpGraphCommon::setHold(bool hold)
{
  if (hold == this->hold())
    return;
  cancelRefresh();
  GraphCommon::setHold(hold);
  holdChanged(hold);
  m_holdItem->setVisible(hold);
  refreshPlot();
}

void QcpGraphCommon::clear()
{
  cancelRefresh();
  clearSamples();
  refreshPlot();
}
