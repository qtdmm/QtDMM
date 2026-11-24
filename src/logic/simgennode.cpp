// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "logic/simgennode.h"

#include <QJsonArray>
#include <QRandomGenerator>

#include "core/sampletypes.h"

#include <cmath>
#include <numbers>

/// The FunctionGenerator the front works on: requests go to the node, which
/// takes them at once (a simulation reads back what it was told).
class SimFunctionGenerator : public FunctionGenerator
{
public:
  explicit SimFunctionGenerator(SimGenNode *node) : m_node(node) {}

  int channelCount() const override { return m_node->channels(); }
  ChannelInfo channel(int ch) const override
  {
    ChannelInfo info;   // the SDS215S's limits are the defaults
    info.name = QStringLiteral("CH%1").arg(ch + 1);
    info.arbitrary = SimGenNode::arbitraryShapes();
    return info;
  }
  /// The channels changed: states and settings follow, each setting as
  /// read back, the outputs as they are.
  void follow()
  {
    channelsChanged();
    for (int ch = 0; ch < channelCount(); ++ch)
      confirmAll(ch);
  }

protected:
  void sendShape(int ch, Shape shape, const QString &arbitrary) override
  {
    if (!m_node->valid(ch))
      return;
    SimGenNode::Setting &s = m_node->m_ch[ch].set;
    s.shape = shape;
    s.arbitrary = shape == Shape::Arbitrary ? arbitrary : QString();
    confirmShape(ch, s.shape, s.arbitrary);
    // a frequency the new shape cannot make comes down, as on the device
    if (s.frequency > maxFrequency(ch, shape))
    {
      s.frequency = maxFrequency(ch, shape);
      confirmValue(ch, Value::Frequency, s.frequency);
    }
  }
  void sendValue(int ch, Value v, double value) override
  {
    if (!m_node->valid(ch))
      return;
    SimGenNode::Setting &s = m_node->m_ch[ch].set;
    switch (v)
    {
      case Value::Frequency: s.frequency = value; break;
      case Value::Amplitude: s.amplitude = value; break;
      case Value::Offset: s.offset = value; break;
      case Value::Duty: s.duty = value; break;
      case Value::Symmetry: s.symmetry = value; break;
    }
    confirmValue(ch, v, value);
  }
  void sendOutput(int ch, bool on) override
  {
    if (!m_node->valid(ch))
      return;
    m_node->m_ch[ch].out = on;
    confirmOutput(ch, on);
  }

private:
  void confirmAll(int ch)
  {
    const SimGenNode::Setting s = m_node->setting(ch);
    confirmShape(ch, s.shape, s.arbitrary);
    confirmValue(ch, Value::Frequency, s.frequency);
    confirmValue(ch, Value::Amplitude, s.amplitude);
    confirmValue(ch, Value::Offset, s.offset);
    confirmValue(ch, Value::Duty, s.duty);
    confirmValue(ch, Value::Symmetry, s.symmetry);
    confirmOutput(ch, m_node->output(ch));
  }

  SimGenNode *m_node;
};

namespace
{
using Shape = SimGenNode::Shape;
constexpr double kPi = std::numbers::pi;
constexpr int kPeriodSamples = 2048;   ///< one period, for mean and RMS

// a 0..1 curve as -1..1
double centred(double v)
{
  return 2 * v - 1;
}
}

// ---------------------------------------------------------------- faults

QJsonObject SimGenNode::Faults::toJson() const
{
  return QJsonObject{ { "frequencyPpm", frequencyPpm }, { "amplitudePct", amplitudePct },
                      { "cutoffHz", cutoffHz },         { "noise", noiseMv } };
}

SimGenNode::Faults SimGenNode::Faults::fromJson(const QJsonObject &o)
{
  Faults f;
  f.frequencyPpm = o.value("frequencyPpm").toDouble(0);   // either way
  f.amplitudePct = o.value("amplitudePct").toDouble(0);
  f.cutoffHz = qMax(0.0, o.value("cutoffHz").toDouble(0));
  f.noiseMv = qMax(0.0, o.value("noise").toDouble(0));
  return f;
}

// ---------------------------------------------------------------- the shapes

QStringList SimGenNode::arbitraryShapes()
{
  // the SDS215S's names for them
  return { QStringLiteral("Sinc"),    QStringLiteral("Gauss"),   QStringLiteral("StairUp"),
           QStringLiteral("StairDn"), QStringLiteral("ExpRise"), QStringLiteral("HaverSine") };
}

