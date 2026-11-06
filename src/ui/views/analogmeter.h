//======================================================================
// File:		meterwid.h
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
//======================================================================

#pragma once

#include <QWidget>
#include <QColor>
#include <QPixmap>
#include <QTimer>
#include <limits>
#include "core/reading.h"   // complete for moc: a slot takes it (GCC 16 -Wsfinae-incomplete)

/// Colour set and options of the analog meter (AnalogMeter).
struct AnalogMeterStyle
{
  QColor face;   ///< dial background
  QColor faceGlow;   ///< radial highlight around the pivot
  QColor bezelLight;   ///< bezel gradient top
  QColor bezelDark;   ///< bezel gradient bottom
  QColor scale;   ///< ticks, labels, arc
  QColor needle;
  QColor redZone;   ///< labels in the red zone
  QColor redBand;   ///< the band on the arc, paler, so labels and marks stand out on it
  QColor passBand;   ///< the limit test's band on the arc (setLimits()), lighter than maxMark
  QColor boxBg;   ///< readout / unit boxes
  QColor boxText;
  QColor lampOff;
  QColor lampOn;
  QColor hold;   ///< HOLD indicator
  QColor minMark;   ///< min/max memory marks on the arc
  QColor maxMark;
  bool   ballistics = true;   ///< damped needle movement
  double redZoneFrom = 0.9;   ///< fraction of full scale

  static AnalogMeterStyle dark();    ///< black dial, white scale (the default)
  static AnalogMeterStyle ivory();   ///< cream dial, black scale, classic look
};

/// Analog moving-coil instrument in the style of a studio VU meter: dark
/// dial, white scale on an arc, red zone at the top end, needle swinging
/// from a pivot below the dial, readout boxes for the minimum and maximum of
/// the min/max memory, an overload lamp. Everything is drawn with QPainter, so it scales
/// with the widget and re-labels itself when the meter changes range.
///
/// The scale is in the unit the multimeter displays (with SI prefix), the
/// full scale comes from the display count of the meter - see
/// fullScaleFromReading(). Values are plain doubles in that unit.

class AnalogMeter : public QWidget
{
  Q_OBJECT
public:
  /// Width/height range the instrument keeps; beyond it the panel stays
  /// centred with room around it.
  static constexpr double kMinAspect = 1.3;
  static constexpr double kMaxAspect = 2.2;
  /// Full scale of a sound level (dB): the top of what the sound level meters measure.
  static constexpr double kSoundFullScale = 130.0;
  /// Where zero sits on the scale.
  enum ScaleMode
  {
    Unipolar,   ///< 0 at the left end, negative values push the needle below it
    Bipolar,    ///< -FS .. 0 .. +FS, zero in the middle
    Auto        ///< Unipolar until a value below -5 % FS arrives, then Bipolar until reset()
    // a quantity that is never negative (neverNegative()) is Unipolar in every mode
  };

  explicit AnalogMeter(QWidget *parent = nullptr);

  /// Current reading in display units. text is the meter's own rendering of
  /// it ("3.856"; MIN and MAX use its decimals), unit what the dial
  /// label shows (e.g. "mV DC"); overload parks the needle at the right stop.
  void setReading(double value, const QString &text, const QString &unit, bool overload, bool hold);
  void setFullScale(double fs);   ///< > 0, in display units
  void setPeak(double value);   ///< MAX box, display units; NaN hides it
  /// Min/max memory as marks on the scale arc (display units; NaN hides one).
  void setMinMax(double minValue, double maxValue);
  double maxMark() const { return m_markMax; }   ///< NaN = none (for the tests)
  double minMark() const { return m_markMin; }
  void setScaleMode(ScaleMode mode);
  /// Colours and options; the static layers are re-rendered.
  void setStyle(const AnalogMeterStyle &style);
  /// An alarm's colour, mixed into the dial (about a third) while it is
  /// raised so scale and pointer stay readable; invalid: the style's own.
  void setAlertColor(const QColor &color);
  QColor alertColor() const { return m_alert; }
  /// The limit test's band (PASS/FAIL) of the value @p id, in SI base
  /// units: green on the arc from @p low to @p high while the needle shows
  /// that value, cut at the ends of the scale. An id of -1 takes it away.
  void setLimits(int id, double low = 0, double high = 0);
  /// Whether the band is on the arc now (for the tests).
  bool limitsShown() const { return m_limitId >= 0 && m_limitId == m_valueId; }
  /// The dial as painted now.
  QColor dialColor() const;
  void reset();   ///< peak and Auto-bipolar latch

