// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>

#include <optional>

#include "device/dmmdecoder.h"

/// BTHome v2 (bthome.io): an open format for sensors that broadcast their
/// values in Bluetooth LE advertisements - ESP32 and other boards
/// (ArduinoBLE with the BtHomeV2Device library, ESPHome), Shelly
/// sensors. The values are the service data of UUID 0xFCD2: a device info
/// byte (bit 0 encrypted, bit 2 sent on a trigger, bits 5-7 the version,
/// 2), then objects, each an id and its value, little endian, in the
/// order of their ids: 02 fa 03 is a temperature of 0x03fa x 0.01 °C =
/// 10.18 °C.
///
/// BleAdvertisementDevice listens to the advertisements of one address
/// and hands each new service data over as a line "BTHOME <hex>"; this
/// decoder turns it into up to four values, the sensor's objects in their
/// order (the first the main value), named by what they are
/// ("Temperature", "Count"). The packet id (0x00) only tells a repeated
/// advertisement from a new one (the transport drops repeats); device
/// information (0xF0-0xF2), text, raw data and events give no value. An
/// object QtDMM does not know ends the advertisement: its length is not
/// known. An encrypted advertisement (a bindkey) is not read yet: the
/// error says so. Spec and vectors in docs/protocols/spec/bthome.yaml.
class DecoderBTHome : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderBTHome(FrameFormat::DataFormat df) : DmmDecoder(df) { m_name = "BTHome"; }

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  bool checkFormat(const char *data, size_t idx) override { return data[idx] == '\n'; }
  size_t getPacketLength() override { return 0; }

  /// The service data UUID of BTHome.
  static constexpr quint16 kServiceUuid = 0xFCD2;

  /// One object of an advertisement.
  struct Object
  {
    quint8  id = 0;
    QString name;       ///< "Temperature", translated
    double  value = 0;  ///< scaled: 10.18 for a temperature
    int     decimals = 0;
    QString unit;       ///< QtDMM's spelling ("C" for °C, "%RH", "kWh")
    QString special;    ///< the decoder's mode code ("TE", "DC", ...)
    bool    isValue = true;   ///< false: packet id, device information, events, text
  };
  /// An advertisement taken apart.
  struct Advertisement
  {
    int     version = 0;
    bool    encrypted = false;
    bool    trigger = false;
    std::optional<int> packetId;
    QList<Object> objects;
    bool    complete = true;   ///< false: an unknown object ended it
  };
  /// The service data @p serviceData (without the UUID); nullopt when it is
  /// too short to be one.
  static std::optional<Advertisement> parse(const QByteArray &serviceData);
  /// The line the transport hands over for @p serviceData.
  static QByteArray frame(const QByteArray &serviceData);
};
