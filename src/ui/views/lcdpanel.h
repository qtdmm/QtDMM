// Copyright (c) 2002 Matthias Toussaint
// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QPixmap>
#include <QString>
#include <QWidget>

#include "core/limittest.h"

class QFontMetricsF;
class QPainter;

/// What every LCD of QtDMM shares, whatever it shows: the housing (bezel,
/// face, the device's name on the bezel), the colours (LcdVariant, an
/// alarm's colour over them), the stale look, and the drawing kit -
/// seven-segment numbers with their unit, annunciators (text or a drawn
/// symbol), the limit test's verdict. Unlit segments and annunciators stay
/// faintly visible (ghost()), like on a real LCD.
///
/// A subclass is a layout: it says which proportions the panel keeps
/// (minAspect(), maxAspect()) and paints its rows with the kit -
/// paintStatic() into the cached part, paintContent() on every paint.
/// LcdWidget is the multimeter's.
class LcdPanel : public QWidget
{
  Q_OBJECT
public:
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

  explicit LcdPanel(QWidget *parent = nullptr);

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
  /// The tint set for Custom (setFaceColor()), invalid when none.
  QColor customFaceColor() const { return m_customFace; }
  /// The width/height range the panel keeps (its layout's).
  double minimumAspect() const { return minAspect(); }
  double maximumAspect() const { return maxAspect(); }
  /// The values are too old to stand for the present (StaleRule): the
  /// segments fade, the panel stays.
  void setStale(bool stale);
  bool isStale() const { return m_stale; }
  /// The device's name on the bezel above the face, like the model name on
  /// an instrument; empty: none, the face takes the room.
  void setCaption(const QString &caption);
  /// Pixels kept free at the right end of the name (for a button there).
  void setPlateReserve(double px);
  /// Where the name is written (widget coordinates); empty without one.
  QRectF plateBand() const;
  /// The colour of the name's lettering: the manufacturer's (@p strong) or the model's.
  QColor plateInk(bool strong) const;

  /// "classic", "blue", "amber", "contrast", "custom"
  static QString lcdVariantName(LcdVariant variant);
  static LcdVariant lcdVariantFromName(const QString &name);
  /// The Appearance page's default tint (Classic's face).
  static QColor classicFace();
  /// Seven-segment bit mask for a character, 0 for unknown (blank).
  /// Bits: 0 a (top), 1 b, 2 c, 3 d (bottom), 4 e, 5 f, 6 g (middle).
  static int segmentsFor(QChar ch);
  /// Light or dark segments, whichever reads better on @p face.
  static QColor segmentsOn(const QColor &face);
  /// Annunciators drawn as a symbol rather than typed: the diode, HOLD (an
  /// H cut out of a square), the battery (a cell with up to three bars).
  /// Their leading \x01 keeps them apart from words.
  static inline const QString kDiodeFlag = QStringLiteral("\x01" "diode");
  static inline const QString kHoldFlag = QStringLiteral("\x01" "hold");
  static inline const QString kBatteryFlag = QStringLiteral("\x01" "battery");

protected:
  /// The width/height range the panel keeps; beyond it the panel stays
  /// centred with room around it.
  virtual double minAspect() const = 0;
  virtual double maxAspect() const = 0;
  /// The bezel and, inside it, the face (below the name when there is one).
  struct Frame
  {
    QRectF bezel, face;
  };
  Frame frame() const;
  /// What does not change with the values (scales, ghost blocks), after
  /// the housing and clipped to the face; cached until invalidateStatic().
  virtual void paintStatic(QPainter &) {}
  /// The values, on every paint: clipped to the face, faded while stale.
  virtual void paintContent(QPainter &p) = 0;
  /// The cached part is drawn anew on the next paint (no update() of its own).
  void invalidateStatic() { m_staticDirty = true; }

  void paintEvent(QPaintEvent *) override;
  void resizeEvent(QResizeEvent *) override;

  /// @name The drawing kit
  /// @{
  static QFont sansFont(double px, bool bold = true);
  /// The colour of an unlit segment or annunciator.
  QColor ghost() const;
  /// Until resetInk() the kit draws with @p segment on @p face instead of
  /// the panel's colours, as on an alarm's colour (for one part of the
  /// panel: a power supply's channel). The face itself is the caller's to fill.
  void setInk(const QColor &face, const QColor &segment) const;
  void resetInk() const;
  /// The colours the kit draws with now.
  QColor ink() const { return m_inkSegment.isValid() ? m_inkSegment : m_segment; }
  QColor inkFace() const { return m_inkFace.isValid() ? m_inkFace : m_face; }
  /// Drawn on an alarm's colour (the panel's, or one part's by setInk()).
  bool onAlert() const { return m_inkFace.isValid() || m_alert.isValid(); }
  void drawDigit(QPainter &p, const QPointF &origin, double h, int mask) const;
  /// A number right-aligned in a field of @p minDigits digits, at digit
  /// height @p h: leading cells ghosted, the sign in its own cell, the
  /// decimal point between cells. Unlit (@p lit false): only ghosts.
  /// Returns the field's width, numberWidth(). Without @p signCell (a
  /// power supply's values) the field starts with the first digit.
  double drawNumber(QPainter &p, const QPointF &origin, double h, const QString &text, int minDigits, bool lit,
                    bool signCell = true) const;
  /// The unit after a number of digit height @p h; returns its width.
  double drawUnit(QPainter &p, const QPointF &origin, double h, const QString &unit) const;
  /// Width of a number field: sign cell, @p digits digits, one decimal point.
  static double numberWidth(double h, int digits, bool signCell = true);
  /// The width annunciator @p t takes at font size @p fontPx.
  static double flagWidth(const QString &t, const QFontMetricsF &fm, double fontPx);
  /// An annunciator in @p r, lit or as a ghost; kBatteryFlag shows
  /// @p batteryBars (0..3) lit bars.
  void drawAnnunciator(QPainter &p, const QRectF &r, const QString &text, bool on, double fontPx,
                       int batteryBars = 0) const;
  /// The limit test's column: PASS on green or FAIL on red, HI and LO
  /// below it as annunciators, the deviation in percent (NaN: none) under
  /// them; without a verdict a ghost of PASS.
  void drawVerdict(QPainter &p, const QRectF &column, LimitTest::Verdict verdict, double deviation) const;
  /// @}

private:
  void applyVariant();
  void renderStatic();

  QColor m_face;
  QColor m_alert;   ///< an alarm's colour over the variant, invalid when none
  QColor m_segment;
  QColor m_customFace;             ///< setFaceColor()
  mutable QColor m_inkFace;        ///< setInk(), invalid: the panel's
  mutable QColor m_inkSegment;
  LcdVariant m_variant = Classic;
  bool m_stale = false;
  QString m_caption;   ///< setCaption()
  double  m_plateReserve = 0;   ///< setPlateReserve()
  QPixmap m_static;
  bool m_staticDirty = true;
};
