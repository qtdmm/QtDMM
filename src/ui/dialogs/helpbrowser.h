// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QTextBrowser>
#include <QTextDocument>

// QTextBrowser lays the document out again after every block it inserts
// while parsing Markdown. For a page with a large table (supported-devices.md)
// that adds up to several seconds; parsing into a document that is not laid
// out yet takes a few milliseconds. Qt 6.4 added the switch for that.
class HelpBrowser : public QTextBrowser   // no Q_OBJECT: no signals or slots of its own
{
public:
  using QTextBrowser::QTextBrowser;

protected:
  // setSource(), backward() and forward() all end up here
  void doSetSource(const QUrl &name, QTextDocument::ResourceType type) override
  {
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    document()->setLayoutEnabled(false);
    QTextBrowser::doSetSource(name, type);
    document()->setLayoutEnabled(true);
#else
    QTextBrowser::doSetSource(name, type);
#endif
  }
};
