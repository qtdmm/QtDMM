// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/decoders/uni_t_ut219pv.h"

#include <QHash>
#include <utility>

#include "core/sampletypes.h"
#include "device/decoders/uni_t_abcd16.h"

// No meter has been tried: the "*".
static const bool registered = []() {
  DmmDecoder::addConfig({"Uni-Trend", "UT219PV *", "", 0, FrameFormat::UniTUT219PV, 8, 1, 1, 0, 10000, 0, 0, 0, "", "", 2});
  return true;
}();

namespace
{
constexpr quint8 kReading = 0x02;
constexpr quint8 kMultiSave = 0x0A;
constexpr quint8 kSingleSave = 0x0C;
constexpr int kReadingLength = 36;   // up to the flags, without the sum
constexpr quint8 kPress = 0x5A;      // the app's "press" byte

// the functions (the app's getLineDisplayName): main and second display as
// QtDMM shows them; nullptr: no second display
struct Function
{
  const char *main;
  const char *second;
};
const Function kFunctions[20] = {
  { "", nullptr },
  { "DC", nullptr },    //  1 DC A
  { "DC", nullptr },    //  2 DC V
  { "AC", "HZ" },       //  3 AC V / Hz
  { "AC", "HZ" },       //  4 AC V low-pass / Hz
  { "DC", "DC" },       //  5 DC power / DC A
  { "DC", "DC" },       //  6 DC V / DC A
  { "AC", "HZ" },       //  7 AC A / Hz
  { "AC", "HZ" },       //  8 AC A low-pass / Hz
  { "AC", "HZ" },       //  9 flex clamp AC A / Hz
  { "AC", "HZ" },       // 10 flex clamp AC A low-pass / Hz
  { "AC", "AC" },       // 11 inrush A / peak A
  { "AC", "AC" },       // 12 inrush A / peak A
  { "OH", nullptr },    // 13 Ω
  { "DI", nullptr },    // 14 diode
  { "CA", nullptr },    // 15 capacitance
  { "DC", nullptr },    // 16 DC mV
  { "TE", "TE" },       // 17 °C / °F
  { "TE", "TE" },       // 18 °F / °C
  { "", nullptr },      // 19 flex clamp not plugged in
};

// the ASCII unit as QtDMM spells it
QString unitOf(const unsigned char *u)
{
  QString s = QString::fromLatin1(reinterpret_cast<const char *>(u), 3);
  s.remove(QLatin1Char(' ')).remove(QChar(0));
  if (s == QLatin1String("oC"))
    return QStringLiteral("C");
  if (s == QLatin1String("oF"))
    return QStringLiteral("dF");
  s.replace(QLatin1Char('K'), QLatin1Char('k')).replace(QLatin1Char('u'), QChar(0xB5));
  if (s.endsWith(QLatin1Char('o')))
    s = s.left(s.size() - 1) + QStringLiteral("Ohm");
  return s;
}

// one display: six ASCII characters, its unit and its OL flag; dval as
// shown, the unit's prefix returned for the caller to scale it
QString fill(DmmDecoder::DmmChannel &c, const unsigned char *value, const unsigned char *unit, quint8 ol)
{
  c.unit = unitOf(unit);
  if (ol == '1' || ol == '2')
  {
    c.val = ol == '2' ? QStringLiteral("-OL") : QStringLiteral("OL");
    return QString();
  }
  QString text = QString::fromLatin1(reinterpret_cast<const char *>(value), 6);
  text.remove(QLatin1Char(' ')).remove(QChar(0));
  bool ok = false;
  const double shown = text.toDouble(&ok);
  c.val = text;
  if (!ok)
  {
    c.noValue = true;   // dashes, a word
    return QString();
  }
  c.dval = shown;
  const bool prefixed = c.unit.size() > 1 && QStringLiteral("nµmkM").contains(c.unit.at(0));
  return prefixed ? c.unit.left(1) : QString();
}
}

QByteArray DecoderUT219PV::pollRequest() const
{
  return UniTAbcd16::command(0x05, QByteArray(1, '\0'));
}

QByteArray DecoderUT219PV::takeCommand()
{
  return std::exchange(m_pending, QByteArray());
}

bool DecoderUT219PV::checkFormat(const char *data, size_t idx)
{
  return UniTAbcd16::endsAt(data, idx, false);
}

std::optional<DmmDecoder::DmmResponse> DecoderUT219PV::decode(const QByteArray &data, int id)
{
  int length = 0;
  const unsigned char *f = UniTAbcd16::find(data, false, &length);
  if (!f)
    return std::nullopt;
  if (f[4] == kMultiSave || f[4] == kSingleSave)
  {
    // a save key pressed on the meter: answered as the app does
    m_pending = UniTAbcd16::command(f[4], QByteArray(1, '\0'));
    return std::nullopt;
  }
  if (f[4] != kReading || length < kReadingLength)
    return std::nullopt;   // a key's answer, a stored reading
  const int function = f[5];
  if (function < 1 || function > 19)
    return std::nullopt;   // a function the app's table lacks
  m_function = function;
  const Function &fn = kFunctions[function];

  m_result = {};
  m_result.channels.resize(2);
  DmmChannel &main = m_result.channels[0];
  DmmChannel &second = m_result.channels[1];
  main.id = id;
  second.id = id + 1;
  m_result.hold = f[33] & 0x04;
  m_result.showBar = true;
  if (function == 19)
  {
    main.val = QStringLiteral("----");   // the flex clamp is not plugged in
    main.noValue = true;
    return m_result;
  }
  main.special = QString::fromLatin1(fn.main);
  main.dval *= prefixFactor(fill(main, f + 7, f + 13, f[29]));
  if (f[33] & 0x02)
    main.states |= SampleFlag::Relative;   // ZERO
  if (fn.second)
  {
    second.special = QString::fromLatin1(fn.second);
    second.dval *= prefixFactor(fill(second, f + 17, f + 23, f[30]));
  }
  return m_result;
}

QByteArray DecoderUT219PV::keyRequest(const QString &key) const
{
  static const QHash<QString, QPair<quint8, quint8>> keys = {
    { "select1", { 0x01, kPress } }, { "zero", { 0x03, kPress } }, { "minmax", { 0x04, 0x01 } },
    { "minmax_off", { 0x04, 0x00 } }, { "autohold", { 0x11, kPress } }, { "hold", { 0x12, kPress } },
    { "inrush", { 0x13, kPress } }, { "lpf", { 0x14, kPress } },
  };
  const auto it = keys.constFind(key);
  if (it == keys.constEnd())
    return QByteArray();
  return UniTAbcd16::command(it->first, QByteArray(1, char(it->second)));
}

bool DecoderUT219PV::keyWorks(const QString &key, const QString &model, const Reading &reading) const
{
  Q_UNUSED(model);
  Q_UNUSED(reading);
  // LPF where the function has a low-pass twin (AC V, AC A, the flex clamp)
  if (key == QLatin1String("lpf") && m_function != 0)
    return (m_function >= 3 && m_function <= 4) || (m_function >= 7 && m_function <= 10);
  return true;
}
