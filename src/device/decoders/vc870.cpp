// Copyright (c) 2025 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later

#include "device/decoders/vc870.h"

#include <vector>

static const bool registered = []() {
  DmmDecoder::addConfig({"Voltcraft", "VC 870", "", 9600, FrameFormat::VC870Continuous, 8, 1, 1, 0, 40000, 0, 0, 1, "", "", 2});
  return true;
}();

namespace
{
// One range of a function: the factor from the digits to the SI value, where
// the decimal point goes (0: none, else insertCommaIT()), and the units shown.
// The second display has a factor and point of its own only where it shows
// another quantity (power factor and frequency); else it takes the first's.
struct Range
{
  double factor;
  int dot;
  const char *unit1;
  const char *unit2;
  double factor2 = 0;
  int dot2 = 0;
};

// The function (byte 0) and its selection (byte 1), with the ranges by the
// range code (byte 2, low nibble). A code not listed is a parser error.
struct Function
{
  quint8 code;
  quint8 select;
  const char *special;
  bool showBar;
  std::vector<Range> ranges;
};

const Function kFunctions[] = {
  { 0x30, 0x30, "DC", true, {                 // DCV: 4, 40, 400, 4000 V (1000 V is the maximum)
      { 1e-4, 2, "V", "V" }, { 1e-3, 3, "V", "V" }, { 1e-2, 4, "V", "V" }, { 1e-1, 5, "V", "V" } } },
  { 0x30, 0x31, "AC", true, {                 // ACV: 4, 40, 400, 4000 V
      { 1e-3, 3, "V", "V" }, { 1e-2, 4, "V", "V" }, { 1e-1, 5, "V", "V" }, { 1, 0, "V", "V" } } },
  { 0x31, 0x30, "DC", true, {                 // DCmV: 400 mV
      { 1e-5, 4, "mV", "mV" } } },
  { 0x31, 0x31, "TE", false, {                // degrees Celsius and Fahrenheit ("dF": "F" alone is farad)
      { 1e-1, 5, "C", "dF" } } },
  { 0x32, 0x30, "OH", true, {                 // resistance
      { 1e-2, 4, "Ohm", "Ohm" }, { 1e-1, 2, "kOhm", "kOhm" }, { 1e1, 4, "kOhm", "kOhm" },
      { 1e2, 5, "kOhm", "kOhm" }, { 1e3, 3, "MOhm", "MOhm" }, { 1e4, 4, "MOhm", "MOhm" } } },
  { 0x32, 0x31, "OH", false, {                // continuity
      { 1e-2, 4, "Ohm", "Ohm" } } },
  { 0x33, 0x30, "CA", false, {                // capacitance
      { 1e-12, 3, "nF", "nF" }, { 1e-11, 4, "nF", "nF" }, { 1e-10, 2, "uF", "uF" }, { 1e-9, 3, "uF", "uF" },
      { 1e-8, 4, "uF", "uF" }, { 1e-7, 5, "uF", "uF" }, { 1e-6, 3, "mF", "mF" } } },
  { 0x34, 0x30, "DI", false, {                // diode
      { 1e-4, 2, "V", "V" } } },
  { 0x35, 0x30, "FR", false, {                // frequency, the factor to Hz
      { 1e-3, 3, "Hz", "Hz" }, { 1e-2, 4, "Hz", "Hz" }, { 1e-1, 2, "kHz", "kHz" }, { 1, 3, "kHz", "kHz" },
      { 1e1, 4, "kHz", "kHz" }, { 1e2, 2, "MHz", "MHz" }, { 1e3, 3, "MHz", "MHz" }, { 1e4, 4, "MHz", "MHz" } } },
  { 0x35, 0x31, "", false, {                  // duty cycle
      { 1e-2, 4, "%", "%" } } },
  { 0x36, 0x30, "DC", true, {                 // DC µA
      { 1e-8, 4, "uA", "uA" }, { 1e-7, 5, "uA", "uA" } } },
  { 0x36, 0x31, "AC", true, {                 // AC µA
      { 1e-7, 5, "uA", "uA" }, { 1e-6, 0, "uA", "uA" } } },
  { 0x37, 0x30, "DC", true, {                 // DC mA
      { 1e-6, 3, "mA", "mA" }, { 1e-5, 4, "mA", "mA" } } },
  { 0x37, 0x31, "AC", true, {                 // AC mA
      { 1e-5, 4, "mA", "mA" }, { 1e-4, 5, "mA", "mA" } } },
  { 0x38, 0x30, "DC", true, {                 // DC A
      { 1e-3, 3, "A", "A" } } },
  { 0x38, 0x31, "AC", true, {                 // AC A
      { 1e-2, 4, "A", "A" } } },
  { 0x39, 0x30, "AC", true, {                 // active and apparent power: 4000 W / 4000 VA
      { 1e-1, 5, "W", "VA" } } },
  { 0x39, 0x31, "AC", true, {                 // power factor and frequency: 40 PF, 4000 Hz
      { 1e-3, 3, "cosphi", "Hz", 1e-1, 5 } } },
  { 0x39, 0x32, "AC", true, {                 // effective voltage and current: 4000 V / 4000 A
      { 1e-3, 5, "V", "A" } } },
};

const Range *findRange(quint8 code, quint8 select, int range, const Function **function)
{
  for (const Function &f : kFunctions)
    if (f.code == code && f.select == select)
    {
      *function = &f;
      return range < int(f.ranges.size()) ? &f.ranges[size_t(range)] : nullptr;
    }
  return nullptr;
}
}

