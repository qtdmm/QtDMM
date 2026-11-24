// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/decoders/owon_spm.h"

#include <QRegularExpression>

#include <cmath>

#include "core/siprefix.h"

// OWON SPM series, from OWON's SPM programming manual (no device tried yet,
// hence the "*"). USB CDC serial, 115200 as TestController uses; 4½ digits
// in the multimeter (200.00 Ohm, 2.0000 V): 20000 counts.
static const bool registered = []() {
  DmmDecoder::addConfig({"OWON", "SPM3051 *", "", 115200, FrameFormat::OwonSPM, 8, 1, 1, 0, 20000, 0, 0, 0, "", "", 4});
  DmmDecoder::addConfig({"OWON", "SPM6053 *", "", 115200, FrameFormat::OwonSPM, 8, 1, 1, 0, 20000, 0, 0, 0, "", "", 4});
  DmmDecoder::addConfig({"OWON", "SPM3103 *", "", 115200, FrameFormat::OwonSPM, 8, 1, 1, 0, 20000, 0, 0, 0, "", "", 4});
  DmmDecoder::addConfig({"OWON", "SPM6103 *", "", 115200, FrameFormat::OwonSPM, 8, 1, 1, 0, 20000, 0, 0, 0, "", "", 4});
  return true;
}();

namespace
{
// The supply's answer to MEAS:ALL:INFO?: U I P OV OC OT mode
constexpr int kSupplyFields = 7;
// The multimeter's answer to CONF:ALL?: function,value,AUTO|Manual,range
constexpr int kMultimeterFields = 4;

QStringList supplyFields(const QString &line)
{
  return line.split(QRegularExpression(QStringLiteral("[\\s,]+")), Qt::SkipEmptyParts);
}

bool isSupply(const QStringList &f)
{
  if (f.size() != kSupplyFields)
    return false;
  bool ok = false;
  f[0].toDouble(&ok);
  return ok;
}

bool isMultimeter(const QStringList &f)
{
  return f.size() == kMultimeterFields && !f[0].isEmpty() && f[0][0].isLetter();
}

// 1|ON a fault, 0|OFF none
bool on(const QString &field)
{
  return field == QLatin1String("1") || field.compare(QLatin1String("ON"), Qt::CaseInsensitive) == 0;
}

// A multimeter function of CONF:ALL?: its decoder special and base unit
struct Function
{
  const char *name;
  const char *special;
  const char *unit;
};

constexpr Function kFunctions[] = {
  { "VOLT:DC", "DC", "V" },    { "VOLT:AC", "AC", "V" }, { "CURR:DC", "DC", "A" },  { "CURR:AC", "AC", "A" },
  { "RES", "OH", "Ohm" },      { "CAP", "CA", "F" },     { "DIOD", "DI", "V" },     { "CONT", "BUZ", "Ohm" },
};

const Function *function(const QString &name)
{
  for (const Function &f : kFunctions)
    if (name.compare(QLatin1String(f.name), Qt::CaseInsensitive) == 0)
      return &f;
  return nullptr;
}

// The unit as the multimeter writes it, in QtDMM's spelling: "KOhm" and
// "kΩ" are "kOhm", "uF" is "µF"
QString unitOf(QString unit)
{
  unit.replace(QStringLiteral("Ω"), QStringLiteral("Ohm"));
  unit.replace(QStringLiteral("ohm"), QStringLiteral("Ohm"));
  if (unit.startsWith('K'))
    unit[0] = 'k';
  if (unit.startsWith('u'))
    unit[0] = QChar(0x00B5);
  return unit;
}
}   // namespace

QByteArray DecoderOwonSPM::pollRequest() const
{
  // asked only when it is sent (FrameReader): one in turn
  return m_requests++ % 2 == 0 ? QByteArrayLiteral("MEAS:ALL:INFO?\n") : QByteArrayLiteral("CONF:ALL?\n");
}

bool DecoderOwonSPM::checkFormat(const char *data, size_t idx)
{
  // a line ends with LF (CR LF taken too); the frame starts at index 0. A
  // line that is no answer of the two stays in the buffer and comes before
  // the next one, which decode() takes
  if (data[idx] != '\n')
    return false;
  size_t start = idx;
  while (start > 0 && data[start - 1] != '\n')
    --start;
  const QString line = QString::fromLatin1(data + start, int(idx - start)).trimmed();
  return isSupply(supplyFields(line)) || isMultimeter(line.split(','));
}

