//======================================================================
// File:		configdlg.cpp
// Author:	Matthias Toussaint
// Created:	Sat Oct 19 14:54:29 CEST 2002
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
// Copyright (c) 2002 Matthias Toussaint
// SPDX-License-Identifier: GPL-3.0-or-later
//======================================================================

#include <QtWidgets>
#include <QtPrintSupport>

#include "ui/settings/settingsdialog.h"
#include "ui/settings/settingspageitem.h"
#include "ui/settings/alarmprefs.h"
#include "ui/settings/scpiprefs.h"
#include "ui/settings/keyboardprefs.h"
#include "ui/settings/graphprefs.h"
#include "ui/settings/generalprefs.h"
#include "ui/settings/guiprefs.h"
#include "ui/settings/recorderprefs.h"
#include "core/settings.h"

#include <iostream>

SettingsDialog::SettingsDialog(Settings* settings, QWidget *parent)
  : QDialog(parent)
  , m_settings(settings)
{
  m_buttonBox_OK = false;
  setupUi(this);

  // Ctrl+PgUp/PgDn flip through the pages, like tabs
  auto pageShortcut = [this](const QKeySequence &key, int delta)
  {
    QAction *a = new QAction(this);
    a->setShortcut(key);
    connect(a, &QAction::triggered, this, [this, delta]
    {
      const int n = ui_list->count();
      if (n > 0)
        ui_list->setCurrentRow((ui_list->currentRow() + delta + n) % n);
    });
    addAction(a);
  };
  pageShortcut(QKeySequence("Ctrl+PgUp"), -1);
  pageShortcut(QKeySequence("Ctrl+PgDown"), 1);

  // CREATE PAGES, in the order the list shows them

  m_general = new GeneralPrefs(ui_stack);
  m_gui = new GuiPrefs(ui_stack);
  m_graph = new GraphPrefs(ui_stack);
  m_recorder = new RecorderPrefs(ui_stack);
  m_alarms = new AlarmPrefs(ui_stack);
  m_scpi = new ScpiPrefs(ui_stack);
  m_keyboard = new KeyboardPrefs(ui_stack);
  const QList<QPair<SettingsPage *, PageType>> pages = {
    { m_general, General }, { m_gui, GUI }, { m_graph, Graph },
    { m_recorder, Recorder }, { m_alarms, Alarms }, { m_scpi, Scpi }, { m_keyboard, Keyboard } };
  for (const auto &[p, id] : pages)
  {
    p->setId(id);
    new SettingsPageItem(p->id(), p->icon(), p->label(), ui_list);
    p->setCfg(m_settings);
    addPage(p);
  }

  // the list as wide as its longest name, not more
  ui_list->setIconSize(QSize(22, 22));
  int listWidth = 0;
  for (int i = 0; i < ui_list->count(); ++i)
    listWidth = qMax(listWidth, ui_list->fontMetrics().horizontalAdvance(ui_list->item(i)->text()));
  ui_list->setFixedWidth(listWidth + 22 + 6 * ui_list->fontMetrics().horizontalAdvance('x'));

  // the header: the page's name, one grey line under it
  QFont title = ui_pageTitle->font();
  title.setPointSizeF(title.pointSizeF() * 1.3);
  title.setBold(true);
  ui_pageTitle->setFont(title);

  // init stuff
  //
  on_ui_buttonBox_rejected();
  showPage(General);
  adjustSize();
}

namespace
{
/// The mouse wheel over a combo box or spin box that has no focus scrolls
/// the page instead of changing the value: scrolling down the page must
/// not switch, say, the analog meter to Centre zero on the way.
class WheelGuard : public QObject
{
public:
  WheelGuard(QScrollArea *area) : QObject(area), m_area(area) {}

  bool eventFilter(QObject *watched, QEvent *event) override
  {
    auto *w = qobject_cast<QWidget *>(watched);
    if (event->type() != QEvent::Wheel || !w || w->hasFocus())
      return false;
    QCoreApplication::sendEvent(m_area->verticalScrollBar(), event);
    return true;
  }

private:
  QScrollArea *m_area;
};

/// A page's scroll area asks for the whole page (QScrollArea's own hint
/// stops at about 24 lines): the dialog is as tall as its tallest page,
/// within the screen (adjustSize(), showEvent()), and scrolls only beyond.
class PageScrollArea : public QScrollArea
{
public:
  using QScrollArea::QScrollArea;

  QSize sizeHint() const override
  {
    if (!widget())
      return QScrollArea::sizeHint();
    const int f = 2 * frameWidth();
    return widget()->sizeHint() + QSize(f + verticalScrollBar()->sizeHint().width(), f);
  }
};
}

