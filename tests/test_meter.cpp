// Tests for the analog meter widget: value-to-angle mapping, full-scale
// derivation, needle ballistics and a headless render smoke test.
//
// Set TEST_METER_DUMP=<dir> to also write the rendered variants as PNG for
// visual review.
#include <QApplication>
#include <QImage>
#include <QDir>
#include <QDebug>
#include <QSignalSpy>
#include <QTest>
#include <cmath>

#include "ui/views/analogmeter.h"
#include "core/readingadapter.h"
#include <limits>
#include "testutil.h"

static bool near(double a, double b, double eps = 1e-6)
{
  return std::fabs(a - b) <= eps;
}

static QImage render(AnalogMeter &w, const QSize &size)
{
  w.resize(size);
  QImage img(size, QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::transparent);
  w.render(&img, QPoint(), QRegion(), QWidget::DrawChildren); // no window background
  return img;
}

// Brightness at a point on the needle's sweep for a given angle.
static int lumaAt(const QImage &img, const QPointF &pivot, double radius, double angleDeg)
{
  const double a = angleDeg * M_PI / 180.0;
  const QPoint pt(int(pivot.x() + radius * std::sin(a)), int(pivot.y() - radius * std::cos(a)));
  int sum = 0, n = 0;
  for (int dy = -1; dy <= 1; ++dy)
    for (int dx = -1; dx <= 1; ++dx)
    {
      const QPoint q = pt + QPoint(dx, dy);
      if (img.rect().contains(q))
      {
        sum += qGray(img.pixel(q));
        n++;
      }
    }
  return n ? sum / n : 0;
}

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  // --- 1. angle mapping ---
  check(near(AnalogMeter::angleForValue(0.0, 4.0, false), -45.0), "unipolar: 0 -> -45 deg");
  check(near(AnalogMeter::angleForValue(4.0, 4.0, false), 45.0), "unipolar: FS -> +45 deg");
  check(near(AnalogMeter::angleForValue(2.0, 4.0, false), 0.0), "unipolar: FS/2 -> 0 deg");
  check(near(AnalogMeter::angleForValue(-0.2, 4.0, false), -49.5), "unipolar: -5% FS -> left stop");
  check(near(AnalogMeter::angleForValue(-1.0, 4.0, false), -49.5), "unipolar: far negative clamps at the stop");
  check(near(AnalogMeter::angleForValue(9.0, 4.0, false), 49.5), "unipolar: over range clamps at the right stop");
  check(near(AnalogMeter::angleForValue(0.0, 4.0, true), 0.0), "bipolar: 0 -> 0 deg");
  check(near(AnalogMeter::angleForValue(-4.0, 4.0, true), -45.0), "bipolar: -FS -> -45 deg");
  check(near(AnalogMeter::angleForValue(4.0, 4.0, true), 45.0), "bipolar: +FS -> +45 deg");
  check(near(AnalogMeter::angleForValue(1.0, 0.0, false), -45.0), "no full scale: rests at the left");
  {
    double last = -100;
    bool monotonic = true;
    for (double v = -0.5; v <= 4.5; v += 0.01)
    {
      const double a = AnalogMeter::angleForValue(v, 4.0, false);
      monotonic = monotonic && a >= last;
      last = a;
    }
    check(monotonic, "angle is monotonic in the value");
  }

  // --- 2. full scale from the reading string ---
  check(near(AnalogMeter::fullScaleFromReading("3.856", 4000), 4.0), "\"3.856\" @4000 -> 4");
  check(near(AnalogMeter::fullScaleFromReading(" 385.6", 4000), 400.0), "\"385.6\" @4000 -> 400");
  check(near(AnalogMeter::fullScaleFromReading("-1234", 2000), 2000.0), "\"-1234\" @2000 -> 2000");
  check(near(AnalogMeter::fullScaleFromReading("71.5", 6000, "%"), 100.0), "a percentage is a 0..100 scale (SOC \"71.5\" @6000)");
  check(near(AnalogMeter::fullScaleFromReading("50.0", 4000, "%"), 100.0), "duty cycle too");
  check(near(AnalogMeter::fullScaleFromReading("36.5", 10000, "dBA"), AnalogMeter::kSoundFullScale), "a sound level has no display count");
  check(near(AnalogMeter::fullScaleFromReading("36.5", 10000, "dBC"), AnalogMeter::kSoundFullScale), "C weighting too");
  check(near(AnalogMeter::fullScaleFromReading("3.856", 4000, "V"), 4.0), "other units keep the count rule");
  check(near(AnalogMeter::fullScaleFromReading("0.000", 6000), 6.0), "\"0.000\" @6000 -> 6");
  check(near(AnalogMeter::fullScaleFromReading("19.99", 2000), 20.0), "\"19.99\" @2000 -> 20");
  check(std::isnan(AnalogMeter::fullScaleFromReading("0.L", 4000)), "\"0.L\" is not a number");
  check(std::isnan(AnalogMeter::fullScaleFromReading("OL", 4000)), "\"OL\" is not a number");
  check(std::isnan(AnalogMeter::fullScaleFromReading("", 4000)), "empty string is not a number");

  // --- a temperature has no range: the scale follows the values (1-2-5) ---
  const double none = std::numeric_limits<double>::quiet_NaN();
  check(near(AnalogMeter::fullScaleWithoutRange(37.2, none), 50.0), "37.2 -> 50");
  check(near(AnalogMeter::fullScaleWithoutRange(46.0, none), 100.0), "46 would be in the red zone of 50 -> 100");
  check(near(AnalogMeter::fullScaleWithoutRange(5.0, none), 10.0), "small values: at least 10");
  check(near(AnalogMeter::fullScaleWithoutRange(-76.0, none), 100.0), "negative: by magnitude");
  check(near(AnalogMeter::fullScaleWithoutRange(950.0, none), 2000.0), "950 -> 2000");
  check(near(AnalogMeter::fullScaleWithoutRange(20.0, 100.0), 100.0), "the scale does not shrink");
  {
    // a 50000-count meter showing 37.2 °C: 50, not 5000; a voltage keeps the count rule
    ReadingAdapter adapter;
    auto reading = [&](const QString &text, const QString &unit, const QString &special)
    {
      return ReadingAdapter::reading(adapter.adaptValue(text.toDouble(), text, unit, special, "AUTO", false, true, false, 0, 0));
    };
    AnalogMeter m;
    m.setDisplayCounts(50000);
    Reading r = reading("37.2", "C", "TE");
    check(r.temperature(), "C with TE is a temperature");
    m.showReading(r);
    check(near(m.fullScale(), 50.0), QString("37.2 C @50000 -> 50, got %1").arg(m.fullScale()));
    r.text = "22.6";
    m.showReading(r);
    check(near(m.fullScale(), 50.0), "cooler again: stays at 50");
    r.text = "81.0";
    m.showReading(r);
    check(near(m.fullScale(), 100.0), "81 -> grows to 100");
    m.reset();
    r.text = "22.6";
    m.showReading(r);
    check(near(m.fullScale(), 50.0), "reset: starts again from the value");
    m.showReading(reading("3.8560", "V", "DC"));
    check(near(m.fullScale(), 5.0), QString("a voltage keeps the count rule (5.0000 @50000), got %1").arg(m.fullScale()));
    // ppm and % are never negative: zero at the left also in Centre zero
    m.setScaleMode(AnalogMeter::Bipolar);
    m.showReading(reading("734", "ppm", ""));
    check(!m.bipolar(), "centre zero: ppm keeps zero at the left");
    m.showReading(reading("3.8560", "V", "DC"));
    check(m.bipolar(), "centre zero: a voltage is bipolar");
    check(AnalogMeter::neverNegative("ppm") && AnalogMeter::neverNegative("%") && !AnalogMeter::neverNegative("mV"),
          "neverNegative: ppm, %, not mV");
    check(AnalogMeter::neverNegative("lx") && AnalogMeter::neverNegative("klx") && AnalogMeter::neverNegative("fc")
              && AnalogMeter::neverNegative("m/s") && AnalogMeter::neverNegative("%RH"),
          "neverNegative: lx, klx, fc (not femto-c), m/s, %RH");
  }
  check(std::isnan(AnalogMeter::fullScaleFromReading("3.856", 0)), "no counts, no scale");

  // --- 2a. the value shown: another id is ignored until chosen; the
  //         min/max marks only while the memory follows this value ---
  {
    ReadingAdapter adapter;
    auto reading = [&](int id, const QString &text, const QString &unit, const QString &special)
    {
      return ReadingAdapter::reading(adapter.adaptValue(text.toDouble(), text, unit, special, "AUTO", false, true, false, id, 0));
    };
    AnalogMeter m;
    m.setDisplayCounts(4000);
    m.showReading(reading(0, "734", "ppm", ""));
    m.showReading(reading(1, "22.5", "C", "TE"));
    check(near(m.fullScale(), 4000.0), QString("value 1: the ppm scale, got %1").arg(m.fullScale()));
    m.setValueId(1);
    m.showReading(reading(0, "734", "ppm", ""));
    m.showReading(reading(1, "22.5", "C", "TE"));
    check(m.valueId() == 1 && near(m.fullScale(), 50.0), QString("value 2: the temperature's scale, got %1").arg(m.fullScale()));
    m.showValueMinMax(0, 700.0, 1000.0);
    check(std::isnan(m.maxMark()), "value 2: the memory of value 1 is no mark here");
    m.showValueMinMax(1, 20.0, 30.0);
    check(near(m.maxMark(), 30.0) && near(m.minMark(), 20.0),
          QString("value 2 with its memory: the marks, got %1..%2").arg(m.minMark()).arg(m.maxMark()));
    m.showValueMinMax(1, std::nan(""), std::nan(""));
    check(std::isnan(m.maxMark()), "its memory cleared: no marks");
  }

  // --- 2b. scale step: 1-2-5, labels do not touch, 0 is a major tick ---
  {
    const double radius = 220.0;   // px from the pivot to the labels
    for (bool bipolar : { false, true })
      for (double fs : { 0.22, 2.2, 4.0, 6.0, 22.0, 40.0, 50.0, 60.0, 220.0, 400.0, 500.0, 600.0, 1000.0 })
      {
        // a label is about as wide as "-1000" in a 20 px bold font
        const double labelWidth = 11.0 * QString::number(bipolar ? -fs : fs).size() + 10.0;
        const double step = AnalogMeter::scaleStep(fs, bipolar, radius, labelWidth);
        const double mag = std::pow(10.0, std::floor(std::log10(step) + 1e-9));
        const long mant = std::lround(step / mag);
        const QString what = QString("FS %1 %2: step %3").arg(fs).arg(bipolar ? "bipolar" : "unipolar").arg(step);
        check(near(step, mant * mag, 1e-9 * mag) && (mant == 1 || mant == 2 || mant == 5), what + " is 1-2-5");
        const double pxPerUnit = radius * (M_PI / 2.0) / (bipolar ? 2.0 * fs : fs);
        check(step * pxPerUnit >= labelWidth - 1e-9, what + " leaves the labels apart");
        check(step <= fs + 1e-9, what + " labels something besides 0");
      }
    // the review cases (R4-03): 22 V bipolar is labelled -20 ... 20 in steps
    // of 5 or 10 (0 included), 500 bipolar no longer crowds eleven labels
    const double s22 = AnalogMeter::scaleStep(22.0, true, radius, 40.0);
    check(near(std::fmod(20.0, s22), 0.0), QString("22 V bipolar: step %1 divides 20").arg(s22));
    check(AnalogMeter::scaleStep(500.0, true, radius, 55.0) >= 200.0 - 1e-9, "500 bipolar: step 200 or more");
  }

  // --- 2c. MIN/MAX readouts ---
  check(AnalogMeter::readoutString(-0.004, 2) == "0.00", "a rounded zero has no minus sign (R4-04)");
  check(AnalogMeter::readoutString(-0.004, 3) == "-0.004", "a small negative value keeps its sign");
  check(AnalogMeter::readoutString(3.856, 3) == "3.856", "three decimals");
  check(AnalogMeter::readoutString(-1.5, 1) == "-1.5", "negative value");
  check(AnalogMeter::readoutString(std::nan(""), 2) == QStringLiteral("—"), "no value: a dash");
  // as many digits as the display has; another range in its own prefix
  check(AnalogMeter::readoutFor(10000.0, 4, 400.0, 40000, "mV") == "10.000 V",
        "MAX from the 40 V range in the mV range: " + AnalogMeter::readoutFor(10000.0, 4, 400.0, 40000, "mV"));
  check(AnalogMeter::readoutFor(3.001, 4, 400.0, 40000, "mV") == "3.0010 mV", "MIN in the range: as the meter shows it");
  check(AnalogMeter::readoutFor(312.5, 4, 400.0, 40000, "mV") == "312.50 mV", "at most five digits on a 40000 count display");
  check(AnalogMeter::readoutFor(3.5, 2, 10.0, 0, "") == "3.50", "no unit, no space");
  check(AnalogMeter::readoutFor(1200.0, 1, 1000.0, 10000, "ppm") == "1200.0 ppm", "ppm keeps its unit");


  // --- 3. ballistics: converges, bounded overshoot, timer stops ---
  {
    AnalogMeter w;
    w.setFullScale(4.0);
    w.setScaleMode(AnalogMeter::Unipolar);
    w.setReading(0.0, "0.000", "V DC", false, false);
    QTest::qWait(1500);
    check(w.isSettled(), "needle settles at rest after 1.5 s");
    check(near(w.needleAngle(), -45.0, 0.1), "resting needle sits at -45 deg");

    w.setReading(2.0, "2.000", "V DC", false, false);
    check(!w.isSettled(), "a step starts the ballistics timer");
    double maxAngle = -100;
    for (int i = 0; i < 100 && !w.isSettled(); ++i)
    {
      QTest::qWait(16);
      maxAngle = qMax(maxAngle, w.needleAngle());
    }
    check(w.isSettled(), "needle settles within 1.6 s after a step");
    check(near(w.needleAngle(), 0.0, 0.1), "needle ends at the target");
    check(maxAngle < 0.0 + 4.5, "overshoot stays below 5% of full scale (4.5 deg)");
    check(maxAngle > 0.0, "slightly under-damped: some overshoot is expected");

    // without ballistics the needle jumps
    AnalogMeterStyle s = AnalogMeterStyle::dark();
    s.ballistics = false;
    w.setStyle(s);
    w.setReading(4.0, "4.000", "V DC", false, false);
    check(w.isSettled() && near(w.needleAngle(), 45.0), "ballistics off: needle jumps to the target");
  }

  // --- 4. auto scale mode latches to bipolar on a negative reading ---
  {
    AnalogMeter w;
    AnalogMeterStyle s = AnalogMeterStyle::dark();
    s.ballistics = false;
    w.setStyle(s);
    w.setFullScale(4.0);
    w.setScaleMode(AnalogMeter::Auto);
    w.setReading(1.0, "1.000", "V DC", false, false);
    check(!w.bipolar(), "auto: positive readings keep the unipolar scale");
    w.setReading(-0.1, "-0.100", "V DC", false, false);
    check(!w.bipolar(), "auto: a small negative value stays in the underswing zone");
    w.setReading(-1.0, "-1.000", "V DC", false, false);
    check(w.bipolar(), "auto: a clearly negative value switches to bipolar");
    w.setReading(1.0, "1.000", "V DC", false, false);
    check(w.bipolar(), "auto: bipolar is latched");
    w.reset();
    check(!w.bipolar(), "reset() releases the latch");
    w.setScaleMode(AnalogMeter::Bipolar);
    w.setReading(-1.0, "-1.000", "V DC", false, false);
    check(w.bipolar(), "explicit bipolar");
    w.setScaleMode(AnalogMeter::Unipolar);
    w.setReading(-1.0, "-1.000", "V DC", false, false);
    check(!w.bipolar(), "explicit unipolar ignores negative readings");
  }

  // --- 5. render smoke test ---
  {
    const QString dump = qEnvironmentVariable("TEST_METER_DUMP");
    const QSize size(480, 270);
    AnalogMeter w;
    AnalogMeterStyle s = AnalogMeterStyle::dark();
    s.ballistics = false;
    w.setStyle(s);
    w.setFullScale(4.0);
    w.setScaleMode(AnalogMeter::Unipolar);
    w.setReading(2.0, "2.000", "V DC", false, false);
    w.setPeak(3.5);
    QImage half = render(w, size);
    check(!half.isNull() && half.size() == size, "renders to an image");
    if (!dump.isEmpty()) half.save(QDir(dump).filePath("meter_dark_half.png"));
    w.setStale(true);
    const QImage stale = render(w, size);
    if (!dump.isEmpty()) stale.save(QDir(dump).filePath("meter_dark_stale.png"));
    check(stale != half, "stale: needle and readouts fade");
    w.setStale(false);
    check(render(w, size) == half, "current again: as before");

    // rough geometry, mirrors AnalogMeter::geometry()
    const double bezelW = qBound(4.0, (size.height() - 2) * 0.12, 12.0);   // PanelFrame::bezelWidth()
    const double fh = size.height() - 2 - 2 * bezelW;
    const double fw = size.width() - 2 - 2 * bezelW;
    const QPointF pivot(size.width() / 2.0, 1 + bezelW + fh + fh * 0.02);
    const double radius = qMin(fh * 0.85, fw * 0.58);

    // the needle at 0 deg lights up the point straight above the pivot;
    // at -45 deg (value 0) it doesn't
    const int onNeedle = lumaAt(half, pivot, radius * 0.6, 0.0);
    w.setReading(0.0, "0.000", "V DC", false, false);
    QImage zero = render(w, size);
    const int offNeedle = lumaAt(zero, pivot, radius * 0.6, 0.0);
    check(onNeedle > offNeedle + 60, QString("needle is visible where it points (%1 vs %2)").arg(onNeedle).arg(offNeedle));
    if (!dump.isEmpty()) zero.save(QDir(dump).filePath("meter_dark_zero.png"));

    // corners outside the rounded bezel stay transparent
    check(qAlpha(half.pixel(0, 0)) == 0 && qAlpha(half.pixel(size.width() - 1, size.height() - 1)) == 0,
          "nothing is painted outside the bezel");

    // red zone has red pixels on the arc near the right end (the band is
    // paler than the red labels, so reddish rather than pure red)
    {
      bool red = false;
      for (double a = 40.0; a <= 45.0 && !red; a += 0.5)
      {
        const double ar = a * M_PI / 180.0;
        const QPoint pt(int(pivot.x() + radius * 0.95 * std::sin(ar)), int(pivot.y() - radius * 0.95 * std::cos(ar)));
        const QRgb px = half.pixel(pt);
        red = qRed(px) > 120 && qRed(px) - qGreen(px) > 70 && qRed(px) - qBlue(px) > 70;
      }
      check(red, "red zone is painted at the top end of the scale");
      // the arc runs through the red zone (drawn over the band)
      bool line = false;
      for (double a = 42.0; a <= 44.0 && !line; a += 0.5)
        for (double f = 0.99; f <= 1.01 && !line; f += 0.002)
        {
          const double ar = a * M_PI / 180.0;
          const QRgb px = half.pixel(int(pivot.x() + radius * f * std::sin(ar)), int(pivot.y() - radius * f * std::cos(ar)));
          line = qRed(px) > 200 && qGreen(px) > 200 && qBlue(px) > 200;
        }
      check(line, "the scale arc is visible in the red zone");
    }

    // min/max marks: a green triangle just outside the arc at the max value,
    // a red one at the min; both gone after reset()
    {
      auto markColor = [&](const QImage &img, double value, bool wantGreen)
      {
        const double a = AnalogMeter::angleForValue(value, 4.0, false) * M_PI / 180.0;
        for (double f = 0.90; f <= 0.98; f += 0.01)
        {
          const QPoint pt(int(pivot.x() + radius * f * std::sin(a)), int(pivot.y() - radius * f * std::cos(a)));
          if (!img.rect().contains(pt)) continue;
          const QRgb px = img.pixel(pt);
          if (wantGreen && qGreen(px) > 150 && qRed(px) < 120) return true;
          if (!wantGreen && qRed(px) > 150 && qGreen(px) < 110) return true;
        }
        return false;
      };
      w.setReading(2.0, "2.000", "V DC", false, false);
      w.setMinMax(1.0, 3.0);
      QImage marks = render(w, size);
      check(markColor(marks, 3.0, true), "green max mark at 3.0");
      check(markColor(marks, 1.0, false), "red min mark at 1.0");
      if (!dump.isEmpty()) marks.save(QDir(dump).filePath("meter_dark_marks.png"));
      w.reset();
      QImage cleared = render(w, size);
      check(!markColor(cleared, 3.0, true) && !markColor(cleared, 1.0, false), "reset() removes the marks");
      w.setMinMax(1.0, 3.0);
    }

    // other variants for the dump
    if (!dump.isEmpty())
    {
      w.setReading(5.0, "0.L", "V DC", true, false);
      render(w, size).save(QDir(dump).filePath("meter_dark_ol.png"));
      w.setReading(1.234, "1.234", "V DC", false, true);
      render(w, size).save(QDir(dump).filePath("meter_dark_hold.png"));
      w.setScaleMode(AnalogMeter::Bipolar);
      w.setReading(-1.5, "-1.500", "V DC", false, false);
      render(w, size).save(QDir(dump).filePath("meter_dark_bipolar.png"));
      w.setScaleMode(AnalogMeter::Unipolar);
      w.setFullScale(400.0);
      w.setReading(385.6, "385.6", "mV AC", false, false);
      w.setPeak(391.2);
      render(w, size).save(QDir(dump).filePath("meter_dark_400mv.png"));
      AnalogMeterStyle iv = AnalogMeterStyle::ivory();
      iv.ballistics = false;
      w.setStyle(iv);
      w.setFullScale(4.0);
      w.setReading(2.0, "2.000", "V DC", false, false);
      w.setPeak(3.5);
      render(w, size).save(QDir(dump).filePath("meter_ivory_half.png"));
      render(w, QSize(960, 540)).save(QDir(dump).filePath("meter_ivory_large.png"));
      render(w, QSize(240, 135)).save(QDir(dump).filePath("meter_ivory_small.png"));
      // issue #143: BM869s, 500 mV range, low reading, long unit labels
      AnalogMeterStyle dk = AnalogMeterStyle::dark();
      dk.ballistics = false;
      w.setStyle(dk);
      w.reset();
      w.setFullScale(500.0);
      w.setReading(14.87, "014.87", "mV AC", false, false);
      w.setMinMax(12.04, 15.84);
      w.setPeak(15.84);
      render(w, QSize(740, 560)).save(QDir(dump).filePath("meter_dark_issue143.png"));
      render(w, QSize(960, 436)).save(QDir(dump).filePath("meter_dark_wide.png"));
      w.setReading(0.0, "000.00", "mV AC+DC", false, false);
      render(w, QSize(740, 560)).save(QDir(dump).filePath("meter_dark_acdc_zero.png"));
      w.setStyle(iv);
      w.setFullScale(4.0);
      w.setReading(0.512, "0.512", "V DIODE", false, false);
      w.setMinMax(0.498, 0.531);
      w.setPeak(0.531);
      render(w, QSize(480, 360)).save(QDir(dump).filePath("meter_ivory_diode.png"));
      render(w, QSize(240, 180)).save(QDir(dump).filePath("meter_ivory_diode_small.png"));
    }
  }

  // --- an alarm's colour mixed into the dial, and back ---
  {
    AnalogMeter w;
    w.setStyle(AnalogMeterStyle::dark());
    const QColor dial = w.dialColor();
    w.setAlertColor(QColor("#d82222"));
    const QColor mixed = w.dialColor();
    check(mixed.red() > dial.red() + 40 && mixed.red() < 0xd8, QString("alarm: dial reddened, not red (%1)").arg(mixed.name()));
    w.setAlertColor(QColor());
    check(w.dialColor() == dial, "no alarm: the style's dial again");
  }

  // --- the limit test's band: green on the arc for the value shown ---
  {
    AnalogMeter w;
    w.setStyle(AnalogMeterStyle::dark());
    w.setFullScale(10.0);
    auto pixels = [&](bool red)
    {
      const QImage img = render(w, QSize(400, 300));
      int n = 0;
      for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
        {
          const QColor c = img.pixelColor(x, y);
          const int main = red ? c.red() : c.green(), other = red ? c.green() : c.red();
          n += main > other + 40 && main > c.blue() + 40;
        }
      return n;
    };
    auto green = [&] { return pixels(false); };
    const int none = green();
    const int redZone = pixels(true);
    w.setLimits(0, 4.0, 6.0);
    const int band = green();
    check(w.limitsShown() && band > none + 50, QString("limits: a green band on the arc (%1 -> %2 pixels)").arg(none).arg(band));
    // (the OL lamp and the minimum mark stay red)
    check(pixels(true) < redZone / 2, QString("limits: no red zone (%1 -> %2 red pixels)").arg(redZone).arg(pixels(true)));
    w.setLimits(0, 4.0, 60.0);
    check(green() > band, "limits beyond the scale: the band reaches its end");
    w.setValueId(1);
    check(!w.limitsShown() && green() == none, "another value shown: no band");
    w.setValueId(0);
    w.setLimits(-1);
    check(!w.limitsShown() && green() == none, "limit test off: no band");
  }

  if (failed == 0)
    qInfo() << "All analog meter tests passed.";
  else
    qWarning() << failed << "analog meter test(s) failed.";
  return failed == 0 ? 0 : 1;
}
