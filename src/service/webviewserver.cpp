// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "service/webviewserver.h"

#include <QBuffer>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QTcpSocket>
#include <QTimer>

#include <algorithm>

namespace
{
// a JPEG of a screen: small, the scales still readable
constexpr int kQuality = 80;

// the page loads nothing from elsewhere and gives the token to no one
const QList<QByteArray> kHeaders{
  "X-Content-Type-Options: nosniff",
  "Referrer-Policy: no-referrer",
  "Content-Security-Policy: default-src 'none'; img-src 'self' blob:; connect-src 'self'; "
  "style-src 'unsafe-inline'; script-src 'unsafe-inline'",
};
}   // namespace

WebViewServer::WebViewServer(QObject *parent) : HttpServer(kMaxClients, parent), m_prune(new QTimer(this))
{
  // a browser that stopped asking no longer counts
  m_prune->setInterval(1000);
  connect(m_prune, &QTimer::timeout, this, &WebViewServer::prune);
  m_prune->start();
}

void WebViewServer::setInterval(int ms)
{
  m_interval = std::clamp(ms, kMinInterval, kMaxInterval);
}

QByteArray WebViewServer::page()
{
  QFile file(":/webview/index.html");
  if (!file.open(QIODevice::ReadOnly))
    return {};
  QByteArray html = file.readAll();
  // the words, as the program speaks; HTML-escaped
  const QList<std::pair<const char *, QString>> words = {
    { "{{LANG}}", tr("en", "the language code (BCP 47) of this translation, for the web view's page") },
    { "{{TITLE}}", tr("QtDMM web view") },
    { "{{FULLSCREEN}}", tr("Full screen") },
    { "{{STALE}}", tr("No new picture") },
    { "{{GONE}}", tr("This link no longer works: ask for the new one.") },
  };
  for (const auto &[key, text] : words)
    html.replace(key, text.toHtmlEscaped().toUtf8());
  return html;
}

QByteArray WebViewServer::forbidden() const
{
  return tr("This link no longer works: ask for the new one.").toUtf8();
}

QByteArray WebViewServer::frame()
{
  // one picture for all browsers per interval (a little early is fine:
  // their requests do not come at the same moment)
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  if (m_frame.isEmpty() || now - m_frameTime >= m_interval * 8 / 10)
  {
    const QImage image = m_frameSource ? m_frameSource() : QImage();
    if (image.isNull())
      return {};
    QByteArray jpeg;
    QBuffer buffer(&jpeg);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "JPG", kQuality))
      return {};
    m_frame = jpeg;
    m_frameTime = now;
  }
  return m_frame;
}

void WebViewServer::seen(const QHostAddress &peer)
{
  const int before = viewers();
  m_viewers.insert(peer.toString(), QDateTime::currentMSecsSinceEpoch());
  if (viewers() != before)
    Q_EMIT viewersChanged(viewers());
}

void WebViewServer::prune()
{
  // gone after three missed pictures, at least 5 s
  const qint64 oldest = QDateTime::currentMSecsSinceEpoch() - std::max(3 * m_interval, 5000);
  const int before = viewers();
  m_viewers.removeIf([oldest](const QHash<QString, qint64>::iterator it) { return it.value() < oldest; });
  if (!isRunning())
    m_viewers.clear();
  if (viewers() != before)
    Q_EMIT viewersChanged(viewers());
}

void WebViewServer::handle(QTcpSocket *socket, const Request &request)
{
  if (request.method != "GET" && request.method != "HEAD")
  {
    answer(socket, 405, "text/plain", "Only to look at");
    return;
  }
  if (request.path.isEmpty())
    answer(socket, 200, "text/html; charset=utf-8", page(), kHeaders);
  else if (request.path == "frame.jpg")
  {
    const QByteArray jpeg = frame();
    if (jpeg.isEmpty())
    {
      answer(socket, 503, "text/plain", "No picture");
      return;
    }
    seen(request.peer);
    answer(socket, 200, "image/jpeg", jpeg, kHeaders + QList<QByteArray>{ "X-Frame-Time: " + QByteArray::number(m_frameTime) });
  }
  else if (request.path == "status.json")
  {
    QJsonObject status = m_statusSource ? m_statusSource() : QJsonObject();
    status.insert("interval", m_interval);
    answer(socket, 200, "application/json", QJsonDocument(status).toJson(QJsonDocument::Compact), kHeaders);
  }
  else
    answer(socket, 404, "text/plain", "Not found");
}
