#include "device/transports/calc.h"
#include "core/sampletypes.h"
#include "core/siprefix.h"

#include <QRegularExpression>

#include <QCoreApplication>
#include <QDateTime>
#include <cmath>

// Registered like a meter so it appears in the model list (vendor "QtDMM")
// and in the supported-devices table. Protocol Sigrok: the ASCII decoder
// reads the lines this device produces.
static const bool registered = []()
{
  DmmDecoder::addConfig({"QtDMM", "Calculated value", "", 0, FrameFormat::Sigrok, 8, 1, 1, 0, 400000, 0, 0, 0});
  DmmDecoder::addConfig({"QtDMM", "Simulated meter", "", 0, FrameFormat::Sigrok, 8, 1, 1, 0, 40000, 0, 0, 0});
  return true;
}();

CalcDevice::CalcDevice(const DmmDecoder::DMMInfo &info, const QString &device,
                       Inputs inputs, QObject *parent)
  : BufferedDevice(parent),
    m_dmmInfo(info),
    m_inputs(std::move(inputs))
{
  // "<unit> <formula>" per value, the values separated by ';': the unit has
  // no spaces, the formula may
  for (const QString &part : device.split(';'))
  {
    const QString d = part.trimmed();
    if (d.isEmpty() && !m_values.isEmpty())
      continue;
    Value v;
    const int space = d.indexOf(' ');
    v.unit = space < 0 ? d : d.left(space);
    v.source = space < 0 ? QString() : d.mid(space + 1).trimmed();
    // "V/AC": coupling rides along with the unit token
    const int slash = v.unit.indexOf('/');
    if (slash >= 0)
    {
      const QString c = v.unit.mid(slash + 1).toUpper();
      v.unit = v.unit.left(slash);
      if (c == "AC" || c == "DC")
        v.special = c;
    }
    m_values << v;
  }

  m_timer.setInterval(kIntervalMs);
  connect(&m_timer, &QTimer::timeout, this, &CalcDevice::tick);
}

bool CalcDevice::open(OpenMode mode)
{
  if (m_values.size() > kMaxValues)
  {
    setErrorString(QCoreApplication::translate("CalcDevice", "At most %1 values.").arg(kMaxValues));
    return false;
  }
  static const QRegularExpression earlier("^v(\\d)$");
  for (int n = 0; n < m_values.size(); ++n)
  {
    Value &v = m_values[n];
    // the value's number in the messages only when there are several
    const QString which = m_values.size() > 1 ? QCoreApplication::translate("CalcDevice", "Value %1: ").arg(n + 1)
                                               : QString();
    if (v.unit.isEmpty())
    {
      setErrorString(which + QCoreApplication::translate("CalcDevice", "No unit given for the calculated value."));
      return false;
    }
    QString error;
    int pos = -1;
    v.expr = CalcExpr::parse(v.source, &error, &pos);
    if (!v.expr)
    {
      setErrorString(which + QCoreApplication::translate("CalcDevice", "Formula error at position %1: %2").arg(pos + 1).arg(error));
      return false;
    }
    for (const QString &var : v.expr->variables())
    {
      const QRegularExpressionMatch m = earlier.match(var);
      if (m.hasMatch() && m.captured(1).toInt() > n && m.captured(1).toInt() <= m_values.size())
      {
        setErrorString(which + QCoreApplication::translate("CalcDevice", "%1 is not a value before this one.").arg(var));
        return false;
      }
    }
  }
  if (!QIODevice::open(mode & ~WriteOnly))
    return false;
  clearBuffer();
  m_lastStatus.clear();
  m_statusSent = false;
  m_openedMs = QDateTime::currentMSecsSinceEpoch();
  m_timer.start();
  return true;
}

void CalcDevice::close()
{
  m_timer.stop();
  clearBuffer();
  if (QIODevice::isOpen())
    QIODevice::close();
}