double SimGenNode::waveform(Shape shape, const QString &arbitrary, double phase, double duty, double symmetry)
{
  const double p = phase - std::floor(phase);
  switch (shape)
  {
    case Shape::Sine:
      return std::sin(2 * kPi * p);
    case Shape::Square:
      return p < 0.5 ? 1 : -1;
    case Shape::Pulse:
      return p < duty / 100 ? 1 : -1;
    case Shape::Ramp:
    {
      // up over the symmetry's part of the period, down over the rest
      const double up = qBound(0.0, symmetry / 100, 1.0);
      if (up <= 0)
        return centred(1 - p);
      if (up >= 1)
        return centred(p);
      return p < up ? centred(p / up) : centred(1 - (p - up) / (1 - up));
    }
    case Shape::Arbitrary:
      break;
  }
  if (arbitrary == QLatin1String("Sinc"))
  {
    // four lobes either side; sin(x)/x runs from -0.2172 to 1
    const double x = (p - 0.5) * 8 * kPi;
    const double v = std::abs(x) < 1e-9 ? 1 : std::sin(x) / x;
    constexpr double low = -0.2172336282;
    return centred((v - low) / (1 - low));
  }
  if (arbitrary == QLatin1String("Gauss"))
    return centred(std::exp(-std::pow((p - 0.5) / 0.12, 2)));
  if (arbitrary == QLatin1String("StairUp"))
    return centred(std::floor(p * 5) / 4);
  if (arbitrary == QLatin1String("StairDn"))
    return centred(1 - std::floor(p * 5) / 4);
  if (arbitrary == QLatin1String("ExpRise"))
    return centred((1 - std::exp(-5 * p)) / (1 - std::exp(-5.0)));
  if (arbitrary == QLatin1String("HaverSine"))
    return centred((1 - std::cos(2 * kPi * p)) / 2);
  return std::sin(2 * kPi * p);   // a name it does not know: a sine
}

SimGenNode::Levels SimGenNode::levels(const Setting &s)
{
  // one period sampled: the mean and the AC part's RMS, as a true RMS meter
  double sum = 0, squares = 0;
  for (int i = 0; i < kPeriodSamples; ++i)
  {
    const double v = s.offset + s.amplitude / 2 * waveform(s.shape, s.arbitrary, (i + 0.5) / kPeriodSamples, s.duty, s.symmetry);
    sum += v;
    squares += v * v;
  }
  Levels l;
  l.mean = sum / kPeriodSamples;
  l.rms = std::sqrt(qMax(0.0, squares / kPeriodSamples - l.mean * l.mean));
  return l;
}

// ---------------------------------------------------------------- the node

SimGenNode::SimGenNode() : m_generator(std::make_unique<SimFunctionGenerator>(this))
{
  setPeriodMs(kDefaultPeriodMs);
  m_ch.resize(1);
  m_ch[0].given = defaultGiven(1);
  m_generator->follow();
}

SimGenNode::~SimGenNode() = default;

FunctionGenerator *SimGenNode::functionGenerator()
{
  return m_generator.get();
}

QString SimGenNode::quantityName(Quantity q)
{
  static const char *const names[kQuantities] = { "Vrms", "Vpp", "f", "mean", "u" };
  return QString::fromLatin1(names[q]);
}

SimGenNode::Given SimGenNode::defaultGiven(int channels)
{
  if (channels <= 1)
    return { true, false, true, true, true };
  return { true, false, true, false, false };
}

void SimGenNode::setChannels(int count)
{
  const int n = qBound(1, count, kMaxChannels);
  const bool changed = n != channels();
  m_ch.resize(n);
  // a new count: what each gives as for that count
  if (changed)
    for (Channel &c : m_ch)
      c.given = defaultGiven(n);
  m_generator->follow();
}

SimGenNode::Given SimGenNode::given(int ch) const
{
  return valid(ch) ? m_ch[ch].given : Given {};
}

void SimGenNode::setGiven(int ch, const Given &given)
{
  if (valid(ch))
    m_ch[ch].given = given;
}

SimGenNode::Setting SimGenNode::setting(int ch) const
{
  return valid(ch) ? m_ch[ch].set : Setting();
}

bool SimGenNode::output(int ch) const
{
  return valid(ch) && m_ch[ch].out;
}

QList<LogicNode::Output> SimGenNode::outputs() const
{
  static const char *const units[kQuantities] = { "V", "V", "Hz", "V", "V" };
  static const char *const couplings[kQuantities] = { "AC", "", "", "DC", "" };
  QList<Output> list;
  for (int ch : m_generator->shownChannels())
    for (int q = 0; q < kQuantities; ++q)
      if (m_ch[ch].given[q] && list.size() < kMaxValues)
      {
        Output o;
        o.name = QStringLiteral("CH%1 ").arg(ch + 1) + quantityName(Quantity(q));
        o.unit = QString::fromLatin1(units[q]);
        o.coupling = QString::fromLatin1(couplings[q]);
        list << o;
      }
  return list;
}

