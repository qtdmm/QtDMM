#include "device/dmmdecoder.h"
#include "decoders.h"
#include "core/siprefix.h"

std::vector<DmmDecoder::DMMInfo> *DmmDecoder::m_configurations;

DmmDecoder::DmmDecoder(FrameFormat::DataFormat df)
  : m_type(df)
{}

void DmmDecoder::addConfig(DMMInfo info)
{
    if(!DmmDecoder::m_configurations)
        DmmDecoder::m_configurations = new std::vector<DMMInfo>();
  info.name = info.vendor + " " + info.model;
  DmmDecoder::m_configurations->push_back(info);
}

bool DmmDecoder::sameModel(const QString &a, const QString &b)
{
  auto bare = [](QString name)
  {
    if (name.endsWith(QLatin1String(" *")))
      name.chop(2);
    return canonicalModel(name);
  };
  return bare(a) == bare(b);
}

QString DmmDecoder::canonicalModel(const QString &name)
{
  if (name == QLatin1String("QtDMM Virtual meter"))
    return QStringLiteral("QtDMM Simulated meter");
  return name;
}

std::vector<DmmDecoder::DMMInfo> DmmDecoder::getDeviceConfigurations()
{
  if(!DmmDecoder::m_configurations)
    DmmDecoder::m_configurations = new std::vector<DMMInfo>();
  return *m_configurations;
}


QString DmmDecoder::insertComma(const QString &val, int pos)
{
  return val.left(2 + pos) + "." + val.right(4 - pos);
}


QString DmmDecoder::insertCommaIT(const QString &val, int pos)
{
  if (pos == 0)
    return val;

  if (val[0] == '-' || val[0] == ' ')
    return (val.left(pos + 1) + "." + val.mid(pos + 1));

  return (val.left(pos) + "." + val.mid(pos));
}

void DmmDecoder::appendSum16(QByteArray &frame)
{
  unsigned sum = 0;
  for (char c : std::as_const(frame))
    sum += static_cast<unsigned char>(c);
  frame.append(char((sum >> 8) & 0xFF));
  frame.append(char(sum & 0xFF));
}

bool DmmDecoder::sum16Valid(const unsigned char *f, int n)
{
  unsigned sum = 0;
  for (int i = 0; i < n - 2; ++i)
    sum += f[i];
  return ((sum >> 8) & 0xFF) == f[n - 2] && (sum & 0xFF) == f[n - 1];
}

bool DmmDecoder::bit(const QByteArray &data, int byte, int bit) const {
  return data[byte] & (1 << bit);
}


double DmmDecoder::prefixFactor(const QString &prefix)
{
  return SiPrefix::factor(prefix);
}

void DmmDecoder::setFromENotation(const QString &number, double value, const QString &unit, int lowestExponent)
{
  const int e = number.indexOf('E', 0, Qt::CaseInsensitive);
  int exponent = 0;
  bool expOk = e >= 0;
  if (expOk)
    exponent = number.mid(e + 1).toInt(&expOk);
  static const QMap<int, QString> prefixes = {
    { -12, "p" }, { -9, "n" }, { -6, "µ" }, { -3, "m" }, { 0, "" }, { 3, "k" }, { 6, "M" }, { 9, "G" } };
  if (expOk && exponent >= lowestExponent && prefixes.contains(exponent))
  {
    QString mantissa = number.left(e);
    if (mantissa.startsWith('+'))
      mantissa.remove(0, 1);
    m_result.main().val = mantissa;
    m_result.main().unit = prefixes.value(exponent) + unit;
  }
  else
  {
    QString prefix;
    m_result.main().val = SiPrefix::format(value, &prefix);
    m_result.main().unit = prefix + unit;
  }
}

void DmmDecoder::formatResultValue(int commaPos, const QString& prefix, const QString& baseUnit)
{
  m_result.main().val = insertCommaIT(m_result.main().val, commaPos);
  m_result.main().dval = QString(m_result.main().val).remove(' ').toDouble() * prefixFactor(prefix);   // "- 0.24"

  m_result.main().unit = prefix + baseUnit;
}

QString DmmDecoder::makeValue(const QByteArray &data, int first, int last, bool neg)
{
  QString val = neg ? "-" : "";

  for(int i=first; i<=last && i<data.size(); i++)
    val += data[i];

  return val;
}

// getInstance(): see protocols.cpp, the table has the factories




