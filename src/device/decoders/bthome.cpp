// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/decoders/bthome.h"

#include <QCoreApplication>

#include <cmath>

#include "core/sampletypes.h"
#include "core/siprefix.h"

// Boards and sensors that send BTHome v2. The model is the format, not a
// device: an ESP32 with ArduinoBLE, ESPHome, a Shelly sensor. Tried with
// an ESP32 sending a temperature and a count (24.11.2026).
static const bool registered = []() {
  // one value at first, more as the advertisements bring them (as a board's
  // text lines): only what the sensor sends is shown
  DmmDecoder::addConfig({"BTHome", "v2 sensor *", "", 0, FrameFormat::BTHome, 8, 1, 1, 0, 0, 0, 0, 0, "", "", 1});
  return true;
}();

namespace
{
// One kind of object (bthome.io/format): its id, length in bytes (-1: a
// length byte first), signed, the factor of the raw value, what it is
struct Kind
{
  quint8      id;
  int         bytes;
  bool        isSigned;
  double      factor;
  int         decimals;
  const char *name;
  const char *unit;      ///< QtDMM's spelling
  const char *special;   ///< the decoder's mode code
  bool        isValue;
};

constexpr Kind kKinds[] = {
  { 0x00, 1, false, 1, 0, QT_TRANSLATE_NOOP("BTHome", "Packet id"), "", "", false },
  { 0x01, 1, false, 1, 0, QT_TRANSLATE_NOOP("BTHome", "Battery"), "%", "BAT", true },
  { 0x02, 2, true, 0.01, 2, QT_TRANSLATE_NOOP("BTHome", "Temperature"), "C", "TE", true },
  { 0x03, 2, false, 0.01, 2, QT_TRANSLATE_NOOP("BTHome", "Humidity"), "%RH", "RH", true },
  { 0x04, 3, false, 0.01, 2, QT_TRANSLATE_NOOP("BTHome", "Pressure"), "hPa", "", true },
  { 0x05, 3, false, 0.01, 2, QT_TRANSLATE_NOOP("BTHome", "Illuminance"), "lx", "", true },
  { 0x06, 2, false, 0.01, 2, QT_TRANSLATE_NOOP("BTHome", "Mass"), "kg", "", true },
  { 0x07, 2, false, 0.01, 2, QT_TRANSLATE_NOOP("BTHome", "Mass"), "lb", "", true },
  { 0x08, 2, true, 0.01, 2, QT_TRANSLATE_NOOP("BTHome", "Dew point"), "C", "TE", true },
  { 0x09, 1, false, 1, 0, QT_TRANSLATE_NOOP("BTHome", "Count"), "", "", true },
  { 0x0A, 3, false, 0.001, 3, QT_TRANSLATE_NOOP("BTHome", "Energy"), "kWh", "", true },
  { 0x0B, 3, false, 0.01, 2, QT_TRANSLATE_NOOP("BTHome", "Power"), "W", "DC", true },
  { 0x0C, 2, false, 0.001, 3, QT_TRANSLATE_NOOP("BTHome", "Voltage"), "V", "DC", true },
  { 0x0D, 2, false, 1, 0, QT_TRANSLATE_NOOP("BTHome", "PM2.5"), "µg/m³", "", true },
  { 0x0E, 2, false, 1, 0, QT_TRANSLATE_NOOP("BTHome", "PM10"), "µg/m³", "", true },
  { 0x0F, 1, false, 1, 0, QT_TRANSLATE_NOOP("BTHome", "Generic"), "", "", true },        // binary: 0 or 1
  { 0x10, 1, false, 1, 0, QT_TRANSLATE_NOOP("BTHome", "Power on"), "", "", true },
  { 0x11, 1, false, 1, 0, QT_TRANSLATE_NOOP("BTHome", "Opening"), "", "", true },
  { 0x12, 2, false, 1, 0, QT_TRANSLATE_NOOP("BTHome", "CO2"), "ppm", "CO2", true },
  { 0x13, 2, false, 1, 0, QT_TRANSLATE_NOOP("BTHome", "VOC"), "µg/m³", "", true },
  { 0x14, 2, false, 0.01, 2, QT_TRANSLATE_NOOP("BTHome", "Moisture"), "%", "", true },
  { 0x2E, 1, false, 1, 0, QT_TRANSLATE_NOOP("BTHome", "Humidity"), "%RH", "RH", true },
  { 0x2F, 1, false, 1, 0, QT_TRANSLATE_NOOP("BTHome", "Moisture"), "%", "", true },
  { 0x3A, 1, false, 1, 0, QT_TRANSLATE_NOOP("BTHome", "Button"), "", "", false },
  { 0x3C, 2, false, 1, 0, QT_TRANSLATE_NOOP("BTHome", "Dimmer"), "", "", false },
  { 0x3D, 2, false, 1, 0, QT_TRANSLATE_NOOP("BTHome", "Count"), "", "", true },
  { 0x3E, 4, false, 1, 0, QT_TRANSLATE_NOOP("BTHome", "Count"), "", "", true },
  { 0x3F, 2, true, 0.1, 1, QT_TRANSLATE_NOOP("BTHome", "Rotation"), "°", "", true },
  { 0x40, 2, false, 1, 0, QT_TRANSLATE_NOOP("BTHome", "Distance"), "mm", "", true },
  { 0x41, 2, false, 0.1, 1, QT_TRANSLATE_NOOP("BTHome", "Distance"), "m", "", true },
  { 0x42, 3, false, 0.001, 3, QT_TRANSLATE_NOOP("BTHome", "Duration"), "s", "", true },
  { 0x43, 2, false, 0.001, 3, QT_TRANSLATE_NOOP("BTHome", "Current"), "A", "DC", true },
  { 0x44, 2, false, 0.01, 2, QT_TRANSLATE_NOOP("BTHome", "Speed"), "m/s", "", true },
  { 0x45, 2, true, 0.1, 1, QT_TRANSLATE_NOOP("BTHome", "Temperature"), "C", "TE", true },
  { 0x46, 1, false, 0.1, 1, QT_TRANSLATE_NOOP("BTHome", "UV index"), "", "", true },
  { 0x47, 2, false, 0.1, 1, QT_TRANSLATE_NOOP("BTHome", "Volume"), "L", "", true },
  { 0x48, 2, false, 1, 0, QT_TRANSLATE_NOOP("BTHome", "Volume"), "mL", "", true },
  { 0x49, 2, false, 0.001, 3, QT_TRANSLATE_NOOP("BTHome", "Volume flow"), "m³/h", "", true },
  { 0x4A, 2, false, 0.1, 1, QT_TRANSLATE_NOOP("BTHome", "Voltage"), "V", "DC", true },
  { 0x4B, 3, false, 0.001, 3, QT_TRANSLATE_NOOP("BTHome", "Gas"), "m³", "", true },
  { 0x4C, 4, false, 0.001, 3, QT_TRANSLATE_NOOP("BTHome", "Gas"), "m³", "", true },
  { 0x4D, 4, false, 0.001, 3, QT_TRANSLATE_NOOP("BTHome", "Energy"), "kWh", "", true },
  { 0x4E, 4, false, 0.001, 3, QT_TRANSLATE_NOOP("BTHome", "Volume"), "L", "", true },
  { 0x4F, 4, false, 0.001, 3, QT_TRANSLATE_NOOP("BTHome", "Water"), "L", "", true },
  { 0x50, 4, false, 1, 0, QT_TRANSLATE_NOOP("BTHome", "Timestamp"), "", "", false },
  { 0x51, 2, false, 0.001, 3, QT_TRANSLATE_NOOP("BTHome", "Acceleration"), "m/s²", "", true },
  { 0x52, 2, false, 0.001, 3, QT_TRANSLATE_NOOP("BTHome", "Gyroscope"), "°/s", "", true },
  { 0x53, -1, false, 1, 0, QT_TRANSLATE_NOOP("BTHome", "Text"), "", "", false },
  { 0x54, -1, false, 1, 0, QT_TRANSLATE_NOOP("BTHome", "Raw"), "", "", false },
  { 0x55, 4, false, 0.001, 3, QT_TRANSLATE_NOOP("BTHome", "Volume"), "L", "", true },
  { 0x56, 2, false, 1, 0, QT_TRANSLATE_NOOP("BTHome", "Conductivity"), "µS/cm", "", true },
  { 0x57, 1, true, 1, 0, QT_TRANSLATE_NOOP("BTHome", "Temperature"), "C", "TE", true },
  { 0x58, 1, true, 0.35, 2, QT_TRANSLATE_NOOP("BTHome", "Temperature"), "C", "TE", true },
  { 0x59, 1, true, 1, 0, QT_TRANSLATE_NOOP("BTHome", "Count"), "", "", true },
  { 0x5A, 2, true, 1, 0, QT_TRANSLATE_NOOP("BTHome", "Count"), "", "", true },
  { 0x5B, 4, true, 1, 0, QT_TRANSLATE_NOOP("BTHome", "Count"), "", "", true },
  { 0x5C, 4, true, 0.01, 2, QT_TRANSLATE_NOOP("BTHome", "Power"), "W", "DC", true },
  { 0x5D, 2, true, 0.001, 3, QT_TRANSLATE_NOOP("BTHome", "Current"), "A", "DC", true },
  { 0x5E, 2, false, 0.01, 2, QT_TRANSLATE_NOOP("BTHome", "Direction"), "°", "", true },
  { 0xF0, 2, false, 1, 0, QT_TRANSLATE_NOOP("BTHome", "Device type"), "", "", false },
  { 0xF1, 4, false, 1, 0, QT_TRANSLATE_NOOP("BTHome", "Firmware"), "", "", false },
  { 0xF2, 3, false, 1, 0, QT_TRANSLATE_NOOP("BTHome", "Firmware"), "", "", false },
};

const Kind *kind(quint8 id)
{
  for (const Kind &k : kKinds)
    if (k.id == id)
      return &k;
  // the binary sensors (0x0F-0x2D): one byte, 0 or 1
  static constexpr Kind binary { 0x0F, 1, false, 1, 0, QT_TRANSLATE_NOOP("BTHome", "State"), "", "", true };
  if (id >= 0x15 && id <= 0x2D)
    return &binary;
  return nullptr;
}

// @p n bytes little endian from @p p, signed if asked
double readValue(const unsigned char *p, int n, bool isSigned)
{
  quint64 raw = 0;
  for (int i = n - 1; i >= 0; --i)
    raw = (raw << 8) | p[i];
  if (isSigned && n < 8 && (raw & (quint64(1) << (8 * n - 1))))
    return double(qint64(raw) - (qint64(1) << (8 * n)));
  return double(raw);
}
}   // namespace

