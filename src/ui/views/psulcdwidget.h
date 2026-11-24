// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QElapsedTimer>
#include <QList>
#include <QSize>

#include <limits>

#include "ui/views/lcdpanel.h"

class QTimer;

/// The display of a power supply: one block per channel, in a fixed grid -
/// 1 channel 1×1, 2 side by side, 3 and 4 as 2×2 (with 3 one cell stays
/// empty). Only the channels the settings show take part (setChannels()).
///
/// A block: a row of annunciators - the channel's name on its colour
/// ("CH1"), OUT, CV, CC, EXT and, at the right end, SET - then the voltage
/// big, the current medium and the power small. Without a current (no
/// channel measures one: a simulated supply of stage 1) the I and P rows
/// are left out and the voltage takes the block. A value missing in one
/// channel leaves its cell blank (ghost digits); the row stays.
/// The power is the device's when it sends one, else U·I when both are
/// there, else blank.
///
/// While the knob turns (showSetpoint()) the block shows the setpoint with
/// SET blinking, for setpointHold() after the last turn, then the measured
/// value again: no row of its own for the setpoint.
///
/// An alarm's colour, the stale look and the limit test's verdict are per
/// block. Proportions: a block keeps kBlockMinAspect..kBlockMaxAspect with
/// a current, kBlockMinAspectU..kBlockMaxAspectU with the voltage alone;
/// the panel's follow from the grid.
class PsuLcdWidget : public LcdPanel
{
  Q_OBJECT
public:
  static constexpr int kMaxChannels = 4;
  static constexpr double kBlockMinAspect = 1.3;
  static constexpr double kBlockMaxAspect = 1.6;
  static constexpr double kBlockMinAspectU = 2.0;
  static constexpr double kBlockMaxAspectU = 2.8;
  static constexpr int kDefaultSetpointHold = 2000;   ///< ms
  static constexpr double kNone = std::numeric_limits<double>::quiet_NaN();

  /// A channel as the display shows it.
  struct Channel
  {
    QString label;            ///< "CH1", in front of the annunciators
    QColor  color;            ///< the label's background
    bool    current = false;  ///< measures a current (and so a power)
    double  uMax = 30;        ///< the largest voltage: sets the digits
    double  iMax = 5;
    int     uDecimals = 2;    ///< digits after the point
    int     iDecimals = 3;
    int     pDecimals = 2;
  };
  enum class Mode
  {
    None,
    CV,   ///< constant voltage
    CC    ///< constant current
  };

  explicit PsuLcdWidget(QWidget *parent = nullptr);

  /// The channels shown, in this order (at most kMaxChannels).
  void setChannels(const QList<Channel> &channels);
  int channelCount() const { return int(m_ch.size()); }
  /// Whether the blocks have the I and P rows: one channel measures a current.
  bool hasCurrentRows() const;
  /// Columns × rows for @p channels: 1×1, 2×1, 2×2.
  static QSize grid(int channels);

  /// The measured values of channel @p ch; kNone: none.
  void setValues(int ch, double u, double i = kNone, double p = kNone);
  void setOutput(int ch, bool on);
  void setMode(int ch, Mode mode);
  /// The setpoint comes from outside (a logic node): EXT.
  void setExternal(int ch, bool on);
  /// The knob turned to @p u: shown with SET for setpointHold().
  void showSetpoint(int ch, double u);
  bool showsSetpoint(int ch) const;
  void setSetpointHold(int ms) { m_hold = ms; }
  int  setpointHold() const { return m_hold; }
  /// An alarm's colour for the block (invalid: none).
  void setChannelAlert(int ch, const QColor &color);
  void setChannelStale(int ch, bool stale);
  /// The limit test of the channel: on, its verdict and the deviation in
  /// percent (NaN: none). On, the block gets the verdict's column.
  void setChannelVerdict(int ch, bool on, LimitTest::Verdict verdict = LimitTest::NoVerdict,
                         double deviation = kNone);

  /// The text in row @p row (0 U, 1 I, 2 P) of channel @p ch as shown now,
  /// empty when blank (for the tests).
  QString text(int ch, int row) const;

  /// Where a block's parts are; all sizes follow from the block's height
  /// *and* width, so nothing overlaps (test_psu_lcd checks it).
  struct Block
  {
    QRectF cell;                  ///< its grid cell
    QRectF flags, u, i, p;        ///< the rows; i and p empty without a current
    QRectF verdict;               ///< the limit test's column, empty without
    QRectF uText, iText, pText;   ///< what the numbers with their units take
    double flagsPx = 0;
    double uH = 0, iH = 0, pH = 0;   ///< digit heights
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
    double u = kNone, i = kNone, p = kNone;
    bool out = false;
    Mode mode = Mode::None;
    bool ext = false;
    double setU = kNone;
    qint64 setUntil = 0;   ///< m_clock time the setpoint is shown until
    QColor alert;
    bool stale = false;
    bool limitOn = false;
    LimitTest::Verdict verdict = LimitTest::NoVerdict;
    double deviation = kNone;
  };
  bool valid(int ch) const { return ch >= 0 && ch < int(m_ch.size()); }
  /// Digits of a field for values up to @p max with @p decimals.
  static int digits(double max, int decimals);
  double flagsWidth(double fontPx, bool current) const;
  void drawBlock(QPainter &p, const Block &b, const State &s) const;
  void drawFlags(QPainter &p, const Block &b, const State &s) const;
  /// Number and unit right-aligned in @p area.
  QRectF valueRect(const QRectF &row, double h, int digits, const QString &unit) const;
  void drawValue(QPainter &p, const QRectF &area, double h, const QString &text, int digits, const QString &unit) const;
  void blink();

  QList<State> m_ch;
  int m_hold = kDefaultSetpointHold;
  QElapsedTimer m_clock;
  QTimer *m_blink;
  bool m_blinkOn = true;
};
