// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/decoders/openosc.h"

#include "core/sampletypes.h"
#include "core/siprefix.h"

#include <cmath>

// OpenOSC's multimeter (issue #603): confirmed on the meter by the issue's author.
// USB CDC, the baud rate is ignored; 25000 counts.
static const bool registered = []() {
  DmmDecoder::addConfig({"ZOTEK", "ZT-703S OpenOSC", "", 115200, FrameFormat::OpenOSC, 8, 1, 1, 0, 25000, 0, 0, 0});
  return true;
}();

namespace
{
// One range: what the display shows, from OpenOSC's ranges.cpp (ZT703S)
struct ShownRange
{
  const char *unit;   ///< with its prefix ("kOhm")
  int decimals;
  double factor;      ///< the prefix's: SI value / factor = the number shown
};
struct Table
{
  const ShownRange *ranges;
  int count;
};

constexpr ShownRange kVoltsDc[] = { { "V", 4, 1 }, { "V", 3, 1 }, { "V", 2, 1 }, { "V", 1, 1 } };   // 2.5, 25, 250, 1000 V
constexpr ShownRange kVoltsAc[] = { { "V", 4, 1 }, { "V", 3, 1 }, { "V", 2, 1 }, { "V", 1, 1 } };   // 2.5, 25, 250, 750 V
constexpr ShownRange kMillivolts[] = { { "mV", 3, 1e-3 }, { "mV", 2, 1e-3 } };                     // 25, 250 mV
constexpr ShownRange kAmps[] = { { "A", 4, 1 }, { "A", 3, 1 } };                                     // 2.5, 10 A
constexpr ShownRange kMilliamps[] = { { "mA", 3, 1e-3 }, { "mA", 2, 1e-3 } };                      // 25, 250 mA
constexpr ShownRange kOhms[] = { { "Ohm", 2, 1 },     { "kOhm", 4, 1e3 }, { "kOhm", 3, 1e3 }, { "kOhm", 2, 1e3 },
                            { "MOhm", 4, 1e6 },  { "MOhm", 3, 1e6 }, { "MOhm", 1, 1e6 } };   // 250 Ohm ... 250 MOhm
constexpr ShownRange kFarads[] = { { "nF", 3, 1e-9 }, { "nF", 2, 1e-9 }, { "nF", 1, 1e-9 }, { "µF", 3, 1e-6 },
                              { "µF", 2, 1e-6 }, { "µF", 1, 1e-6 }, { "mF", 3, 1e-3 }, { "mF", 2, 1e-3 } };   // 9.999 nF ... 99.99 mF

template <int N>
constexpr Table table(const ShownRange (&r)[N])
{
  return { r, N };
}

// A function (field 1): unit, the decoder's special, its ranges; without
// ranges (continuity, diode, temperature) @p decimals is used
struct Function
{
  const char *unit;
  const char *special;
  Table ranges;
  int decimals;
};

const Function *function(int code)
{
  static const Function functions[] = {
    { "V", "DC", table(kVoltsDc), 0 },       // 0 V DC
    { "V", "AC", table(kVoltsAc), 0 },       // 1 V AC
    { "Ohm", "OH", table(kOhms), 0 },        // 2 resistance
    { "Ohm", "BUZ", { nullptr, 0 }, 1 },     // 3 continuity
    { "V", "DI", { nullptr, 0 }, 3 },        // 4 diode
    { "F", "CA", table(kFarads), 0 },        // 5 capacitance
    { "A", "DC", table(kAmps), 0 },          // 6 A DC
    { "A", "AC", table(kAmps), 0 },          // 7 A AC
    { "V", "DC", table(kMillivolts), 0 },    // 8 mV DC
    { "V", "AC", table(kMillivolts), 0 },    // 9 mV AC
    { "A", "DC", table(kMilliamps), 0 },     // 10 mA DC
    { "A", "AC", table(kMilliamps), 0 },     // 11 mA AC
    { "C", "TE", { nullptr, 0 }, 1 },        // 12 temperature, type K, always °C over SCPI
  };
  return code >= 0 && code < int(std::size(functions)) ? &functions[code] : nullptr;
}

enum Status { Ok = 0, Overload = 6, NoReading = 14 };
}   // namespace