std::optional<DecoderBTHome::Advertisement> DecoderBTHome::parse(const QByteArray &serviceData)
{
  if (serviceData.isEmpty())
    return std::nullopt;
  const auto *d = reinterpret_cast<const unsigned char *>(serviceData.constData());
  const int n = int(serviceData.size());
  Advertisement a;
  a.encrypted = d[0] & 0x01;
  a.trigger = d[0] & 0x04;
  a.version = (d[0] >> 5) & 0x07;
  if (a.encrypted || a.version != 2)
    return a;   // nothing more to read here
  int at = 1;
  while (at < n)
  {
    const Kind *k = kind(d[at]);
    if (!k)
    {
      a.complete = false;   // its length is not known: the rest is not either
      break;
    }
    int len = k->bytes;
    int start = at + 1;
    if (len < 0)
    {
      // text and raw data: a length byte first
      if (start >= n)
      {
        a.complete = false;
        break;
      }
      len = d[start];
      ++start;
    }
    if (start + len > n)
    {
      a.complete = false;   // cut off
      break;
    }
    Object o;
    o.id = d[at];
    o.name = QCoreApplication::translate("BTHome", k->name);
    o.unit = QString::fromUtf8(k->unit);
    o.special = QString::fromLatin1(k->special);
    o.decimals = k->decimals;
    o.isValue = k->isValue;
    if (k->bytes > 0)
    {
      o.value = readValue(d + start, len, k->isSigned) * k->factor;
      // by division: 10.18 stays 10.18
      if (k->decimals > 0)
      {
        double scale = 1;
        for (int i = 0; i < k->decimals; ++i)
          scale *= 10;
        o.value = std::round(o.value * scale) / scale;
      }
    }
    if (o.id == 0x00)
      a.packetId = int(o.value);
    a.objects << o;
    at = start + len;
  }
  return a;
}

