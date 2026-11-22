// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QRadioButton;
class Settings;

/// Export in one dialog: what (the recording, the readings table, a picture
/// of the graph), in which format, and where to (a file or the open project
/// basket), and whether LabPlot opens the recording then. It only asks;
/// MainWindow::exportDialog() writes. The last choice is kept
/// (Export/format-<what>, Export/to-basket, Export/dir, Export/labplot,
/// Export/values). Ctrl+E opens it.
class ExportDlg : public QDialog
{
  Q_OBJECT

public:
  enum What { Recording, Table, Picture };

  /// @p what preselected (the view it was opened from); @p basket the open
  /// project basket, empty: none, then only a file.
  ExportDlg(What what, const QString &basket, Settings *cfg, QWidget *parent = nullptr);

  What        what() const;
  /// The file suffix of the format: csv, xlsx, ods, qtdmm-rec, lml, labplot.csv,
  /// png, svg, pdf, jpg.
  QString     format() const;
  /// The recording with the values of the graph used last only, not all devices'.
  bool        graphValuesOnly() const;
  /// CSV with every reading instead of the recording's grid.
  bool        everyReading() const;
  /// Into the project basket, the name made there; else file().
  bool        toBasket() const;
  /// The file chosen, with the format's suffix.
  QString     file() const;
  /// Open the recording in LabPlot once written (as a LabPlot project).
  bool        openInLabPlot() const;

  void        accept() override;

  /// The formats of @p what: suffix and text, the first is the default.
  static QList<QPair<QString, QString>> formats(What what);

private:
  void        fillFormats();
  void        browse();

  Settings     *m_cfg;
  QComboBox    *m_what;
  QComboBox    *m_format;
  QRadioButton *m_toFile;
  QRadioButton *m_toBasket;
  QLineEdit    *m_file;
  QCheckBox    *m_labPlot;
  QComboBox    *m_values;
};