bool DecoderOpenOSC::checkFormat(const char *data, size_t idx)
{
  // a line ends with LF (CR LF taken too); the frame starts at index 0. A
  // line that is no result (an answer to something else, "NAN") stays in
  // the buffer and comes before the next one, which decode() takes.
  if (data[idx] != '\n')
    return false;
  size_t start = idx;
  while (start > 0 && data[start - 1] != '\n')
    --start;
  int commas = 0;
  for (size_t i = start; i < idx; ++i)
    commas += data[i] == ',';
  return commas == 10 && data[start] >= '0' && data[start] <= '9';
}

std::optional<DmmDecoder::DmmResponse> DecoderOpenOSC::decode(const QByteArray &data, int id)
{
  m_result = {};
  m_result.main().id = id;

  // the last line with the 11 fields
  QStringList fields;
  for (const QByteArray &line : data.split('\n'))
  {
    const QStringList f = QString::fromLatin1(line).trimmed().split(',');
    if (f.size() == 11)
      fields = f;
  }
  if (fields.isEmpty())
    return std::nullopt;
  // polled faster than the meter measures: the same reading again. Not
  // while HOLD, which freezes seq too: the held display still counts
  const QString seq = fields[9].trimmed();
  if (seq == m_lastSeq && fields[7].trimmed() != "1")
    return std::nullopt;
  m_lastSeq = seq;
  bool ok = false;
  const Function *fn = function(fields[0].toInt(&ok));
  if (!ok || !fn)
    return std::nullopt;
  const int status = fields[2].toInt();
  const int range = fields[5].toInt();   // 1-based, 0 unknown
  const int mode = fields[6].toInt();
  m_result.range = mode == 1 ? "AUTO" : mode == 0 ? "MANU" : QString();
  m_result.hold = fields[7].trimmed() == "1";
  DmmChannel &main = m_result.main();
  main.special = fn->special;
  if (fields[8].trimmed() == "1")
    main.states |= SampleFlag::Relative;

  const ShownRange *shown = range >= 1 && range <= fn->ranges.count ? &fn->ranges.ranges[range - 1] : nullptr;
  if (status == Overload)
  {
    main.val = "OL";
    main.unit = shown ? QString::fromUtf8(shown->unit) : QString::fromLatin1(fn->unit);
    main.dval = 0;
    return m_result;
  }
  if (status != Ok)
  {
    // no number on the display (14), or an error of the firmware
    main.val = "----";
    main.unit = shown ? QString::fromUtf8(shown->unit) : QString::fromLatin1(fn->unit);
    main.noValue = true;
    return m_result;
  }
  const double value = fields[1].toDouble(&ok);
  if (!ok || !std::isfinite(value))
    return std::nullopt;
  // dval is what the display shows (DmmChannel): the firmware sends more
  // digits than the meter has (a temperature as 2.51228227615e+1)
  if (shown)
  {
    main.val = QString::number(value / shown->factor, 'f', shown->decimals);
    main.unit = QString::fromUtf8(shown->unit);
    main.dval = main.val.toDouble() * shown->factor;
  }
  else if (fn->ranges.count == 0)
  {
    main.val = QString::number(value, 'f', fn->decimals);
    main.unit = fn->unit;
    main.dval = main.val.toDouble();
  }
  else
  {
    // a range the table does not know: the prefix that fits
    QString prefix;
    main.val = SiPrefix::format(value, &prefix);
    main.unit = prefix + fn->unit;
    main.dval = value;
  }

  // the secondary value (FUNC2 FREQ): only Hz has a use here
  const double second = fields[3].toDouble(&ok);
  if (ok && std::isfinite(second) && fields[4].toInt() == 5)
  {
    DmmChannel &freq = m_result.addChannel(1);
    QString prefix;
    freq.dval = second;
    freq.val = SiPrefix::format(second, &prefix);
    freq.unit = prefix + "Hz";
    freq.special = "FR";
  }
  return m_result;
}
