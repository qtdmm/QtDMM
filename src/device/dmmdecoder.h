#pragma once

#include <QtCore>
#include <QString>
#include <QByteArray>
#include <optional>

#include "device/frameformat.h"

/// Base class of all protocol decoders and registry of the supported meters.
///
/// A decoder turns one frame of meter bytes into a DmmResponse. Each protocol
/// (FrameFormat::DataFormat) has one subclass in src/decoders/; that file also
/// registers the meters speaking the protocol with addConfig() from a static
/// initialiser, so adding a meter is a one-line change there and the
/// supported-devices page (tests/generate_docs.py) picks it up.
///
/// How a frame reaches decode(): FrameReader feeds every received byte into
/// a ring buffer and calls checkFormat() on it; once that returns true the
/// last getPacketLength() bytes are handed to decode().
///
/// The frame formats are documented in docs/protocols/.
class DmmDecoder : public QObject
{
  Q_OBJECT

public:
  /// One value of a decoded frame: the main display or a secondary one.
  ///
  /// Contract for the value triple - decoders MUST follow this, because the
  /// display and the recorder consume the two forms differently:
  ///
  ///   - val, unit: display form, unit carries the SI prefix ("1.234", "kOhm")
  ///   - dval: the same measurement in SI *base* units (1234.0)
  ///
  /// LcdWidget shows val+unit verbatim, while GraphWidget plots dval and strips
  /// the prefix off the unit for its axis label (GraphWidget::setUnit). A decoder
  /// that leaves dval unscaled therefore records values whose magnitude jumps
  /// by the prefix factor whenever the meter changes range.
  ///
  /// Use formatResultValue() to get this right; it inserts the decimal point,
  /// scales dval and assembles unit in one step.
  ///
  /// Not to be confused with PortKey::channel, the meter's input: a DmmChannel
  /// is one of the values it shows, SCPI channel id + 1.
  struct DmmChannel
  {
    int id = 0;         ///< which value this is: 0 = main display, 1.. secondary ones
    double dval = 0;    ///< value in SI base units, what the graph and recorder use
    QString val;        ///< value as shown on the meter's display, e.g. "-1.234" or "0.L";
                        ///< empty for a secondary display that is off, so it is cleared
    QString unit;       ///< display unit including SI prefix, e.g. "kOhm", "mV"
    QString special;    ///< coupling/function: "AC", "DC", "ACDC", "DI", "BUZ", ...
    quint32 states = 0; ///< other annunciators of this value: SampleFlag
                        ///< Max, Min, Relative, Peak (the meter's own keys)
    /// The meter shows no number for this value ("----", blank, "--"): val
    /// has what it shows, dval is not used. A gap, not an overload.
    bool noValue = false;
    /// The meter shows a fault word instead of a number ("FUSE", "Er",
    /// "PROBE", "BATT"): the word, also in val. A gap and a message.
    QString fault;
    /// The meter's own name of this value, translated ("Battery voltage");
    /// empty when it has none (the views then name it by what it measures).
    QString name;
  };

  /// A decoded frame: its values (channels[0] the main display) and the
  /// annunciators that belong to the whole meter.
  class DmmResponse
  {
  public:
    QVector<DmmChannel> channels{DmmChannel{}}; ///< always at least the main value
    QString range;    ///< "AUTO", "MANU" or empty
    bool hold = false;    ///< the meter's HOLD annunciator is on
    bool showBar = false; ///< the meter shows a bar graph for this reading
    bool lowBat = false;  ///< low battery indicator
    QString error;    ///< non-empty when the frame was understood but flagged an error
    qint64 received = 0;     ///< Sample::now() when the frame came in (FrameReader); 0 = not known, take the time of adapting
    qint64 receivedWall = 0; ///< the same as ms since the epoch

    /// The main value, channels[0]. A meter that sends one frame per value
    /// (AirCO2ntrol) has only this one and tells which value it is by its id.
    DmmChannel &main() { return channels[0]; }
    const DmmChannel &main() const { return channels[0]; }
    /// Appends a secondary value with @p id; the decoder fills it in. A
    /// meter with a secondary display adds it with an empty val while the
    /// display is off, so the views clear it.
    DmmChannel &addChannel(int id)
    {
      channels.append(DmmChannel{});
      channels.last().id = id;
      return channels.last();
    }
  };

