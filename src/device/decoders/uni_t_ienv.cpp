// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/decoders/uni_t_ienv.h"

#include <algorithm>
#include <cmath>

#include <QHash>
#include <QRegularExpression>

#include "core/sampletypes.h"

// The display counts are the frame's digits, not a manual's ranges. Only the
// UT333BT has been tried with QtDMM, so the others keep the "*".
static const bool registered = []() {
  DmmDecoder::addConfig({"Uni-Trend", "UT333BT", "", 0, FrameFormat::UniTiENV, 8, 1, 1, 0, 1000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"Uni-Trend", "UT353BT", "", 0, FrameFormat::UniTiENV, 8, 1, 1, 0, 10000, 0, 0, 0});
  DmmDecoder::addConfig({"Uni-Trend", "UT363BT", "", 0, FrameFormat::UniTiENV, 8, 1, 1, 0, 10000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"Uni-Trend", "UT383BT", "", 0, FrameFormat::UniTiENV, 8, 1, 1, 0, 100000, 0, 0, 0});
  return true;
}();

namespace
{
// frame byte 4: the measurement channel, one per frame
constexpr quint8 kFnTemp = 0x30;     // UT333BT / UT363BT: the second value
constexpr quint8 kFnHumidity = 0x34; // UT333BT
constexpr quint8 kFnWind = 0x37;     // UT363BT
constexpr quint8 kFnLight = 0x3A;    // UT383BT
constexpr quint8 kFnSound = 0x3B;    // UT353BT

// frame byte 15 on the sound meter, the vendor app's constants (Slow=51,
// Fast=52); UNVERIFIED
constexpr quint8 kModeSlow = 0x33;
constexpr quint8 kModeFast = 0x34;

// The unit ids: frame byte 14 and the SET payload share one id space. "next"
// is what a unit key selects (a ring, so the wind speed's five units can be
// stepped through). "FC", "dBC" and the wind units are the vendor app's, not
// captured.
//
// The display text is in the base unit (°C, m/s, lx) whatever unit the meter
// shows: the unit id only tells which one it shows, the value has to be
// converted (scale, then offset), as UNI-T's app does. Tried with °F on a
// UT333BT; the wind and light units follow the app (its mph factor is a
// little off, the exact one is used here).
struct UnitInfo
{
  quint8 id;
  const char *name;
  quint8 next;
  double scale = 1;
  double offset = 0;
};
const UnitInfo kUnits[] = {
  { 0x30, "°C",     0x31 }, { 0x31, "°F",  0x30, 1.8, 32 },
  { 0x32, "%RH",    0x33 }, { 0x33, "%",   0x32 },
  { 0x34, "m/s",    0x35 }, { 0x35, "km/h", 0x36, 3.6 }, { 0x36, "ft/min", 0x37, 60 / 0.3048 },
  { 0x37, "knots",  0x38, 3600 / 1852.0 }, { 0x38, "mph", 0x34, 3600 / 1609.344 },
  { 0x3B, "lx",     0x3C }, { 0x3C, "fc",  0x3B, 1 / 10.7639104 },
  { 0x3D, "dBA",    0x3E }, { 0x3E, "dBC", 0x3D },
};

const UnitInfo *findUnit(quint8 id)
{
  for (const UnitInfo &u : kUnits)
    if (u.id == id)
      return &u;
  return nullptr;
}

// @p base in the unit @p u, as text with the meter's decimals, one more or
// fewer for each factor of 10 in the scale (23.4 °C -> 74.1 °F, 9.9 °C ->
// 49.8 °F, 1234 lx -> 114.6 fc, 1.23 m/s -> 242 ft/min)
QString convertText(double base, const QString &text, const UnitInfo &u, double *value)
{
  *value = base * u.scale + u.offset;
  const qsizetype point = text.indexOf('.');
  int decimals = 0;
  if (point >= 0)
    for (qsizetype i = point + 1; i < text.size() && text[i].isDigit(); ++i)
      ++decimals;
  decimals -= int(std::lround(std::log10(u.scale)));
  return QString::number(*value, 'f', std::max(0, decimals));
}

const QByteArray kPoll(1, char(0x5E));
}

