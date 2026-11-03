// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// MeterConnection's connection state machine against a fake RFC 2217 server: Connecting
// until the first frame, Connected while frames arrive, Timeout when they
// stop, Error when the server goes away, reconnect when it is back, and a
// refused connection reported with its reason. Finally a polled,
// variable-length protocol (Fluke QM) through the reader, and sigrok-cli
// started and stopped without blocking.

#include <QtCore>
#include <QtNetwork>

#include "device/meterconnection.h"
#include "testutil.h"

// Sigrok-style frame: 30 bytes, LF terminated, "<special> <value> <unit> <range>"
static QByteArray frame(const QString &value)
{
  QByteArray line = QString("DC %1 V AUTO").arg(value).toLatin1();
  line.prepend(QByteArray(29 - line.size(), ' '));
  return line + "\n";
}

static const char *name(MeterConnection::LinkState s)
{
  switch (s)
  {
    case MeterConnection::LinkState::Closed: return "Closed";
    case MeterConnection::LinkState::Connecting: return "Connecting";
    case MeterConnection::LinkState::Connected: return "Connected";
    case MeterConnection::LinkState::Timeout: return "Timeout";
    case MeterConnection::LinkState::Error: return "Error";
  }
  return "?";
}

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);

  QTcpServer server;
  check(server.listen(QHostAddress::LocalHost, 0), "fake server listens");
  QTcpSocket *peer = nullptr;
  QObject::connect(&server, &QTcpServer::newConnection, [&] { peer = server.nextPendingConnection(); });

  MeterConnection dmm(nullptr);
  DmmDecoder::DMMInfo info;
  info.baud = 9600;
  info.bits = 8;
  dmm.setDmmInfo(info);
  dmm.setFormat(FrameFormat::Sigrok);
  dmm.setTimeout(500);
  dmm.setReconnectInterval(1);

  QList<MeterConnection::LinkState> states;
  QStringList messages;
  QObject::connect(&dmm, &MeterConnection::linkStateChanged, [&](MeterConnection::LinkState s, const QString &m)
  {
    states << s;
    messages << m;
  });
  int readings = 0;
  QObject::connect(&dmm, &MeterConnection::value, [&] { ++readings; });

  // --- 1. open -> Connecting; first frame -> Connected ---
  dmm.setDevice(QString("RFC2217 127.0.0.1:%1").arg(server.serverPort()));
  check(dmm.open(), "open() succeeds");
  check(dmm.linkState() == MeterConnection::LinkState::Connecting, "Connecting right after open()");
  check(waitFor([&] { return peer != nullptr; }, 2000), "client connected to the fake server");
  if (peer)
  {
    peer->readAll();   // the negotiation
    peer->write(frame("1.234"));
    peer->write(frame("1.235"));
    peer->flush();
  }
  check(waitFor([&] { return dmm.linkState() == MeterConnection::LinkState::Connected; }, 2000),
        QString("Connected after the first frame (is %1)").arg(name(dmm.linkState())));
  check(readings >= 1, "reading emitted");
  check(dmm.errorString().startsWith("connected (network)\n127.0.0.1:"), "status text: " + dmm.errorString());

  // --- 2. frames stop -> Timeout; frames resume -> Connected ---
  check(waitFor([&] { return dmm.linkState() == MeterConnection::LinkState::Timeout; }, 3000),
        QString("Timeout after %1 ms of silence (is %2)").arg(dmm.timeout()).arg(name(dmm.linkState())));
  check(dmm.errorString().contains("Timeout"), "timeout text: " + dmm.errorString());
  if (peer)
  {
    peer->write(frame("2.000"));
    peer->flush();
  }
  check(waitFor([&] { return dmm.linkState() == MeterConnection::LinkState::Connected; }, 2000), "back to Connected");

  // --- 3. server drops the connection -> Error; server still there -> reconnect ---
  QTcpSocket *old = peer;
  peer = nullptr;
  if (old)
    old->disconnectFromHost();
  check(waitFor([&] { return dmm.linkState() == MeterConnection::LinkState::Error; }, 3000),
        QString("Error after the server closed (is %1)").arg(name(dmm.linkState())));
  check(dmm.errorString().contains("Lost connection"), "loss text: " + dmm.errorString());
  check(waitFor([&] { return peer != nullptr; }, 4000), "reconnected within the retry interval");
  check(dmm.linkState() == MeterConnection::LinkState::Connecting, "Connecting again after reconnect");
  if (peer)
  {
    peer->readAll();
    peer->write(frame("3.000"));
    peer->flush();
  }
  check(waitFor([&] { return dmm.linkState() == MeterConnection::LinkState::Connected; }, 2000), "Connected after reconnect");

  // --- 4. close() -> Closed, no reconnect ---
  dmm.close();
  check(dmm.linkState() == MeterConnection::LinkState::Closed, "Closed after close()");
  check(!dmm.isOpen(), "port closed");
  const int before = states.size();
  waitFor([] { return false; }, 1500);
  check(states.size() == before, "no state changes after close()");

  // --- 5. nobody listening -> Error with the socket's reason ---
  const quint16 deadPort = server.serverPort();
  server.close();
  MeterConnection refused(nullptr);
  refused.setDmmInfo(info);
  refused.setFormat(FrameFormat::Sigrok);
  refused.setReconnectInterval(0);
  refused.setDevice(QString("RFC2217 127.0.0.1:%1").arg(deadPort));
  check(refused.open(), "open() itself succeeds (connect is asynchronous)");
  // Windows reports a refused loopback connect only after its SYN retries (~3 s)
  check(waitFor([&] { return refused.linkState() == MeterConnection::LinkState::Error; }, 15000),
        QString("refused connection -> Error (is %1)").arg(name(refused.linkState())));
  check(!refused.errorString().isEmpty() && refused.errorString().contains("Lost connection"),
        "reason reported: " + refused.errorString());
  refused.close();

  // --- 6. malformed address fails open() at once ---
  MeterConnection bad(nullptr);
  bad.setDmmInfo(info);
  bad.setFormat(FrameFormat::Sigrok);
  bad.setDevice("RFC2217 nonsense");
  check(!bad.open(), "open() fails for an address without port");
  check(bad.errorString().contains("host:port"), "malformed address text: " + bad.errorString());

  // --- 7. polled, variable-length protocol (Fluke QM) end to end ---
  // The reader must send the decoder's poll request and cut the answer -
  // CMD_ACK line plus reading line, arriving in pieces - into one frame.
  {
    QTcpServer fluke;
    check(fluke.listen(QHostAddress::LocalHost, 0), "fluke server listens");
    QTcpSocket *client = nullptr;
    QByteArray received;
    int polls = 0;
    QObject::connect(&fluke, &QTcpServer::newConnection, [&]
    {
      client = fluke.nextPendingConnection();
      QObject::connect(client, &QTcpSocket::readyRead, [&]
      {
        received += client->readAll();
        while (received.contains("QM\r"))
        {
          received.remove(0, received.indexOf("QM\r") + 3);
          ++polls;
          client->write("0\r-0.02");
          client->flush();
          QTimer::singleShot(50, client, [client] { client->write("3E-3,VDC,NORMAL,NONE\r"); client->flush(); });
        }
      });
    });

    MeterConnection meter(nullptr);
    DmmDecoder::DMMInfo flukeInfo;
    flukeInfo.baud = 115200;
    flukeInfo.bits = 8;
    meter.setDmmInfo(flukeInfo);
    meter.setFormat(FrameFormat::FlukeQM);
    meter.setDevice(QString("RFC2217 127.0.0.1:%1").arg(fluke.serverPort()));
    double lastValue = 0;
    QString lastVal, lastUnit;
    QObject::connect(&meter, &MeterConnection::value, [&](double dval, const QString &val, const QString &unit)
    {
      lastValue = dval;
      lastVal = val;
      lastUnit = unit;
    });
    check(meter.open(), "fluke open()");
    check(waitFor([&] { return polls >= 1; }, 4000), "poll request QM sent");
    check(waitFor([&] { return lastUnit == "mV"; }, 4000), "reading decoded from the split answer");
    check(lastVal == "-0.023" && qFuzzyCompare(lastValue + 1, -0.000023 + 1),
          "reading is -0.023 mV: " + lastVal + " " + lastUnit);
    check(meter.linkState() == MeterConnection::LinkState::Connected, "fluke Connected");
    check(waitFor([&] { return polls >= 2; }, 4000), "polled again");
    meter.close();
  }

  // --- 8. Fluke 45: compound query, answer line plus prompt line ---
  // Echo "On" on the meter echoes the query first; the echo and the prompt
  // must not surface as frames (they would show as "Error").
  {
    QTcpServer bench;
    check(bench.listen(QHostAddress::LocalHost, 0), "fluke 45 server listens");
    QTcpSocket *client = nullptr;
    QByteArray received;
    int polls = 0;
    QObject::connect(&bench, &QTcpServer::newConnection, [&]
    {
      client = bench.nextPendingConnection();
      QObject::connect(client, &QTcpSocket::readyRead, [&]
      {
        received += client->readAll();
        while (received.contains("VAL1?\r"))
        {
          received.remove(0, received.indexOf("VAL1?\r") + 6);
          ++polls;
          client->write("FUNC1?;AUTO?;MOD?;VAL1?\r\nOHMS;1;0;+12.345E+6\r\n=>\r\n");
          client->flush();
        }
      });
    });

    MeterConnection meter(nullptr);
    DmmDecoder::DMMInfo benchInfo;
    benchInfo.baud = 9600;
    benchInfo.bits = 8;
    meter.setDmmInfo(benchInfo);
    meter.setFormat(FrameFormat::Fluke45);
    meter.setDevice(QString("RFC2217 127.0.0.1:%1").arg(bench.serverPort()));
    int readings45 = 0;
    QString lastVal, lastUnit;
    QObject::connect(&meter, &MeterConnection::value, [&](double, const QString &val, const QString &unit)
    {
      ++readings45;
      lastVal = val;
      lastUnit = unit;
    });
    check(meter.open(), "fluke 45 open()");
    check(waitFor([&] { return polls >= 2 && readings45 >= 2; }, 6000), "fluke 45 polled twice, two readings");
    check(lastVal == "12.345" && lastUnit == "MOhm", "fluke 45 reading 12.345 MOhm: " + lastVal + " " + lastUnit);
    check(meter.linkState() == MeterConnection::LinkState::Connected && !meter.errorString().startsWith("Error"),
          "fluke 45 Connected without error text: " + meter.errorString());
    meter.close();
  }

