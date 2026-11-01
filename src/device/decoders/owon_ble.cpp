// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/decoders/owon_ble.h"

#include "core/sampletypes.h"

#include <QCoreApplication>
#include <cmath>

static const bool registered = []() {
  // Bluetooth LE, no baud rate. Tried on the CM2100B (all keys and ranges);
  // the others, built much the same, not yet. Counts from OWON's manuals
  // (dmm-tools spec §9.1)
  DmmDecoder::addConfig({"OWON", "B33 *", "", 0, FrameFormat::OwonBLE, 8, 1, 1, 0, 4000, 0, 0, 0});
  DmmDecoder::addConfig({"OWON", "B33T+ *", "", 0, FrameFormat::OwonBLE, 8, 1, 1, 0, 4000, 0, 0, 0});
  DmmDecoder::addConfig({"OWON", "B35T+ *", "", 0, FrameFormat::OwonBLE, 8, 1, 1, 0, 6000, 0, 0, 0});
  DmmDecoder::addConfig({"OWON", "B41T+ *", "", 0, FrameFormat::OwonBLE, 8, 1, 1, 0, 22000, 0, 0, 0});
  DmmDecoder::addConfig({"OWON", "OW16B *", "", 0, FrameFormat::OwonBLE, 8, 1, 1, 0, 6000, 0, 0, 0});
  DmmDecoder::addConfig({"OWON", "OW18B *", "", 0, FrameFormat::OwonBLE, 8, 1, 1, 0, 6000, 0, 0, 0});
  DmmDecoder::addConfig({"OWON", "OW18E *", "", 0, FrameFormat::OwonBLE, 8, 1, 1, 0, 20000, 0, 0, 0});
  DmmDecoder::addConfig({"OWON", "CM2100B", "", 0, FrameFormat::OwonBLE, 8, 1, 1, 0, 20000, 0, 0, 0});
  // the 15-byte frame, a main and a sub-display; none tried. Counts from
  // OWON's app (spec §10.1); UNI-T's UT217A/B are the CMS061/101 (§9.4)
  DmmDecoder::addConfig({"OWON", "CMS101 *", "", 0, FrameFormat::OwonBLE15, 8, 1, 1, 0, 20000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"OWON", "CMS061 *", "", 0, FrameFormat::OwonBLE15, 8, 1, 1, 0, 20000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"Uni-Trend", "UT217B *", "", 0, FrameFormat::OwonBLE15, 8, 1, 1, 0, 20000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"Uni-Trend", "UT217A *", "", 0, FrameFormat::OwonBLE15, 8, 1, 1, 0, 20000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"OWON", "OW65B *", "", 0, FrameFormat::OwonBLE15, 8, 1, 1, 0, 6000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"OWON", "OW67B *", "", 0, FrameFormat::OwonBLE15, 8, 1, 1, 0, 60000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"OWON", "OW69B *", "", 0, FrameFormat::OwonBLE15, 8, 1, 1, 0, 60000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"Voltcraft", "VC871 *", "", 0, FrameFormat::OwonBLE15, 8, 1, 1, 0, 60000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"Voltcraft", "VC891 *", "", 0, FrameFormat::OwonBLE15, 8, 1, 1, 0, 60000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"Voltcraft", "VC915 *", "", 0, FrameFormat::OwonBLE15, 8, 1, 1, 0, 20000, 0, 0, 0, "", "", 2});
  DmmDecoder::addConfig({"Voltcraft", "VC925 PV *", "", 0, FrameFormat::OwonBLE15, 8, 1, 1, 0, 20000, 0, 0, 0, "", "", 2});
  return true;
}();