  /// @name Fed by the MeterController
  /// The value valueId() of a reading (other ids are ignored): full scale
  /// from the meter's display count and the decimals shown, the dial label
  /// from the unit and the coupling, overload from letters in the text.
  /// @{
  void setDisplayCounts(int counts) { m_counts = counts; }
  /// Which value of the meter the needle shows (Reading::id; the value
  /// menu); another one starts afresh, its min/max marks come with
  /// showValueMinMax().
  void setValueId(int id);
  int  valueId() const { return m_valueId; }
public Q_SLOTS:
  void showReading(const Reading &reading);
  /// The value is too old to stand for the present (StaleRule): needle
  /// and readouts fade, the dial stays.
  void setStale(bool stale);
  bool isStale() const { return m_stale; }
  /// Where the value comes from (ValueFeed::caption()), on the bezel above
  /// the dial as on the front panel's LCD; empty: none, the dial takes the room.
  void setCaption(const QString &caption);
  /// Words in place of the unit while there is no value (ValueFeed::notice()).
  void setNotice(const QString &notice);
  /// The min/max memory of value @p id (MeterController::valueMinMaxChanged),
  /// in SI base units, NaN when empty; drawn as marks in display units when
  /// it is the value shown.
  void showValueMinMax(int id, double minimum, double maximum);
  void clearMinMax();
  /// @}
public:

  ScaleMode scaleMode() const { return m_scaleMode; }
  bool bipolar() const { return m_bipolar; }
  double fullScale() const { return m_fullScale; }
  double needleAngle() const { return m_angle; }   ///< degrees, 0 = straight up
  /// True once the needle ballistics have come to rest.
  bool isSettled() const { return !m_timer.isActive(); }

  /// Needle angle for a value: -45 deg at the left end of the scale, +45 deg at
  /// the right end, clamped to the mechanical stops at +-49.5 deg.
  static double angleForValue(double value, double fullScale, bool bipolar);

