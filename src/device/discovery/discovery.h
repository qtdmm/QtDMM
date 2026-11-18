// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <memory>

#include "device/frameformat.h"

class DmmDecoder;
class FrameReader;
class MdnsBrowser;
class QIODevice;

/// One thing a search found: a meter, a cable of a meter family, a serial
/// port, a bridge - with what is known about which meters it can be.
struct Candidate
{
  enum Kind
  {
    UsbCable,    ///< a USB-HID cable or sensor of a known family (VID:PID)
    Serial,      ///< a serial port, a USB-serial adapter
    Bluetooth,   ///< a Bluetooth LE meter or Victron device
    Network      ///< a qtdmm-bridge port in the network
  };

  Kind        kind = Serial;
  QString     key;       ///< identifies it across a search (path, address)
  QString     title;     ///< "UNI-T UT61B+/D+/E+ or UT161"
  QString     detail;    ///< "UT-D09 cable · /dev/hidraw3"
  QString     hint;      ///< what to do at the meter ("press RS232")
  /// The meter keys that say where it is: Port settings/device, or
  /// Port settings/ble-address for Bluetooth.
  QVariantMap keys;
  /// The models (DMMInfo::name) it can be, the likeliest first; empty: any
  /// model with a serial protocol.
  QStringList models;
  /// Why it cannot be used yet ("/dev/hidraw3 belongs to root"); empty: fine.
  QString     problem;
  /// How to fix the problem, with the commands.
  QString     fix;
  /// A protocol whose frames tell the models apart (DmmDecoder::tellsModel()):
  /// a FrameProbe on it finds out which of models it is. Invalid: none.
  FrameFormat::DataFormat probe = FrameFormat::Invalid;
};
Q_DECLARE_METATYPE(Candidate)

/// One way of searching. start() reports each find as found() as soon as it
/// is known and ends with finished(); stop() ends it early.
class Discoverer : public QObject
{
  Q_OBJECT
public:
  using QObject::QObject;
  virtual void start() = 0;
  virtual void stop() {}
  /// Why the search stopped early, empty when it ran to its end (asked
  /// after finished()).
  QString     error() const { return m_error; }

Q_SIGNALS:
  void        found(const Candidate &candidate);
  void        finished();

protected:
  QString     m_error;
};

/// The meter families a cable or sensor stands for, and the hints.
namespace Families
{
/// A USB-HID cable or sensor by VID:PID; nullopt for an unknown one.
struct Family
{
  QString     title;
  QString     cable;       ///< "UT-D04", empty for a sensor
  QString     hint;
  QString     portType;    ///< "HID" or "HIDHOLTEK"
  QStringList models;
};
std::optional<Family> forUsb(quint16 vid, quint16 pid);
/// The chip of a USB-serial adapter ("FTDI", "CH340", "PL2303", "CP210x"),
/// empty when unknown.
QString     adapter(quint16 vid, quint16 pid);
/// The maker of a microcontroller board with its own USB ("Arduino",
/// "Raspberry Pi Pico", "ESP32", "Teensy", ...), by VID (and PID); empty for
/// anything else - a board with a CH340, CP210x or FTDI chip says only that.
QString     board(quint16 vid, quint16 pid);
/// The models (DMMInfo::name) with @p format, the serial ones (baud > 0)
/// or the others; all serial models for FrameFormat::Invalid.
QStringList models(FrameFormat::DataFormat format, bool serial);
/// The models of @p vendor with a serial protocol.
QStringList vendorModels(const QString &vendor);
/// @p models with the one that fits the device's own name first: an
/// "APPA S1" is the "APPA S1 *", a "BENNING MM10-1" the "BENNING MM 10-1 *"
/// (spaces, " *" and " (Bluetooth)" aside); otherwise by the first word, a
/// Victron "SmartShunt HQ2203" is the "Victron SmartShunt".
QStringList preferByName(QStringList models, const QString &deviceName);
}

/// Why QtDMM cannot open @p path, and how to fix it; empty when it can.
/// @p vid and @p pid go into the udev rule of a HID device.
struct AccessProblem
{
  QString problem;
  QString fix;
};
AccessProblem accessProblem(const QString &path, bool hid, quint16 vid = 0, quint16 pid = 0);
/// The udev rule that lets the logged-in user open the HID cables and
/// sensors QtDMM knows (what the packages install).
QString udevRules();

/// USB-HID cables and sensors of the known families (hidapi).
class UsbDiscoverer : public Discoverer
{
  Q_OBJECT
public:
  using Discoverer::Discoverer;
  void        start() override;
};

/// Serial ports, with the chip of a USB-serial adapter.
class SerialDiscoverer : public Discoverer
{
  Q_OBJECT
public:
  using Discoverer::Discoverer;
  void        start() override;
};

/// qtdmm-bridge ports in the network (mDNS).
class BridgeDiscoverer : public Discoverer
{
  Q_OBJECT
public:
  using Discoverer::Discoverer;
  void        start() override;
  void        stop() override;

private:
  MdnsBrowser *m_browser = nullptr;
};

/// Listens to a meter for its first frame and reports the models the frame
/// names (DmmDecoder::modelsFromFrame()), or those its device information
/// names (DmmDecoder::modelsFromInfo()): which of the meters that share a
/// Bluetooth name it is. Takes the port, opens it and closes it when done.
class FrameProbe : public QObject
{
  Q_OBJECT
public:
  FrameProbe(QIODevice *port, FrameFormat::DataFormat format, QObject *parent = nullptr);
  ~FrameProbe() override;
  /// Opens the port and listens, at most @p timeoutMs.
  void        start(int timeoutMs = kTimeoutMs);
  /// A Bluetooth link takes about five seconds to set up, then the ZOTEK
  /// meters send three packets a second.
  static constexpr int kTimeoutMs = 15000;
  /// The port of a Bluetooth find: a GATT link to its address for its first
  /// model's protocol; null in a build without Bluetooth.
  static QIODevice *blePort(const Candidate &candidate);

Q_SIGNALS:
  /// Once: the models, the likeliest first; empty when no frame came in
  /// time, the link failed or the frame names none.
  void        finished(const QStringList &models);

private:
  void        finish(const QStringList &models);

  QIODevice  *m_port = nullptr;
  FrameReader *m_reader = nullptr;
  std::shared_ptr<DmmDecoder> m_decoder;
  QTimer      m_timeout;
  bool        m_done = false;
};

/// Bluetooth LE: one scan for the GATT meters and the Victron devices.
/// Without Bluetooth support it finds nothing; available() says why.
class BleDiscoverer : public Discoverer
{
  Q_OBJECT
public:
  explicit BleDiscoverer(QObject *parent = nullptr);
  ~BleDiscoverer() override;
  void        start() override;
  void        stop() override;
  /// Empty when a scan is possible, else why not: a build without
  /// Bluetooth, no adapter (or no Bluetooth service), the adapter off.
  static QString unavailable();

private:
  struct Private;
  Private    *d;
};
