// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QIODevice>
#include <QStringList>
#include <QThread>

#include "device/dmmdecoder.h"
#include "device/transports/hidreader.h"

/// Holtek HID sensors that are not a serial cable - today the TFA Dostmann
/// AIRCO2NTROL (USB 04d9:a052, CO2 / temperature / humidity).
///
/// Unlike HIDSerialDevice there is no UART behind the HID endpoint. The
/// device sends 8-byte input reports carrying one record each:
///
///   @0 address  0x50 CO2 [ppm], 0x42 temperature, 0x41 humidity
///               (others exist and are ignored)
///   @1 value, high byte
///   @2 value, low byte
///   @3 checksum, (@0 + @1 + @2) & 0xff
///   @4 0x0d
///
///   CO2 = value ppm, temperature = value / 16 - 273.15 degC,
///   humidity = value / 100 %.
///
/// The records arrive one at a time in no fixed order. Once one of each of
/// the three has been seen, this device turns the set into three text lines
/// - "50 <raw>\n", "42 <raw>\n", "41 <raw>\n", always in that order - which
/// DecoderTfaAirControl converts (FrameReader hands them over as frames 0, 1
/// and 2 of a reading, so the meter is configured with numValues 3). The
/// conversion lives in the decoder so it is testable and documented with the
/// other protocols; this class only frames the records.
///
/// A HidReader thread does the blocking hidapi reads, as for HIDSerialDevice.
/// Port string: "HIDHOLTEK 0x04d9:0xa052 <path>" (availablePorts()).
class HidHoltekDevice : public QIODevice
{
  Q_OBJECT
public:
  static constexpr unsigned short VendorId  = 0x04d9;
  static constexpr unsigned short ProductId = 0xa052;

  /// Record addresses (byte 0 of a report).
  enum Address : quint8
  {
    Humidity    = 0x41,
    Temperature = 0x42,
    Co2         = 0x50,
  };

  /// One record of an input report.
  struct Record
  {
    quint8  address = 0;
    quint16 raw = 0;
  };

  /// @param info   the meter (unused so far; the sensor has no line settings)
  /// @param device an entry from availablePorts(), or a bare HID path
  explicit HidHoltekDevice(const DmmDecoder::DMMInfo &info, const QString &device, QObject *parent = Q_NULLPTR);
  ~HidHoltekDevice() override;

  /// Appends the sensors found via hidapi, as "HIDHOLTEK 0xvvvv:0xpppp path".
  static bool availablePorts(QStringList &portlist);
  /// The path part of a port entry (a bare path is returned as it is).
  static QString pathForEntry(const QString &entry);
  /// Reads the record of one input report. False for a report that is too
  /// short or fails its checksum (reports of 3 bytes carry no checksum and
  /// are taken as they are). Pure, for tests.
  static bool parseReport(const QByteArray &report, Record &out);
  /// The text line DecoderTfaAirControl reads: "<address hex> <raw dec>\n".
  static QByteArray formatLine(quint8 address, quint16 raw);

  /// Fails when the hidapi handle could not be opened, so MeterConnection
  /// reports the reason (errorString()) instead of waiting for frames.
  bool open(OpenMode mode) override;
  void close() override;
  qint64 bytesAvailable() const override;
  bool isSequential() const override { return true; }

Q_SIGNALS:
  /// The sensor is gone: the read loop ended with an error while the device
  /// was open. MeterConnection reopens it later.
  void finished();

private:
  qint64 readData(char *data, qint64 maxSize) override;
  qint64 writeData(const char *data, qint64 len) override;
  /// Queued from the reader thread: one input report.
  void onReport(const QByteArray &raw);
  void onReadError(const QString &what);
  /// Stops the reader and waits for it; the reader closes the handle.
  void stopReader();

  bool m_isOpen = false;
  QString m_openError;
  hid_device *m_handle = Q_NULLPTR;   ///< owned by the reader once it runs
  QThread *m_thread = Q_NULLPTR;
  HidReader *m_reader = Q_NULLPTR;
  QByteArray m_rx;                    ///< text lines not yet read, main thread only
  /// The set being collected: latest raw value per quantity.
  quint16 m_co2 = 0, m_temperature = 0, m_humidity = 0;
  bool m_haveCo2 = false, m_haveTemperature = false, m_haveHumidity = false;
};