namespace
{
// the functions of the function/range word, bits 6-9 (spec §6.2); 14 and 15
// only in OWON's app, on no dial of these meters
struct Function
{
  const char *special;
  const char *unit;
  bool prefixed;   ///< the prefix bits apply (not for %, °C, °F, hFE, NCV, spec §6.3)
};
const Function kFunctions[16] = {
  { "DC", "V", true },     { "AC", "V", true },  { "DC", "A", true },   { "AC", "A", true },
  { "OH", "Ohm", true },   { "CA", "F", true },  { "HZ", "Hz", true },  { "DU", "%", false },
  { "TE", "C", false },    { "TE", "dF", false }, { "DI", "V", true },  { "BUZ", "Ohm", true },
  { "HFE", "", false },    { "NCV", "", false },  { "", "W", true },    { "", "VA", true },
};
// bits 3-5 (spec §6.3)
const char *const kPrefixes[8] = { "p", "n", "µ", "m", "", "k", "M", "G" };

// the model code in byte 0 of 0xFFF2 (spec §1, §10.1), both frames; 18 is
// the OW18B's, the OW16B's by the PC software's name for it ("OW18_16"); a
// UT217's own code is not known, the CMS's taken
struct ModelCode
{
  const char *model;   ///< DMMInfo::model without " *"
  int code;
};
const ModelCode kModels[] = {
  { "B33", 33 },      { "B33T+", 33 },   { "B35T+", 35 },  { "B41T+", 41 },    { "OW18B", 18 },   { "OW16B", 18 },
  { "OW18E", 20 },    { "CM2100B", 21 }, { "CMS101", 101 }, { "UT217B", 101 }, { "CMS061", 61 }, { "UT217A", 61 },
  { "OW65B", 65 },    { "OW67B", 67 },   { "OW69B", 69 },  { "VC871", 87 },    { "VC891", 89 },   { "VC915", 91 },
  { "VC925 PV", 92 },
};

QString bareModel(const QString &model)
{
  QString bare = model;
  bare.remove(QLatin1String(" *"));
  return bare;
}

// the count that means no reading in OWON's app (spec §6.5)
constexpr int kNoReading = 0x6FFF;

quint16 word(const QByteArray &data, int i)
{
  return quint16(quint8(data[i]) | (quint8(data[i + 1]) << 8));
}
}

bool DecoderOwon::checkFormat(const char *data, size_t idx)
{
  // no header: the frames come whole in the notifications. Bits 10-15 of the
  // function word are 111100 in every frame captured (spec §6.1, §14.5),
  // which finds the frames again after a lost byte. @p idx counts the bytes
  // since the last frame: a frame needs six of them
  if (idx < kFrameLength - 1)
    return false;
  unsigned char f[kFrameLength];
  lastBytes(data, idx, kFrameLength, f);
  return (f[1] & 0xFC) == 0xF0;
}

std::optional<DmmDecoder::DmmResponse> DecoderOwon::decode(const QByteArray &data, int id)
{
  if (data.size() < kFrameLength)
    return std::nullopt;
  const quint16 gear = word(data, 0);
  const quint16 status = word(data, 2);
  const quint16 reading = word(data, 4);
  const int dp = gear & 7;
  const int magnitude = reading & 0x7FFF;
  const bool minus = reading & 0x8000;
  if (magnitude == kNoReading)
    return std::nullopt;
  const Function &f = kFunctions[(gear >> 6) & 0xF];
  const QString prefix = f.prefixed ? QString::fromUtf8(kPrefixes[(gear >> 3) & 7]) : QString();

  m_result = {};
  DmmChannel &main = m_result.main();
  main.id = id;
  m_result.hold = status & 0x01;
  m_result.lowBat = status & 0x08;
  m_result.range = (status & 0x04) ? "AUTO" : "MANU";
  if (status & 0x02)
    main.states |= SampleFlag::Relative;
  if (status & 0x10)
    main.states |= SampleFlag::Min;
  if (status & 0x20)
    main.states |= SampleFlag::Max;
  main.special = f.special;
  main.unit = f.unit[0] ? prefix + QString::fromUtf8(f.unit) : QString();

  const QString sign = minus ? QStringLiteral("-") : QString();
  if (dp == 6 || dp == 7)
  {
    main.val = sign + (dp == 7 ? "OL" : "UL");
    main.dval = 0;
  }
  else if (main.special == QLatin1String("NCV"))
  {
    // the field strength: EF, then one to four dashes (spec §6.7)
    const int level = int(magnitude / std::pow(10.0, dp));
    if (level > 4)
      return std::nullopt;
    main.val = level == 0 ? QStringLiteral("EF") : QString(level, '-');
    main.dval = 0;
    main.noValue = true;
  }
  else
  {
    const double shown = magnitude / std::pow(10.0, dp);
    main.val = sign + QString::number(shown, 'f', dp);
    main.dval = (minus ? -shown : shown) * prefixFactor(prefix);
  }
  return m_result;
}