QByteArray CalcDevice::currentLine(qint64 now, QString *status) const
{
  QString message;
  QByteArray lines;
  const QMap<QString, Input> readings = m_inputs ? m_inputs() : QMap<QString, Input>();
  static const QRegularExpression earlier("^v(\\d)$");
  QList<std::optional<double>> results;   // of the values before, v1, v2 ...

  for (int n = 0; n < m_values.size(); ++n)
  {
    const Value &v = m_values[n];
    std::optional<double> result;
    if (v.expr)
    {
      // the inputs' names as variables; "uni-t_803" is reachable as uni_t_803;
      // v1, v2 ... the values before this one
      QMap<QString, double> values;
      values.insert("t", (now - m_openedMs) / 1000.0);
      QStringList missing, stale, invalid;
      bool earlierMissing = false;
      for (const QString &var : v.expr->variables())
      {
        if (var == "t")
          continue;
        const QRegularExpressionMatch m = earlier.match(var);
        const int index = m.hasMatch() ? m.captured(1).toInt() - 1 : -1;
        if (index >= 0 && index < results.size())
        {
          if (results[index])
            values.insert(var, *results[index]);
          else
            earlierMissing = true;
          continue;
        }
        QString id = var;
        if (!readings.contains(id))
        {
          for (auto it = readings.constBegin(); it != readings.constEnd(); ++it)
            if (QString(it.key()).replace('-', '_') == var)
              id = it.key();
        }
        if (!readings.contains(id))
        {
          missing << var;
          continue;
        }
        const Input &r = readings[id];
        if (now - r.msecs > kStaleMs)
          stale << var;
        else if (!r.valid)
          invalid << var;
        else
          values.insert(var, r.value);
      }

      if (missing.isEmpty() && stale.isEmpty() && invalid.isEmpty() && !earlierMissing)
        result = v.expr->eval(values);

      // the first problem is the message; a value without result because
      // one before it has none says nothing new
      if (message.isEmpty())
      {
        if (!missing.isEmpty())
          message = QCoreApplication::translate("CalcDevice", "Waiting for '%1'").arg(missing.join("', '"));
        else if (!stale.isEmpty())
          message = QCoreApplication::translate("CalcDevice", "No current value from '%1'").arg(stale.join("', '"));
        else if (!invalid.isEmpty())
          message = QCoreApplication::translate("CalcDevice", "'%1' shows no numeric value").arg(invalid.join("', '"));
        else if (!result && !earlierMissing)
          message = QCoreApplication::translate("CalcDevice", "Formula has no result (division by zero?)");
      }
    }
    results << result;

    // one value: "DC 6.02 W AUTO" as ever; several: sigrok-cli's channel in
    // front, "P2: 0.500 A DC AUTO"
    QByteArray line;
    QString value = "inf", prefix;
    if (result)
      value = formatValue(*result, m_dmmInfo.display, &prefix);
    if (m_values.size() == 1)
      line = v.special.toUtf8() + " " + value.toUtf8() + " " + (prefix + v.unit).toUtf8() + " AUTO";
    else
      line = "P" + QByteArray::number(n + 1) + ": " + value.toUtf8() + " " + (prefix + v.unit).toUtf8() + " "
             + v.special.toUtf8() + " AUTO";

    // fixed length, like SigrokDevice: the decoder's frame is 30 bytes ending in LF
    if (line.size() >= kLineLength - 1)
      line = line.left(kLineLength - 1);
    else
      line.prepend(QByteArray(kLineLength - 1 - line.size(), ' '));
    lines += line + '\n';
  }

  if (status)
    *status = message;
  return lines;
}

QString CalcDevice::formatValue(double value, int counts, QString *prefix)
{
  const double scaled = SiPrefix::scale(value, prefix);
  // like an autoranging meter: as many decimals as the display count
  // allows, 40000 counts -> 3.9999 in the 4 range, 9.640 in the 40 range;
  // the analog meter takes its range from them
  int digits = 0;
  for (int c = qMax(1, counts); c > 0; c /= 10)
    digits++;
  digits = qBound(3, digits, 9);
  const double limit = qMax(1000, counts);
  int decimals = digits - 1;
  while (decimals > 0 && std::round(std::fabs(scaled) * std::pow(10.0, decimals)) >= limit)
    decimals--;
  return QString::number(scaled, 'f', decimals);
}

void CalcDevice::tick()
{
  QString message;
  if (!replaceBuffer(currentLine(QDateTime::currentMSecsSinceEpoch(), &message)))
    return;
  if (!m_statusSent || message != m_lastStatus)
  {
    m_statusSent = true;
    m_lastStatus = message;
    Q_EMIT status(message);
  }
  Q_EMIT readyRead();
}

qint64 CalcDevice::writeData(const char *, qint64)
{
  return -1;
}
