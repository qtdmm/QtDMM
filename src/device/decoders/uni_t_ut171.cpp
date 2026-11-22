// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/decoders/uni_t_ut171.h"

#include "core/sampletypes.h"

#include <QCoreApplication>
#include <cmath>
#include <cstring>

static const bool registered = []() {
  // the UT-D09 cable at 9600 8N1, the UT171B/C also the UT-D07B adapter
  // (baud 0: the Bluetooth entry); none tried. The main value and the aux
  // display's (Hz beside V~, the parts of AC+DC, ...)
  DmmDecoder::addConfig({"Uni-Trend", "UT171A *", "", 9600, FrameFormat::UniTUT171, 8, 1, 1, 0, 40000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"Uni-Trend", "UT171B *", "", 9600, FrameFormat::UniTUT171, 8, 1, 1, 0, 60000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"Uni-Trend", "UT171C *", "", 9600, FrameFormat::UniTUT171, 8, 1, 1, 0, 60000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"Uni-Trend", "UT171B/C (UT-D07B Bluetooth) *", "", 0, FrameFormat::UniTUT171, 8, 1, 1, 0, 60000, 0, 0, 0, "", "", 2});
  return true;
}();

namespace
{
constexpr quint8 kMeasurement = 0x02;
constexpr int kMain = 11;   // type ... unit code (spec §5.1)
constexpr quint8 kNcv = 0x1A, kSquareWave = 0x1D;

// the unit codes of the main and the aux value (spec §5.6); the float is
// already in that unit
struct Unit
{
  const char *special;
  const char *prefix;
  const char *unit;
};
const Unit kUnits[0x20] = {
  { "DC", "", "V" },    { "AC", "", "V" },    { "ACDC", "", "V" },                         // 00-02
  { "DC", "m", "V" },   { "AC", "m", "V" },   { "ACDC", "m", "V" },                        // 03-05
  { "DC", "µ", "A" },   { "AC", "µ", "A" },   { "ACDC", "µ", "A" },                        // 06-08
  { "DC", "m", "A" },   { "AC", "m", "A" },   { "ACDC", "m", "A" },                        // 09-0B
  { "DC", "", "A" },    { "AC", "", "A" },    { "ACDC", "", "A" },                         // 0C-0E
  { "OH", "", "Ohm" },  { "OH", "k", "Ohm" }, { "OH", "M", "Ohm" },                        // 0F-11
  { "HZ", "", "Hz" },   { "HZ", "k", "Hz" },  { "HZ", "M", "Hz" },   { "DU", "", "%" },    // 12-15
  { "CA", "n", "F" },   { "CA", "µ", "F" },   { "CA", "m", "F" },                          // 16-18
  { "TE", "", "C" },    { "TE", "", "dF" },   { "DI", "", "V" },     { "BUZ", "", "Ohm" }, // 19-1C
  { "", "n", "S" },     { "NCV", "", "" },    { "", "m", "s" },                            // 1D-1F
};

// the value's own name by the mode, translated; null: by what it measures
const char *nameOf(quint8 mode)
{
  switch (mode)
  {
    case 0x01: return QT_TRANSLATE_NOOP("UT171", "LoZ");
    case 0x1E: return QT_TRANSLATE_NOOP("UT171", "VFC");
    case 0x1F: return QT_TRANSLATE_NOOP("UT171", "4-20 mA loop");
    default:   return nullptr;
  }
}

// the rows of each mode's range table (spec §5.4); 0: none, RANGE sends nothing
int rowsOf(quint8 mode)
{
  static const quint8 rows[0x20] = {
    0, 4, 4, 4, 4, 1, 1, 1, 1, 1, 6, 8, 1, 0, 1, 7,   // 00-0F
    0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 2, 2, 0, 2, 1,   // 10-1F
  };
  return mode < 0x20 ? rows[mode] : 0;
}

float floatAt(const QByteArray &d, int i)
{
  const quint32 bits = quint32(quint8(d[i])) | (quint32(quint8(d[i + 1])) << 8) | (quint32(quint8(d[i + 2])) << 16)
                       | (quint32(quint8(d[i + 3])) << 24);
  float f;
  std::memcpy(&f, &bits, sizeof f);
  return f;
}

// the float with the decimals the meter sends (1-4); with none, the
// shortest form that gives the float back, no exponent
QString shown(float f, int decimals)
{
  if (decimals >= 1 && decimals <= 4)
    return QString::number(double(f), 'f', decimals);
  const QString text = QString::number(double(f), 'g', 7);
  if (!text.contains('e'))
    return text;
  const int places = qMax(0, 6 - int(std::floor(std::log10(std::fabs(double(f))))));
  QString plain = QString::number(double(f), 'f', places);
  if (plain.contains('.'))
  {
    while (plain.endsWith('0'))
      plain.chop(1);
    if (plain.endsWith('.'))
      plain.chop(1);
  }
  return plain;
}

// a value: the float at @p at, its decimals/state byte and unit code after
// it (spec §5.5, §5.6); a unit code past the table gives no unit
void fill(DmmDecoder::DmmChannel &c, const QByteArray &d, int at, double (*factor)(const QString &))
{
  const quint8 format = quint8(d[at + 4]), code = quint8(d[at + 5]);
  const int decimals = format >> 4, state = format & 0x0F;
  const Unit u = code < 0x20 ? kUnits[code] : Unit{ "", "", "" };
  c.special = QString::fromLatin1(u.special);
  c.unit = QString::fromUtf8(u.prefix) + QString::fromUtf8(u.unit);
  const float v = floatAt(d, at);
  switch (state)
  {
    case 1:
      c.val = QStringLiteral("OL");
      c.dval = 0;
      return;
    case 2:
      c.val = QStringLiteral("-OL");
      c.dval = 0;
      return;
    case 3:   // dashes by the decimals: ----.- ... -.----
    {
      const int after = decimals >= 1 && decimals <= 4 ? decimals : 1;
      c.val = QString(5 - after, '-') + '.' + QString(after, '-');
      c.noValue = true;
      return;
    }
    case 4: c.val = QStringLiteral("LEAD"); c.noValue = true; return;
    case 5: c.val = QStringLiteral("DISC"); c.noValue = true; return;
    case 6: c.val = QStringLiteral("Lo");   c.noValue = true; return;
    case 7: c.val = QStringLiteral("Hi");   c.noValue = true; return;
    default: break;   // 0, and 8-15 where the software keeps the value
  }
  if (!std::isfinite(v))
  {
    c.val = QStringLiteral("?");
    c.noValue = true;
    return;
  }
  c.val = shown(v, decimals);
  c.dval = c.val.toDouble() * factor(QString::fromUtf8(u.prefix));
  if (c.unit.isEmpty())
    c.noValue = true;   // no unit code the table knows: no reading to plot
}
}

