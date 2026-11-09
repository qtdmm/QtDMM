// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/capturelog.h"

#include <QRegularExpression>
#include <QStringList>

#include "device/protocols.h"

namespace
{
// a YAML string in double quotes, on one line
QString quoted(QString s)
{
  s.replace('\\', "\\\\").replace('"', "\\\"").replace('\n', ' ').replace('\r', ' ').replace('\t', ' ');
  return '"' + s + '"';
}

QString hex(const QByteArray &bytes)
{
  return QString::fromLatin1(bytes.toHex(' ').toUpper());
}

// the reading of a kept frame in the spec's form: what QtDMM read, to be
// checked against what the meter showed (source_line)
QString reading(const DmmDecoder::DmmResponse &r)
{
  const DmmDecoder::DmmChannel &m = r.main();
  QStringList parts{ "value: " + quoted(m.val.trimmed()), "unit: " + quoted(m.unit) };
  if (r.range == "AUTO")
    parts << "range_mode: \"auto\"";
  else if (r.range == "MANU")
    parts << "range_mode: \"manual\"";
  if (r.hold)
    parts << "flags: [HOLD]";
  if (!m.fault.isEmpty())
    parts << "shown: \"fault\"";
  else if (m.noValue)
    parts << "shown: \"none\"";
  return '{' + parts.join(", ") + '}';
}

QString expect(const DmmDecoder::DmmResponse &r)
{
  QStringList parts;
  // a meter that sends one frame per value tells which by its id (TFA)
  if (r.main().id != 0)
    parts << "id: " + QString::number(r.main().id);
  if (!r.main().special.isEmpty())
    parts << "special: " + quoted(r.main().special);
  // the second value as the specs name it
  if (r.channels.size() > 1)
  {
    const DmmDecoder::DmmChannel &c = r.channels.at(1);
    parts << "val2: " + quoted(c.val) << "unit2: " + quoted(c.unit);
  }
  return '{' + parts.join(", ") + '}';
}
}

void CaptureLog::add(Frame frame)
{
  m_frames.append(std::move(frame));
  while (m_frames.size() > kFrames)
    m_frames.removeFirst();
}

bool CaptureLog::keep(const QString &shown, int index)
{
  if (m_frames.isEmpty())
    return false;
  if (index < 0)
    index = int(m_frames.size()) - 1;
  if (index >= m_frames.size())
    return false;
  m_kept.append(Kept{ m_frames.at(index), shown.simplified() });
  return true;
}

void CaptureLog::keep(const Frame &frame, const QString &shown)
{
  m_kept.append(Kept{ frame, shown.simplified() });
}

void CaptureLog::setKeptStep(const QString &step, bool unexpected)
{
  if (m_kept.isEmpty())
    return;
  m_kept.last().step = step;
  m_kept.last().unexpected = unexpected;
}

void CaptureLog::removeKept(int index)
{
  if (index >= 0 && index < m_kept.size())
    m_kept.removeAt(index);
}

QString CaptureLog::readingText(const Frame &frame)
{
  if (!frame.response)
    return QString();
  const DmmDecoder::DmmResponse &r = *frame.response;
  QStringList values;
  for (const DmmDecoder::DmmChannel &c : r.channels)
  {
    if (c.val.isEmpty())
      continue;
    QStringList v{ c.val.trimmed() };
    if (!c.unit.isEmpty())
      v << c.unit;
    if (!c.special.isEmpty())
      v << c.special;
    values << v.join(' ');
  }
  QStringList text{ values.join(" | ") };
  if (!r.range.isEmpty())
    text << r.range;
  if (r.hold)
    text << "HOLD";
  if (r.lowBat)
    text << "BAT";
  if (!r.error.isEmpty())
    text << r.error;
  return text.join(' ').trimmed();
}

QString CaptureLog::fileName(const QString &vendor, const QString &model, const QDateTime &date)
{
  QString device = (vendor + ' ' + model).toLower();
  device.replace(QRegularExpression("[^a-z0-9]+"), "-");
  device = device.mid(device.startsWith('-') ? 1 : 0);
  if (device.endsWith('-'))
    device.chop(1);
  if (device.isEmpty())
    device = "meter";
  return QString("qtdmm-capture-%1-%2.yaml").arg(device, date.toString("yyyy-MM-dd-HHmm"));
}

QString CaptureLog::connectionName(Transport::PortType type)
{
  switch (type)
  {
  case Transport::PortType::Serial:
    return "serial";
  case Transport::PortType::Hid:
  case Transport::PortType::HidHoltek:
    return "USB-HID";
  case Transport::PortType::Sigrok:
    return "sigrok-cli";
  case Transport::PortType::RFC2217:
    return "RFC 2217";
  case Transport::PortType::Calc:
    return "calculated";
  case Transport::PortType::Ble:
  case Transport::PortType::BleGatt:
    return "Bluetooth LE";
  case Transport::PortType::None:
    break;
  }
  return "unknown";
}

QString CaptureLog::yaml(const Header &header, bool allFrames) const
{
  const ProtocolInfo *protocol = protocolInfo(header.protocol);
  const QString device = QString("%1 %2").arg(header.vendor, header.model).trimmed();
  QStringList out;
  out << "# A capture of QtDMM: frames of the meter and what it showed, in the form of"
      << "# the test vectors in docs/protocols/spec. 'source_line' is what the meter"
      << "# showed, 'reading' and 'expect' what QtDMM read - where they differ, QtDMM"
      << "# reads it wrong."
      << "protocol: " + QString::fromLatin1(protocol ? protocol->name : "unknown")
      << "devices: [" + quoted(device) + ']'
      << "frames_per_reading: " + QString::number(header.framesPerReading)
      << "sources:"
      << "  - " + quoted(QString("capture with QtDMM %1, %2 over %3, %4")
                           .arg(header.version, device, connectionName(header.connection),
                                header.date.toString("yyyy-MM-dd")))
      << ""
      << "vectors:";
  for (const Kept &k : m_kept)
  {
    if (!k.step.isEmpty())
      out << "  # step " + k.step + (k.unexpected ? " - QtDMM read something else than the step wants" : QString());
    out << "  - source_line: " + quoted(k.shown) << "    bytes: " + hex(k.frame.bytes);
    if (k.frame.response)
      out << "    reading: " + reading(*k.frame.response) << "    expect: " + expect(*k.frame.response);
    else
      out << "    # QtDMM did not understand this frame";
  }
  if (!m_skipped.isEmpty())
    out << "# steps skipped: " + m_skipped.join(", ");
  if (allFrames)
  {
    out << "" << "frames:";
    for (const Frame &f : m_frames)
    {
      QString line = QString("  - {t: %1, bytes: %2}")
                       .arg(quoted(QDateTime::fromMSecsSinceEpoch(f.wall).toString("HH:mm:ss.zzz")), hex(f.bytes));
      const QString read = readingText(f);
      out << line + (read.isEmpty() ? QString("   # not understood") : "   # " + read);
    }
  }
  return out.join('\n') + '\n';
}
