// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "service/httpserver.h"

#include <QImage>
#include <QJsonObject>

#include <functional>

class QTimer;

/// The web view: a browser opens url() and sees the picture of QtDMM's
/// window area (the devices' windows, graphs, nodes), new every interval().
/// Only to look at, nothing to operate.
///
/// The token is the caller's and stays (the settings keep it): the link can
/// be kept as a bookmark or on a kiosk screen until a new token is set.
/// Without it every path gets 403.
///
/// Paths after /<token>/:
/// - "" the page (assets/webview/index.html): a line with where it comes
///   from, the time of the picture and the alarm state, a button for full
///   screen, the picture below as large as fits
/// - "frame.jpg" the picture as JPEG, its time (ms since the epoch) in the
///   header X-Frame-Time
/// - "status.json" status() of the caller plus "interval" (ms)
///
/// The picture is made only when a browser asks, at most once per
/// interval for all of them: with nobody watching it costs nothing.
class WebViewServer : public HttpServer
{
  Q_OBJECT

public:
  static constexpr quint16 kDefaultPort = 8088;
  static constexpr int kDefaultInterval = 1000;   ///< ms
  static constexpr int kMinInterval = 200;
  static constexpr int kMaxInterval = 10000;
  /// Browsers open several connections each.
  static constexpr int kMaxClients = 32;

  explicit WebViewServer(QObject *parent = nullptr);

  /// What the picture is (called in the GUI thread, when one is needed).
  void setFrameSource(std::function<QImage()> source) { m_frameSource = std::move(source); }
  /// What the line above the picture says: "host", "project", "alarm"
  /// (text, empty: none).
  void setStatusSource(std::function<QJsonObject()> source) { m_statusSource = std::move(source); }
  /// How often the page asks for a new picture (ms, kMinInterval..kMaxInterval).
  void setInterval(int ms);
  int interval() const { return m_interval; }
  /// The browsers that fetched a picture lately (by address).
  int viewers() const { return int(m_viewers.size()); }

  /// The page with the words in the language of the program.
  static QByteArray page();

Q_SIGNALS:
  void viewersChanged(int viewers);

protected:
  void handle(QTcpSocket *socket, const Request &request) override;
  QByteArray forbidden() const override;

private:
  QByteArray frame();
  void seen(const QHostAddress &peer);
  void prune();

  std::function<QImage()> m_frameSource;
  std::function<QJsonObject()> m_statusSource;
  int m_interval = kDefaultInterval;
  QByteArray m_frame;          ///< the last picture, JPEG
  qint64 m_frameTime = 0;      ///< when it was made (ms since the epoch)
  QHash<QString, qint64> m_viewers;   ///< address -> last picture fetched (ms since the epoch)
  QTimer *m_prune;
};