QByteArray DecoderOwon::keyRequest(const QString &key) const
{
  // <code> <01 short, 00 long> on 0xFFF3 (spec §7.1). HOLD held is the
  // light: the meters share HOLD and the light on one key (§7.3, §9.1)
  static const QHash<QString, QByteArray> codes = {
    { "select1", QByteArray::fromHex("0101") }, { "range", QByteArray::fromHex("0201") },
    { "hold", QByteArray::fromHex("0301") },    { "lamp", QByteArray::fromHex("0300") },
    { "rel", QByteArray::fromHex("0401") },     { "zero", QByteArray::fromHex("0401") },
    { "select2", QByteArray::fromHex("0501") }, { "minmax", QByteArray::fromHex("0601") },
  };
  return codes.value(key);
}

// The keys of OWON's app per model (spec §7.3); key 4 is REL on the B
// series and ZERO on the CM2100B, key 5 Hz/duty
bool DecoderOwon::hasKey(const QString &key, const QString &model) const
{
  static const QHash<QString, QStringList> keys = {
    { "B33", { "select1", "range", "hold", "rel", "select2" } },
    { "B33T+", { "select1", "range", "hold", "rel", "select2" } },
    { "B35T+", { "select1", "range", "hold", "lamp", "rel", "select2", "minmax" } },
    { "B41T+", { "select1", "range", "hold", "lamp", "rel", "select2", "minmax" } },
    { "OW16B", { "select1", "range", "hold", "lamp", "select2" } },
    { "OW18B", { "select1", "range", "hold", "lamp", "select2" } },
    { "OW18E", { "select1", "range", "hold", "lamp", "select2" } },
    { "CM2100B", { "select1", "hold", "lamp", "zero" } },
  };
  return keys.value(bareModel(model)).contains(key);
}

QStringList DecoderOwon::modelsFromInfo(const QByteArray &info) const
{
  if (info.isEmpty())
    return {};
  const int code = quint8(info[0]);
  QStringList out;
  for (const DMMInfo &m : getDeviceConfigurations())
  {
    if (m.protocol != FrameFormat::OwonBLE && m.protocol != FrameFormat::OwonBLE15)
      continue;
    for (const ModelCode &c : kModels)
      if (c.code == code && bareModel(m.model) == QLatin1String(c.model))
        out << m.name;
  }
  return out;
}

// ---------------------------------------------------------------- 15-byte

namespace
{
// functions 14-29 of the 15-byte frame (spec §10.5); 0-13 as kFunctions.
// The power functions' V and A are AC or DC by the status (§14.3 D11)
struct Function15
{
  const char *special;
  const char *unit;
  bool prefixed;
  const char *name;   ///< the value's own name, translated; null: by what it measures
};
const Function15 kFunctions15[16] = {
  { "", "W", true, nullptr },        { "", "VA", true, nullptr },
  { "", "cosphi", false, nullptr },  { "", "%", false, QT_TRANSLATE_NOOP("Owon", "4-20 mA loop") },
  { "", "Ah", true, nullptr },       { "", "s", false, QT_TRANSLATE_NOOP("Owon", "Time") },
  { "", "Wh", true, nullptr },       { "DC", "V", true, nullptr },
  { "DC", "A", true, nullptr },      { "ACDC", "V", true, nullptr },
  { "", "", false, QT_TRANSLATE_NOOP("Owon", "Motor") }, { "", "W/m²", true, QT_TRANSLATE_NOOP("Owon", "Irradiance") },
  { "", "°", true, QT_TRANSLATE_NOOP("Owon", "Angle") }, { "", "°", true, QT_TRANSLATE_NOOP("Owon", "Compass") },
  { "DC", "V", true, nullptr },      { "AC", "V", true, nullptr },
};
constexpr int kNcv = 13, kPowerV = 21, kPowerA = 22, kMotor = 24, kLastFunction = 29;
constexpr quint32 kSubPresent = 1u << 12;
constexpr quint32 kAvg = 1u << 6, kPeak = 1u << 10, kAcPower = 1u << 13, kScope = 1u << 18;
constexpr quint8 kMarker = 0xF0;   // a function word's top byte (spec §10.9, §14.2)

quint32 word24(const QByteArray &data, int i)
{
  return quint32(quint8(data[i])) | (quint32(quint8(data[i + 1])) << 8) | (quint32(quint8(data[i + 2])) << 16);
}

}