  /// Describes one supported meter: what the settings page fills in when the
  /// model is chosen. Registered by the decoder files via addConfig().
  class DMMInfo
  {
  public:
    QString vendor;                    ///< manufacturer as shown in the model list
    QString model;
    QString name;                      ///< "vendor model", filled in by addConfig()
    int   baud{0};                     ///< 600 ... 19200
    FrameFormat::DataFormat protocol{FrameFormat::Invalid}; ///< which decoder handles the meter
    int   bits{8};                     ///< data bits, 5..8
    int   stopBits{1};                 ///< 1 or 2
    int   framesPerReading{1};         ///< frames per reading, for meters that send one line per value (Metex)
    int   parity{0};                   ///< 0 none, 1 even, 2 odd
    int   display{0};                  ///< display counts: 2000, 4000, 6000, ... 1000000
    bool  externalSetup{false};        ///< true: leave the port settings alone, the user configured it
    bool  rts{false};                  ///< drive RTS high (some cables are powered from it)
    bool  dtr{false};                  ///< drive DTR high
    QString sigrokExe{""};             ///< sigrok-cli path for SigrokDevice, empty otherwise
    QString sigrokDriver{""};          ///< libsigrok driver of a bench meter read through sigrok-cli ("scpi-dmm"), empty otherwise
    int   channels{0};                 ///< values the meter shows (main + secondary displays); 0 = framesPerReading

    /// How many values the meter has: channels, or one per frame.
    int valueCount() const { return channels > 0 ? channels : framesPerReading; }
  };

  explicit DmmDecoder(FrameFormat::DataFormat df);
  virtual ~DmmDecoder() = default;
  /// Fixed length of one frame in bytes. 0 means variable length: the frame
  /// is everything received since the previous one (a line protocol whose
  /// checkFormat() fires on the terminator, see DecoderFlukeQM).
  virtual size_t                     getPacketLength() = 0;
  /// Frame delimiter: true when the ring buffer @p data, whose newest byte is
  /// at index @p len, ends with a complete frame (typically checked on the
  /// terminator and the high nibbles of the VC820-style byte counters).
  virtual bool                       checkFormat(const char* data, size_t len) = 0; // TBD use qbytearray or similar instead for data
  /// For checkFormat(): the byte @p back places before the newest one in the
  /// reader's ring buffer; back 0 is data[idx], the byte just received.
  static unsigned char               ringByte(const char *data, size_t idx, int back)
  {
    return static_cast<unsigned char>(data[(idx + FIFO_LENGTH - back) % FIFO_LENGTH]);
  }
  /// Appends the 16-bit sum of all bytes of @p frame, high byte first (the
  /// UNI-T Bluetooth frames).
  static void                        appendSum16(QByteArray &frame);
  /// True when the last two of the @p n bytes of @p f are the 16-bit sum of
  /// the others, high byte first.
  static bool                        sum16Valid(const unsigned char *f, int n);
  /// For checkFormat(): the last @p n bytes of the ring buffer, oldest first.
  static void                        lastBytes(const char *data, size_t idx, int n, unsigned char *out)
  {
    for (int i = 0; i < n; ++i)
      out[i] = ringByte(data, idx, n - 1 - i);
  }
  /// Bytes to send to the meter to make it emit a frame; empty for meters
  /// that stream on their own. FrameReader writes it once per read cycle.
  virtual QByteArray pollRequest() const { return QByteArray(); }
  /// Bytes that press the meter's key @p key once (ControlBar's names,
  /// keyNames()); empty when the meter has no such key, cannot be
  /// remote-controlled or the key needs a reading first.
  virtual QByteArray keyRequest(const QString &key) const { Q_UNUSED(key); return QByteArray(); }
  /// True when the meter @p model (DMMInfo::model) has the key @p key. By
  /// default every key keyRequest() knows; a decoder for several meters with
  /// different keypads, or one whose keys need a reading first (a unit key
  /// steps from the unit shown), tells it by the model.
  virtual bool hasKey(const QString &key, const QString &model) const
  {
    Q_UNUSED(model);
    return !keyRequest(key).isEmpty();
  }
  /// Every key name keyRequest() may know, in the order of the meters' keypads.
  static QStringList keyNames()
  {
    return { "fn_v", "fn_a", "fn_a_long", "fn_ohm", "fn_ncv", "fn_ncv_long", "fn_auto", "fn_cap", "fn_diode",
             "fn_hz", "select1", "select2", "range", "auto", "hold", "rel", "zero", "minmax", "minmax_off", "peak",
             "peak_off", "max", "min", "avg", "unit", "unit2", "weighting", "lamp" };
  }
  /// True when the meter tells which model of the protocol it is: its frames
  /// (ZotekBLE: the layout, modelsFromFrame()) or its device information
  /// (OwonBLE: modelsFromInfo()).
  virtual bool tellsModel() const { return false; }
  /// The meter chosen (DMMInfo::model), for a protocol whose models read a
  /// frame differently; set before the first frame, from the GUI thread.
  virtual void setModel(const QString &model) { Q_UNUSED(model); }
  /// The models (DMMInfo::name) a meter that sent @p frame can be; empty
  /// when the frames do not tell (tellsModel()).
  virtual QStringList modelsFromFrame(const QByteArray &frame) const { Q_UNUSED(frame); return {}; }
  /// The models a Bluetooth meter can be by @p info, the value of its
  /// device-information characteristic (BleGattDevice::Profile::info, OWON:
  /// the model code); empty when it does not tell.
  virtual QStringList modelsFromInfo(const QByteArray &info) const { Q_UNUSED(info); return {}; }
  /// Decodes one frame (getPacketLength() bytes, or a variable-length one
  /// with its terminator). @p id tells which of
  /// DMMInfo::framesPerReading frames this is. Returns nothing when the frame is
  /// corrupt; the caller then keeps the previous reading.
  virtual std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) = 0;

  FrameFormat::DataFormat getType() { return m_type; };
  /// Short protocol name for --debug output and the device table.
  QString name() const { return m_name; };

  /// All meters registered with addConfig(), in registration order.
  static std::vector<DMMInfo>        getDeviceConfigurations();
  /// Registers a meter; called from the decoder files' static initialisers.
  static void                        addConfig(DMMInfo info);
  /// True when two model names ("vendor model") name the same meter. A
  /// trailing " *" (not yet confirmed on hardware) is ignored, so a saved
  /// setting keeps its meter when the mark goes away after a confirmation;
  /// a renamed model's old name (canonicalModel()) is the new one.
  static bool                        sameModel(const QString &a, const QString &b);
  /// A model name as registered now: "QtDMM Virtual meter" (before 26.2)
  /// is "QtDMM Simulated meter".
  static QString                     canonicalModel(const QString &name);
  /// Factory: the decoder for a protocol, null for FrameFormat::Invalid.
  static std::shared_ptr<DmmDecoder> getInstance(FrameFormat::DataFormat df);
  /// Factory by protocol name (FrameFormat::toString()).
  static std::shared_ptr<DmmDecoder> getInstance(QString df);


