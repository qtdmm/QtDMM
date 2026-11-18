// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/decoders/uni_t_ut117c.h"

#include <QHash>
#include <algorithm>

#include "core/sampletypes.h"
#include "device/decoders/uni_t_abcd16.h"

// No meter has been tried: the "*".
static const bool registered = []() {
  DmmDecoder::addConfig({"Uni-Trend", "UT117C *", "", 0, FrameFormat::UniTUT117C, 8, 1, 1, 0, 6000, 0, 0, 0});
  return true;
}();

namespace
{
constexpr quint8 kReading = 0x02;
constexpr int kReadingLength = 24;
constexpr quint8 kPress = 0x5A;   // the app's "press" byte

// the functions (the app's config_ut117c.json) as QtDMM shows them; ""
// for AC or DC as flag B says (LoZ)
const char *const kSpecial[20] = {
  nullptr,
  "AC", "AC", "AC",          //  1 AC V,  2 AC V low-pass,  3 AC mV
  "DC", "DC",                //  4 DC V,  5 DC mV
  "OH", "BUZ", "DI", "CA",   //  6 Ω,  7 continuity,  8 diode,  9 capacitance
  "",                        // 10 LoZ V
  "HZ", "HZ", "NCV",         // 11 Hz on V, 12 Hz low-pass, 13 NCV
  "DC", "AC", "HZ",          // 14 DC A, 15 AC A, 16 Hz on A
  "AC", "HZ", "DC",          // 17-19 the clamp's AC A, Hz, DC A
};

// the ASCII unit as QtDMM spells it: "o" is Ω, "K" kilo, "u" micro
QString unitOf(const unsigned char *u)
{
  QString s = QString::fromLatin1(reinterpret_cast<const char *>(u), 3);
  s.remove(QLatin1Char(' ')).remove(QChar(0));
  s.replace(QLatin1Char('K'), QLatin1Char('k')).replace(QLatin1Char('u'), QChar(0xB5));
  if (s.endsWith(QLatin1Char('o')))
    s = s.left(s.size() - 1) + QStringLiteral("Ohm");
  return s;
}

// which functions a key works in (the app's UT117CUtils)
bool inList(int function, std::initializer_list<int> list)
{
  return std::find(list.begin(), list.end(), function) != list.end();
}
}

QByteArray DecoderUT117C::pollRequest() const
{
  return UniTAbcd16::command(0x05, QByteArray(1, '\0'));
}

bool DecoderUT117C::checkFormat(const char *data, size_t idx)
{
  return UniTAbcd16::endsAt(data, idx, true);
}

std::optional<DmmDecoder::DmmResponse> DecoderUT117C::decode(const QByteArray &data, int id)
{
  int length = 0;
  const unsigned char *f = UniTAbcd16::find(data, true, &length);
  if (!f || f[4] != kReading || length < kReadingLength)
    return std::nullopt;   // a key's answer, or a broken frame
  const int function = f[5];
  if (function < 1 || function > 19)
    return std::nullopt;   // a function the app's table lacks
  m_function = function;
  const quint8 flagsA = f[20], flagsB = f[21];

  m_result = {};
  DmmChannel &c = m_result.main();
  c.id = id;
  c.special = *kSpecial[function] ? QString::fromLatin1(kSpecial[function]) : QString::fromLatin1(flagsB & 0x02 ? "AC" : "DC");
  c.unit = unitOf(f + 15);
  if (flagsA & 0x01)
    c.states |= SampleFlag::Relative;
  m_result.hold = flagsA & 0x02;
  m_result.lowBat = flagsA & 0x08;
  m_result.showBar = true;

  if (function == 13)
  {
    // NCV: the strength, no number
    c.val = f[6] == '1' ? QStringLiteral("HI") : QStringLiteral("LO");
    c.unit.clear();
    c.noValue = true;
    return m_result;
  }
  QString text = QString::fromLatin1(reinterpret_cast<const char *>(f + 8), 7);
  text.remove(QLatin1Char(' ')).remove(QChar(0));
  bool ok = false;
  const double shown = text.toDouble(&ok);
  if (ok && (f[18] == '1' || f[18] == '2'))
  {
    c.val = f[18] == '2' ? QStringLiteral("-OL") : QStringLiteral("OL");
    return m_result;
  }
  if (!ok)
  {
    // a word on the display ("LEAd", dashes): as it is, no number
    c.val = text;
    c.noValue = true;
    return m_result;
  }
  c.val = text;
  const bool prefixed = c.unit.size() > 1 && QStringLiteral("nµmkM").contains(c.unit.at(0));
  const QString prefix = prefixed ? c.unit.left(1) : QString();
  c.dval = shown * prefixFactor(prefix);
  return m_result;
}

QByteArray DecoderUT117C::keyRequest(const QString &key) const
{
  static const QHash<QString, QPair<quint8, quint8>> keys = {
    { "select1", { 0x01, kPress } }, { "range", { 0x02, 0x01 } }, { "auto", { 0x02, 0x00 } },
    { "rel", { 0x03, kPress } }, { "minmax", { 0x04, 0x01 } }, { "minmax_off", { 0x04, 0x00 } },
    { "hold", { 0x12, kPress } }, { "lpf", { 0x14, kPress } }, { "lamp", { 0x15, kPress } },
  };
  const auto it = keys.constFind(key);
  if (it == keys.constEnd())
    return QByteArray();
  return UniTAbcd16::command(it->first, QByteArray(1, char(it->second)));
}

bool DecoderUT117C::keyWorks(const QString &key, const QString &model, const Reading &reading) const
{
  Q_UNUSED(model);
  Q_UNUSED(reading);
  const int f = m_function;
  if (f == 0)
    return true;
  if (key == QLatin1String("hold"))
    return f != 13;
  if (key == QLatin1String("lpf"))
    return inList(f, { 1, 2, 11, 12 });
  if (key == QLatin1String("rel") || key == QLatin1String("minmax") || key == QLatin1String("minmax_off"))
    return inList(f, { 1, 2, 3, 4, 5, 6, 7, 8, 9, 14, 15, 17, 19 });
  if (key == QLatin1String("range") || key == QLatin1String("auto"))
    return inList(f, { 1, 4, 6, 9, 11, 14, 15, 16 });
  if (key == QLatin1String("select1"))
    return f != 4 && f != 10 && f != 14;
  return true;
}