void SettingsDialog::addPage(SettingsPage *page)
{
  auto *scroll = new PageScrollArea(ui_stack);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setWidget(page);
  ui_stack->insertWidget(page->id(), scroll);

  auto *guard = new WheelGuard(scroll);
  QList<QWidget *> fields;
  for (QComboBox *combo : page->findChildren<QComboBox *>())
    fields << combo;
  for (QAbstractSpinBox *spin : page->findChildren<QAbstractSpinBox *>())
    fields << spin;
  for (QWidget *field : fields)
  {
    field->setFocusPolicy(Qt::StrongFocus);   // the wheel alone does not focus it
    field->installEventFilter(guard);
  }
}

SettingsPage *SettingsDialog::page(int index) const
{
  auto *scroll = qobject_cast<QScrollArea *>(ui_stack->widget(index));
  return scroll ? qobject_cast<SettingsPage *>(scroll->widget()) : nullptr;
}

void SettingsDialog::showEvent(QShowEvent *event)
{
  QDialog::showEvent(event);
  // symbols and the grey of the header after the design changed
  updateIcons();
  if (SettingsPage *p = page(ui_stack->currentIndex()))
    showHeader(p);
  // the pages scroll, so the dialog may be smaller than its content; leave
  // room for the window frame, which frameGeometry() only knows once shown
  const QRect avail = screen()->availableGeometry();
  const QSize frame = frameGeometry().size() - size();
  const QSize max = avail.size() - frame.expandedTo(QSize(0, 40));
  if (width() > max.width() || height() > max.height())
    resize(size().boundedTo(max));
  QRect r = frameGeometry();
  r.moveTop(qBound(avail.top(), r.top(), qMax(avail.top(), avail.bottom() - r.height())));
  r.moveLeft(qBound(avail.left(), r.left(), qMax(avail.left(), avail.right() - r.width())));
  if (r.topLeft() != frameGeometry().topLeft())
    move(r.topLeft());
}

void SettingsDialog::showPage(SettingsDialog::PageType page)
{
  SettingsPage *wid = nullptr;
  for (int entry = 0; entry < ui_list->count(); entry++)
  {
    SettingsPageItem *item = dynamic_cast<SettingsPageItem *>(ui_list->item(entry));
    if (item && item->id() == page)
    {
      ui_list->setCurrentRow(entry);
      ui_stack->setCurrentIndex(page);
      wid = this->page(page);
      break;
    }
  }
  if (wid)
    showHeader(wid);
}

void SettingsDialog::showHeader(SettingsPage *page)
{
  // grey: two thirds of the way from the background to the text, readable
  // in every design
  const QColor text = palette().color(QPalette::WindowText), back = palette().color(QPalette::Window);
  const QColor grey((text.red() * 2 + back.red()) / 3, (text.green() * 2 + back.green()) / 3,
                    (text.blue() * 2 + back.blue()) / 3);
  ui_pageHint->setStyleSheet(QString("color: %1;").arg(grey.name()));
  ui_pageTitle->setText(page->label());
  ui_pageHint->setText(page->description());
}

void SettingsDialog::updateIcons()
{
  // the design may have turned light or dark
  for (int i = 0; i < ui_list->count(); ++i)
    if (auto *item = dynamic_cast<SettingsPageItem *>(ui_list->item(i)))
      item->setIcon(page(item->id())->icon());
}

void SettingsDialog::on_ui_factoryDefaults_clicked()
{
  page(ui_stack->currentIndex())->factoryDefaultsSLOT();
}

void SettingsDialog::zoomInSLOT(double fac)
{
  m_graph->zoomInSLOT(fac);
  Q_EMIT zoomed();
}

void SettingsDialog::zoomOutSLOT(double fac)
{
  m_graph->zoomOutSLOT(fac);
  Q_EMIT zoomed();
}

void SettingsDialog::setSampleTimeSLOT(int sampleTime)
{
  m_recorder->setSampleTimeSLOT(sampleTime);
  on_ui_buttonBox_accepted();
}

void SettingsDialog::setWindowSecondsSLOT(int seconds)
{
  m_graph->setWindowSecondsSLOT(seconds);
  Q_EMIT zoomed();
}

void SettingsDialog::setRecordingLength(int value, int unit)
{
  // kept at once, without applying the other pages
  m_recorder->setLength(value, unit);
  m_settings->setInt("Sample/time", m_recorder->lengthValue());
  m_settings->setInt("Sample/time-unit", m_recorder->lengthUnit());
  m_settings->save();
}

QRect SettingsDialog::winRect() const
{
  QRect rect(m_settings->getInt("Position/x"),
             m_settings->getInt("Position/y"),
             m_settings->getInt("Position/width", 500),
             m_settings->getInt("Position/height", 350));

  return rect;
}

void SettingsDialog::setWinRect(const QRect &rect)
{
  m_winRect = rect;
}

