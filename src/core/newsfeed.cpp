// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/newsfeed.h"

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QXmlStreamReader>

#include <algorithm>

namespace
{
const char *const kFeedUrl = "https://qtdmm.de/feed.xml";
}

NewsFeed::NewsFeed(QObject *parent) : QObject(parent) {}

QList<NewsItem> NewsFeed::parse(const QByteArray &rss)
{
  QList<NewsItem> items;
  QXmlStreamReader xml(rss);
  NewsItem item;
  bool inItem = false;
  while (!xml.atEnd())
  {
    xml.readNext();
    if (xml.isStartElement() && xml.name() == QLatin1String("item"))
    {
      item = NewsItem();
      inItem = true;
    }
    else if (xml.isEndElement() && xml.name() == QLatin1String("item"))
    {
      if (item.guid.isEmpty())
        item.guid = item.link.toString();
      if (!item.guid.isEmpty() && !item.title.isEmpty())
        items.append(item);
      inItem = false;
    }
    else if (inItem && xml.isStartElement())
    {
      const QStringView name = xml.name();
      if (name == QLatin1String("title"))
        item.title = xml.readElementText().simplified();
      else if (name == QLatin1String("link"))
        item.link = QUrl(xml.readElementText().trimmed());
      else if (name == QLatin1String("guid"))
        item.guid = xml.readElementText().trimmed();
      else if (name == QLatin1String("pubDate"))
        item.date = QDateTime::fromString(xml.readElementText().trimmed(), Qt::RFC2822Date);
    }
  }
  if (xml.hasError())
    return {};
  std::stable_sort(items.begin(), items.end(), [](const NewsItem &a, const NewsItem &b) { return a.date > b.date; });
  return items;
}

QList<NewsItem> NewsFeed::unseen(const QList<NewsItem> &items, const QString &seen)
{
  QList<NewsItem> newer;
  for (const NewsItem &item : items)
  {
    if (item.guid == seen)
      return newer;
    newer.append(item);
  }
  return newer;
}

QUrl NewsFeed::feedUrl()
{
  const QString env = qEnvironmentVariable("QTDMM_NEWS_URL");
  return env.isEmpty() ? QUrl(kFeedUrl) : QUrl::fromUserInput(env);
}

void NewsFeed::fetch()
{
  if (!m_net)
    m_net = new QNetworkAccessManager(this);
  QNetworkRequest request(feedUrl());
  request.setHeader(QNetworkRequest::UserAgentHeader, QString("QtDMM/%1").arg(QCoreApplication::applicationVersion()));
  request.setTransferTimeout(15000);
  QNetworkReply *reply = m_net->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError)
      Q_EMIT failed(reply->errorString());
    else
      Q_EMIT fetched(parse(reply->readAll()));
  });
}