std::optional<DmmDecoder::DmmResponse> DecoderUT171::decode(const QByteArray &data, int id)
{
  const QByteArray d = payloadOf(data);
  // only the measurements; the replies (01 OK/ER/NO, 72) are skipped
  if (d.size() < 4 || quint8(d[0]) != kMeasurement)
    return std::nullopt;
  const quint8 a = quint8(d[1]), b = quint8(d[2]), mode = quint8(d[3]);
  if (mode != kNcv && d.size() < kMain)
    return std::nullopt;
  heard();

  m_result = {};
  m_result.hold = a & 0x80;
  m_result.range = (b & 0x01) ? "AUTO" : "MANU";
  // low battery: the software reads no bit for it; gulux's captures have
  // status A bit 2, dmm-tools reads it so (spec §10 cross-reference)
  m_result.lowBat = a & 0x04;
  DmmChannel &main = m_result.main();
  main.id = id;
  if (a & 0x10)
    main.states |= SampleFlag::Relative;
  if (a & 0x40)
    main.states |= SampleFlag::Peak;
  // the aggregate MAX/MIN shows: bit 6 alone MAX, bit 5 alone AVG, both MIN
  switch (b & 0x60)
  {
    case 0x40: main.states |= SampleFlag::Max; break;
    case 0x20: main.states |= SampleFlag::Avg; break;
    case 0x60: main.states |= SampleFlag::Min; break;
    default: break;
  }
  if (const char *name = nameOf(mode))
    main.name = QCoreApplication::translate("UT171", name);

  {
    QMutexLocker locker(&m_keyLock);
    m_seen = true;
    m_mode = mode;
    m_range = d.size() > 4 ? quint8(d[4]) : 0;
    m_auto = b & 0x01;
    m_rel = a & 0x10;
    m_peak = a & 0x40;
    m_shown.clear();
  }

  if (mode == kNcv)
  {
    // the software reads nothing after the mode; the meter shows EF or
    // dashes, a level without a number (spec §5.8)
    main.special = "NCV";
    main.val = QStringLiteral("----");
    main.noValue = true;
    return m_result;
  }
  if (mode == kSquareWave)
  {
    // the output's settings: frequency and duty (spec §5.9), as two values
    if (d.size() < 13)
      return std::nullopt;
    main.special = "HZ";
    main.unit = QStringLiteral("Hz");
    main.name = QCoreApplication::translate("UT171", "Square wave out");
    main.val = shown(floatAt(d, 5), 1);
    main.dval = main.val.toDouble();
    DmmChannel &duty = m_result.addChannel(id + 1);
    duty.special = "DU";
    duty.unit = QStringLiteral("%");
    duty.val = shown(floatAt(d, 9), 1);
    duty.dval = duty.val.toDouble();
    return m_result;
  }

  fill(main, d, 5, prefixFactor);
  if (!main.noValue && (main.val != "OL" && main.val != "-OL"))
  {
    QMutexLocker locker(&m_keyLock);
    m_shown = d.mid(5, 4);
  }
  // the aux block, the aux display's value: Hz beside V~, AC or DC part
  // of AC+DC, REL's reference ... (spec §5.2)
  if ((a & 0x01) && d.size() >= kMain + 6)
    fill(m_result.addChannel(id + 1), d, kMain, prefixFactor);
  return m_result;
}

