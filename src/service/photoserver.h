// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "service/httpserver.h"

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
class PhotoServer : public HttpServer
{
  Q_OBJECT

public:
  /// The largest photo taken (bytes).
  static constexpr qint64 kMaxBytes = 25 * 1024 * 1024;
  /// The most connections at once; more are closed at once.
  static constexpr int kMaxClients = 8;

  explicit PhotoServer(QObject *parent = nullptr);

  /// Listens on @p address, on a port the system picks (@p port 0) or
  /// @p port, with a new token. False when it cannot (error()).
  bool start(const QHostAddress &address, quint16 port = 0);

  /// The page for the phone with the words in the language of the program.
  static QByteArray page();

Q_SIGNALS:
  /// A photo sent by the phone, as it came.
  void received(const QByteArray &data);
  /// A phone opened the page.
  void visited();

protected:
  void handle(QTcpSocket *socket, const Request &request) override;
  QByteArray forbidden() const override;
  qint64 maxBody() const override { return kMaxBytes; }
  QByteArray tooLarge() const override;
};
