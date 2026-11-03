// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/decoders/voltcraft_vc8x0.h"

#include "core/sampletypes.h"

#include <QCoreApplication>

static const bool registered = []() {
  // the CP2110 cable at 9600 8N1; none tried. Counts from the manuals
  DmmDecoder::addConfig({"Voltcraft", "VC-880 *", "", 9600, FrameFormat::VoltcraftVC880, 8, 1, 1, 0, 40000, 0, 0, 0});
  DmmDecoder::addConfig({"Voltcraft", "VC650BT *", "", 9600, FrameFormat::VoltcraftVC880, 8, 1, 1, 0, 40000, 0, 0, 0});
  DmmDecoder::addConfig({"Voltcraft", "VC-890 *", "", 9600, FrameFormat::VoltcraftVC890, 8, 1, 1, 0, 60000, 0, 0, 0});
  return true;
}();

namespace
{
constexpr quint8 kLiveData = 0x01;
constexpr int kMainAt = 6;   // the main display: 7 ASCII characters

// the functions in the VC-880's numbering (vc880 spec §4.1), the VC-890's
// remapped onto them (vc890 spec, Function Codes)
struct Function
{
  const char *special;
  const char *prefix;   ///< the function's own (mV, µA, mA); the range's comes with prefixFor()
  const char *unit;
  const char *name;     ///< the value's own name, translated; null: by what it measures
};
const Function kFunctions[0x13] = {
  { "DC", "", "V", nullptr },    { "ACDC", "", "V", nullptr }, { "DC", "m", "V", nullptr },  { "HZ", "", "Hz", nullptr },
  { "DU", "", "%", nullptr },    { "AC", "", "V", nullptr },   { "OH", "", "Ohm", nullptr }, { "DI", "", "V", nullptr },
  { "BUZ", "", "Ohm", nullptr }, { "CA", "", "F", nullptr },   { "TE", "", "C", nullptr },   { "TE", "", "dF", nullptr },
  { "DC", "µ", "A", nullptr },   { "AC", "µ", "A", nullptr },  { "DC", "m", "A", nullptr },  { "AC", "m", "A", nullptr },
  { "DC", "", "A", nullptr },    { "AC", "", "A", nullptr },
  { "AC", "", "V", QT_TRANSLATE_NOOP("Voltcraft", "Low-pass") },
};
const quint8 kVc890ToVc880[0x13] = { 0x05, 0x12, 0x00, 0x01, 0x02, 0x03, 0x04, 0x06, 0x08, 0x07,
                                     0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11 };

// the prefix the range gives (vc880 spec §4.2, vc890 spec Range Tables):
// the same ladders on both, 4/40/400 against 6/60/600
QString prefixFor(int function, int range)
{
  switch (function)
  {
    case 0x03:   // Hz, kHz, MHz
      return range <= 1 ? QString() : range <= 4 ? QStringLiteral("k") : QStringLiteral("M");
    case 0x06:   // Ohm, kOhm, MOhm
      return range == 0 ? QString() : range <= 3 ? QStringLiteral("k") : QStringLiteral("M");
    case 0x09:   // nF, µF, mF
      return range <= 1 ? QStringLiteral("n") : range <= 5 ? QStringLiteral("µ") : QStringLiteral("m");
    default:
      return QString();
  }
}

quint16 sum16(const unsigned char *f, int n)
{
  quint16 s = 0;
  for (int i = 0; i < n; ++i)
    s = quint16(s + f[i]);
  return s;
}
}

DecoderVC8x0::DecoderVC8x0(FrameFormat::DataFormat df) :
  DmmDecoder(df),
  m_vc890(df == FrameFormat::VoltcraftVC890),
  m_length(m_vc890 ? kMaxLength : 39),
  m_statusAt(m_vc890 ? 56 : 30)
{
  m_name = m_vc890 ? "VoltcraftVC890" : "VoltcraftVC880";
}

