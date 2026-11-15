// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The program itself, without anybody at the window: qtdmm on the offscreen
// platform with a meter in the window and two more with --also, read over
// the SCPI server's TCP port. Two simulated meters and a calculated value of
// both: INSTrument reaches each, the calculated value is right, the three
// record together on one time base, a recording is saved, a meter closes and
// opens again. Its own config and home: nothing of a QtDMM
// running here is touched. Unix only (the console and the end of a GUI
// process on Windows).
//
// Argument: the qtdmm executable.

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>

#include <cstdio>
#include <cstdlib>

#include "core/settings.h"
#include "testutil.h"

namespace
{
// one line of SCPI over TCP, and the answer of a query
class Client
{
public:
  bool connectTo(quint16 port, int ms)
  {
    return waitFor([&]
    {
      if (m_socket.state() == QAbstractSocket::ConnectedState)
        return true;
      m_socket.abort();
      m_socket.connectToHost("127.0.0.1", port);
      return m_socket.waitForConnected(200);
    }, ms);
  }
  void send(const QString &line)
  {
    m_socket.write(line.toLatin1() + '\n');
    m_socket.waitForBytesWritten(1000);
  }
  /// The answer, a definite-length block (#<n><length><data>) as its data.
  /// Empty after 5 s; the answer that comes later is skipped by the next
  /// query, or each answer after it would be the one before (slow builders)
  QString ask(const QString &line)
  {
    send(line);
    ++m_pending;
    QString answer;
    while (m_pending > 0)
    {
      if (!waitFor([&] { return m_socket.canReadLine() || m_socket.waitForReadyRead(50); }, 5000))
        return QString();
      answer = readAnswer();
      --m_pending;
    }
    return answer;
  }

private:
  QString readAnswer()
  {
    if (m_socket.peek(1) != "#")
      return QString::fromUtf8(m_socket.readLine()).trimmed();
    while (m_socket.bytesAvailable() < 2 && m_socket.waitForReadyRead(1000)) { }
    m_socket.read(1);
    const int digits = m_socket.read(1).toInt();
    while (m_socket.bytesAvailable() < digits && m_socket.waitForReadyRead(1000)) { }
    const int length = m_socket.read(digits).toInt();
    while (m_socket.bytesAvailable() < length + 1 && m_socket.waitForReadyRead(1000)) { }
    const QString data = QString::fromUtf8(m_socket.read(length));
    m_socket.readLine();   // the line's end after the block
    return data;
  }

