// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The SCPI text rules (device/scpi/scpi.h): numbers with their special
// values and the handheld meters' OL, lists with quoted strings, *IDN?
// and SYST:ERR? answers, which lines are queries, numbers for commands.

#include <cmath>

#include "device/scpi/scpi.h"
#include "testutil.h"

namespace
{
using Kind = Scpi::Number::Kind;

void numbers()
{
  auto n = Scpi::number(u"12");
  check(n.isValue() && n.value == 12 && n.suffix.isEmpty(), "NR1");
  n = Scpi::number(u"  -1.5 ");
  check(n.isValue() && n.value == -1.5, "NR2 with spaces");
  n = Scpi::number(u"+1.234000E-03");
  check(n.isValue() && qAbs(n.value - 1.234e-3) < 1e-15, "NR3");
  n = Scpi::number(u".5");
  check(n.isValue() && n.value == 0.5, "no digit before the point");
  n = Scpi::number(u"1.5V");
  check(n.isValue() && n.value == 1.5 && n.suffix == u"V", "suffix after the number");
  n = Scpi::number(u"+6.000000E+00 VDC");
  check(n.isValue() && n.value == 6 && n.suffix == u"VDC", "suffix after a space");

  n = Scpi::number(u"+9.90000000E+37");
  check(n.kind == Kind::Overload && std::isinf(n.value) && !n.negative(), "9.9E37 is an overload");
  n = Scpi::number(u"-9.90000000E+37");
  check(n.kind == Kind::Overload && n.negative(), "-9.9E37 a negative one");
  n = Scpi::number(u"9.91E37");
  check(n.kind == Kind::NotANumber && std::isnan(n.value), "9.91E37 is not a number");
  n = Scpi::number(u"9.8E37");
  check(n.isValue(), "below 9.9E37 a value");

  check(Scpi::number(u"INF").kind == Kind::Overload, "INF");
  check(Scpi::number(u"ninf").negative(), "NINF negative, any case");
  check(Scpi::number(u"-INF").negative(), "-INF negative");
  check(Scpi::number(u"NAN").kind == Kind::NotANumber, "NAN");
  check(Scpi::number(u"INFO").kind == Kind::Invalid, "INFO is no INF");

  n = Scpi::number(u"OL");
  check(n.kind == Kind::Overload && !n.negative(), "OL");
  n = Scpi::number(u"+OL.000V");
  check(n.kind == Kind::Overload && n.suffix == u"V", "+OL.000V keeps the unit");
  n = Scpi::number(u"-OL MV");
  check(n.kind == Kind::Overload && n.negative() && n.suffix == u"MV", "-OL with a unit");

  check(Scpi::number(u"").kind == Kind::Invalid, "empty");
  check(Scpi::number(u"VOLT:DC").kind == Kind::Invalid, "a word");
  check(Scpi::number(u"-").kind == Kind::Invalid, "a sign alone");
}

void lists()
{
  check(Scpi::list(u"").isEmpty(), "empty response, no elements");
  check(Scpi::list(u"1, 2 ,3") == QStringList({ "1", "2", "3" }), "split and trimmed");
  check(Scpi::list(u"a,,b") == QStringList({ "a", "", "b" }), "an empty element stays");
  check(Scpi::list(u"\"a,b\",c") == QStringList({ "\"a,b\"", "c" }), "no split inside quotes");
  check(Scpi::list(u"'x,\"y',z") == QStringList({ "'x,\"y'", "z" }), "single quotes, a double inside");
  check(Scpi::list(u"\"a\"\",b\",c").size() == 2, "a doubled quote stays inside");

  check(Scpi::unquote(u" \"a\"\"b\" ") == u"a\"b", "unquote with a doubled quote");
  check(Scpi::unquote(u"'it''s'") == u"it's", "single quotes");
  check(Scpi::unquote(u" plain ") == u"plain", "not quoted: trimmed");
  check(Scpi::unquote(u"\"") == u"\"", "a lone quote stays");
}

void identities()
{
  auto id = Scpi::identity(u"OWON,SPM6103,2304001,FV:V1.2.0\n");
  check(id.isValid() && id.vendor == u"OWON" && id.model == u"SPM6103", "vendor and model");
  check(id.serial == u"2304001" && id.firmware == u"FV:V1.2.0", "serial and firmware");
  id = Scpi::identity(u"Keysight Technologies,E36312A,MY1234,1.0.4-1.0.4-1.12,X");
  check(id.firmware == u"1.0.4-1.0.4-1.12,X", "commas in the firmware kept");
  id = Scpi::identity(u"Rigol,DP832");
  check(id.isValid() && id.serial.isEmpty(), "two fields are enough");
  check(!Scpi::identity(u"garbage").isValid(), "one field is not");
}

void errors()
{
  auto e = Scpi::error(u"-113,\"Undefined header\"");
  check(e.isError() && e.code == -113 && e.message == u"Undefined header", "code and text");
  e = Scpi::error(u"+0,\"No error\"\n");
  check(!e.isError() && e.message == u"No error", "no error");
  e = Scpi::error(u"-222");
  check(e.code == -222 && e.message.isEmpty(), "code only");
  e = Scpi::error(u"what?");
  check(!e.isError() && e.message == u"what?", "unreadable: code 0, text kept");
}

void queries()
{
  check(Scpi::isQuery("*IDN?\n"), "*IDN?");
  check(Scpi::isQuery("MEAS:VOLT? (@1)"), "query with a parameter");
  check(Scpi::isQuery("  CONF?"), "spaces before the header");
  check(!Scpi::isQuery("VOLT 1.5\n"), "a command");
  check(!Scpi::isQuery("OUTP ON"), "a command without a number");
  check(Scpi::isQuery("VOLT 1;:MEAS?"), "query after a ;");
  check(Scpi::isQuery("MEAS?;:VOLT 2"), "query before a ;");
  check(!Scpi::isQuery("DISP:TEXT \"Ready?\""), "? in a string is no query");
  check(!Scpi::isQuery("DISP:TEXT 'a;b?'"), "; and ? in a string");
  check(!Scpi::isQuery(""), "empty");
}

void formats()
{
  check(Scpi::format(12.5) == "12.5", "12.5");
  check(Scpi::format(0.001) == "0.001", "0.001");
  check(Scpi::format(-3) == "-3", "-3");
  check(Scpi::format(1.0 / 3) == "0.333333333333", "12 significant digits");
  check(Scpi::format(INFINITY) == "9.9E37", "infinity");
  check(Scpi::format(-INFINITY) == "-9.9E37", "-infinity");
  check(Scpi::format(NAN) == "9.91E37", "NaN");
  check(Scpi::number(QString::fromLatin1(Scpi::format(2.5e-7))).value == 2.5e-7, "formatted reads back");
}
}

int main()
{
  numbers();
  lists();
  identities();
  errors();
  queries();
  formats();
  return failed == 0 ? 0 : 1;
}
