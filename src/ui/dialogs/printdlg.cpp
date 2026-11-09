//======================================================================
// File:		printdlg.cpp
// Author:	Matthias Toussaint
// Created:	Wed Apr 11 16:54:59 CEST 2001
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


#include <QtGui>
#include <QtWidgets>
#include <QtPrintSupport>

#include "ui/dialogs/printdlg.h"
namespace qtdmm
{
  PrintDlg::PrintDlg(QWidget *parent) : QDialog(parent)
  {
    setupUi(this);
  }

  void PrintDlg::setPrinter(QPrinter *prt)
  {
    m_printer = prt;
    createPrinterString();
  }

  void PrintDlg::on_configBut_clicked()
  {
    static QPrintDialog *dlgprinter = 0;
    if (!dlgprinter)
      dlgprinter = new QPrintDialog(m_printer, this);
    if (dlgprinter->exec() == QDialog::Accepted)
      createPrinterString();
  }

  void PrintDlg::createPrinterString()
  {
    QString txt = m_printer->printerName();

    if (m_printer->outputFormat() == QPrinter::PdfFormat)
      txt = QString("File: %1").arg(m_printer->outputFileName());

    txt.append(" ");
    txt.append(m_printer->pageLayout().pageSize().name());
    txt.append(" ");

    if (m_printer->pageLayout().orientation() == QPageLayout::Landscape)
      txt.append("Landscape");
    else
      txt.append("Portrait");

    if (m_printer->colorMode() == QPrinter::Color)
      txt.append(" Color");
    else
      txt.append(" Grayscale");

    printerLabel->setText(txt);

    printBut->setEnabled(true);
  }

  void PrintDlg::on_helpBut_clicked()
  {
    QWhatsThis::enterWhatsThisMode();
  }
}