// one display: function/range word @p gear, reading word @p v, the
// function @p function (the VC871's sub-display may take the main one's)
void DecoderOwon15::fill(DmmChannel &c, quint32 gear, quint32 v, int function, quint32 status)
{
  const int dp = gear & 7;
  const int prefixCode = (gear >> 3) & 7;
  const bool minus = v & 0x800000;
  const quint32 magnitude = v & 0x7FFFF;
  const QString sign = minus ? QStringLiteral("-") : QString();
  if (function > kLastFunction)
  {
    c.val = QStringLiteral("----");
    c.noValue = true;
    return;
  }
  const char *special;
  const char *unit;
  bool prefixed;
  const char *name = nullptr;
  if (function < 16)
  {
    special = kFunctions[function].special;
    unit = kFunctions[function].unit;
    prefixed = kFunctions[function].prefixed;
  }
  else
  {
    const Function15 &f = kFunctions15[function - 14];
    special = f.special;
    unit = f.unit;
    prefixed = f.prefixed;
    name = f.name;
  }
  // the power functions' V and A: AC under the AC power context
  if ((function == kPowerV || function == kPowerA) && (status & kAcPower))
    special = "AC";
  const QString prefix = prefixed ? QString::fromUtf8(kPrefixes[prefixCode]) : QString();
  c.special = QString::fromUtf8(special);
  c.unit = unit[0] ? prefix + QString::fromUtf8(unit) : QString();
  if (name)
    c.name = QCoreApplication::translate("Owon", name);

  // the reading word's status (spec §10.4)
  switch ((v >> 20) & 7)
  {
    case 1: c.val = sign + "OL"; c.dval = 0; return;
    case 2: c.val = QStringLiteral("UL"); c.noValue = true; return;
    case 3: c.val = QStringLiteral("HI"); c.noValue = true; return;
    case 4: c.val = QStringLiteral("LO"); c.noValue = true; return;
    case 6:
    case 7: c.val = QStringLiteral("----"); c.noValue = true; return;
    default: break;   // 5: a number OWON's log may filter out
  }
  if (function == kNcv && dp == 0 && !minus && magnitude <= 4)
  {
    // EF, then one to four dashes (spec §6.7)
    c.val = magnitude == 0 ? QStringLiteral("EF") : QString(int(magnitude), '-');
    c.noValue = true;
    return;
  }
  if (function == kMotor)
  {
    // the rotation, no number (spec §10.4)
    static const char *const texts[4] = { "- - -", "- - -", "1-2-3", "3-2-1" };
    c.val = magnitude < 4 && dp == 0 ? QString::fromLatin1(texts[magnitude]) : QStringLiteral("----");
    c.noValue = true;
    return;
  }
  if (dp == 6 || dp == 7)
  {
    // UL: under the range, no number
    c.val = dp == 7 ? sign + "OL" : QStringLiteral("UL");
    c.dval = 0;
    c.noValue = dp == 6;
    return;
  }
  const double shown = magnitude / std::pow(10.0, dp);
  c.val = sign + QString::number(shown, 'f', dp);
  c.dval = (minus ? -shown : shown) * prefixFactor(prefix);
}

bool DecoderOwon15::checkFormat(const char *data, size_t idx)
{
  // the marker at byte 2, function bits 13-15 clear; a frame starting six
  // bytes late has the next frame's marker at byte 11 (dmm-tools frame15.rs)
  if (idx < kFrameLength15 - 1)
    return false;
  unsigned char f[kFrameLength15];
  lastBytes(data, idx, kFrameLength15, f);
  return f[2] == kMarker && (f[1] & 0xE0) == 0 && f[11] != kMarker;
}

