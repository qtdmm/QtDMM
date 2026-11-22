// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// PhotoServer: the page a phone opens, the token, what is taken and what is
// refused; the QR code of its link; the order of the addresses.

#include <QApplication>
#include <QBuffer>
#include <QImage>
#include <QSignalSpy>
#include <QTabWidget>
#include <QTcpSocket>
#include <QTemporaryDir>

#include "service/photoserver.h"
#include "ui/dialogs/photodlg.h"
#include "ui/qrcodeimage.h"
#include "testutil.h"

namespace
{
// one request, the whole answer (the server closes after an error)
struct Reply
{
  int status = 0;
  QByteArray body;
};

Reply request(quint16 port, const QByteArray &head, const QByteArray &body = {})
{
  QTcpSocket s;
  s.connectToHost(QHostAddress::LocalHost, port);
  if (!s.waitForConnected(2000))
    return {};
  s.write(head + "\r\n" + body);
  QByteArray all;
  // until the head and the body of its Content-Length are in
  waitFor([&]
  {
    all += s.readAll();
    const qsizetype end = all.indexOf("\r\n\r\n");
    if (end < 0)
      return false;
    const qsizetype at = all.toLower().indexOf("content-length:");
    const qsizetype len = at < 0 ? 0 : all.mid(at + 15, all.indexOf("\r\n", at) - at - 15).trimmed().toLongLong();
    return all.size() >= end + 4 + len;
  }, 3000);
  Reply r;
  r.status = all.mid(9, 3).toInt();
  r.body = all.mid(all.indexOf("\r\n\r\n") + 4);
  return r;
}

QByteArray png()
{
  QImage image(8, 6, QImage::Format_RGB32);
  image.fill(Qt::green);
  QByteArray data;
  QBuffer buffer(&data);
  buffer.open(QIODevice::WriteOnly);
  image.save(&buffer, "PNG");
  return data;
}
}   // namespace

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  PhotoServer server;
  QSignalSpy received(&server, &PhotoServer::received);
  QSignalSpy visited(&server, &PhotoServer::visited);
  check(!server.isRunning() && server.url().isEmpty(), "not running at first, no link");
  check(server.start(QHostAddress::LocalHost), "started: " + server.error());
  const quint16 port = quint16(server.url().port());
  const QByteArray token = server.token().toLatin1();
  check(token.size() == 32 && server.url().toString() == QString("http://127.0.0.1:%1/%2/").arg(port).arg(server.token()),
        "the link: address, port and a token of 128 bits: " + server.url().toString());

  // the page, with the token only
  Reply r = request(port, "GET /" + token + "/ HTTP/1.1\r\nHost: x\r\n");
  check(r.status == 200 && r.body.contains("capture=\"environment\"") && !r.body.contains("{{") && visited.size() == 1
          && r.body.contains("<html lang=\"en\">"),
        QString("GET with the token: the page, words filled in (%1)").arg(r.status));
  r = request(port, "GET / HTTP/1.1\r\nHost: x\r\n");
  check(r.status == 403 && visited.size() == 1, QString("without the token: 403 (%1)").arg(r.status));
  r = request(port, "GET /0123456789abcdef0123456789abcdef/ HTTP/1.1\r\nHost: x\r\n");
  check(r.status == 403, QString("another token: 403 (%1)").arg(r.status));
  r = request(port, "GET /" + token + "/favicon.ico HTTP/1.1\r\nHost: x\r\n");
  check(r.status == 404, QString("anything else: 404 (%1)").arg(r.status));

  // a photo: taken; others refused
  const QByteArray photo = png();
  const QByteArray upload = "POST /" + token + "/upload HTTP/1.1\r\nHost: x\r\n";
  r = request(port, upload + "Content-Type: image/png\r\nContent-Length: " + QByteArray::number(photo.size()) + "\r\n", photo);
  check(r.status == 200 && received.size() == 1 && received.at(0).at(0).toByteArray() == photo,
        QString("a photo: 200, received() with its bytes (%1)").arg(r.status));
  r = request(port, upload + "Content-Type: text/plain\r\nContent-Length: 5\r\n", "hello");
  check(r.status == 415 && received.size() == 1, QString("no image type: 415 (%1)").arg(r.status));
  r = request(port, upload + "Content-Type: image/jpeg\r\nContent-Length: 10\r\n", "not a jpeg");
  check(r.status == 415 && received.size() == 1, QString("an image type, no picture: 415 (%1)").arg(r.status));
  r = request(port, upload + "Content-Type: image/jpeg\r\nContent-Length: " + QByteArray::number(PhotoServer::kMaxBytes + 1) + "\r\n");
  check(r.status == 413 && received.size() == 1, QString("too large: 413 before anything is read (%1)").arg(r.status));
  r = request(port, upload + "Content-Type: image/png\r\nTransfer-Encoding: chunked\r\n");
  check(r.status == 400, QString("no length: 400 (%1)").arg(r.status));
  r = request(port, "GET /" + token + "/upload HTTP/1.1\r\nHost: x\r\n");
  check(r.status == 405, QString("GET on upload: 405 (%1)").arg(r.status));
  r = request(port, QByteArray(20000, 'x'));
  check(r.status == 400, QString("an endless head: 400 (%1)").arg(r.status));

  // started again: a new token, the old link is gone; stopped: nothing listens
  check(server.start(QHostAddress::LocalHost) && server.token().toLatin1() != token, "a new start, a new token");
  r = request(quint16(server.url().port()), "GET /" + token + "/ HTTP/1.1\r\nHost: x\r\n");
  check(r.status == 403, QString("the old link: 403 (%1)").arg(r.status));
  const quint16 last = quint16(server.url().port());
  server.stop();
  QTcpSocket gone;
  gone.connectToHost(QHostAddress::LocalHost, last);
  check(!server.isRunning() && !gone.waitForConnected(1000), "stopped: nothing listens");

  // the order of the addresses: LAN/WLAN first, virtual cards last
  check(PhotoServer::rank(QHostAddress("192.168.1.20"), "wlan0") < PhotoServer::rank(QHostAddress("172.17.0.1"), "docker0")
          && PhotoServer::rank(QHostAddress("10.0.0.5"), "Ethernet") < PhotoServer::rank(QHostAddress("10.8.0.2"), "tun0")
          && PhotoServer::rank(QHostAddress("192.168.1.20"), "enp3s0") < PhotoServer::rank(QHostAddress("100.64.1.2"), "enp3s0")
          && PhotoServer::rank(QHostAddress("192.168.56.1"), "VirtualBox Host-Only Network")
               > PhotoServer::rank(QHostAddress("192.168.1.20"), "WLAN"),
        "addresses: private LAN/WLAN first, virtual cards and VPN last");

  // the QR code of a link
  const QImage qr = qrCodeImage("http://192.168.178.20:45123/0123456789abcdef0123456789abcdef/", 4);
  check(!qr.isNull() && qr.width() == qr.height() && qr.width() % 4 == 0 && qr.pixelColor(0, 0) == Qt::white
          && qr.pixelColor(4 * 4, 4 * 4) == Qt::black,
        "QR code: square, white quiet zone, a finder pattern's corner black");
  check(qrCodeImage(QString(5000, 'x')).isNull(), "QR code: too long, none");

  // the dialog: the server runs while the phone's tab shows, and a photo it
  // gets goes into the project
  {
    QTemporaryDir project;
    PhotoDlg dlg(project.path());
    QSignalSpy added(&dlg, &PhotoDlg::photoAdded);
    check(!dlg.server()->isRunning(), "dialog: no server before it shows");
    dlg.show();
    const bool network = !PhotoServer::addresses().isEmpty();
    check(dlg.server()->isRunning() == network, "dialog shown on the phone's tab: the server runs (with a network)");
    check(dlg.addData(photo) && added.size() == 1 && dlg.photoCount() == 1, "dialog: a photo from the phone into the project");
    dlg.findChild<QTabWidget *>("ui_photoSources")->setCurrentIndex(1);
    check(!dlg.server()->isRunning(), "dialog: the file's tab, the server stops");
    dlg.findChild<QTabWidget *>("ui_photoSources")->setCurrentIndex(0);
    dlg.hide();
    check(!dlg.server()->isRunning(), "dialog hidden: the server stops");
  }

  if (failed == 0)
    qInfo() << "All photo server tests passed.";
  else
    qWarning() << failed << "photo server test(s) failed.";
  return failed == 0 ? 0 : 1;
}