std::optional<DmmDecoder::DmmResponse> DecoderOwonSPM::decode(const QByteArray &data, int id)
{
  Q_UNUSED(id);
  // the last line that is an answer
  const QList<QByteArray> lines = data.split('\n');
  for (auto it = lines.crbegin(); it != lines.crend(); ++it)
  {
    const QString line = QString::fromLatin1(*it).trimmed();
    if (const QStringList f = supplyFields(line); isSupply(f))
      return decodeSupply(f);
    if (const QStringList f = line.split(','); isMultimeter(f))
      return decodeMultimeter(f);
  }
  return std::nullopt;
}

std::optional<DmmDecoder::DmmResponse> DecoderOwonSPM::decodeSupply(const QStringList &f)
{
  m_result = {};
  struct Column
  {
    Value id;
    const char *unit;
    QString name;
  };
  const Column columns[] = { { Voltage, "V", tr("Voltage") }, { Current, "A", tr("Current") }, { Power, "W", tr("Power") } };
  QVector<DmmChannel> channels;
  for (int i = 0; i < 3; ++i)
  {
    bool ok = false;
    const double value = f[i].toDouble(&ok);
    if (!ok || !std::isfinite(value))
      return std::nullopt;
    DmmChannel c;
    c.id = columns[i].id;
    c.val = SiPrefix::withoutLeadingZeros(f[i].startsWith('+') ? f[i].mid(1) : f[i]);
    c.dval = value;
    c.unit = QString::fromLatin1(columns[i].unit);
    c.special = QStringLiteral("DC");
    c.name = columns[i].name;
    channels.append(c);
  }
  m_result.channels = channels;   // never empty: main() stays valid

  // a protection that tripped, or a failure: the reading's error
  QStringList errors;
  if (on(f[3]))
    errors << QStringLiteral("OVP");
  if (on(f[4]))
    errors << QStringLiteral("OCP");
  if (on(f[5]))
    errors << QStringLiteral("OTP");
  if (f[6].toInt() == 3)
    errors << tr("Failure");
  m_result.error = errors.join(' ');
  return m_result;
}

std::optional<DmmDecoder::DmmResponse> DecoderOwonSPM::decodeMultimeter(const QStringList &f)
{
  const Function *fn = function(f[0].trimmed());
  if (!fn)
    return std::nullopt;
  m_result = {};
  DmmChannel &main = m_result.main();
  main.id = Multimeter;
  main.name = tr("Multimeter");
  main.special = QString::fromLatin1(fn->special);
  const QString mode = f[2].trimmed();
  m_result.range = mode.compare(QLatin1String("AUTO"), Qt::CaseInsensitive) == 0     ? QStringLiteral("AUTO")
                   : mode.compare(QLatin1String("Manual"), Qt::CaseInsensitive) == 0 ? QStringLiteral("MANU")
                                                                                      : QString();

  // "+000.26Ohm": the number as shown, then the unit with its prefix
  static const QRegularExpression valueAndUnit(QStringLiteral("^([+-]?[0-9.]+(?:[eE][+-]?[0-9]+)?)\\s*(.*)$"));
  const QString text = f[1].trimmed();
  const QRegularExpressionMatch m = valueAndUnit.match(text);
  if (!m.hasMatch())
  {
    // no number: an overload ("OL", "+OL.000V" - not seen yet), else nothing shown
    static const QRegularExpression unitAfterOl(QStringLiteral("OL[.0-9]*\\s*(.*)$"), QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch ol = unitAfterOl.match(text);
    if (ol.hasMatch())
    {
      main.val = QStringLiteral("OL");
      main.unit = ol.captured(1).isEmpty() ? QString::fromLatin1(fn->unit) : unitOf(ol.captured(1));
      main.dval = 0;
      return m_result;
    }
    main.val = QStringLiteral("----");
    main.unit = QString::fromLatin1(fn->unit);
    main.noValue = true;
    return m_result;
  }
  QString number = m.captured(1);
  if (number.startsWith('+'))
    number.remove(0, 1);
  bool ok = false;
  const double shown = number.toDouble(&ok);
  if (!ok || !std::isfinite(shown))
    return std::nullopt;
  main.unit = m.captured(2).isEmpty() ? QString::fromLatin1(fn->unit) : unitOf(m.captured(2));
  main.val = SiPrefix::withoutLeadingZeros(number);
  main.dval = shown * SiPrefix::factor(SiPrefix::split(main.unit).prefix);
  return m_result;
}