std::optional<DmmDecoder::DmmResponse> DecoderOwon15::decode(const QByteArray &data, int id)
{
  // OWON's app skips a frame ending in FF (spec §10.2)
  if (data.size() < kFrameLength15 || quint8(data[kFrameLength15 - 1]) == 0xFF)
    return std::nullopt;
  const quint32 gear = word24(data, 0);
  const quint32 status = word24(data, 12);
  // the scope's readings are not charted (spec §10.6)
  if (status & kScope)
    return std::nullopt;
  const int function = (gear >> 6) & 0x1F;

  m_result = {};
  DmmChannel &main = m_result.main();
  main.id = id;
  fill(main, gear, word24(data, 3), function, status);
  m_result.hold = status & 0x01;
  m_result.lowBat = status & 0x08;
  m_result.range = (status & 0x04) ? "AUTO" : "MANU";
  const bool rel = status & 0x02, minmax = status & 0x30;
  if (rel)
    main.states |= SampleFlag::Relative;
  if (status & 0x10)
    main.states |= SampleFlag::Min;
  if (status & 0x20)
    main.states |= SampleFlag::Max;
  if (status & kAvg)
    main.states |= SampleFlag::Avg;
  if (status & kPeak)
    main.states |= SampleFlag::Peak;

  // the sub-display, only when the main word says so; else off (cleared)
  m_result.channels.resize(2);
  DmmChannel &sub = m_result.channels[1];
  sub.id = id + 1;
  if (gear & kSubPresent)
  {
    const quint32 subGear = word24(data, 6);
    // the VC871 under REL, MAX or MIN: the main function, the reference or
    // the live reading (spec §10.7, §9.4)
    const bool follows = m_subFollowsMain && (rel || minmax);
    fill(sub, subGear, word24(data, 9), follows ? function : int((subGear >> 6) & 0x1F), status);
    if (follows)
      sub.name = rel ? QCoreApplication::translate("Owon", "Reference") : QCoreApplication::translate("Owon", "Live");
  }
  return m_result;
}

QByteArray DecoderOwon15::keyRequest(const QString &key) const
{
  // as the 6-byte meters (spec §10.8); HOLD held is the light 09 01, PEAK 8
  static const QHash<QString, QByteArray> codes = {
    { "select1", QByteArray::fromHex("0101") }, { "range", QByteArray::fromHex("0201") },
    { "hold", QByteArray::fromHex("0301") },    { "lamp", QByteArray::fromHex("0901") },
    { "rel", QByteArray::fromHex("0401") },     { "select2", QByteArray::fromHex("0501") },
    { "minmax", QByteArray::fromHex("0601") },  { "peak", QByteArray::fromHex("0801") },
  };
  return codes.value(key);
}

// the keys of OWON's app per model (spec §10.8): the CMS has Hz/duty, the
// VC915/925 have no light on HOLD, the OW67/69 and VC871/891 PEAK. Keys
// QtDMM has no button for (LPF, Display, Compare, AC/DC, Motor, 4~20mA,
// Inrush) are left out
bool DecoderOwon15::hasKey(const QString &key, const QString &model) const
{
  static const QStringList cms = { "select1", "range", "hold", "lamp", "rel", "select2", "minmax" };
  static const QStringList vc9 = { "select1", "range", "hold", "rel", "minmax" };
  static const QStringList ow65 = { "select1", "range", "hold", "lamp", "rel", "minmax" };
  static const QStringList ow67 = { "select1", "range", "hold", "lamp", "rel", "minmax", "peak" };
  static const QStringList ow69 = { "select1", "range", "hold", "lamp", "rel", "select2", "minmax", "peak" };
  static const QHash<QString, const QStringList *> keys = {
    { "CMS101", &cms }, { "CMS061", &cms },   { "UT217B", &cms },  { "UT217A", &cms },
    { "VC915", &vc9 },  { "VC925 PV", &vc9 }, { "OW65B", &ow65 },  { "OW67B", &ow67 },
    { "VC871", &ow67 }, { "OW69B", &ow69 },   { "VC891", &ow69 },
  };
  const QStringList *list = keys.value(bareModel(model));
  return list && list->contains(key);
}

void DecoderOwon15::setModel(const QString &model)
{
  m_subFollowsMain = bareModel(model) == QLatin1String("VC871");
}
