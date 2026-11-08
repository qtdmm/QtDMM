// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QStringList>
#include <QWidget>

class QFileSystemModel;
class QModelIndex;
class QLabel;
class QStackedWidget;
class QTreeView;

/// The project basket: the files that belong to a bench (recordings,
/// pictures, PDFs, LabPlot projects, anything), as a window.
///
/// The basket is a plain folder next to the bench file, named after it
/// (`Power supply.qtdmm-workspace` and `Power supply/`), so the two move
/// together and the folder can be used in a file manager as well. The
/// window shows what is in it. Without a saved bench there is no basket:
/// the window offers to save one. The folder is made when the first file
/// goes in. Files come in as copies, never moved, never overwritten.
class BasketWidget : public QWidget
{
  Q_OBJECT

public:
  explicit BasketWidget(QWidget *parent = nullptr);

  /// The basket of the bench saved as @p benchFile; empty: no bench, no basket.
  void        setBench(const QString &benchFile);
  /// The folder shown, empty without a bench.
  QString     folder() const { return m_folder; }
  /// Copies @p files into the basket (makes the folder first) under names
  /// not taken yet; returns the paths they got. Not a file, or not
  /// readable: left out, an error() names it.
  QStringList addFiles(const QStringList &files);

  /// The basket folder of @p benchFile: beside it, its name without suffix.
  static QString folderFor(const QString &benchFile);
  /// @p name in @p dir if free, else "name (2).ext", "name (3).ext" ...
  static QString freeName(const QString &dir, const QString &name);

Q_SIGNALS:
  /// No bench yet: the user wants to save one.
  void        saveBenchRequested();
  /// A file to open (double click, Open): the receiver loads a QtDMM
  /// recording into the graph, anything else goes to openWithSystem().
  void        openRequested(const QString &path);
  void        error(const QString &text);

public Q_SLOTS:
  void        addSLOT();
  /// Opens @p path with the program the system has for it.
  void        openWithSystem(const QString &path);

protected:
  void        dragEnterEvent(QDragEnterEvent *event) override;
  void        dropEvent(QDropEvent *event) override;

private:
  void        showFolder();
  void        contextMenu(const QPoint &pos);
  QString     pathAt(const QModelIndex &index) const;
  void        updateEmpty();

  QString           m_folder;
  QStackedWidget   *m_pages;
  QLabel           *m_noBench;
  QLabel           *m_title;
  QLabel           *m_empty;
  QTreeView        *m_view;
  QFileSystemModel *m_model;
};
