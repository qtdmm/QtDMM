// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QElapsedTimer>
#include <QList>

#include <limits>

#include "core/functiongenerator.h"
#include "ui/views/lcdpanel.h"

class QTimer;

/// The display of a function generator: one block per channel shown, side
/// by side (the SDS215S has one). It shows what the generator is set to -
/// a generator measures nothing.
///
/// A block: a row of annunciators - the channel's name on its colour, the
/// shape as a symbol and a word (SINE, SQUARE, RAMP, PULSE, or the name of
/// one of the device's other shapes), OUT, EXT and, at the right end, SET
/// - then the frequency big with its prefix (Hz, kHz, MHz; seven digits),
/// amplitude (Vpp) and offset (V) side by side below it, and in the last
/// row the duty cycle of a pulse or the symmetry of a ramp (blank for the
/// other shapes; the row stays, so the block does not jump).
///
/// SET blinks while a request waits for its answer and for setHold() after
/// the last turn of a knob (markSet()); a request the device did not take
/// lights ERR in its place (setChannelError()).
class GenLcdWidget : public LcdPanel
{
  Q_OBJECT
public:
  using Shape = FunctionGenerator::Shape;
  static constexpr int kMaxChannels = FunctionGenerator::kMaxChannels;
  static constexpr double kBlockMinAspect = 1.5;
  static constexpr double kBlockMaxAspect = 2.2;
  static constexpr int kDefaultHold = 2000;   ///< ms
  static constexpr int kFrequencyDigits = 7;
  static constexpr double kNone = std::numeric_limits<double>::quiet_NaN();

  /// A channel as the display shows it.
  struct Channel
  {
    QString label;              ///< "CH1"
    QColor  color;              ///< the label's background
    double  ampMax = 3;         ///< Vpp: sets the digits
    double  offsetMax = 3;      ///< V
    int     ampDecimals = 3;
    int     offsetDecimals = 3;
    int     percentDecimals = 1;
  };
  /// The rows of a block, for text().
  enum Row { Frequency, Amplitude, Offset, Extra };

  explicit GenLcdWidget(QWidget *parent = nullptr);

  void setChannels(const QList<Channel> &channels);
  int  channelCount() const { return int(m_ch.size()); }

  /// The shape of channel @p ch; @p arbitrary names one of the device's others.
  void setShape(int ch, Shape shape, const QString &arbitrary = QString());
  /// What channel @p ch is set to; kNone: blank.
  void setValues(int ch, double frequency, double amplitude, double offset, double duty = kNone,
                 double symmetry = kNone);
  void setOutput(int ch, bool on);
  void setExternal(int ch, bool on);
  /// A knob turned or a request is under way: SET blinks for hold().
  void markSet(int ch);
  bool setShown(int ch) const;
  void setHold(int ms) { m_hold = ms; }
  int  hold() const { return m_hold; }
  /// A request the device did not take (empty: none): ERR in place of SET.
  void setChannelError(int ch, const QString &error);

  /// A frequency as the display writes it: the number with seven digits
  /// and its unit ("1.000000", "kHz"); empty for NaN.
  static std::pair<QString, QString> frequencyText(double hz);
  /// The text of @p row as shown now (Frequency without its unit), empty
  /// when blank; frequencyUnit() is the frequency's unit.
  QString text(int ch, Row row) const;
  QString frequencyUnit(int ch) const;
  /// The word for the shape ("SINE"), for the annunciators and the tests.
  QString shapeWord(int ch) const;

  /// Where a block's parts are (test_gen_lcd checks they stay apart).
  struct Block
  {
    QRectF cell;
    QRectF flags, freq, amp, offset, extra;   ///< the rows; amp and offset share one
    QRectF freqText, ampText, offsetText, extraText;
    double flagsPx = 0, freqH = 0, smallH = 0, extraH = 0;
  };
  QList<Block> layout() const;

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

protected:
  double minAspect() const override;
  double maxAspect() const override;
  void paintContent(QPainter &p) override;

private:
  struct State
  {
    Channel spec;
    Shape  shape = Shape::Sine;
    QString arbitrary;
    double f = kNone, amp = kNone, offset = kNone, duty = kNone, symmetry = kNone;
    bool   out = false, ext = false;
    qint64 setUntil = 0;   ///< m_clock time SET blinks until
    QString error;
  };
  bool valid(int ch) const { return ch >= 0 && ch < int(m_ch.size()); }
  static int digits(double max, int decimals);
  double flagsWidth(double fontPx) const;
  /// @p signCell: the offset's field has one, the others are never negative.
  QRectF valueRect(const QRectF &row, double h, int digits, const QString &unit, Qt::Alignment align,
                   bool signCell = false) const;
  void drawBlock(QPainter &p, const Block &b, int ch) const;
  void drawFlags(QPainter &p, const Block &b, int ch) const;
  void drawShape(QPainter &p, const QRectF &r, Shape shape, bool on) const;
  void blink();

  QList<State> m_ch;
  int m_hold = kDefaultHold;
  QElapsedTimer m_clock;
  QTimer *m_blink;
  bool m_blinkOn = true;
};