bool DecoderVC870::checkFormat(const char* data, size_t idx)
{
  return (m_type == FrameFormat::VC870Continuous && (idx) && (data[idx - 1] == 0x0d) && (data[idx] == 0x0a));
}

size_t DecoderVC870::getPacketLength()
{
  return  (m_type == FrameFormat::VC870Continuous ? 23 : 0);
}

std::optional<DmmDecoder::DmmResponse> DecoderVC870::decode(const QByteArray &data, int id)
{
  // Support for the Voltcraft VC870 digital multimeter was contributed by Florian Evers, florian-evers@gmx.de, under the "GPLv3 or later" licence
  m_result = {};
  m_result.main().id = id;
  m_result.hold      = false;
  m_result.range     = "";

  const char *in = data.data();
  const Function *function = nullptr;
  const Range *range = findRange(quint8(in[0]), quint8(in[1]), in[2] & 0x0f, &function);
  if (!range)
  {
    m_result.error = tr("Parser errors");
    return m_result;
  }

  // the value of a display: OL, open, lo and hi flags, else the digits
  // with their sign
  auto display = [&](int first, int last, bool ol, bool negative, double factor, int dot, double *dval)
  {
    *dval = 0;
    if (ol || (in[19] & 0x01))   // overload or open_flag
      return QString("0L");
    if (in[19] & 0x04)           // lo_flag
      return QString("L 0");
    if (in[19] & 0x02)           // hi_flag
      return QString("H 1");
    QString val = (negative ? " -" : "  ") + makeValue(data, first, last);
    *dval = factor * val.toDouble();
    return dot ? insertCommaIT(val, dot) : val;
  };

  double dval1 = 0;
  double dval2 = 0;
  const QString val1 = display(3, 7, in[15] & 0x01, in[15] & 0x04, range->factor, range->dot, &dval1);
  const QString val2 = display(8, 12, in[17] & 0x08, in[15] & 0x08, range->factor2 != 0.0 ? range->factor2 : range->factor,
                               range->dot2 ? range->dot2 : range->dot, &dval2);

  m_result.main().special = function->special;
  m_result.showBar = function->showBar;
  m_result.main().dval    = dval1;
  m_result.main().val     = val1;
  m_result.main().unit    = range->unit1;
  m_result.main().id      = 0;
  DmmChannel &second = m_result.addChannel(1);
  if (in[20] == 0x31)   // LCD need Dual display
  {
    second.dval = dval2;
    second.val  = val2;
    second.unit = range->unit2;
  }
  return m_result;
}
