// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>
#include <QMap>
#include <QVariantMap>
#include <functional>

#include "device/discovery/discovery.h"
#include "device/dmmdecoder.h"

class DeviceLibrary;
class DeviceSettings;
class DeviceSummary;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QToolButton;

/// The assistant "Add device": how the meter is connected (page 1), where
/// (page 2: a passive search for that connection, every port found, the
/// recognised ones first, or typed in), which meter it is with its settings
/// and a name (page 3), and whether it is connected at once or only kept
/// (page 4). sigrok and simulated meters have no page 2.
/// The caller keeps the result in My devices - a find that already is one
/// of them (knownDevice()) changes that entry instead.
class AddDeviceDlg : public QDialog
{
  Q_OBJECT
public:
  /// Simulated, Calculated, Integral: a generator, a formula over values or
  /// an integral of one (logic nodes), set up in their own dialogs - the assistant ends at once
  /// (connection()).
  enum Connection { Cable, Bluetooth, Network, Sigrok, Simulated, Calculated, Integral };
  enum Page { ConnectionPage, PortPage, DevicePage, TargetPage };
  /// Page 4: into My devices and connected at once, or into My devices only.
  enum Target { NoTarget, Connect, SaveOnly };

  explicit AddDeviceDlg(DeviceLibrary *library, QWidget *parent = nullptr);
  ~AddDeviceDlg() override;

  /// The ports for the port field (Transport::availablePorts() and the custom ports).
  void        setPorts(const QStringList &ports);
  void        setSigrokExe(const QString &exe);
  /// Places (DeviceLibrary::place()) the open devices use, with what a
  /// find at that place says ("In use here").
  void        setPlacesInUse(const QMap<QString, QString> &places) { m_inUse = places; }
  /// The searches of a connection; by default UsbDiscoverer and
  /// SerialDiscoverer for a cable, BleDiscoverer for Bluetooth,
  /// BridgeDiscoverer for the network. The test
  /// gives none and adds the finds itself.
  void        setDiscoverers(std::function<QList<Discoverer *>(Connection)> make) { m_makeDiscoverers = std::move(make); }
  /// The port a FrameProbe listens to for a find that tells its model by
  /// its first frame (Candidate::probe); FrameProbe::blePort() by default.
  /// The test gives one of its own.
  void        setProbePort(std::function<QIODevice *(const Candidate &)> make) { m_makeProbePort = std::move(make); }
  /// Why Bluetooth cannot be used, empty when it can (BleDiscoverer::unavailable();
  /// for the tests): the Bluetooth tile is off then and its tooltip says why.
  void        setBluetoothCheck(std::function<QString()> check);

  /// Page 1: the connection; goes on to the next page.
  void        chooseConnection(Connection connection);
  Connection  connection() const { return m_connection; }
  Page        page() const;
  /// Whether "Next" may be pressed now.
  bool        canGoNext() const;
  /// Page 2: a find of the search, shown as a card (recognised meters first).
  void        addCandidate(const Candidate &candidate);
  /// Page 2: the cards in their order, by Candidate::key.
  QStringList candidates() const;
  /// Page 2: chooses the card @p key; its port goes into the port field.
  /// A find whose first frame tells its model is listened to (FrameProbe),
  /// and the model it names comes first.
  void        chooseCandidate(const QString &key);
  /// Page 2: the models a find can be, the likeliest first.
  QStringList candidateModels(const QString &key) const;
  /// Page 2: the port field (a port, a Bluetooth address, or host:port).
  void        setPort(const QString &port);
  /// The entry of My devices the device is (found at its place, the model
  /// unchanged on page 3); empty for a new device.
  QString     knownDevice() const;

  /// The device's meter keys, as DeviceSettings::keys() gives them.
  QVariantMap keys() const;
  QString     name() const;
  /// Where the device goes; set by the button on page 4.
  Target      target() const { return m_target; }
  DeviceSettings *settings() const { return m_settings; }

  /// The models a connection offers on page 3.
  static bool offers(Connection connection, const DmmDecoder::DMMInfo &info);
  /// Whether the connection has page 2 (cable, Bluetooth, network).
  static bool hasPortPage(Connection connection);

public Q_SLOTS:
  void        back();
  void        next();

private:
  QToolButton *tile(const QString &icon, const QString &text, const QString &toolTip);
  void        showPage(Page page);
  void        updateButtons();
  /// The name follows the model until it is typed in.
  void        suggestName();
  void        finish(Target target);
  void        startSearch();
  void        stopSearch();
  /// The card for the port field's text, or -1 when it was typed in.
  int         chosenCandidate() const;
  void        updateHint();
  /// Listens to the find in row @p row for its first frame; stopProbe()
  /// ends it.
  void        startProbe(int row);
  void        stopProbe();
  /// From page 2 to page 3: the find (or the typed port) into the fields,
  /// a known entry with its name.
  void        takePort();
  /// Page 4: the picture of the kind of instrument and what was set up.
  void        updateDeviceInfo();

  DeviceLibrary  *m_library = nullptr;
  DeviceSettings *m_settings = nullptr;
  QStackedWidget *m_pages = nullptr;
  QLabel         *m_title = nullptr;
  QLineEdit      *m_name = nullptr;
  QToolButton    *m_connect = nullptr;
  QToolButton    *m_saveOnly = nullptr;
  DeviceSummary  *m_summary = nullptr;   ///< page 4: the device as set up
  QProgressBar   *m_progress = nullptr;
  QListWidget    *m_cards = nullptr;
  QLabel         *m_hint = nullptr;
  QPushButton    *m_fix = nullptr;
  QPushButton    *m_searchAgain = nullptr;
  QLineEdit      *m_port = nullptr;
  QList<Candidate> m_found;   ///< in the order of the cards
  QMap<QString, QString> m_inUse;
  QList<Discoverer *> m_running;
  std::function<QList<Discoverer *>(Connection)> m_makeDiscoverers;
  std::function<QIODevice *(const Candidate &)> m_makeProbePort = &FrameProbe::blePort;
  FrameProbe     *m_probe = nullptr;
  QStringList     m_probed;   ///< the finds listened to, by Candidate::key
  QString         m_probeHint;   ///< the hint of the find m_probe listens to, before it
  QString         m_known;    ///< the entry of My devices at the chosen place
  QPushButton    *m_back = nullptr;
  QPushButton    *m_next = nullptr;
  Connection      m_connection = Cable;
  Target          m_target = NoTarget;
  QString         m_model;   ///< the model the name was suggested for
  QToolButton    *m_bluetoothTile = nullptr;
  QString         m_bluetoothTip;   ///< its tooltip while Bluetooth can be used
  std::function<QString()> m_bluetoothCheck;
  QString         m_searchError;    ///< why the last search stopped early (Discoverer::error())
  /// The Bluetooth tile on or off by m_bluetoothCheck.
  void            updateBluetoothTile();
};