QByteArray DecoderUT171::pollRequest() const
{
  // connect: the stream on while none comes (spec §4.3)
  return silent() ? command(0x0A, QByteArray(1, '\x01')) : QByteArray();
}

QByteArray DecoderUT171::keyRequest(const QString &key) const
{
  auto frame = [](quint8 cmd, QByteArray arg) { return command(cmd, arg); };
  if (key == QLatin1String("hold"))
    return frame(0x07, QByteArray(1, '\x5A'));
  if (key == QLatin1String("select2"))   // Hz%
    return frame(0x03, QByteArray(1, '\x5A'));
  if (key == QLatin1String("auto"))
    return frame(0x02, QByteArray(1, '\x00'));
  if (key == QLatin1String("minmax"))
    return frame(0x05, QByteArray(1, '\x01'));
  if (key == QLatin1String("minmax_off"))
    return frame(0x05, QByteArray(1, '\x00'));
  if (key == QLatin1String("peak"))
    return frame(0x06, QByteArray(1, '\x01'));
  if (key == QLatin1String("peak_off"))
    return frame(0x06, QByteArray(1, '\x00'));
  QMutexLocker locker(&m_keyLock);
  if (!m_seen)
    return QByteArray();   // the keys below need a reading first
  if (key == QLatin1String("range"))
  {
    // from auto the row shown (the meter's own key goes manual there), in
    // manual the next one, after the last the first; none without rows
    const int rows = rowsOf(m_mode);
    if (rows == 0)
      return QByteArray();
    const bool known = m_range >= 1 && m_range <= rows;
    const int row = !known ? 1 : m_auto ? m_range : m_range % rows + 1;
    return frame(0x02, QByteArray(1, char(row)));
  }
  if (key == QLatin1String("rel"))
  {
    // the meter's REL takes the reading shown: the software's command with
    // that float; while REL is on, leave it
    if (m_rel)
      return frame(0x04, QByteArray(1, '\x00'));
    return m_shown.size() == 4 ? frame(0x04, QByteArray(1, '\x01') + m_shown) : QByteArray();
  }
  return QByteArray();
}

bool DecoderUT171::hasKey(const QString &key, const QString &model) const
{
  Q_UNUSED(model);
  static const QStringList keys = { "range", "auto", "hold", "rel", "minmax", "minmax_off", "peak", "peak_off",
                                    "select2" };
  return keys.contains(key);
}
