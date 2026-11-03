//======================================================================
// File:		displaywid.h
// Author:	Matthias Toussaint
// Created:	Thu Dec 26 12:04:35 CET 2002
//----------------------------------------------------------------------
// This file is part of QtDMM.
//
// QtDMM is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// QtDMM is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with QtDMM.  If not, see <http://www.gnu.org/licenses/>.
//----------------------------------------------------------------------
// Copyright (c) 2002 Matthias Toussaint
//======================================================================

#pragma once

#include <optional>

#include <QWidget>
#include <QColor>
#include <QPixmap>

#include "core/sampletypes.h"
#include "core/limittest.h"
#include "core/reading.h"
#include "device/devicetype.h"   // complete for moc: a slot takes it (GCC 16 -Wsfinae-incomplete)

/// The digital display: an LCD panel in the same housing as the analog
/// meter. Seven-segment digits, unit, annunciators (AUTO; AC, "+" for AC+DC,
/// DC, diode, continuity, HOLD as an H in a square), the min/max memory, a
/// bar graph and up to three secondary values are all drawn with QPainter
/// and scale with the widget.
/// Unlit segments and annunciators stay faintly visible, like on a real LCD.

class LcdWidget : public QWidget
{
  Q_OBJECT
public:
  /// Width/height range the LCD panel keeps; beyond it the panel stays
  /// centred with room around it.
  static constexpr double kMinAspect = 1.8;
  static constexpr double kMaxAspect = 3.2;
  /// The LCD's colours (context menu "LCD colours", Display/lcd). Custom is
  /// the tint from the Appearance page, with dark segments, as before the
  /// variants.
  enum LcdVariant
  {
    Classic,        ///< yellow-green reflective LCD, the old default tint
    BacklightBlue,  ///< negative LCD: light segments on a blue backlight
    Amber,          ///< amber backlight, dark segments
    HighContrast,   ///< black on white
    Custom          ///< the tint from the settings (setFaceColor)
  };

  LcdWidget(QWidget *parent = nullptr);

  /// Value text as the meter shows it; id 0 is the main display, 1..3 the
  /// secondary values of multi-line meters. The small line shows the first
  /// kSmallLineValues values after the big one, the others only as primary.
  void setValue(int id, const QString &);
  void setUnit(int id, const QString &);
  /// The value shown big (the sidebar's value menu, kept with the window
  /// layout). One the meter does not have (numValues of setDisplayMode())
  /// gives the main value. Only the display: the recording stays the main value's.
  void setPrimary(int id);
  int  primary() const { return m_primary; }
  /// The value shown big now: primary(), or 0 when the meter lacks it.
  int  primaryShown() const;
  /// The text on line @p line (0 the big value, 1.. the small line; for the tests).
  QString lineValue(int line) const { return m_value[idAt(line)]; }
  /// The annunciators of value @p id: AC, DC and diode from @p flags
  /// (SampleFlag), the continuity buzzer from @p quantity.
  void setMode(int id, quint32 flags, Quantity quantity = Quantity::Unknown);
  /// @name Min/max memory row
  /// @{
  void setMinValue(const QString &);
  void setMaxValue(const QString &);
  void setMinUnit(const QString &);
  void setMaxUnit(const QString &);
  /// @}
  /// Layout: digit count from the display counts, which rows to show and how
  /// many secondary values.
  void setDisplayMode(int counts, bool minMax, bool bar, int numValues);
  /// The annunciators a meter's own display has (Annunciator bits).
  enum Annunciator : quint16
  {
    AnnAuto = 0x001, AnnPeak = 0x002, AnnMax = 0x004, AnnMin = 0x008, AnnAvg = 0x010,
    AnnAc = 0x020, AnnDc = 0x040, AnnDiode = 0x080, AnnContinuity = 0x100, AnnHold = 0x200,
    AnnAll = 0x3ff
  };
  /// What the display of a kind of meter has: a bar graph, its annunciators.
  struct Look
  {
    bool    bar = true;
    quint16 annunciators = AnnAll;
  };
  static Look lookOf(DeviceType::Type type);
  /// Draws only what @p look has: the others are left out (an annunciator a
  /// reading lights still shows); their room stays, so the panels of
  /// different meters line up.
  void setLook(const Look &look);
  Look look() const { return m_look; }
  /// Only the number of values: the main one and up to three more, of which
  /// the small line shows kSmallLineValues.
  void setValueCount(int numValues);
  /// Values on the small line at most: a third one does not fit beside them.
  static constexpr int kSmallLineValues = 2;
  /// The values on the small line, in this order (the sidebar's value menu,
  /// kept with the window layout); the meter's others and the big one are
  /// left out, at most kSmallLineValues. A place left free (nothing chosen,
  /// or fewer than the meter has) takes the next value after the big one.
  void setSecondaries(const std::optional<QList<int>> &ids);
  /// The values on the small line now (lines 1 .. of lineValue()).
  QList<int> secondaries() const;
  /// How many values the small line shows now.
  int  smallLineValues() const { return int(secondaries().size()); }
  void setShowBar(bool);   ///< per-reading flag from the decoder
  /// @name Annunciators
  /// @{
  void setHold(bool);
  void setAuto(bool);
  void setManu(bool);
  /// @}
  /// The tint for Custom (Appearance page); the others bring their own.
  void setFaceColor(const QColor &);
  void setLcdVariant(LcdVariant variant);
  /// An alarm's colour for the face while it is raised (invalid: the
  /// variant's own); the segments turn light or dark, whichever reads
  /// better on it. The variant stays.
  void setAlertColor(const QColor &color);
  QColor alertColor() const { return m_alert; }
  /// The face as painted now (the alarm's colour or the variant's).
  QColor faceColor() const { return m_face; }
  QColor segmentColor() const { return m_segment; }
  LcdVariant lcdVariant() const { return m_variant; }
  /// The limit test is on: a column right of the big value with PASS or
  /// FAIL, HI or LO, and the deviation; the panel grows wider by
  /// kLimitWiden for it. Off, the LCD is as without the test.
  void setLimitTest(bool on);
  bool limitTest() const { return m_limitOn; }
  /// The verdict on the last reading and its deviation in percent (NaN: none).
  void setVerdict(LimitTest::Verdict verdict, double deviation);
  LimitTest::Verdict verdict() const { return m_verdict; }
  static constexpr double kLimitWiden = 1.25;
  /// The value is too old to stand for the present (StaleRule): the
  /// segments fade, the panel stays.
  void setStale(bool stale);
  bool isStale() const { return m_stale; }
  /// The device's name on the bezel above the face, like the model name on
  /// an instrument; empty: none, the face takes the room.
  void setCaption(const QString &caption);
  /// Words in place of the number while there is none ("Waiting for data",
  /// ValueFeed::notice()); empty: the number.
  void setNotice(const QString &notice);
  /// "classic", "blue", "amber", "contrast", "custom"
  static QString lcdVariantName(LcdVariant variant);
  static LcdVariant lcdVariantFromName(const QString &name);
  /// The Appearance page's default tint (Classic's face).
  static QColor classicFace();

public Q_SLOTS:
  /// @name Fed by the MeterController
  /// @{
  /// A reading: annunciators, value and unit of its line; a held reading
  /// keeps the old value.
  void showReading(const Reading &reading);
  /// A new minimum/maximum for the MIN/MAX row (@p value is not used).
  void showMinimum(double value, const QString &text, const QString &unit);
  void showMaximum(double value, const QString &text, const QString &unit);
  void clearMinMax();
  /// @}

public:

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

