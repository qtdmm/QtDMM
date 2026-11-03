// Tests for the digital display: seven-segment glyph table and a headless
// render check. TEST_DISPLAY_DUMP=<dir> writes PNGs for visual review.
#include <QApplication>
#include <QImage>
#include <QDir>
#include <QDebug>

#include "ui/views/lcdwidget.h"
#include "ui/panelframe.h"
#include "core/siprefix.h"
#include "testutil.h"

static QImage render(LcdWidget &w, const QSize &size)
{
  w.resize(size);
  QImage img(size, QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::transparent);
  w.render(&img, QPoint(), QRegion(), QWidget::DrawChildren);
  return img;
}

static int darkPixels(const QImage &img)
{
  int n = 0;
  for (int y = 0; y < img.height(); ++y)
    for (int x = 0; x < img.width(); ++x)
      if (qAlpha(img.pixel(x, y)) > 0 && qGray(img.pixel(x, y)) < 60)
        n++;
  return n;
}

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  // --- 0. the name plate: manufacturer and model apart, a name of its own whole ---
  {
    const auto [vendor, model] = PanelFrame::splitName("Uni-Trend UT61E");
    check(vendor == "Uni-Trend" && model == "UT61E", "plate: " + vendor + " | " + model);
    const auto [none, own] = PanelFrame::splitName("Bench sim");
    check(none.isEmpty() && own == "Bench sim", "plate: own name whole, got " + none + " | " + own);
    const auto [alone, rest] = PanelFrame::splitName("Uni-Trend");
    check(alone.isEmpty() && rest == "Uni-Trend", "plate: a manufacturer alone stays whole");
  }

  // --- 1. glyphs: every digit, the letters meters send (OL, Err, dIodE, bAt) ---
  check(LcdWidget::segmentsFor('8') == 0x7f, "8 lights all seven segments");
  check(LcdWidget::segmentsFor('1') == 0x06, "1 lights b and c");
  check(LcdWidget::segmentsFor('-') == 0x40, "minus is the middle segment");
  check(LcdWidget::segmentsFor(' ') == 0, "space is blank");
  for (QChar ch : QString("0123456789abcdefhlnoprstuy"))
    check(LcdWidget::segmentsFor(ch) != 0, QString("glyph for '%1'").arg(ch));
  check(LcdWidget::segmentsFor('L') == LcdWidget::segmentsFor('l'), "letters are case-insensitive");
  check(LcdWidget::segmentsFor('0') != LcdWidget::segmentsFor('o'), "0 and o differ (o is lower case)");

  // --- 1b. units as the displays write them (LCD and analog meter share it) ---
  check(SiPrefix::displayText("Ohm") == QStringLiteral("Ω"), "Ohm -> Ω");
  check(SiPrefix::displayText("kOhm") == QStringLiteral("kΩ"), "kOhm -> kΩ");
  check(SiPrefix::displayText("MOhm") == QStringLiteral("MΩ"), "MOhm -> MΩ");
  check(SiPrefix::displayText("C") == QStringLiteral("°C"), "C -> °C");
  check(SiPrefix::displayText("dF") == QStringLiteral("°F"), "dF -> °F");
  check(SiPrefix::displayText("cosphi") == QStringLiteral("cosφ"), "cosphi -> cosφ");
  check(SiPrefix::displayText("mV") == "mV" && SiPrefix::displayText("Hz") == "Hz", "other units unchanged");

  // units of their own stay whole, prefixed ones are split
  for (const char *whole : { "ppm", "fc", "min", "mol", "cosphi", "m/s", "%RH", "dBA", "dBC" })
    check(SiPrefix::split(whole).prefix.isEmpty() && SiPrefix::split(whole).baseUnit == whole,
          QString("%1 stays whole").arg(whole));
  check(SiPrefix::split("mV").prefix == "m" && SiPrefix::split("kOhm").baseUnit == "Ohm", "mV, kOhm split");

  // ... and take no prefix, nor do °C/°F, % and no unit: 1200 ppm, not 1.2 kppm
  for (const char *plain : { "ppm", "%RH", "m/s", "dBA", "cosphi", "%", "C", "dF", "°C", "" })
  {
    QString prefix = "x";
    const double v = SiPrefix::scale(1200.0, QString::fromUtf8(plain), &prefix);
    check(!SiPrefix::takesPrefix(QString::fromUtf8(plain)) && v == 1200.0 && prefix.isEmpty(),
          QString("%1 takes no prefix").arg(plain));
  }
  {
    QString prefix;
    check(SiPrefix::takesPrefix("V") && SiPrefix::format(1200.0, "V", &prefix) == "1.2" && prefix == "k", "1200 V is 1.2 kV");
    check(SiPrefix::format(1234.5, "ppm", &prefix) == "1234.5" && prefix.isEmpty(), "1234.5 ppm stays");
  }

  // --- 1c. leading zeros go, as on the meter (table, analog meter) ---
  check(SiPrefix::withoutLeadingZeros("000.00") == "0.00", "000.00 -> 0.00");
  check(SiPrefix::withoutLeadingZeros("-029.30") == "-29.30", "-029.30 -> -29.30");
  check(SiPrefix::withoutLeadingZeros(" 00012 ") == "12" && SiPrefix::withoutLeadingZeros("0") == "0", "integers");
  check(SiPrefix::withoutLeadingZeros("OL") == "OL" && SiPrefix::withoutLeadingZeros("0.L") == "0.L", "text stays");

  // --- 1d. the LCD: leading zeros dark, the minus always in the sign cell
  //         ("-029.30" shows "-  29.30" like the UT61E, not "- 029.30"
  //         nor " -29.30"): the same picture as the value without them ---
  {
    LcdWidget a, b;
    for (LcdWidget *d : { &a, &b })
    {
      d->setDisplayMode(22000, true, true, 1);
      d->setUnit(0, "mV");
    }
    a.setValue(0, "000.00");
    b.setValue(0, "0.00");
    check(render(a, QSize(520, 200)) == render(b, QSize(520, 200)), "LCD: 000.00 looks like 0.00");
    a.setValue(0, "-029.30");
    b.setValue(0, "-29.30");
    const QImage zeros = render(a, QSize(520, 200));
    check(zeros == render(b, QSize(520, 200)), "LCD: -029.30 looks like -29.30");
    // the minus in the same place for 2 and 3 digits: the pixels "-" adds
    // to "29.30" and to "129.30" are the same
    auto minusPixels = [&](const QString &digits)
    {
      b.setValue(0, digits);
      const QImage plain = render(b, QSize(520, 200));
      b.setValue(0, "-" + digits);
      const QImage minus = render(b, QSize(520, 200));
      QRect box;
      for (int y = 0; y < plain.height(); ++y)
        for (int x = 0; x < plain.width(); ++x)
          if (plain.pixel(x, y) != minus.pixel(x, y))
            box |= QRect(x, y, 1, 1);
      return box;
    };
    const QRect two = minusPixels("29.30"), three = minusPixels("129.30");
    check(!two.isEmpty() && two == three,
          QString("LCD: the minus in the sign cell for 29.30 (%1,%2) and 129.30 (%3,%4)")
            .arg(two.x()).arg(two.width()).arg(three.x()).arg(three.width()));
  }

  // --- 1d2. the display after the kind of meter: a thermometer has no
  //          bar and no AC/DC, a Victron no annunciators; the room stays,
  //          an annunciator a reading lights still shows ---
  {
    check(LcdWidget::lookOf(DeviceType::HandheldDmm).bar && LcdWidget::lookOf(DeviceType::Virtual).annunciators == LcdWidget::AnnAll,
          "LCD look: multimeters and the Simulated meter have all of it");
    const LcdWidget::Look thermo = LcdWidget::lookOf(DeviceType::Thermometer);
    check(!thermo.bar && (thermo.annunciators & LcdWidget::AnnHold) && !(thermo.annunciators & LcdWidget::AnnAc),
          "LCD look: a thermometer has HOLD, no bar, no AC");
    check(LcdWidget::lookOf(DeviceType::BatteryMonitor).annunciators == 0, "LCD look: a Victron has no annunciators");
    LcdWidget a, b;
    for (LcdWidget *d : { &a, &b })
    {
      d->setDisplayMode(6000, true, true, 1);
      d->setUnit(0, "V");
      d->setValue(0, "1.234");
    }
    b.setLook(LcdWidget::lookOf(DeviceType::BatteryMonitor));
    a.resize(520, 200);
    b.resize(520, 200);
    check(a.layout().main == b.layout().main && a.layout().bar == b.layout().bar, "LCD look: the layout stays");
    check(render(a, QSize(520, 200)) != render(b, QSize(520, 200)), "LCD look: no bar, no annunciators drawn");
    const QImage off = render(b, QSize(520, 200));
    b.setHold(true);
    check(render(b, QSize(520, 200)) != off, "LCD look: HOLD lit by a reading shows anyway");
  }

  // --- 1e. a value with more digits than the display count (a Victron
  //         current "-10.967" at 6000 counts) widens the field: the same
  //         picture as with a count of five digits, the unit after the number
  //         and not on its last digit ---
  {
    LcdWidget a, b;
    a.setDisplayMode(6000, true, false, 1);
    b.setDisplayMode(60000, true, false, 1);
    for (LcdWidget *d : { &a, &b })
    {
      d->setUnit(0, "A");
      d->setValue(0, "-10.967");
      d->setMinUnit("A");
      d->setMinValue("-11.100");
    }
    check(render(a, QSize(520, 200)) == render(b, QSize(520, 200)), "LCD: -10.967 A at 6000 counts gets five digits");
  }

  // --- 2. render: a value paints more dark pixels than an empty display ---
  const QString dump = qEnvironmentVariable("TEST_DISPLAY_DUMP");
  const QSize size(520, 200);
  LcdWidget w;
  w.setDisplayMode(4000, true, true, 1);
  QImage empty = render(w, size);
  check(!empty.isNull(), "renders empty");
  if (!dump.isEmpty()) empty.save(QDir(dump).filePath("display_empty.png"));

  w.setValue(0, "-3.856");
  w.setUnit(0, "mV");
  w.setMode(0, SampleFlag::DC);
  w.setAuto(true);
  w.setShowBar(true);
  w.setMinValue("-3.900");
  w.setMinUnit("mV");
  w.setMaxValue("0.412");
  w.setMaxUnit("mV");
  QImage value = render(w, size);
  check(darkPixels(value) > darkPixels(empty) + 500, "lit segments add dark pixels");
  if (!dump.isEmpty()) value.save(QDir(dump).filePath("display_value.png"));
  check(qAlpha(value.pixel(0, 0)) == 0, "corners outside the bezel stay transparent");

  // a stale value fades, but stays readable; a current one is back as it was
  w.setStale(true);
  QImage stale = render(w, size);
  if (!dump.isEmpty()) stale.save(QDir(dump).filePath("display_stale.png"));
  check(darkPixels(stale) < darkPixels(value) - 200 && darkPixels(stale) >= darkPixels(empty),
        QString("stale: fewer dark pixels (%1 < %2)").arg(darkPixels(stale)).arg(darkPixels(value)));
  w.setStale(false);
  check(render(w, size) == value, "current again: as before");

  // --- 3. aspect clamp: a strip-shaped widget keeps a panel of sane shape ---
  {
    QImage wide = render(w, QSize(1400, 200));
    // far left/right of a very wide widget is outside the panel
    check(qAlpha(wide.pixel(20, 100)) == 0 && qAlpha(wide.pixel(1379, 100)) == 0,
          "very wide widget: panel does not stretch to the edges");
    check(qAlpha(wide.pixel(700, 100)) != 0, "very wide widget: panel is centred");
    QImage tall = render(w, QSize(300, 400));
    check(qAlpha(tall.pixel(150, 10)) == 0 && qAlpha(tall.pixel(150, 389)) == 0,
          "very tall widget: panel does not stretch vertically");
  }

  // --- 4. narrow panels: flags and MIN/MAX blocks must fit the width ---
  // (they used to overlap: MANU into AC, MIN's unit under MAX)
  {
    LcdWidget n;
    n.setDisplayMode(50000, true, true, 1);   // 5 digits
    n.setValue(0, "12.345");
    n.setUnit(0, "MOhm");
    n.setMode(0, SampleFlag::AC);
    n.setManu(true);
    n.setMinValue("11.111");
    n.setMinUnit("MOhm");
    n.setMaxValue("13.333");
    n.setMaxUnit("MOhm");
    // CI runners without fonts (Windows, FreeBSD) fall back to a font whose
    // metrics do not follow the pixel size; the fit cannot hold there
    QFont probe("Sans");
    probe.setPixelSize(10);
    const double at10 = QFontMetricsF(probe).horizontalAdvance("MANU");
    probe.setPixelSize(20);
    const bool scalableFont = QFontMetricsF(probe).horizontalAdvance("MANU") > at10 * 1.5;
    if (!scalableFont)
      qWarning() << "no scalable font on this machine - width checks skipped";
    for (int width : {160, 200, 260, 320, 400, 520})   // down to well below minimumSizeHint (260)
    {
      n.resize(width, 400);
      const LcdWidget::Layout l = n.layout();
      // fits - or the font is already at its 5 px floor (a fallback font
      // on a machine without fonts is wide even there)
      const bool flagsFloor = l.flagsPx <= 5.5, smallFloor = l.smallH <= 5.5;
      check(!scalableFont || flagsFloor || n.flagsWidth(l.flagsPx) <= l.flags.width() + 0.5,
            QString("width %1: annunciators fit (%2 <= %3)").arg(width).arg(n.flagsWidth(l.flagsPx)).arg(l.flags.width()));
      check(!scalableFont || smallFloor || 2 * l.minMaxBlockW + l.smallH <= l.minMax.width() + 0.5,
            QString("width %1: MIN and MAX blocks fit (%2 <= %3)").arg(width).arg(2 * l.minMaxBlockW + l.smallH).arg(l.minMax.width()));
      check(l.flagsPx > 0 && l.smallH > 0, QString("width %1: sizes stay positive").arg(width));
      if (!dump.isEmpty())
        render(n, QSize(width, 400)).save(QDir(dump).filePath(QString("display_narrow_%1.png").arg(width)));
    }
    // a roomy panel is not shrunk by the width rule
    n.resize(700, 200);   // the left row has P before MAX and MIN
    const LcdWidget::Layout l = n.layout();
    check(l.flagsPx <= l.flags.height() * 0.75 + 0.01, "annunciator font never exceeds the row's");
    check(!scalableFont || qFuzzyCompare(l.flagsPx, l.flags.height() * 0.75), "wide panel: annunciator font is the row's");
  }

  if (!dump.isEmpty())
  {
    w.setValue(0, "0.L");
    w.setUnit(0, "Ohm");
    w.setMode(0, 0, Quantity::Resistance);
    w.setHold(true);
    render(w, size).save(QDir(dump).filePath("display_ol.png"));
    w.setHold(false);
    w.setValue(0, "1.234");
    w.setUnit(0, "V");
    w.setMode(0, SampleFlag::Diode, Quantity::Voltage);
    render(w, size).save(QDir(dump).filePath("display_diode.png"));
    w.setDisplayMode(22000, true, true, 2);
    w.setValue(0, "12.345");
    w.setUnit(0, "V");
    w.setMode(0, SampleFlag::AC);
    w.setValue(1, "50.02");
    w.setUnit(1, "Hz");
    render(w, size).save(QDir(dump).filePath("display_two_values.png"));
    w.setDisplayMode(4000, false, false, 1);
    w.setValue(0, "3.856");
    w.setUnit(0, "kOhm");
    w.setMode(0, 0, Quantity::Resistance);
    render(w, QSize(300, 110)).save(QDir(dump).filePath("display_small_plain.png"));
    render(w, QSize(1000, 400)).save(QDir(dump).filePath("display_large_plain.png"));
    w.setFaceColor(QColor(0x20, 0x30, 0x40));
    render(w, size).save(QDir(dump).filePath("display_dark_face.png"));
    w.setFaceColor(QColor(0xda, 0xdc, 0x77));
    w.setDisplayMode(4000, true, true, 1);
    render(w, QSize(1400, 200)).save(QDir(dump).filePath("display_very_wide.png"));
    render(w, QSize(300, 400)).save(QDir(dump).filePath("display_very_tall.png"));
  }

  // --- 6. the annunciators: AUTO lit or not (no MANU), HOLD as the H symbol
  //        at the right, "+" between AC and DC for AC+DC ---
  {
    LcdWidget f;
    f.setDisplayMode(4000, false, false, 1);
    f.setValue(0, "1.000");
    f.setUnit(0, "V");
    auto lit = [&](quint32 flags, bool hold, bool autoRange)
    {
      f.setMode(0, flags);
      f.setHold(hold);
      f.setAuto(autoRange);
      return darkPixels(render(f, QSize(520, 200)));
    };
    const int none = lit(0, false, false);
    const int ac = lit(SampleFlag::AC, false, false);
    const int dc = lit(SampleFlag::DC, false, false);
    const int acdc = lit(SampleFlag::AC | SampleFlag::DC, false, false);
    check(acdc > ac + dc - none + 5, QString("AC+DC lights the plus: %1 > %2 + %3 - %4").arg(acdc).arg(ac).arg(dc).arg(none));
    check(lit(0, true, false) > none + 20, "HOLD lights its symbol");
    check(lit(0, false, true) > none + 20, "AUTO lights");
    // the meter's own MAX, MIN and AVG (UNI-T iENV, APPA)
    const int max = lit(SampleFlag::Max, false, false);
    const int min = lit(SampleFlag::Min, false, false);
    const int avg = lit(SampleFlag::Avg, false, false);
    check(max > none + 20 && min > none + 20 && avg > none + 20, "MAX, MIN and AVG light");
    // the UT202BT's peak maximum and minimum: P before MAX and MIN lights too
    check(lit(SampleFlag::Peak | SampleFlag::Max, false, false) > max + 5
            && lit(SampleFlag::Peak | SampleFlag::Min, false, false) > min + 5, "peak: P lights before MAX and MIN");
    check(lit(SampleFlag::Max | SampleFlag::Min, false, false) > max + 20, "MAX and MIN light both");
    lit(0, false, true);
    f.setManu(true);
    check(darkPixels(render(f, QSize(520, 200))) == none, "manual range: AUTO off, nothing else lit");
    if (!dump.isEmpty())
    {
      f.setMode(0, SampleFlag::AC | SampleFlag::DC | SampleFlag::Max);
      f.setHold(true);
      f.setAuto(true);
      render(f, QSize(520, 200)).save(QDir(dump).filePath("display_flags.png"));
    }
  }

  // --- the primary value: shown big, the others follow on the small line
  //     in their order; one the meter does not have gives the main value ---
  {
    LcdWidget f;
    f.setDisplayMode(4000, true, true, 3);
    f.setValue(0, "734");
    f.setValue(1, "22.50");
    f.setValue(2, "45.10");
    auto lines = [&] { return QStringList{ f.lineValue(0), f.lineValue(1), f.lineValue(2) }.join(' '); };
    check(lines() == "734 22.50 45.10", "primary: the main value at first, got " + lines());
    f.setPrimary(2);
    check(f.primaryShown() == 2 && lines() == "45.10 734 22.50", "primary 3: big, then 1 and 2, got " + lines());
    f.setPrimary(1);
    check(lines() == "22.50 734 45.10", "primary 2: big, then 1 and 3, got " + lines());
    f.setDisplayMode(4000, true, true, 1);
    check(f.primaryShown() == 0 && f.lineValue(0) == "734", "primary: a meter with one value shows its main value");
    check(f.primary() == 1, "primary: the choice stays for a meter that has it");
    check(f.smallLineValues() == 0, "one value: no small line");
    // four values: the small line has two of them, the fourth can be the
    // big one; the number alone (setValueCount)
    f.setValueCount(4);
    check(f.smallLineValues() == 2, "four values: two on the small line");
    f.setValue(3, "1013");
    auto four = [&] { return QStringList{ f.lineValue(0), f.lineValue(1), f.lineValue(2), f.lineValue(3) }.join(' '); };
    check(four() == "22.50 734 45.10 1013", "four values, primary 2, got " + four());
    f.setPrimary(3);
    check(f.primaryShown() == 3 && four() == "1013 734 22.50 45.10",
          "primary 4: big, then 1 and 2 on the small line, got " + four());
    // the small line chosen: in this order, the big one and missing ones left out
    f.setSecondaries(QList<int>{ 2, 0 });
    check(four() == "1013 45.10 734 22.50", "small line 3 and 1, got " + four());
    f.setSecondaries(QList<int>{ 3, 1 });
    check(f.secondaries() == (QList<int>{ 1, 0 }), "the big one is not on the small line again, the place filled");
    f.setSecondaries(QList<int>());
    check(f.secondaries() == (QList<int>{ 0, 1 }) && f.lineValue(0) == "1013", "none chosen: the first values");
    // a choice from a meter of two values: the next meter's third value
    // takes the free place
    f.setSecondaries(QList<int>{ 2 });
    check(f.secondaries() == (QList<int>{ 2, 0 }), "one chosen: the free place filled");
    f.setSecondaries(QList<int>{ 1, 2 });
    f.setValueCount(2);
    check(f.secondaries() == QList<int>{ 1 }, "a value the meter lacks is left out, got " + QString::number(f.smallLineValues()));
    f.setValueCount(5);
    f.setPrimary(4);
    check(f.primaryShown() == 0, "at most four values: a fifth is none");
  }

  // --- an alarm's colour over the variant, and back ---
  {
    LcdWidget f;
    f.setLcdVariant(LcdWidget::Amber);
    const QColor face = f.faceColor(), segment = f.segmentColor();
    f.setAlertColor(QColor("#d82222"));
    check(f.faceColor() == QColor("#d82222"), "alarm: the face takes the alarm's colour");
    check(f.segmentColor().lightness() > 200, "alarm red: light segments read better");
    f.setAlertColor(QColor("#ffe040"));
    check(f.segmentColor().lightness() < 60, "alarm yellow: dark segments");
    check(f.lcdVariant() == LcdWidget::Amber, "the variant stays");
    f.setAlertColor(QColor());
    check(f.faceColor() == face && f.segmentColor() == segment, "no alarm: the variant's colours again");
    f.setAlertColor(QColor("#d82222"));
    f.setLcdVariant(LcdWidget::BacklightBlue);
    check(f.faceColor() == QColor("#d82222"), "a variant chosen during the alarm keeps the alarm's colour");
  }

  // --- the limit test: a column right of the big value, the panel wider ---
  {
    LcdWidget t;
    t.setDisplayMode(6000, true, true, 1);
    t.setValue(0, "5.031");
    t.setUnit(0, "V");
    t.resize(900, 200);
    const LcdWidget::Layout off = t.layout();
    t.setLimitTest(true);
    const LcdWidget::Layout on = t.layout();
    check(off.verdict.isEmpty() && !on.verdict.isEmpty(), "limit test: a column only while on");
    check(on.bezel.width() > off.bezel.width() * 1.2, "limit test: the panel grows wider");
    check(on.main.right() <= on.verdict.left() + 0.5 && on.verdict.right() <= on.face.right(),
          "limit test: the column beside the value, inside the face");
    check(qAbs(on.digitH - off.digitH) < 0.5, "limit test: the digits keep their size where there is room");
    check(t.sizeHint().width() > LcdWidget().sizeHint().width(), "limit test: a wider size hint");
    if (!dump.isEmpty())
    {
      const std::pair<LimitTest::Verdict, const char *> shots[] = {
        { LimitTest::NoVerdict, "none" }, { LimitTest::Pass, "pass" }, { LimitTest::High, "high" }, { LimitTest::Low, "low" } };
      for (const auto &[v, name] : shots)
      {
        t.setVerdict(v, v == LimitTest::Pass ? 0.62 : v == LimitTest::High ? 2.84 : -2.54);
        render(t, QSize(900, 200)).save(QDir(dump).filePath(QString("display_limit_%1.png").arg(name)));
      }
    }
    t.setLimitTest(false);
    check(t.layout().verdict.isEmpty() && t.verdict() == LimitTest::NoVerdict, "limit test off: no column, no verdict");
  }

  if (failed == 0)
    qInfo() << "All digital display tests passed.";
  else
    qWarning() << failed << "digital display test(s) failed.";
  return failed == 0 ? 0 : 1;
}