  /// Full scale implied by a reading string and the meter's display count:
  /// ("3.856", 4000) -> 4, ("385.6", 4000) -> 400. NaN if the string is not
  /// a number (OL etc.), so the caller keeps the previous scale.
  static double fullScaleFromReading(const QString &value, int counts);
  /// The same, but a percentage (unit "%": state of charge, duty cycle) is
  /// always a 0..100 scale, whatever the display count says.
  static double fullScaleFromReading(const QString &value, int counts, const QString &unit);
  /// True for a unit whose quantity cannot be negative (ppm, %): its scale
  /// starts at 0 also in the modes Bipolar and Auto.
  static bool neverNegative(const QString &unit);
  /// Full scale for a value without a measuring range (a temperature): the
  /// next step of 10, 20, 50, 100, ... that keeps @p value below the red
  /// zone, and never less than @p current (NaN: none yet), so the scale only
  /// grows while the readings stay in that unit.
  static double fullScaleWithoutRange(double value, double current);
  /// Distance of the labelled (major) ticks: a 1-2-5 value for about 5
  /// (unipolar) or 8 (bipolar) intervals, made coarser until neighbouring
  /// labels labelWidth px wide fit on an arc labelRadius px from the pivot.
  /// The ticks sit on multiples of it, counted from 0.
  static double scaleStep(double fullScale, bool bipolar, double labelRadius, double labelWidth);
  /// A MIN/MAX readout: value with that many decimals, "—" for NaN, and no
  /// minus sign on a rounded zero ("-0.00" -> "0.00").
  static QString readoutString(double value, int decimals);
  /// A MIN/MAX readout as shown: @p value with the display unit @p unit (with
  /// its prefix), at most as many digits as the display has (@p counts) and
  /// at most @p decimals; a value beyond the scale (@p fullScale, another
  /// range) in a prefix of its own: "10.000 V", not "10000.0000 mV" in the
  /// mV range.
  static QString readoutFor(double value, int decimals, double fullScale, int counts, const QString &unit);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

protected:
  void paintEvent(QPaintEvent *) override;
  void resizeEvent(QResizeEvent *) override;

private:
  struct Geometry
  {
    QRectF bezel, face;
    QPointF pivot;
    double radius = 0;
    double fontPx = 10;
  };
  Geometry geometry() const;
  void renderStatic();   ///< dial, scale, boxes -> m_static
  void drawBezel(QPainter &p, const Geometry &g) const;
  void drawScale(QPainter &p, const Geometry &g) const;
  void drawBoxes(QPainter &p, const Geometry &g) const;
  void drawNeedle(QPainter &p, const Geometry &g) const;
  void drawReadouts(QPainter &p, const Geometry &g) const;
  void drawLamp(QPainter &p, const Geometry &g) const;
  void drawMarks(QPainter &p, const Geometry &g) const;
  /// The MIN (left) and MAX (right) readout boxes, kept clear of the needle.
  void readoutRects(const Geometry &g, QRectF *min, QRectF *max) const;
  /// Centre of the overload lamp; its "OL" caption fits to its right.
  QPointF lampCenter(const Geometry &g) const;
  /// A readout value with the decimals the meter shows (m_decimals).
  QString readoutText(double value) const;
  void retarget();
  void stepBallistics();

  static QString formatLabel(double v);
  static double niceStep(double range, int targetMajors);

  void applyMinMax();   ///< m_minBase/m_maxBase -> marks and peak
  /// m_bipolar from the mode, the Auto latch and m_neverNegative.
  void updatePolarity();

  int m_counts = 4000;
  QString m_unitText;   ///< unit of the last reading, with prefix
  int    m_valueId = 0;
  double m_minBase = std::numeric_limits<double>::quiet_NaN();
  double m_maxBase = std::numeric_limits<double>::quiet_NaN();
  int m_decimals = 0;   ///< most decimals of a reading (not an overload) since reset()
  double m_rangelessScale = std::numeric_limits<double>::quiet_NaN();   ///< grown scale of a temperature, NaN otherwise
  AnalogMeterStyle m_style = AnalogMeterStyle::dark();
  QColor m_alert;   ///< an alarm's colour over the dial, invalid when none
  int    m_limitId = -1;   ///< the limit test's value, -1 = none
  double m_limitLow = 0;   ///< SI base units
  double m_limitHigh = 0;
  ScaleMode m_scaleMode = Auto;
  bool m_bipolar = false;   ///< the scale shown: -FS .. FS
  bool m_latched = false;   ///< Auto: a clearly negative value came since reset()
  bool m_neverNegative = false;   ///< the unit of the last reading, neverNegative()
  double m_fullScale = 4.0;
  double m_value = 0.0;
  QString m_text;
  QString m_unit;
  QString m_caption;   ///< setCaption()
  QString m_notice;    ///< setNotice()
  bool m_overload = false;
  bool m_hold = false;
  bool m_stale = false;
  double m_peak;   ///< NaN = none
  double m_markMin;   ///< NaN = none
  double m_markMax;

  double m_angle = -45.0;   ///< where the needle is
  double m_velocity = 0.0;
  double m_target = -45.0;   ///< where it should be
  QTimer m_timer;

  QPixmap m_static;
  bool m_staticDirty = true;
};
