// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <functional>
#include <optional>

#include "service/vxi11server.h"

class HislipServer;
class MdnsResponder;
class ScpiServer;
class WebViewServer;

/// What the SCPI server should do (from the settings page).
struct ScpiConfig
{
  bool    enabled = false;
  bool    allInterfaces = false;   ///< else localhost only
  quint16 port = 5025;
  bool    mdns = false;            ///< announce as _scpi-raw._tcp (and _hislip._tcp)
  bool    hislip = false;          ///< also HiSLIP for VISA (NI-VISA, LabVIEW, PyVISA)
  quint16 hislipPort = 4880;
  bool    vxi11 = false;           ///< also VXI-11 (portmapper on 111): older VISA, lxi discover
  Vxi11Server::RpcbindMode vxi11Rpc = Vxi11Server::RpcbindMode::Auto;   ///< system rpcbind or own portmapper
};

/// What the web view should do (from its settings page).
struct WebViewConfig
{
  bool    enabled = false;
  bool    allInterfaces = false;   ///< else localhost only
  quint16 port = 8088;
  int     interval = 1000;         ///< ms between pictures
  QString token;                   ///< the link's, kept in the settings
};

/// One network service of the session (the SCPI server, HiSLIP), as the
/// sidebar shows it.
struct ServiceState
{
  QString id;               ///< "scpi", "hislip", "vxi11", "webview"
  QString name;             ///< "SCPI", "HiSLIP", "VXI-11", "Web view"
  bool    running = false;
  quint16 port = 0;
  QString address;          ///< "localhost:5025", "TCPIP::localhost::hislip0::INSTR", the web view's link
  int     clients = 0;
  QString error;            ///< switched on but not running: why (port taken, ...)
  QString announced;        ///< the mDNS instance name, empty when not announced
};

/// The network services of a QtDMM process, once however many meters it
/// reads: the SCPI server with HiSLIP and VXI-11 on the same interpreter, and
/// their mDNS announcements. The meters take part through their ScpiDevice
/// (scpi()->addDevice()); INSTrument chooses among them.
///
/// MainWindow keeps one and hands it to its MeterController; a controller
/// made without one (tests, headless) keeps its own.
class ServiceHost : public QObject
{
  Q_OBJECT
public:
  explicit ServiceHost(QObject *parent = nullptr);
  ~ServiceHost() override;

  ScpiServer *scpi() const { return m_scpi; }
  /// The web view: its picture and status come from the window (MainWindow).
  WebViewServer *webView() const { return m_webView; }

  /// Starts, restarts or stops the servers; keeps a running server when
  /// nothing about it changed, so its clients stay connected. @p instanceId
  /// and @p model go into the mDNS announcement.
  void        apply(const ScpiConfig &config, const QString &instanceId, const QString &model);
  /// The VISA address of the HiSLIP server ("TCPIP::localhost::hislip0::INSTR"),
  /// empty while it does not run.
  QString     hislipResource() const;
  /// The VISA address of the VXI-11 server, "TCPIP::host::INSTR"; empty when it does not run.
  QString     vxi11Resource() const;
  /// How VXI-11 reaches the portmapper, from --vxi11-rpc: it replaces the setting for this run
  /// (scripts, service starts). Set before the first apply().
  static void setVxi11RpcOverride(std::optional<Vxi11Server::RpcbindMode> mode);
  /// From --vxi11-portmapper-port (tests, development): an own portmapper on this port instead of
  /// 111 for this run (0: any free one). Set before the first apply().
  static void setVxi11PortmapperPort(std::optional<quint16> port);
  /// Starts, restarts or stops the web view; keeps it running (and its
  /// browsers) when only the interval or the token changed. A port taken:
  /// the next free of the ten after it.
  void        applyWebView(const WebViewConfig &config);
  /// The web view's link with its token (http://192.168.1.20:8088/<token>/),
  /// empty while it does not run. On all networks the computer's first
  /// LAN address, else localhost.
  QString     webViewUrl() const;
  /// The web view's DNS-SD TXT records: no token, "path" is "/" (the hint
  /// page). Public for the test.
  static QMap<QString, QString> webViewTxt(const QString &instanceId);
  /// The SCPI server, HiSLIP, VXI-11 and the web view, running or not.
  QList<ServiceState> services() const;
  /// Source of the SCPI screen dump (HCOPy:SDUMp:DATA?): format -> image bytes.
  void        setScreenshotSource(std::function<QByteArray(const QByteArray &)> source);

Q_SIGNALS:
  /// The state as a sentence for the settings page; services() changed with it.
  void        statusChanged(const QString &detail);
  /// A server could not start.
  void        error(const QString &text);

private:
  void        updateStatus();
  /// "QtDMM <host>" or "QtDMM <instance id>", the name every announcement uses.
  QString     announcedName() const;

  ScpiServer         *m_scpi;
  MdnsResponder      *m_mdns;
  HislipServer       *m_hislip;       ///< the same interpreter over HiSLIP
  MdnsResponder      *m_mdnsHislip;
  Vxi11Server        *m_vxi11;        ///< and over VXI-11
  MdnsResponder      *m_mdnsVxi11;
  WebViewServer      *m_webView;
  MdnsResponder      *m_mdnsWebView;    ///< "_http._tcp", only on all interfaces
  QString             m_webViewError;
  QString             m_scpiError;    ///< why the switched-on server is not running
  QString             m_hislipError;
  QString             m_vxi11Error;
  QString             m_instanceId = "default";
  QString             m_model;
};
