// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

// Checks that the handbook pages under docs/user/ are embedded as resources
// (:/Help/) and that what index.md links to actually exists and renders - so a
// renamed or forgotten page fails the build, not the user pressing F1.
#include <QApplication>
#include <QTextBrowser>
#include <QAbstractTextDocumentLayout>
#include <QFile>
#include <QDir>
#include <QRegularExpression>
#include <QDebug>
#include <QElapsedTimer>

#include "ui/dialogs/helpdlg.h"
#include "testutil.h"

static QString readResource(const QString &path)
{
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    return QString();
  return QString::fromUtf8(f.readAll());
}

// Loads a page the way HelpDlg does and returns its rendered plain text.
static QString renderPage(const QString &page)
{
  QTextBrowser browser;
  browser.setSource(QUrl("qrc:/Help/" + page));
  return browser.toPlainText();
}

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  // 1. the table of contents is embedded and renders to something
  const QString index = readResource(":/Help/index.md");
  check(!index.isEmpty(), "index.md is not embedded under :/Help/");
  check(renderPage("index.md").contains("Contents"),
        "index.md did not render as Markdown (no 'Contents' heading in the text)");

  // 2. every page index.md links to exists and renders
  static const QRegularExpression pageLink(R"(\]\(([A-Za-z0-9_./-]+\.md)\))");
  int pages = 0;
  auto it = pageLink.globalMatch(index);
  while (it.hasNext())
  {
    const QString page = it.next().captured(1);
    pages++;
    check(QFile::exists(":/Help/" + page),
          QString("index.md links to '%1', which is not embedded").arg(page));
    check(renderPage(page).size() > 50,
          QString("'%1' rendered to (almost) nothing").arg(page));
  }
  check(pages >= 3, "index.md should link to at least three pages");

  // 3. every image referenced by any page is embedded
  static const QRegularExpression imageLink(R"(!\[[^\]]*\]\(([^)]+)\))");
  QDir helpDir(":/Help");
  for (const QString &name : helpDir.entryList(QStringList() << "*.md", QDir::Files))
  {
    auto images = imageLink.globalMatch(readResource(":/Help/" + name));
    while (images.hasNext())
    {
      const QString img = images.next().captured(1);
      check(QFile::exists(":/Help/" + img),
            QString("'%1' references image '%2', which is not embedded").arg(name, img));
    }
  }

  // 4. the dialog itself: its table of contents must be exactly the pages
  //    index.md links to (plus the contents entry), and showPage() must land
  //    on the requested page
  {
    QStringList linked;
    auto links = pageLink.globalMatch(index);
    while (links.hasNext())
    {
      const QString page = links.next().captured(1);
      if (!linked.contains(page))
        linked << page;
    }
    linked.prepend("index.md");

    HelpDlg dlg(nullptr);
    check(dlg.contentPages() == linked,
          QString("HelpDlg table of contents %1 differs from the links in index.md %2")
            .arg(dlg.contentPages().join(", "), linked.join(", ")));
    check(dlg.currentPage() == "index.md", "HelpDlg should open on index.md");
    // the device table is the heaviest page; with the layout left enabled
    // while parsing, QTextBrowser lays the document out again after every
    // block and needs seconds for it, not milliseconds. The time depends on
    // the machine (slow on RISC-V, #130), so the check counts layout passes:
    // HelpBrowser parses with the layout switched off, which leaves a handful
    // of size changes instead of one per block.
    dlg.show();
    QTextBrowser *browser = dlg.findChild<QTextBrowser *>("ui_page");
    check(browser != nullptr, "HelpDlg has no ui_page browser");
    int relayouts = 0;
    if (browser)
      QObject::connect(browser->document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged,
                       [&relayouts] { relayouts++; });
    QElapsedTimer timer;
    timer.start();
    dlg.showPage("supported-devices.md");
    const qint64 ms = timer.elapsed();
    check(dlg.currentPage() == "supported-devices.md",
          "showPage(\"supported-devices.md\") did not switch the page");
    check(relayouts < 20, QString("supported-devices.md was laid out %1 times while loading").arg(relayouts));
    qInfo() << "supported-devices.md loaded in" << ms << "ms," << relayouts << "layout passes";

    // Ctrl+F search: first match selected, wraps around, misses reported
    check(dlg.search("UT61E") && dlg.selectedText() == "UT61E",
          "search(\"UT61E\") should select the first match in the device table");
    check(!dlg.search("no-such-meter-xyz"), "search() should report a miss");
  }

  if (failed == 0)
    qInfo() << "All handbook checks passed (" << pages << "pages linked from index.md).";
  return failed == 0 ? 0 : 1;
}