  /// Seven-segment bit mask for a character, 0 for unknown (blank).
  /// Bits: 0 a (top), 1 b, 2 c, 3 d (bottom), 4 e, 5 f, 6 g (middle).
  static int segmentsFor(QChar ch);

  /// Where the rows sit and how big their glyphs are - all sizes are
  /// derived from the row height *and* the width, so nothing overlaps on
  /// a narrow panel (test_display checks the width constraints).
  struct Layout
  {
    QRectF bezel, face;
    QRectF flags, main, minMax, bar, extra;
    QRectF verdict;               // the limit test's column right of main, empty without
    double digitH = 0;            // main digit height
    double smallH = 0;            // secondary digit height (MIN/MAX, second value)
    double flagsPx = 0;           // annunciator font size
    double minMaxBlockW = 0;      // width of one MIN/MAX block at smallH
  };
  Layout layout() const;
  /// Width the annunciator row needs at font size @p fontPx (all flags
  /// with their gaps), so layout() can shrink the font to fit.
  double flagsWidth(double fontPx) const;
  /// Width of one MIN/MAX block (label, number, unit) at digit height @p h.
  double minMaxBlockWidth(double h) const;

protected:
  void paintEvent(QPaintEvent *) override;
  void resizeEvent(QResizeEvent *) override;

private:
  int numDigits() const;
  /// The value id on line @p line: 0 the big one, 1.. the small line.
  int idAt(int line) const;
  // remembers a value with more digits than the display count
  void fitDigits(const QString &value);
  void renderStatic();
  void drawFlags(QPainter &p, const Layout &l) const;
  void drawMain(QPainter &p, const Layout &l) const;
  void drawMinMax(QPainter &p, const Layout &l) const;
  void drawBar(QPainter &p, const Layout &l, bool staticPart) const;
  void drawExtra(QPainter &p, const Layout &l) const;
  void drawVerdict(QPainter &p, const Layout &l) const;

  /// Seven-segment rendering. Returns the width used.
  double drawNumber(QPainter &p, const QPointF &origin, double h, const QString &text, int minDigits, bool lit) const;
  void drawDigit(QPainter &p, const QPointF &origin, double h, int mask) const;
  double drawUnit(QPainter &p, const QPointF &origin, double h, const QString &unit) const;
  double numberWidth(double h, int digits) const;
  void drawAnnunciator(QPainter &p, const QRectF &r, const QString &text, bool on, double fontPx) const;

  QColor m_face;
  QColor m_alert;   ///< an alarm's colour over the variant, invalid when none
  QColor m_segment;
  QColor m_customFace;             ///< setFaceColor()
  LcdVariant m_variant = Classic;
  void applyVariant();
  QColor ghost() const;

  QString m_value[kMaxValues];
  QString m_unit[kMaxValues];
  quint32  m_flags[kMaxValues] = {};
  Quantity m_quantity[kMaxValues] = { Quantity::Unknown, Quantity::Unknown, Quantity::Unknown, Quantity::Unknown };
  QString m_minValue, m_maxValue, m_minUnit, m_maxUnit;
  int m_counts = 4000;
  int m_seenDigits = 0;   ///< widest value since the display count was set
  bool m_showMinMax = true;
  bool m_showBar = true;
  bool m_paintBar = false;
  Look m_look;   ///< setLook()
  int m_numValues = 1;
  int m_primary = 0;   ///< setPrimary()
  std::optional<QList<int>> m_secondaries;   ///< setSecondaries(), unset: the first ones
  bool m_hold = false;
  bool m_auto = false;
  bool m_stale = false;
  QString m_caption;   ///< setCaption()
  QString m_notice;    ///< setNotice()
  bool m_limitOn = false;
  LimitTest::Verdict m_verdict = LimitTest::NoVerdict;
  double m_deviation = 0;   ///< NaN: none

  QPixmap m_static;
  bool m_staticDirty = true;
};
