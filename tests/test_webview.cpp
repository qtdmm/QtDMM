// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// WebViewServer: the token (403 without it, a new one ends the old link),
// the page, the picture made once per interval for all browsers, its time,
// the status, what is refused, the viewers.

#include <QCoreApplication>
#include <QGuiApplication>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>

#include "service/webviewserver.h"
#include "httprequest.h"
#include "testutil.h"

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QGuiApplication app(argc, argv);

  WebViewServer server;
  check(!server.isRunning() && server.url().isEmpty() && server.port() == 0, "not running: no link");
  const QString token = HttpServer::newToken();
  check(token.size() == 32 && token != HttpServer::newToken(), "a token: 32 hex digits, new each time");
  check(server.start(QHostAddress::LocalHost, 0, token), "listens: " + server.error());
  const quint16 port = server.port();
  check(server.url().toString() == QString("http://127.0.0.1:%1/%2/").arg(port).arg(token)
          && server.url("raspi").host() == "raspi",
        "the link: " + server.url().toString());
  const QByteArray base = "/" + token.toLatin1() + "/";
  auto get = [&](const QByteArray &path) { return request(port, "GET " + path + " HTTP/1.1\r\nHost: x\r\n"); };

  // the token
  Reply r = get("/");
  check(r.status == 403 && r.body.contains("no longer works"), "no token: 403");
  r = get("/" + HttpServer::newToken().toLatin1() + "/frame.jpg");
  check(r.status == 403, "a wrong token: 403");

  // the page
  r = get(base);
  check(r.status == 200 && r.body.contains("<title>QtDMM web view</title>") && !r.body.contains("{{")
          && r.header("content-security-policy").contains("default-src 'none'")
          && r.header("referrer-policy") == "no-referrer",
        "the page, its words filled in, nothing from elsewhere");
  check(get(base + "?kiosk").status == 200, "the page with a query");

  // no picture yet
  check(get(base + "frame.jpg").status == 503 && server.viewers() == 0, "no source: 503, no viewer");

  // the picture: once per interval, whoever asks
  int grabs = 0;
  server.setFrameSource([&grabs] {
    ++grabs;
    QImage image(320, 200, QImage::Format_RGB32);
    image.fill(grabs % 2 ? Qt::red : Qt::blue);
    return image;
  });
  server.setInterval(50);
  check(server.interval() == WebViewServer::kMinInterval, "the interval: at least kMinInterval");
  server.setInterval(60000);
  check(server.interval() == WebViewServer::kMaxInterval, "the interval: at most kMaxInterval");
  server.setInterval(WebViewServer::kMinInterval);
  QSignalSpy viewers(&server, &WebViewServer::viewersChanged);
  const qint64 before = QDateTime::currentMSecsSinceEpoch();
  r = get(base + "frame.jpg");
  const Reply again = get(base + "frame.jpg");
  const QImage picture = QImage::fromData(r.body, "JPG");
  check(r.status == 200 && r.header("content-type") == "image/jpeg" && picture.size() == QSize(320, 200),
        "a JPEG of the source's size");
  check(grabs == 1 && again.body == r.body && again.header("x-frame-time") == r.header("x-frame-time")
          && r.header("x-frame-time").toLongLong() >= before,
        "two browsers at once: one picture, its time");
  check(server.viewers() == 1 && viewers.size() == 1 && viewers.last().at(0).toInt() == 1, "one viewer (by address)");
  waitFor([] { return false; }, WebViewServer::kMinInterval + 50);
  r = get(base + "frame.jpg");
  check(grabs == 2 && r.header("x-frame-time") != again.header("x-frame-time"), "after the interval: a new picture");

  // the status
  server.setStatusSource([] { return QJsonObject{ { "host", "raspi" }, { "project", "Bench" }, { "alarm", "" } }; });
  r = get(base + "status.json");
  const QJsonObject status = QJsonDocument::fromJson(r.body).object();
  check(r.status == 200 && status.value("host").toString() == "raspi" && status.value("project").toString() == "Bench"
          && status.value("interval").toInt() == WebViewServer::kMinInterval,
        "the status: " + r.body);

  // refused: operating, a body, other paths
  check(request(port, "POST " + base + "frame.jpg HTTP/1.1\r\nContent-Length: 0\r\n").status == 405, "POST: 405");
  check(request(port, "GET " + base + " HTTP/1.1\r\nContent-Length: 3\r\n", "abc").status == 413, "a body: 413");
  check(get(base + "../etc/passwd").status == 404 && get(base + "frame.png").status == 404, "other paths: 404");

  // a new link: the old one ends at once
  const QString newer = HttpServer::newToken();
  server.setToken(newer);
  check(get(base + "frame.jpg").status == 403 && get("/" + newer.toLatin1() + "/frame.jpg").status == 200,
        "a new token: the old link 403, the new one works");

  // a browser that stopped asking no longer counts (after at least 5 s)
  check(waitFor([&] { return server.viewers() == 0; }, 8000) && viewers.last().at(0).toInt() == 0,
        "the viewer gone after it stopped asking");

  server.stop();
  check(!server.isRunning() && server.url().isEmpty() && server.token().isEmpty(), "stopped");

  if (failed == 0)
    qInfo() << "All web view tests passed.";
  return failed == 0 ? 0 : 1;
}
