// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore>
#include "ui_uihelpdlg.h"

class Settings;


/// The handbook: renders the Markdown pages from docs/user/, embedded as
/// resources under :/Help/, in a QTextBrowser. The same files feed the MkDocs
/// site, so there is one source for both.
class HelpDlg : public QDialog, private Ui::UIHelpDlg
{
  Q_OBJECT
public:
  explicit HelpDlg(Settings *settings, QWidget *parent = nullptr);

  /// Shows a page by its path below :/Help/, e.g. "recorder.md".
  void showPage(const QString &page);

  /// Pages listed in the table of contents, as derived from index.md.
  QStringList contentPages() const { return m_contentPages; }
  /// Path of the page shown, e.g. "recorder.md".
  QString currentPage() const;
  /// Ctrl+F search: selects the first match of @p text; false when there is none.
  bool search(const QString &text);
  /// Text currently selected in the page (what search() found).
  QString selectedText() const;

protected:
  void closeEvent(QCloseEvent *) Q_DECL_OVERRIDE;

protected Q_SLOTS:
  void on_ui_home_clicked();
  void on_ui_close_clicked();
  /// Ctrl+F: focus the search field.
  void focusSearch();
  /// Search from the top while typing; false when nothing was found.
  void searchChanged(const QString &text);
  /// Enter/F3 and Shift+Enter/Shift+F3.
  void findNext();
  void findPrevious();
  void on_ui_contents_currentRowChanged(int row);
  void pageChanged(const QUrl &url);

private:
  void buildContents();
  bool find(QTextDocument::FindFlags flags);
  void saveGeometry();

  Settings *m_cfg;
  QStringList m_contentPages;   // page path per list row, parallel to ui_contents
};
