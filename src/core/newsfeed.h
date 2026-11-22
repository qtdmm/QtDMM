// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDateTime>
#include <QObject>
#include <QUrl>

class QNetworkAccessManager;

/// One entry of the news on qtdmm.de.
struct NewsItem
{
  QString guid;
  QString title;
  QUrl link;
  QDateTime date;
  /// A release's news (its link has "-release-" in it, as the website
  /// names them: news.html#2026-11-22-release-26.4).
  bool isRelease() const { return link.toString().contains("-release-"); }
  /// Whether it is the news of release @p version.
  bool isReleaseOf(const QString &version) const { return link.toString().endsWith("-release-" + version); }
};

/// The news feed of qtdmm.de (RSS 2.0, https://qtdmm.de/feed.xml), fetched
/// with the update check under the same consent (UpdateBar). The URL can be
/// set with QTDMM_NEWS_URL (tests: a file:// URL).
class NewsFeed : public QObject
{
  Q_OBJECT
public:
  explicit NewsFeed(QObject *parent = nullptr);

  /// The entries of @p rss, newest first; empty for what is no RSS.
  static QList<NewsItem> parse(const QByteArray &rss);
  /// The entries newer than the one with guid @p seen, newest first (all
  /// of them when @p seen is not among them any more).
  static QList<NewsItem> unseen(const QList<NewsItem> &items, const QString &seen);
  /// The feed's address: QTDMM_NEWS_URL, else qtdmm.de's.
  static QUrl feedUrl();

  /// Fetches the feed; ends in fetched() or failed().
  void fetch();

Q_SIGNALS:
  void fetched(const QList<NewsItem> &items);
  void failed(const QString &error);

private:
  QNetworkAccessManager *m_net = nullptr;
};