LogicNode::Values SimGenNode::evaluate(const Context &ctx)
{
  QRandomGenerator *random = QRandomGenerator::global();
  Values values;
  for (int ch : m_generator->shownChannels())
  {
    const Channel &c = m_ch[ch];
    std::array<std::optional<double>, kQuantities> all;
    // what the output really does: the faults on the setting
    Setting real = c.set;
    real.frequency = c.set.frequency * (1 + m_faults.frequencyPpm * 1e-6);
    double gain = 1 + m_faults.amplitudePct / 100;
    if (m_faults.cutoffHz > 0)
      gain /= std::sqrt(1 + std::pow(real.frequency / m_faults.cutoffHz, 2));
    real.amplitude = c.set.amplitude * gain;
    const double noise = m_faults.noiseMv / 1000;
    auto noisy = [&](double v) { return noise > 0 ? v + noise * (2 * random->generateDouble() - 1) : v; };

    all[Frequency] = real.frequency;
    if (!c.out)
    {
      all[Rms] = all[PeakToPeak] = all[Mean] = 0.0;
      if (real.frequency <= kSlowHz)
        all[Now] = 0.0;
    }
    else
    {
      const Levels l = levels(real);
      all[Rms] = noisy(l.rms);
      // every shape spans -1..1: peak to peak is the amplitude
      all[PeakToPeak] = real.amplitude;
      all[Mean] = noisy(l.mean);
      if (real.frequency <= kSlowHz)
      {
        // the voltage now, on the core clock: the graph shows the curve
        const double phase = double(ctx.t) / 1000 * real.frequency;
        all[Now] = noisy(real.offset + real.amplitude / 2 * waveform(real.shape, real.arbitrary, phase, real.duty, real.symmetry));
      }
    }
    for (int q = 0; q < kQuantities; ++q)
      if (c.given[q] && values.size() < kMaxValues)
        values << all[q];
  }
  return values;
}

// ---------------------------------------------------------------- stored

QJsonObject SimGenNode::parameters() const
{
  QJsonArray settings;
  for (const Channel &c : m_ch)
  {
    QJsonArray given;
    for (int q = 0; q < kQuantities; ++q)
      if (c.given[q])
        given.append(quantityName(Quantity(q)));
    settings.append(QJsonObject{ { "given", given },
                                 { "shape", FunctionGenerator::shapeName(c.set.shape) },
                                 { "arbitrary", c.set.arbitrary },
                                 { "frequency", c.set.frequency },
                                 { "amplitude", c.set.amplitude },
                                 { "offset", c.set.offset },
                                 { "duty", c.set.duty },
                                 { "symmetry", c.set.symmetry } });
  }
  return QJsonObject{ { "channels", channels() },
                      { "settings", settings },
                      { "faults", m_faults.toJson() },
                      { "generator", m_generator->settingsJson() } };
}

void SimGenNode::setParameters(const QJsonObject &p)
{
  m_faults = Faults::fromJson(p.value("faults").toObject());
  m_ch = QList<Channel>(qBound(1, p.value("channels").toInt(1), kMaxChannels));
  for (Channel &c : m_ch)
    c.given = defaultGiven(channels());
  // the outputs always start off; each value within the generator's limits
  // (follow() reads them back as they are, the front clamps the next turn)
  const QJsonArray settings = p.value("settings").toArray();
  for (int ch = 0; ch < channels() && ch < settings.size(); ++ch)
  {
    const QJsonObject o = settings[ch].toObject();
    Setting &s = m_ch[ch].set;
    s.shape = FunctionGenerator::shapeOf(o.value("shape").toString());
    s.arbitrary = o.value("arbitrary").toString();
    if (s.shape == Shape::Arbitrary && !arbitraryShapes().contains(s.arbitrary))
      s.shape = Shape::Sine;
    s.frequency = o.value("frequency").toDouble(s.frequency);
    s.amplitude = o.value("amplitude").toDouble(s.amplitude);
    s.offset = o.value("offset").toDouble(s.offset);
    s.duty = o.value("duty").toDouble(s.duty);
    s.symmetry = o.value("symmetry").toDouble(s.symmetry);
    if (o.contains("given"))
    {
      const QJsonArray given = o.value("given").toArray();
      for (int q = 0; q < kQuantities; ++q)
        m_ch[ch].given[q] = given.contains(quantityName(Quantity(q)));
    }
  }
  m_generator->follow();
  m_generator->setSettingsJson(p.value("generator").toObject());
  // stored values outside the limits (an edited file): clamped as a request would be
  for (int ch = 0; ch < channels(); ++ch)
    for (Value v : { Value::Frequency, Value::Amplitude, Value::Offset, Value::Duty, Value::Symmetry })
    {
      const double now = m_generator->state(ch).get(v);
      const double ok = m_generator->limited(ch, v, now);
      if (ok != now)
        m_generator->requestValue(ch, v, ok);
    }
}