QByteArray DecoderUniTiENV::pollRequest() const
{
  return kPoll;
}

// SET frame: AA BB 04 30 <payload> <16-bit BE sum of the first five>
// (HOLD: AA BB 04 30 42 01 DB)
QByteArray DecoderUniTiENV::commandFrame(quint8 payload)
{
  QByteArray f = QByteArray::fromHex("aabb0430");
  f.append(char(payload));
  appendSum16(f);
  return f;
}

// HOLD is confirmed on a UT333BT. MAX/MIN/AVG are the protocol's ids; what
// repeated presses do is UNVERIFIED (the status byte shows it). "unit" steps
// the main value's unit, "unit2" the temperature's, "weighting" the sound
// meter's Fast/Slow. A unit key sends nothing before a frame has told the
// current unit.
QByteArray DecoderUniTiENV::keyRequest(const QString &key) const
{
  static const QHash<QString, quint8> codes = {
    { "hold", 0x42 }, { "max", 0x3F }, { "min", 0x40 }, { "avg", 0x41 },
  };
  const auto it = codes.constFind(key);
  if (it != codes.constEnd())
    return commandFrame(it.value());

  auto stepUnit = [](quint8 cur) -> QByteArray {
    const UnitInfo *u = findUnit(cur);
    return u ? commandFrame(u->next) : QByteArray();
  };

  if (key == "unit")
    return stepUnit(m_mainUnit);
  if (key == "unit2")
    return stepUnit(m_tempUnitByte);
  if (key == "weighting")
    return commandFrame(m_weighting == kModeFast ? 0x44 : 0x43); // Slow after Fast, else Fast
  return QByteArray();
}

// The keys of UNI-T's app per meter: it sends HOLD, MAX and MIN, on the
// UT363BT HOLD, MAX and AVG (its other mode buttons send nothing). Fast/Slow
// only on the sound meter, a temperature unit only on the UT333BT.
// The UT333BT has no unit key for the humidity (%RH only; tried).
bool DecoderUniTiENV::hasKey(const QString &key, const QString &model) const
{
  const bool temp = model.contains("UT333BT");
  const bool wind = model.contains("UT363BT");
  const bool sound = model.contains("UT353BT");
  const bool light = model.contains("UT383BT");
  if (key == "hold" || key == "max")
    return true;
  if (key == "unit")
    return wind || light;
  if (key == "min")
    return !wind;
  if (key == "avg")
    return wind;
  if (key == "unit2")
    return temp;   // the UT363BT has no °C/°F key (Sarah's meter: MAX/AVG, HOLD, UNIT)
  if (key == "weighting")
    return sound;
  return false;
}

bool DecoderUniTiENV::frameValid(const unsigned char *f)
{
  // type 0x01 live data, 0x05 "broadcast data" read alike by UNI-T's app; the
  // 26-byte device id frame (type 0x00) has another length
  if (f[0] != 0xAA || f[1] != 0xBB || f[2] != kFrameLength - 3 || (f[3] != 0x01 && f[3] != 0x05))
    return false;
  return sum16Valid(f, kFrameLength);
}

// As the iDMM frame: the last kFrameLength bytes, header, length and
// checksum; the reader's FIFO is a ring. A first answer of 26 bytes (the
// frame and seven more, seen on a UT383BT) is read at the frame's end.
bool DecoderUniTiENV::checkFormat(const char *data, size_t idx)
{
  unsigned char f[kFrameLength];
  lastBytes(data, idx, kFrameLength, f);
  return frameValid(f);
}

