// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/scpi/decoderscpi.h"

#include <QtGlobal>

#include <algorithm>

DecoderScpi::DecoderScpi(FrameFormat::DataFormat df, const QList<Step> &plan, const QByteArray &terminator)
  : DmmDecoder(df), m_plan(plan), m_terminator(terminator)
{
  Q_ASSERT(std::any_of(m_plan.cbegin(), m_plan.cend(), [](const Step &s) { return s.every <= 1; }));
}

bool DecoderScpi::checkFormat(const char *data, size_t idx)
{
  // the last line, without CR and spaces; the frame starts at index 0
  // (FrameReader starts over after each one), lines before it come along
  if (data[idx] != '\n')
    return false;
  size_t start = idx;
  while (start > 0 && data[start - 1] != '\n')
    --start;
  const QByteArrayView line = QByteArrayView(data + start, qsizetype(idx - start)).trimmed();
  return !line.isEmpty() && isFrame(line);
}

std::optional<DmmDecoder::DmmResponse> DecoderScpi::decode(const QByteArray &data, int id)
{
  QStringList lines;
  for (const QByteArray &raw : data.split('\n'))
    if (const QString line = QString::fromLatin1(raw).trimmed(); !line.isEmpty())
      lines << line;
  // the line that ends the frame answers the question; lines before it
  // waited in the buffer (a subclass's checkFormat() took no frame there)
  qsizetype answer = lines.size() - 1;
  while (answer >= 0 && isUnasked(lines[answer]))
    --answer;
  std::optional<DmmResponse> reading;
  for (qsizetype i = 0; i < lines.size(); ++i)
  {
    Asked asked;
    if (i == answer)
    {
      asked = m_pending.value_or(Asked());
      m_pending.reset();
    }
    if (auto r = parseAnswer(asked, lines[i], id))
      reading = std::move(r);
  }
  return reading;
}

QByteArray DecoderScpi::pollRequest() const
{
  if (m_plan.isEmpty())
    return {};
  if (m_pending)
    ++m_missed;
  const Asked asked = next();
  m_pending = asked;
  const Step &s = m_plan[asked.step];
  QByteArray out;
  if (!s.before.isEmpty())
    out += withChannel(s.before, asked.channel) + m_terminator;
  return out + asked.query + m_terminator;
}

DecoderScpi::Asked DecoderScpi::next() const
{
  const int channels = qMax(1, planChannels());
  if (m_soon >= 0 && m_soon < m_plan.size())
  {
    const int step = std::exchange(m_soon, -1);
    const int ch = m_plan[step].perChannel ? 1 : 0;
    return { step, ch, withChannel(m_plan[step].query, ch) };
  }
  // at most every step of every channel once in each round until one is due
  int longest = 1;
  for (const Step &s : m_plan)
    longest = qMax(longest, s.every);
  for (int tries = int(m_plan.size()) * channels * longest; tries >= 0; --tries)
  {
    if (m_step >= m_plan.size())
    {
      m_step = 0;
      m_channel = 1;
      ++m_round;
    }
    const Step &s = m_plan[m_step];
    const int ch = s.perChannel ? qMin(m_channel, channels) : 0;
    const bool due = s.every <= 1 || m_round % s.every == 0;
    const Asked asked { int(m_step), ch, withChannel(s.query, ch) };
    if (s.perChannel && m_channel < channels)
      ++m_channel;
    else
    {
      m_channel = 1;
      ++m_step;
    }
    if (due)
      return asked;
  }
  return { 0, 0, m_plan[0].query };   // not reached: a step asked every round is there (constructor)
}

QByteArray DecoderScpi::withChannel(const QByteArray &text, int ch)
{
  if (ch <= 0)
    return text;
  QByteArray out = text;
  return out.replace("{ch}", QByteArray::number(ch));
}