QByteArray DecoderBTHome::frame(const QByteArray &serviceData)
{
  return "BTHOME " + serviceData.toHex() + '\n';
}

std::optional<DmmDecoder::DmmResponse> DecoderBTHome::decode(const QByteArray &data, int id)
{
  Q_UNUSED(id);
  const QByteArray line = data.trimmed();
  if (!line.startsWith("BTHOME "))
    return std::nullopt;
  const std::optional<Advertisement> a = parse(QByteArray::fromHex(line.mid(7)));
  if (!a)
    return std::nullopt;
  m_result = {};
  if (a->encrypted)
  {
    m_result.main().noValue = true;
    m_result.main().val = QStringLiteral("----");
    m_result.error = tr("Encrypted (a bindkey): not read yet");
    return m_result;
  }
  if (a->version != 2)
  {
    m_result.main().noValue = true;
    m_result.main().val = QStringLiteral("----");
    m_result.error = tr("BTHome version %1: only version 2 is read").arg(a->version);
    return m_result;
  }
  QVector<DmmChannel> channels;
  for (const Object &o : a->objects)
  {
    if (!o.isValue || channels.size() >= kMaxValues)
      continue;
    DmmChannel c;
    c.id = int(channels.size());
    c.name = o.name;
    c.unit = o.unit;
    c.special = o.special;
    c.val = QString::number(o.value, 'f', o.decimals);
    // in base units where the unit has a prefix (kWh, mm, mL); m³ and
    // µg/m³ are units of their own
    static const QStringList prefixed = { QStringLiteral("kWh"), QStringLiteral("mm"), QStringLiteral("mL") };
    c.dval = prefixed.contains(o.unit) ? o.value * SiPrefix::factor(o.unit.left(1)) : o.value;
    channels << c;
  }
  if (channels.isEmpty())
    return std::nullopt;   // only a packet id or events: nothing to show
  m_result.channels = channels;
  return m_result;
}