// The second display of a two-channel meter: the cached temperature, or
// channel 1 with an empty val while none has been seen (the display off, see
// DmmResponse::addChannel()).
void DecoderUniTiENV::attachTemp(DmmDecoder::DmmResponse &r) const
{
  DmmChannel &second = r.addChannel(1);
  if (!m_haveTemp)
    return;
  second.val = m_tempVal;
  second.dval = m_tempD;
  second.unit = m_tempUnit;
}

std::optional<DmmDecoder::DmmResponse> DecoderUniTiENV::decode(const QByteArray &data, int id)
{
  // every frame is a whole reading, the main value with the temperature as
  // second value (channel 1); channels 2 in the table gives the display its
  // second line. @p id is not used: one frame per reading
  Q_UNUSED(id);
  m_result = {};
  m_result.main().id = 0;
  m_result.showBar = true;
  if (data.size() < kFrameLength)
    return std::nullopt;
  const auto *f = reinterpret_cast<const unsigned char *>(data.constData()) + data.size() - kFrameLength;
  if (!frameValid(f))
    return std::nullopt;

  const quint8 func = f[4];
  const quint8 unitId = f[14];

  bool temp = false;
  bool twoChannel = false;
  switch (func)
  {
  case kFnTemp:
    temp = true;
    m_tempUnitByte = unitId;
    break;
  case kFnHumidity:
  case kFnWind:
    twoChannel = true;
    m_mainUnit = unitId;
    break;
  case kFnLight:
  case kFnSound:
    m_mainUnit = unitId;
    break;
  default:
    return std::nullopt; // a channel this decoder does not know
  }

  // the display: 6 characters, sign and decimal point included
  QString text = QString::fromLatin1(reinterpret_cast<const char *>(f + 5), 6);
  text.remove(' ');
  bool ok = false;
  const double shown = text.toDouble(&ok);
  // no overload captured. A word ("OL") goes on as it comes, ReadingAdapter
  // makes it an overload; dashes or a blank display are no reading now (as
  // with APPA), not a valid 0
  static const QRegularExpression letters("[A-Za-z]");
  if (!ok && !text.contains(letters))
    return std::nullopt;

  // the unit from the unit id, the value converted from the base unit; an id
  // not listed falls back to the meter's own unit word (frame bytes 11..13)
  QString unit;
  double value = shown;
  if (const UnitInfo *u = findUnit(unitId))
  {
    unit = QString::fromUtf8(u->name);
    if (ok && (u->scale != 1 || u->offset != 0))
      text = convertText(shown, text, *u, &value);
  }
  else
    unit = QString::fromLatin1(reinterpret_cast<const char *>(f + 11), 3).trimmed();

  if (temp)
  {
    m_tempVal = text;
    m_tempD = ok ? value : 0;
    m_tempUnit = unit;
    m_haveTemp = true;
    if (!m_haveMain)
      return std::nullopt; // nothing to hang it on yet
    // the main value of the last humidity / wind frame, with the fresh
    // temperature, so every poll answer updates the display
    DmmDecoder::DmmResponse r = m_main;
    r.main().id = 0;
    attachTemp(r);
    return r;
  }

  m_result.main().val = text;
  m_result.main().dval = ok ? value : 0;
  m_result.main().unit = unit;

  // status byte: bit0 HOLD, bit1 AVG, bit2 MIN, bit3 MAX (HOLD confirmed on
  // a UT333BT, the others from the vendor app)
  m_result.hold = f[16] & 0x01;
  if (f[16] & 0x02)
    m_result.main().states |= SampleFlag::Avg;
  if (f[16] & 0x08)
    m_result.main().states |= SampleFlag::Max;
  if (f[16] & 0x04)
    m_result.main().states |= SampleFlag::Min;

  if (func == kFnSound)
  {
    m_weighting = f[15];
    if (m_weighting == kModeFast)
      m_result.range = "FAST";
    else if (m_weighting == kModeSlow)
      m_result.range = "SLOW";
  }

  if (twoChannel)
  {
    m_main = m_result;
    m_haveMain = true;
    attachTemp(m_result);
  }
  return m_result;
}