void SettingsDialog::reloadSettings()
{
  m_settings->clear();
  int count = m_settings->getInt("Custom colors/count");
  for (int i = 0; i < count; i++)
    QColorDialog::setCustomColor(i, m_settings->getColor(QString("Custom colors/color_%1").arg(i)));
  for (int i = 0; i < NumItems; ++i)
    page(i)->defaultsSLOT();
}

void SettingsDialog::on_ui_buttonBox_rejected()
{
  reloadSettings();
  hide();
}

void SettingsDialog::on_ui_buttonBox_accepted()
{
  m_settings->setInt("Custom colors/count", QColorDialog::customCount());

  for (int i = 0; i < QColorDialog::customCount(); i++)
    m_settings->setColor(QString("Custom colors/color_%1").arg(i), QColorDialog::customColor(i));
  m_settings->setInt("Position/x", m_winRect.x());
  m_settings->setInt("Position/y", m_winRect.y());
  m_settings->setInt("Position/width", m_winRect.width());
  m_settings->setInt("Position/height", m_winRect.height());

  m_settings->setInt("Printer/page-size", static_cast<int>(m_printer->pageLayout().pageSize().id()));
  m_settings->setInt("Printer/page-orientation", static_cast<int>(m_printer->pageLayout().orientation()));
  m_settings->setInt("Printer/color", static_cast<int>(m_printer->colorMode()));
  m_settings->setString("Printer/name", m_printer->printerName());
  m_settings->setString("Printer/filename", m_printer->outputFileName());
  m_settings->setBool("Printer/print-file", (m_printer->outputFormat() == QPrinter::PdfFormat) ? true : false);

  for (int i = 0; i < NumItems; ++i)
    page(i)->applySLOT();

  m_settings->save();
  reloadSettings();

  if ((sender() == ui_buttonBox) && m_buttonBox_OK)
  {
    // no meter here: a device is chosen in the sidebar or Add device
    // (InstanceWidget::takeOver()), OK on these pages leaves an empty window empty
    Q_EMIT accepted();
    hide();
  }
  else
    Q_EMIT applied();
}

void SettingsDialog::on_ui_buttonBox_clicked(QAbstractButton *button)
{
  m_buttonBox_OK = (ui_buttonBox->buttonRole(button) == QDialogButtonBox::AcceptRole);

  if (ui_buttonBox->buttonRole(button) == QDialogButtonBox::ApplyRole)
    on_ui_buttonBox_accepted();

  if (ui_buttonBox->buttonRole(button) == QDialogButtonBox::RejectRole)
    Q_EMIT rejected();
}

void SettingsDialog::writePrinter(QPrinter *printer)
{
  m_printer = printer;
  on_ui_buttonBox_accepted();
}

void SettingsDialog::readPrinter(QPrinter *printer)
{
  m_printer = printer;

  m_printer->setPageSize(QPageSize(static_cast<QPageSize::PageSizeId>(m_settings->getInt("Printer/page-size"))));
  m_printer->setPageOrientation(static_cast<QPageLayout::Orientation>(m_settings->getInt("Printer/page-orientation")));
  m_printer->setColorMode(static_cast<QPrinter::ColorMode>(m_settings->getInt("Printer/color", 1)));
  m_printer->setPrinterName(m_settings->getString("Printer/name", "lp"));
  m_printer->setOutputFileName(m_settings->getString("Printer/filename"));
  m_printer->setOutputFormat((m_settings->getBool("Printer/print-file")) ? QPrinter::PdfFormat : QPrinter::NativeFormat);
}

void SettingsDialog::on_ui_list_currentItemChanged(QListWidgetItem *current, QListWidgetItem *)
{
  int id = dynamic_cast<SettingsPageItem *>(current)->id();
  SettingsPage *wid = page(id);
  ui_stack->setCurrentIndex(id);
  showHeader(wid);
}

void SettingsDialog::thresholdChangedSLOT(GraphWidget::CursorMode mode, double value)
{
  switch (mode)
  {
    case GraphWidget::Trigger:
      m_recorder->setThreshold(value);
      break;
    case GraphWidget::Integration:
      m_graph->setIntThreshold(value);
      break;
    default:
      std::cerr << "Unexpected CursorMode in configdlg.cpp:418" << std::endl;
      break;
  }
}

/////////////////////////////////////////////////////////////////
// RECORDER
//

/////////////////////////////////////////////////////////////////
// EXECUTE
//
/////////////////////////////////////////////////////////////////
// General, Appearance
//

/////////////////////////////////////////////////////////////////
// SCALE
//

/////////////////////////////////////////////////////////////////
// INTEGRATION
//

/////////////////////////////////////////////////////////////////
// GRAPH
//

