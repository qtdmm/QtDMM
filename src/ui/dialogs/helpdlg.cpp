// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtGui>
#include <QtWidgets>
#include <QRegularExpression>

#include "ui/dialogs/helpdlg.h"
#include "core/settings.h"

namespace
{
const QString kHelpRoot = QStringLiteral("qrc:/Help/");
const QString kIndexPage = QStringLiteral("index.md");
}

HelpDlg::HelpDlg(Settings *settings, QWidget *parent)
  : QDialog(parent),
    m_cfg(settings)
{
  setupUi(this);
  setWindowFlag(Qt::Window);   // own top-level window, not a modal dialog

  ui_back->setIcon(QIcon::fromTheme("go-previous"));
  ui_forward->setIcon(QIcon::fromTheme("go-next"));
  ui_home->setIcon(QIcon::fromTheme("go-home"));

  ui_back->setEnabled(false);
  ui_forward->setEnabled(false);
  connect(ui_back, &QToolButton::clicked, ui_page, &QTextBrowser::backward);
  connect(ui_forward, &QToolButton::clicked, ui_page, &QTextBrowser::forward);
  connect(ui_page, &QTextBrowser::backwardAvailable, ui_back, &QToolButton::setEnabled);
  connect(ui_page, &QTextBrowser::forwardAvailable, ui_forward, &QToolButton::setEnabled);
  connect(ui_page, &QTextBrowser::sourceChanged, this, &HelpDlg::pageChanged);

  // keyboard: Alt+Left/Right/Home like a browser, Ctrl+F for the search
  // field, F3/Shift+F3 for the next/previous match, Esc closes
  ui_back->setShortcut(QKeySequence("Alt+Left"));
  ui_forward->setShortcut(QKeySequence("Alt+Right"));
  ui_home->setShortcut(QKeySequence("Alt+Home"));
  ui_close->setShortcut(QKeySequence::Cancel);
  auto shortcut = [this](const QKeySequence &key, auto slot)
  {
    QAction *a = new QAction(this);
    a->setShortcut(key);
    connect(a, &QAction::triggered, this, slot);
    addAction(a);
  };
  shortcut(QKeySequence::Find, &HelpDlg::focusSearch);
  shortcut(QKeySequence::FindNext, &HelpDlg::findNext);
  shortcut(QKeySequence::FindPrevious, &HelpDlg::findPrevious);
  shortcut(QKeySequence("Shift+Return"), &HelpDlg::findPrevious);
  connect(ui_search, &QLineEdit::textChanged, this, &HelpDlg::searchChanged);
  connect(ui_search, &QLineEdit::returnPressed, this, &HelpDlg::findNext);

  ui_splitter->setStretchFactor(0, 0);
  ui_splitter->setStretchFactor(1, 1);

  if (m_cfg)
  {
    const int w = m_cfg->getInt("Help/width");
    const int h = m_cfg->getInt("Help/height");
    if (w > 200 && h > 150)
    {
      resize(w, h);
      move(m_cfg->getInt("Help/x"), m_cfg->getInt("Help/y"));
    }
  }

  buildContents();
  showPage(kIndexPage);
}

void HelpDlg::showPage(const QString &page)
{
  // QTextBrowser picks Markdown from the .md extension; relative links inside
  // the page resolve against qrc:/Help/, the same tree MkDocs sees in docs/user.
  ui_page->setSource(QUrl(kHelpRoot + page));
}

// The table of contents is read off index.md itself - the "[Title](page.md)"
// links in it, in order - so the landing page and the side list cannot drift.
void HelpDlg::buildContents()
{
  ui_contents->clear();
  m_contentPages.clear();

  QFile index(":/Help/" + kIndexPage);
  if (!index.open(QIODevice::ReadOnly | QIODevice::Text))
    return;
  const QString text = QString::fromUtf8(index.readAll());

  ui_contents->addItem(tr("Contents"));
  m_contentPages << kIndexPage;

  static const QRegularExpression link(R"(\[([^\]]+)\]\(([A-Za-z0-9_./-]+\.md)\))");
  auto it = link.globalMatch(text);
  while (it.hasNext())
  {
    const QRegularExpressionMatch m = it.next();
    if (m_contentPages.contains(m.captured(2)))
      continue;
    ui_contents->addItem(m.captured(1));
    m_contentPages << m.captured(2);
  }
}

void HelpDlg::on_ui_contents_currentRowChanged(int row)
{
  if (row < 0 || row >= m_contentPages.size())
    return;
  if (ui_page->source().toString() != kHelpRoot + m_contentPages[row])
    showPage(m_contentPages[row]);
}

void HelpDlg::pageChanged(const QUrl &url)
{
  const QString page = url.toString().mid(kHelpRoot.size());
  const int row = m_contentPages.indexOf(page);
  if (row >= 0 && ui_contents->currentRow() != row)
  {
    const QSignalBlocker block(ui_contents);
    ui_contents->setCurrentRow(row);
  }
}

QString HelpDlg::currentPage() const
{
  return ui_page->source().toString().mid(kHelpRoot.size());
}

bool HelpDlg::search(const QString &text)
{
  ui_search->setText(text);   // searchChanged() runs the search
  return ui_page->textCursor().hasSelection();
}

QString HelpDlg::selectedText() const
{
  return ui_page->textCursor().selectedText();
}

void HelpDlg::focusSearch()
{
  ui_search->setFocus();
  ui_search->selectAll();
}

bool HelpDlg::find(QTextDocument::FindFlags flags)
{
  const QString text = ui_search->text();
  if (text.isEmpty())
    return true;
  bool found = ui_page->find(text, flags);
  if (!found)
  {
    // wrap around
    QTextCursor c = ui_page->textCursor();
    c.movePosition(flags & QTextDocument::FindBackward ? QTextCursor::End : QTextCursor::Start);
    ui_page->setTextCursor(c);
    found = ui_page->find(text, flags);
  }
  // red background when there is no match
  ui_search->setStyleSheet(found ? QString() : QStringLiteral("QLineEdit { background: #f8d0d0; }"));
  return found;
}

void HelpDlg::searchChanged(const QString &text)
{
  QTextCursor c = ui_page->textCursor();
  c.movePosition(QTextCursor::Start);
  ui_page->setTextCursor(c);
  if (text.isEmpty())
    ui_search->setStyleSheet(QString());
  else
    find(QTextDocument::FindFlags());
}

void HelpDlg::findNext()
{
  find(QTextDocument::FindFlags());
}

void HelpDlg::findPrevious()
{
  find(QTextDocument::FindBackward);
}

void HelpDlg::on_ui_home_clicked()
{
  showPage(kIndexPage);
}

void HelpDlg::on_ui_close_clicked()
{
  close();
}

void HelpDlg::saveGeometry()
{
  if (!m_cfg)
    return;
  m_cfg->setInt("Help/x", x());
  m_cfg->setInt("Help/y", y());
  m_cfg->setInt("Help/width", width());
  m_cfg->setInt("Help/height", height());
  m_cfg->save();
}

void HelpDlg::closeEvent(QCloseEvent *ev)
{
  saveGeometry();
  QDialog::closeEvent(ev);
}
