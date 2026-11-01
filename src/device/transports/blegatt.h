// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/transports/buffereddevice.h"

#include <QBluetoothUuid>
#include <QElapsedTimer>
#include <QLowEnergyCharacteristic>
#include <QStringList>

#include "device/dmmdecoder.h"

class QBluetoothDeviceDiscoveryAgent;
class QBluetoothDeviceInfo;
class QLowEnergyController;
class QLowEnergyService;
class QTimer;

/// A Bluetooth LE meter reached over a GATT connection - the counterpart of
/// BleAdvertisementDevice, which only listens to broadcasts. It connects to
/// the address, finds the meter's serial-like service, subscribes to its
/// notify characteristic and hands every notification on as received bytes;
/// what the reader thread writes (the decoder's poll request) goes to the
/// write characteristic. The protocol stays in the decoder.
///
/// Which service and characteristics a meter uses comes from the protocol
/// (profile()); the UNI-T iDMM meters (UT60BT, UT161) use the Microchip/ISSC
/// "Transparent UART" service. Port string: "<address>". open() returns at
/// once and the connection is set up in the background: first a short scan
/// for the address - BlueZ forgets a meter that is not paired a while after
/// it was last seen, and a connect to an address it does not know never
/// answers (the assistant's search hid that: it had just seen the meter) -
/// then the connect. A lost link emits finished() and MeterConnection
/// reconnects as for the other port types.
///
/// Polled meters answer one request with one frame. The reader asks once a
/// second; to follow the meter's own update rate the device repeats the last
/// request as soon as an answer has come in, at most every kRepollMs.
class BleGattDevice : public BufferedDevice
{
  Q_OBJECT
public:
  /// The GATT layout of one protocol family.
  struct Profile
  {
    QBluetoothUuid service;
    QBluetoothUuid notify;
    QList<QBluetoothUuid> write;   ///< in order of preference
    QStringList namePrefixes;      ///< for scan(): advertised names that belong to it
    QByteArray hello;              ///< written once when ready, before the first poll
    bool byNameOnly = false;       ///< the service is a common one (0xFFF0 of many BLE
                                   ///< UART modules): only the name tells the meter
    bool helperScan = false;       ///< Linux: bluetoothctl scans beside the search (bleHelperScan())
    quint16 manufacturer = 0;      ///< a company id of its manufacturer data that tells the
                                   ///< meter too (0: none); Qt reports it also when the
                                   ///< service UUIDs come too late or not at all
    QBluetoothUuid info;           ///< a characteristic read once on connecting, the meter's
                                   ///< device information (info(), infoRead()); null: none
    QByteArray (*helloFor)(const QString &address) = nullptr;   ///< hello made for the meter's
                                   ///< address instead (a login that names it); null: hello
    /// True when a device seen in a scan, with @p name, @p services and the
    /// company ids of its manufacturer data, is one of these.
    bool matches(const QString &name, const QList<QBluetoothUuid> &services,
                 const QList<quint16> &manufacturers = {}) const;
  };
  /// The profile for @p format, or nullopt when it is not a GATT protocol.
  static std::optional<Profile> profile(FrameFormat::DataFormat format);

  explicit BleGattDevice(const DmmDecoder::DMMInfo &info, const QString &device, QObject *parent = nullptr);
  ~BleGattDevice() override;

  bool open(OpenMode mode) override;
  void close() override;
  bool isSequential() const override { return true; }

  /// True once the meter has confirmed the notification subscription and the
  /// write characteristic is known; until then open() has returned but the
  /// link is still being set up (up to 20 s). The last poll written before
  /// is sent on becoming ready.
  bool isReady() const { return m_ready; }

  /// The value of the profile's info characteristic, read on connecting;
  /// empty before that or without one.
  QByteArray info() const { return m_info; }

  /// Writes @p frame once (a key of the meter), without making it the poll
  /// that write() keeps and repeats. False while the link is not ready.
  bool sendCommand(const QByteArray &frame);

  /// Shortest time between two repeated polls.
  static constexpr int kRepollMs = 300;
  /// After a key no poll for this long: a poll sent before the meter's
  /// acknowledgement can go unanswered, and the key took effect late
  /// (UT202BT; dmm-tools waits for the ack likewise, up to 415 ms seen)
  static constexpr int kKeyQuietMs = 600;

  /// Meters of @p format seen in a short scan, as "<address> <name>". Blocks
  /// for @p ms; when the scan fails, @p error (if given) says why.
  static QStringList scan(FrameFormat::DataFormat format, int ms = 6000, QString *error = nullptr);

Q_SIGNALS:
  /// The link is gone or could not be set up; the text says why.
  void finished(const QString &reason);
  /// Once per link, before it is ready: the info characteristic's value.
  void infoRead(const QByteArray &info);

private:
  /// Connects to @p info (from the scan, or the bare address).
  void connectTo(const QBluetoothDeviceInfo &info);
  void onServiceDiscovered(const QBluetoothUuid &uuid);
  void onDiscoveryFinished();
  void onServiceState();
  void setReady();
  void fail(const QString &reason);
  qint64 writeData(const char *data, qint64 len) override;

  QString m_address;
  std::optional<Profile> m_profile;
  QBluetoothDeviceDiscoveryAgent *m_finder = nullptr;
  QLowEnergyController *m_controller = nullptr;
  QLowEnergyService *m_service = nullptr;
  QLowEnergyCharacteristic m_writeChar;
  QTimer *m_connectTimeout = nullptr;
  QTimer *m_repoll = nullptr;
  QByteArray m_lastPoll;
  QElapsedTimer m_keySent;   ///< no poll for kKeyQuietMs after a key (sendCommand())
  QElapsedTimer m_pollSent;  ///< a poll out and not answered yet: no second one
  void sendPoll();
  QByteArray m_info;
  bool m_serviceFound = false;
  bool m_ready = false;
};