protected:
  /// Decimal point for the 4-digit decoders (VC820, DTM0660) whose value
  /// string is "<sign><blank>dddd": inserts it after @p pos digits.
  QString insertComma(const QString &val, int pos);
  /// Inserts the decimal point after @p pos digits, skipping a leading sign
  /// or blank ("-1234", 2 -> "-12.34"). pos 0 leaves the string unchanged.
  QString insertCommaIT(const QString &val, int pos);
  /// Scale factor for an SI prefix ("k" -> 1e3). Unknown prefixes yield 1.0.
  static double prefixFactor(const QString &prefix);
  /// m_result.main().val and .unit from @p number in E notation ("+12.345E+6") and
  /// its @p value: with the meter's digits ("12.345", "M" + unit) when the
  /// exponent is a multiple of three from @p lowestExponent (at least -12) up
  /// to 9, otherwise as SiPrefix::format() puts it (the SCPI-like answers of
  /// the Fluke meters).
  void setFromENotation(const QString &number, double value, const QString &unit, int lowestExponent);
  /// Finishes m_result from the raw digit string in m_result.main().val: inserts
  /// the decimal point at @p commaPos, sets unit to prefix+baseUnit and dval
  /// to the value scaled by the prefix. See the DmmChannel contract.
  void formatResultValue(int commaPos, const QString& prefix, const QString& baseUnit);
  /// For decoders without a checksum, at the end of decode(): a function or
  /// range byte they do not know set no unit. That frame is damaged, its digits
  /// are no reading; without this they would count as a valid 0.
  void noValueWithoutUnit();
  /// Tests bit @p bit (0 = LSB) of byte @p byte of the frame.
  bool bit(const QByteArray &data, int byte, int bit) const;
  /// Copies the ASCII digits data[first..last] into a string, with an
  /// optional leading minus.
  QString makeValue(const QByteArray &data, int first, int last, bool neg=false);

  static std::vector<DMMInfo> *m_configurations;
  DmmResponse m_result;
  FrameFormat::DataFormat m_type;
  QString m_name;
};




