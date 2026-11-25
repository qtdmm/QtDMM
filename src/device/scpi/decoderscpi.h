// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>

#include <optional>

#include "device/dmmdecoder.h"

/// The base of the decoders that poll an instrument with SCPI, one line per
/// answer (OWON SPM, Keysight U12xx, OpenOSC). A subclass gives its plan of
/// questions and reads each answer in parseAnswer(); framing, the order of
/// the questions and which answer belongs to which question are done here.
///
/// - **Frames:** a non-empty line ending in LF (CR LF too) is a frame
///   (getPacketLength() 0) when isFrame() takes it - every one unless a
///   subclass says otherwise. Other lines stay in the buffer and come with
///   the next frame (the Keysight's CONF? answer with its value): a frame
///   that gives no reading shows the connection's Error, so a line that
///   may come without one (an echo, a switch message) is better none.
/// - **Plan:** the steps are asked one at a time, in order, round after
///   round. A step can
///   - send commands without an answer before its question (`before`,
///     "INST:SEL CH{ch}"), in the same write;
///   - be asked only every n-th round (`every`: status, SYST:ERR?);
///   - stand for all channels (`perChannel`: asked once for each of
///     planChannels(), `{ch}` replaced by 1, 2, ...).
///   askSoon() puts a step before the others once (the Keysight's CONF?
///   after its rotary switch turned).
/// - **Pairing:** the line that ends a frame answers the question sent
///   last, so parseAnswer() is told which one it answers - also for
///   answers that cannot tell themselves ("MEAS:VOLT?" → "1.234"). Lines
///   before it in the frame, and a line the instrument sends unasked
///   (isUnasked()), answer none. A question without a frame for an answer
///   when the next is due (FrameReader asks again after two ticks) counts
///   in missedAnswers(), the plan goes on; an answer that comes that late
///   is taken for the next question.
/// - **Without a question** (a test vector, an answer after a reconnect)
///   parseAnswer() gets an Asked that is not valid: a decoder whose answers
///   tell what they are reads them by their content.
///
/// pollRequest(), checkFormat() and decode() all run in the connection's
/// thread (FrameReader calls decode() through MeterConnection directly).
class DecoderScpi : public DmmDecoder
{
  Q_OBJECT
public:
  /// One step of the plan.
  struct Step
  {
    QByteArray query;            ///< "MEAS:VOLT? (@{ch})", without the terminator
    QByteArray before;           ///< commands without an answer sent first ("INST:SEL CH{ch}"); empty: none
    int        settleMs = 0;     ///< the wait after `before`; not kept yet (one write), for the SCPI client
    int        every = 1;        ///< asked in every n-th round only
    bool       perChannel = false;   ///< once for each channel, `{ch}` replaced
  };
  /// The question an answer belongs to.
  struct Asked
  {
    int        step = -1;        ///< index into the plan; -1: no question known
    int        channel = 0;      ///< 1.. for a perChannel step, else 0
    QByteArray query;            ///< as sent, `{ch}` replaced
    bool       isValid() const { return step >= 0; }
  };

  size_t getPacketLength() final { return 0; }
  /// A non-empty line ending in LF that isFrame().
  bool checkFormat(const char *data, size_t idx) final;
  /// Each line of @p data to parseAnswer(), the last one (not unasked)
  /// with the question; the last reading they gave.
  std::optional<DmmResponse> decode(const QByteArray &data, int id) final;
  /// The next step of the plan, `before` and query each with the terminator.
  QByteArray pollRequest() const override;
  /// Questions that got no answer before the next was due.
  int missedAnswers() const { return m_missed; }

protected:
  /// @p terminator ends every command sent ("\n"; the Keysight's "\n\r\n").
  DecoderScpi(FrameFormat::DataFormat df, const QList<Step> &plan, const QByteArray &terminator = QByteArrayLiteral("\n"));

  /// Reads @p line, the answer to @p asked (not valid: unknown); a reading,
  /// or nothing for an answer that gives none (a function, a status).
  virtual std::optional<DmmResponse> parseAnswer(const Asked &asked, const QString &line, int id) = 0;
  /// Whether @p line (trimmed, not empty) ends a frame; every line by default.
  virtual bool isFrame(QByteArrayView line) const { Q_UNUSED(line); return true; }
  /// Whether @p line came unasked (the Keysight's "*3"): it answers nothing.
  virtual bool isUnasked(const QString &line) const { Q_UNUSED(line); return false; }
  /// How many channels a perChannel step is asked for.
  virtual int planChannels() const { return 1; }
  /// Step @p step is asked next, before the plan goes on.
  void askSoon(int step) { m_soon = step; }
  const QList<Step> &plan() const { return m_plan; }

private:
  /// The next question of the plan, and the plan moved on past it.
  Asked next() const;
  static QByteArray withChannel(const QByteArray &text, int ch);

  QList<Step> m_plan;
  QByteArray  m_terminator;
  mutable int m_step = 0;      ///< the step asked next
  mutable int m_channel = 1;   ///< its channel (perChannel)
  mutable int m_round = 0;     ///< rounds through the plan so far
  mutable int m_soon = -1;     ///< askSoon()'s step, -1: none
  mutable int m_missed = 0;
  mutable std::optional<Asked> m_pending;   ///< sent, not answered yet
};