#ifdef Q_OS_UNIX
  // --- 9. sigrok-cli without blocking: a fake one (shell script) ---
  // open() does not wait for it to start, close() not for it to end; one that
  // fails or exits is a lost port with its reason
  {
    QTemporaryDir dir;
    auto script = [&](const QString &name, const QByteArray &body)
    {
      QFile f(dir.filePath(name));
      check(f.open(QIODevice::WriteOnly), "write " + name);
      f.write("#!/bin/sh\n" + body);
      f.close();
      f.setPermissions(f.permissions() | QFile::ExeOwner);
      return f.fileName();
    };
    DmmDecoder::DMMInfo sigInfo = info;

    sigInfo.sigrokExe = script("running", "echo '" + frame("1.250").trimmed() + "'\nexec sleep 30\n");
    MeterConnection running(nullptr);
    running.setDmmInfo(sigInfo);
    running.setFormat(FrameFormat::Sigrok);
    running.setDevice("SIGROK fake");
    QString runningVal;
    QObject::connect(&running, &MeterConnection::value, [&](double, const QString &val) { runningVal = val; });
    check(running.open(), "sigrok open()");
    check(waitFor([&] { return runningVal == "1.250"; }, 3000), "sigrok reading 1.250: " + runningVal);
    QElapsedTimer closing;
    closing.start();
    running.close();
    check(closing.elapsed() < 200, QString("sigrok close() does not wait (%1 ms)").arg(closing.elapsed()));

    sigInfo.sigrokExe = script("failing", "echo 'No devices found.' >&2\nexit 1\n");
    MeterConnection failing(nullptr);
    failing.setDmmInfo(sigInfo);
    failing.setFormat(FrameFormat::Sigrok);
    failing.setReconnectInterval(0);
    failing.setDevice("SIGROK fake");
    check(failing.open(), "sigrok open() of a failing sigrok-cli");
    check(waitFor([&] { return failing.linkState() == MeterConnection::LinkState::Error; }, 3000),
          QString("exited sigrok-cli -> Error (is %1)").arg(name(failing.linkState())));
    check(failing.errorString().contains("No devices found."), "sigrok's reason: " + failing.errorString());
    failing.close();

    sigInfo.sigrokExe = dir.filePath("missing");
    MeterConnection missing(nullptr);
    missing.setDmmInfo(sigInfo);
    missing.setFormat(FrameFormat::Sigrok);
    missing.setReconnectInterval(0);
    missing.setDevice("SIGROK fake");
    check(missing.open(), "sigrok open() of a missing sigrok-cli");
    check(waitFor([&] { return missing.linkState() == MeterConnection::LinkState::Error; }, 3000),
          QString("missing sigrok-cli -> Error (is %1)").arg(name(missing.linkState())));
    check(missing.errorString().contains("missing"), "names the program: " + missing.errorString());
    missing.close();
  }
#endif

  if (failed == 0)
    qInfo() << "All MeterConnection link state tests passed.";
  else
    qWarning() << failed << "DMM link state test(s) failed.";
  return failed == 0 ? 0 : 1;
}
