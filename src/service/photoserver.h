// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QHostAddress>
#include <QObject>
#include <QUrl>

class QTcpServer;
class QTcpSocket;

/// A small HTTP server through which a phone sends photos of the setup
/// (PhotoDlg): the phone opens url() - shown as a QR code - and gets a page
/// (assets/photo/upload.html) whose button opens its camera; each photo is
/// sent at once with POST and comes out as received().
///
/// It runs only while the dialog is open, on one address of the computer.
/// Every path starts with a random token of 128 bits, made new by each
/// start(): without it the server answers 403. Only pictures are taken
/// (Content-Type image/..., and Qt must read them: 415 otherwise), up to
/// kMaxBytes (413). Plain HTTP on purpose: a file input with
/// capture="environment" opens the camera without HTTPS, which getUserMedia
/// would need.
class PhotoServer : public QObject
{
  Q_OBJECT
public:
  /// The largest photo taken (bytes).
  static constexpr qint64 kMaxBytes = 25 * 1024 * 1024;
  /// The most connections at once; more are closed at once.
  static constexpr int kMaxClients = 8;

  explicit PhotoServer(QObject *parent = nullptr);
  ~PhotoServer() override;

  /// Listens on @p address, on a port the system picks (@p port 0) or
  /// @p port, with a new token. False when it cannot (error()).
  bool        start(const QHostAddress &address, quint16 port = 0);
  void        stop();
  bool        isRunning() const;
  QString     error() const { return m_error; }
  /// The link to open: http://<address>:<port>/<token>/, empty while not running.
  QUrl        url() const;
  QString     token() const { return m_token; }

  /// The addresses a phone may reach the computer on: IPv4 of the network
  /// cards up and running, without loopback; LAN and WLAN first, virtual
  /// bridges, VPN and container networks last. Each with the card's name.
  struct Address
  {
    QHostAddress address;
    QString      card;   ///< the network card's name ("wlan0", "Ethernet")
  };
  static QList<Address> addresses();
  /// The order of addresses(): lower first. A private address on a card
  /// whose name is no virtual one's (docker, veth, virbr, vmnet, tun ...)
  /// comes first.
  static int  rank(const QHostAddress &address, const QString &card);
  /// The page for the phone with the words in the language of the program.
  static QByteArray page();

Q_SIGNALS:
  /// A photo sent by the phone, as it came.
  void        received(const QByteArray &data);
  /// A phone opened the page.
  void        visited();

private:
  struct Client
  {
    QByteArray head;        ///< up to the empty line
    QByteArray body;
    QByteArray method, path, type;
    qint64     length = -1;   ///< Content-Length, -1 before the head is read
  };
  void        onConnection();
  void        onData(QTcpSocket *socket);
  void        answer(QTcpSocket *socket, int status, const QByteArray &type, const QByteArray &body);
  void        handle(QTcpSocket *socket, const Client &client);

  QTcpServer *m_server;
  QString     m_token;
  QString     m_error;
  QHash<QTcpSocket *, Client> m_clients;
};