QByteArray DecoderVC8x0::command(quint8 cmd, const QByteArray &data)
{
  QByteArray frame("\xAB\xCD", 2);
  frame.append(char(data.size() + 3)).append(char(cmd)).append(data);
  const quint16 s = sum16(reinterpret_cast<const unsigned char *>(frame.constData()), int(frame.size()));
  return frame.append(char(s >> 8)).append(char(s & 0xFF));
}

bool DecoderVC8x0::checkFormat(const char *data, size_t idx)
{
  // a live frame of this meter's length: header, length, type, sum; the
  // command results and other frames do not fit it
  if (idx < size_t(m_length - 1))
    return false;
  unsigned char f[kMaxLength] = {};
  lastBytes(data, idx, m_length, f);
  return f[0] == 0xAB && f[1] == 0xCD && f[2] == m_length - 3 && f[3] == kLiveData
         && sum16(f, m_length - 2) == quint16((f[m_length - 2] << 8) | f[m_length - 1]);
}

std::optional<DmmDecoder::DmmResponse> DecoderVC8x0::decode(const QByteArray &data, int id)
{
  if (data.size() < m_length)
    return std::nullopt;
  int function = quint8(data[4]);
  if (function > 0x12)
    return std::nullopt;
  if (m_vc890)
    function = kVc890ToVc880[function];
  const int range = quint8(data[5]) - 0x30;
  const auto status = [&](int i) { return quint8(data[m_statusAt + i]); };
  const Function &f = kFunctions[function];

  m_result = {};
  DmmChannel &main = m_result.main();
  main.id = id;
  main.special = f.special;
  const QString prefix = QString::fromUtf8(f.prefix) + prefixFor(function, range);
  main.unit = prefix + QString::fromUtf8(f.unit);
  if (f.name)
    main.name = QCoreApplication::translate("Voltcraft", f.name);
  m_result.hold = status(2) & 0x01;
  m_result.range = (status(2) & 0x02) ? "MANU" : "AUTO";
  if (!m_vc890)
    m_result.lowBat = status(3) & 0x08;
  if (status(1) & 0x01)
    main.states |= SampleFlag::Relative;
  if (status(1) & 0x02)
    main.states |= SampleFlag::Avg;
  if (status(1) & 0x04)
    main.states |= SampleFlag::Min;
  if (status(1) & 0x08)
    main.states |= SampleFlag::Max;

  // the display as sent, its sign and point in it (vc880 spec §4)
  const QString shown = QString::fromLatin1(data.mid(kMainAt, 7)).remove(' ');
  bool number = false;
  const double value = shown.toDouble(&number);
  if ((status(2) & 0x04) || shown.contains(QLatin1String("OL")))
  {
    main.val = shown.startsWith('-') ? QStringLiteral("-OL") : QStringLiteral("OL");
    main.dval = 0;
  }
  else if (!number)
  {
    // dashes or a word: no reading
    main.val = shown.isEmpty() ? QStringLiteral("----") : shown;
    main.noValue = true;
  }
  else
  {
    main.val = shown;
    main.dval = value * prefixFactor(prefix);
  }
  return m_result;
}

QByteArray DecoderVC8x0::pollRequest() const
{
  // the VC-880 streams; the VC-890 answers 5E, after the result frame
  // Voltcraft's software sends around every exchange (vc890 spec)
  if (!m_vc890)
    return QByteArray();
  return command(0xFF, QByteArray(1, '\0')) + command(0x5E);
}

QByteArray DecoderVC8x0::keyRequest(const QString &key) const
{
  static const QHash<QString, quint8> codes = {
    { "range", 0x46 },  { "auto", 0x47 }, { "rel", 0x48 },  { "minmax", 0x49 },
    { "minmax_off", 0x43 }, { "hold", 0x4A }, { "lamp", 0x4B }, { "select1", 0x4C },
  };
  const auto it = codes.constFind(key);
  if (it == codes.constEnd())
    return QByteArray();
  const QByteArray frame = command(*it);
  return m_vc890 ? command(0xFF, QByteArray(1, '\0')) + frame : frame;
}