  QTcpSocket m_socket;
  int        m_pending = 0;   ///< queries sent whose answer has not been read
};

void write(const QString &path, const QString &text)
{
  QFile f(path);
  check(f.open(QIODevice::WriteOnly | QIODevice::Truncate), "write " + path);
  f.write(text.toUtf8());
}

// the seconds of QTDMM:RECord? - "VIEW,2.160,9,"V""
double seconds(const QString &record)
{
  return record.section(',', 1, 1).toDouble();
}
}

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);
  if (argc < 2)
  {
    qWarning() << "usage: test_process_e2e <qtdmm>";
    return 1;
  }
  QTemporaryDir dir;
  const QString cfg = dir.path() + "/cfg";
  const QString home = dir.path() + "/home";
  QDir().mkpath(cfg);
  QDir().mkpath(home);

  // a free port for the SCPI server
  quint16 port = 0;
  {
    QTcpServer probe;
    probe.listen(QHostAddress::LocalHost, 0);
    port = probe.serverPort();
  }

  // My devices: two simulated meters at 1.5 V and 2.5 V, their product.
  // The port string says what kind of meter an entry is, the other keys
  // are kept by that
  write(cfg + "/devices.conf", R"([device-a]
name=Sim A
order=1
DMM\model=QtDMM Simulated meter
Port%20settings\device=calc V/DC 1.5
DMM\virtual-waveform=0
DMM\virtual-min=1.5
DMM\virtual-max=1.5

[device-b]
name=Sim B
order=2
DMM\model=QtDMM Simulated meter
Port%20settings\device=calc V/DC 2.5
DMM\virtual-waveform=0
DMM\virtual-min=2.5
DMM\virtual-max=2.5

[device-p]
name=Power
order=3
DMM\model=QtDMM Calculated value
Port%20settings\device=calc W sim_a * sim_b
DMM\calc-unit=W
DMM\calc-expression=sim_a * sim_b
)");
  // the window's meter is Sim A: its own keys and the entry's id. Settings
  // names the file as qtdmm does (macOS: after the native plist's name)
  QCoreApplication::setApplicationName(APP_NAME);
  QCoreApplication::setOrganizationName(APP_ORGANIZATION);
  write(Settings("e2e", cfg).fileName(), QString(R"([DMM]
my-device=a
model=QtDMM Simulated meter
virtual-waveform=0
virtual-min=1.5
virtual-max=1.5

[Port%20settings]
device=calc V/DC 1.5

[Scpi]
enabled=true
all-interfaces=false
port=%1
mdns=false
hislip=false
vxi11=false
)").arg(port));

  QProcess qtdmm;
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("HOME", home);
  env.insert("CFFIXED_USER_HOME", home);   // macOS: Documents ignores HOME
  for (const char *name : { "XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_CACHE_HOME", "XDG_DOCUMENTS_DIR" })
    env.remove(name);
  env.insert("QT_QPA_PLATFORM", "offscreen");
  // the SCPI server says on which port it listens: the one asked for may be
  // taken by another test meanwhile (ctest runs in parallel), and it takes
  // the next one then
  env.insert("QT_LOGGING_RULES", "qtdmm.scpi.debug=true");
  qtdmm.setProcessEnvironment(env);
  qtdmm.setProcessChannelMode(QProcess::ForwardedOutputChannel);
  QByteArray messages;
  QObject::connect(&qtdmm, &QProcess::readyReadStandardError, [&qtdmm, &messages]
  {
    const QByteArray more = qtdmm.readAllStandardError();
    messages += more;
    std::fwrite(more.constData(), 1, size_t(more.size()), stderr);   // as forwarded before
  });
  qtdmm.start(QString::fromLocal8Bit(argv[1]), { "--config-dir", cfg, "--config-id", "e2e", "--also", "b", "--also", "p" });
  check(qtdmm.waitForStarted(5000), "qtdmm starts: " + qtdmm.errorString());
  // never left running: ended before ctest's timeout (60 s) ends the test
  QTimer::singleShot(45000, &qtdmm, [&qtdmm]
  {
    qWarning() << "test_process_e2e: 45 s, qtdmm ended";
    qtdmm.kill();
    qtdmm.waitForFinished(2000);
    std::exit(1);
  });

  // a slow builder (FreeBSD VM, tests in parallel) may take more than 15 s
  static const QRegularExpression listening(QStringLiteral("listening on \\S+ (\\d+)"));
  QRegularExpressionMatch m;
  if (waitFor([&] { m = listening.match(QString::fromUtf8(messages)); return m.hasMatch(); }, 30000))
    port = quint16(m.captured(1).toUInt());
  Client scpi;
  if (!m.hasMatch() || !scpi.connectTo(port, 10000))
  {
    check(false, QString("SCPI on port %1 (%2, qtdmm %3)")
                   .arg(port)
                   .arg(m.hasMatch() ? "listening" : "no listening message")
                   .arg(qtdmm.state() == QProcess::Running ? "running" : "ended"));
    qtdmm.kill();
    return 1;
  }
  // a value of @p inst, once it has one
  auto value = [&](const char *inst)
  {
    scpi.send(QString("INST ") + inst);
    QString v;
    waitFor([&] { v = scpi.ask("MEAS?"); return !v.isEmpty() && !v.startsWith("9.9"); }, 5000);
    return v.toDouble();
  };

  // 1. three meters on one server, each its value; the calculated one of both
  waitFor([&] { return scpi.ask("INST:CAT?").count(',') == 2; }, 5000);
  check(scpi.ask("INST:CAT?") == "\"SIM_A\",\"SIM_B\",\"POWER\"", "INST:CAT?: " + scpi.ask("INST:CAT?"));
  check(qAbs(value("SIM_A") - 1.5) < 1e-9, "Sim A 1.5 V");
  check(qAbs(value("SIM_B") - 2.5) < 1e-9, "Sim B 2.5 V");
  check(qAbs(value("POWER") - 3.75) < 1e-9, "Power = sim_a * sim_b = 3.75 W");

  // 2. the window's INIT records all three, ABOR stops them: they span the
  // same time, from the same start
  scpi.send("INST SIM_A");
  scpi.send("*CLS");
  scpi.send("INIT");
  waitFor([] { return false; }, 2200);
  scpi.send("ABOR");
  waitFor([] { return false; }, 300);
  QList<double> spans;
  for (const char *inst : { "SIM_A", "SIM_B", "POWER" })
  {
    scpi.send(QString("INST ") + inst);
    const QString rec = scpi.ask("QTDMM:REC?");
    check(rec.startsWith("VIEW,") && rec.section(',', 2, 2).toInt() >= 3, QString("%1 recorded: %2").arg(inst, rec));
    spans << seconds(rec);
    const QString data = scpi.ask("QTDMM:REC:DATA? 1000");
    check(data.section('\n', 1, 1).startsWith("0.000,"), QString("%1 begins at 0: %2").arg(inst, data.section('\n', 1, 1)));
  }
  check(qAbs(spans[0] - spans[1]) < 0.1 && qAbs(spans[0] - spans[2]) < 0.1,
        QString("one time base: %1, %2, %3 s").arg(spans[0]).arg(spans[1]).arg(spans[2]));
  check(scpi.ask("SYST:ERR?").startsWith("0,"), "no error");

  // 3. INIT on a meter without a window starts the window's too - not over
  // the window's recording not saved yet, which nobody could be asked about
  scpi.send("INST SIM_B");
  scpi.send("INIT");
  waitFor([] { return false; }, 500);
  check(scpi.ask("SYST:ERR?").startsWith("-213,"), "INIT on Sim B, the window's not saved: -213");
  scpi.send("INST SIM_A");
  scpi.send("QTDMM:REC:SAVE \"sima\"");
  check(scpi.ask("SYST:ERR?").startsWith("0,"), "the window's saved");
  scpi.send("INST SIM_B");
  scpi.send("INIT");
  waitFor([] { return false; }, 500);
  scpi.send("INST SIM_A");
  check(scpi.ask("QTDMM:REC?").startsWith("REC,"), "INIT on Sim B: the window records");
  scpi.send("INST SIM_B");
  scpi.send("ABOR");
  waitFor([] { return false; }, 300);
  check(scpi.ask("QTDMM:REC?").startsWith("VIEW,"), "ABOR on Sim B");

  // 4. saved into Documents/QtDMM of its home
  scpi.send("*CLS");
  scpi.send("QTDMM:REC:SAVE \"simb\"");
  check(scpi.ask("SYST:ERR?").startsWith("0,"), "QTDMM:REC:SAVE");
  QFile saved(home + "/Documents/QtDMM/simb.csv");
  check(saved.open(QIODevice::ReadOnly), "the file " + saved.fileName());
  const QStringList lines = QString::fromUtf8(saved.readAll()).split('\n', Qt::SkipEmptyParts);
  check(lines.size() >= 2 && lines.value(1).contains(";2.5;V;"), "its readings: " + lines.value(1));

  // 5. a meter closed and opened again in the running program
  scpi.send("QTDMM:DEV:CLOS \"p\"");
  check(scpi.ask("INST:CAT?") == "\"SIM_A\",\"SIM_B\"", "closed: " + scpi.ask("INST:CAT?"));
  scpi.send("QTDMM:DEV:OPEN \"p\"");
  check(scpi.ask("SYST:ERR?").startsWith("0,") && scpi.ask("INST:CAT?").endsWith("\"POWER\""), "opened again");
  check(qAbs(value("POWER") - 3.75) < 1e-9, "Power again 3.75 W");

  qtdmm.terminate();
  if (!qtdmm.waitForFinished(5000))
    qtdmm.kill();
  qtdmm.waitForFinished(2000);
  return failed == 0 ? 0 : 1;
}
