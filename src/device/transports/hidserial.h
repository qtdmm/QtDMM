#pragma once

#include <QtCore>
#include <QIODevice>
#include <QThread>

#include "device/transports/hidreader.h"
#include "device/dmmdecoder.h"

/// A meter behind a USB HID "serial" cable.
///
/// These cables enumerate as HID devices rather than serial ports. Three
/// chips are known (Chip):
///  - WCH CH9325 / Hoitek HE2325U (UT-D04 and older UNI-T cables, many
///    others): the UART speed is set with a feature report, the meter's
///    bytes then arrive in 8-byte input reports (0xF0 | count, up to 7
///    payload bytes with the top bit set).
///  - Silicon Labs CP2110 (UNI-T UT-D09 first revision, 10c4:ea80): UART
///    enabled and configured with two feature reports (0x41, 0x50), data in
///    reports whose id is the byte count (1..63). Per sigrok's
///    serial_hid_cp2110.c / SiLabs AN434.
///  - WCH CH9329 in custom-HID mode (UNI-T UT-D09 second revision,
///    1a86:e429): a plain UART tunnel, fixed 9600 8N1, no feature report,
///    64-byte input reports (count, then up to 63 raw bytes). Layout per
///    sigrok PR #298.
///  - Brymen BU-86X infrared adapter (0820:0001): raw 8-byte reports that
///    are nothing but UART bytes, fixed speed, no configuration. The meter
///    only answers to a request (DmmDecoder::pollRequest()), so this is the
///    one cable QtDMM writes to.
/// Both UT-D09 revisions look alike; lsusb tells them apart.
/// A HidReader in its own thread blocks in hid_read_timeout() and hands each
/// report over as a queued signal; this object stays in the main thread,
/// unpacks the UART bytes into a buffer and serves FrameReader through
/// readData()/bytesAvailable() with readyRead(). The line configuration
/// (feature reports) is sent in the constructor, so a cable that refuses it
/// fails open() at once.
class HIDSerialDevice : public QIODevice {
    Q_OBJECT
public:
  /// The cable chip, which decides the report layout.
  enum class Chip { CH9325, CP2110, CH9329, BU86X };

  /// @param info   the meter, for its baud rate and data bits
  /// @param device an entry from availablePorts(): "HID 0xvvvv:0xpppp path"
  ///               (the vid:pid selects the Chip; a bare path means CH9325)
  /// @param p      parent object
  explicit HIDSerialDevice(const DmmDecoder::DMMInfo info, QString device, QObject *p = Q_NULLPTR);
 ~HIDSerialDevice();

  /// Chip for a port entry / vid:pid; unknown ids are treated as CH9325.
  static Chip chipFor(unsigned short vendorId, unsigned short productId);
  static Chip chipForEntry(const QString &entry);
  /// The path part of a port entry ("HID 0x1a86:0xe008 /dev/hidraw2" -> "/dev/hidraw2").
  static QString pathForEntry(const QString &entry);
  /// Extracts the UART bytes of one input report into @p out (at least 63
  /// bytes); returns the count, -1 for a malformed report. Pure, for tests.
  static int unpackReport(Chip chip, const unsigned char *report, int reportLen, unsigned char *out);
  /// The output report(s) that carry @p data to the cable, report id
  /// placeholder included; empty when the chip cannot send (CH9325).
  static QByteArray packWrite(Chip chip, const QByteArray &data);
  /// The CP2110 UART_CONFIG feature report (9 bytes incl. report id 0x50)
  /// for the given line settings; parity 0 none / 1 even / 2 odd. Pure.
  static QByteArray cp2110ConfigReport(int baud, int bits, int parity, int stopBits);
  /// The CH9325 feature report (6 bytes incl. report id 0): baud little
  /// endian, data bits as (bits - 5); parity/stop bits are not settable. Pure.
  static QByteArray ch9325ConfigReport(int baud, int bits);
  /// Chip name as used in tests/data/hid_cables.json and the bridge.
  static QString chipName(Chip chip);
  static Chip chipFromName(const QString &name, bool *ok = nullptr);
  Chip chip() const { return m_chip; }

  /// Appends the known cable chips found via hidapi to @p portlist.
  static bool availablePorts(QStringList &portlist);
  /// Fails when the hidapi handle could not be opened, so MeterConnection reports an
  /// error instead of waiting for frames that never come.
  bool open(OpenMode mode) override;
  /// True once the cable has delivered an input report (even an empty one),
  /// i.e. the cable itself is configured and alive.
  bool cableAnswers() const { return m_reportsSeen > 0; }
  /// True once an input report carried UART bytes. A cable that answers but
  /// never carries data means the meter is not sending (UNI-T: RS232 button).
  bool dataSeen() const { return m_dataSeen; }
  void close() override;

  /// Bytes waiting for readData(). Must stay const: QIODevice's version
  /// is const and virtual, so a non-const one would hide instead of override
  /// it and FrameReader (holding a QIODevice*) would get the base version.
  qint64 bytesAvailable() const override;

Q_SIGNALS:
  /// The cable is gone: the read loop ended with an error while the device
  /// was open. MeterConnection reopens it later.
  void finished();

protected:
  static bool availablePorts(QStringList &portlist,unsigned short vendor_id, unsigned short product_id);
  /// Sends the chip's line configuration (feature reports); false on failure.
  bool configureCable();
  /// Queued from the reader thread: unpack one report into m_rx.
  void onReport(const QByteArray &raw);
  void onReadError(const QString &what);
  /// Stops the reader and waits for it; the reader closes the handle.
  void stopReader();

  DmmDecoder::DMMInfo m_dmmInfo;
  Chip m_chip = Chip::CH9325;
  bool m_isOpen = false;
  int m_reportsSeen = 0;
  bool m_dataSeen = false;
  hid_device *m_handle = Q_NULLPTR;   ///< owned by the reader once it runs
  QThread *m_thread = Q_NULLPTR;
  HidReader *m_reader = Q_NULLPTR;
  QByteArray m_rx;                    ///< UART bytes received, main thread only

  qint64 readData(char *data, qint64 maxSize)  override;
  qint64 writeData(const char *data, qint64 len) override;
};
