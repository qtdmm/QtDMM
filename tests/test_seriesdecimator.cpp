// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SeriesDecimator on its own: the bucket size, the points of one bucket
// (minimum and maximum in time order, gaps, hidden readings, the integral)
// and a series thinned bucket by bucket.
#include <QCoreApplication>
#include <cmath>

#include "recording/seriesdecimator.h"
#include "testutil.h"

static RecordingSeries seriesOf(std::initializer_list<RawPoint> points)
{
  RecordingSeries s;
  for (const RawPoint &p : points)
    s.append(p);
  return s;
}

static RawPoint gapPoint(qint64 t, Quality quality)
{
  return RawPoint{ t, qQNaN(), qQNaN(), quality, 0 };
}

// "0.1:2 0.3:nan": x in seconds, the value
static QString text(const QList<QPointF> &points)
{
  QStringList parts;
  for (const QPointF &p : points)
    parts << QString("%1:%2").arg(p.x()).arg(p.y());
  return parts.join(' ');
}

static void checkPoints(const QList<QPointF> &points, const QString &expected, const QString &what)
{
  check(text(points) == expected, QString("%1: \"%2\", expected \"%3\"").arg(what, text(points), expected));
}

static void testBucketSize()
{
  check(SeriesDecimator::bucketSize(0, 800) == 1, "nothing shown: 1 ms");
  check(SeriesDecimator::bucketSize(800, 800) == 1, "one ms per column");
  check(SeriesDecimator::bucketSize(801, 800) == 2, "a little more than one ms per column: 2 ms");
  check(SeriesDecimator::bucketSize(3600 * 1000, 1000) == 4096, "one hour on 1000 columns: 4096 ms");
  check(SeriesDecimator::bucketSize(1000, 0) == 1024, "no width counts as one column");
}

static void testBucketStart()
{
  const RecordingSeries s = seriesOf({ { 0, 1 }, { 90, 2 }, { 100, 3 }, { 150, 4 }, { 250, 5 } });
  check(SeriesDecimator::bucketStart(s, 0, 100) == 0, "first reading starts its bucket");
  check(SeriesDecimator::bucketStart(s, 1, 100) == 0, "second reading of the first bucket");
  check(SeriesDecimator::bucketStart(s, 3, 100) == 2, "bucket 100..199 starts at index 2");
  check(SeriesDecimator::bucketStart(s, 4, 100) == 4, "a bucket of one");
}

static void testOneBucket()
{
  QList<QPointF> out;
  const RecordingSeries one = seriesOf({ { 100, 2.5 } });
  check(SeriesDecimator::pointsOf(one, 0, 0, out) == 1, "one reading: one point");
  checkPoints(out, "0.1:2.5", "one reading");

  out.clear();
  const RecordingSeries same = seriesOf({ { 100, 2 }, { 200, 2 }, { 300, 2 } });
  check(SeriesDecimator::pointsOf(same, 0, 2, out) == 1, "equal readings: one point");
  checkPoints(out, "0.1:2", "equal readings");

  out.clear();
  const RecordingSeries rising = seriesOf({ { 100, 1 }, { 200, 3 }, { 300, 2 } });
  check(SeriesDecimator::pointsOf(rising, 0, 2, out) == 2, "minimum and maximum: two points");
  checkPoints(out, "0.1:1 0.2:3", "minimum before maximum");

  out.clear();
  const RecordingSeries falling = seriesOf({ { 100, 2 }, { 200, 5 }, { 300, -1 } });
  SeriesDecimator::pointsOf(falling, 0, 2, out);
  checkPoints(out, "0.2:5 0.3:-1", "maximum before minimum, in time order");

  out.clear();
  SeriesDecimator::pointsOf(falling, 1, 1, out);
  checkPoints(out, "0.2:5", "a part of the series");
}

static void testGaps()
{
  QList<QPointF> out;
  const RecordingSeries onlyGap = seriesOf({ gapPoint(100, Quality::Overload), gapPoint(200, Quality::Overload) });
  check(SeriesDecimator::pointsOf(onlyGap, 0, 1, out) == 1, "only overload: one NaN point");
  check(out.size() == 1 && out[0].x() == 0.2 && std::isnan(out[0].y()), "the gap sits at the last reading");

  out.clear();
  const RecordingSeries overloadInside = seriesOf({ { 100, 1 }, gapPoint(200, Quality::Overload), { 300, 4 } });
  SeriesDecimator::pointsOf(overloadInside, 0, 2, out);
  checkPoints(out, "0.1:1 0.3:4", "an overload inside the bucket does not count");

  out.clear();
  const RecordingSeries overloadLast = seriesOf({ { 100, 1 }, { 200, 4 }, gapPoint(300, Quality::Overload) });
  check(SeriesDecimator::pointsOf(overloadLast, 0, 2, out) == 2, "overload at the end: no gap point");

  out.clear();
  const RecordingSeries staleLast = seriesOf({ { 100, 1 }, { 200, 4 }, gapPoint(300, Quality::Stale) });
  check(SeriesDecimator::pointsOf(staleLast, 0, 2, out) == 3, "stale at the end: the bucket ends with the gap");
  check(out.size() == 3 && out[2].x() == 0.3 && std::isnan(out[2].y()), "the gap point after minimum and maximum");

  out.clear();
  const RecordingSeries gapFirst = seriesOf({ gapPoint(100, Quality::Stale), { 200, 4 } });
  SeriesDecimator::pointsOf(gapFirst, 0, 1, out);
  checkPoints(out, "0.2:4", "a gap before the first value of the bucket is passed over");
}

static void testHidden()
{
  const RecordingSeries s = seriesOf({ { 100, 1 }, { 200, 99 }, { 300, 2 } });
  SeriesDecimator::Options options;
  options.hidden = [](int i) { return i == 1; };
  QList<QPointF> out;
  SeriesDecimator::pointsOf(s, 0, 2, out, options);
  checkPoints(out, "0.1:1 0.3:2", "a hidden reading is not the maximum");

  out.clear();
  check(SeriesDecimator::pointsOf(s, 1, 1, out, options) == 0 && out.isEmpty(), "only hidden readings: no point, no gap");

  out.clear();
  const RecordingSeries withGap = seriesOf({ gapPoint(100, Quality::Overload), { 200, 99 } });
  check(SeriesDecimator::pointsOf(withGap, 0, 1, out, options) == 1 && std::isnan(out[0].y()),
        "a gap and hidden readings: the gap");

  out.clear();
  options.integral = true;
  SeriesDecimator::pointsOf(s, 0, 2, out, options);
  check(out.size() == 1, "the integral does not ask for hidden readings");
}

static void testIntegral()
{
  const RecordingSeries s = seriesOf({ { 0, 5, 0 }, { 1000, 5, 5 }, { 2000, 5, 10 } });
  SeriesDecimator::Options options;
  options.integral = true;
  QList<QPointF> out;
  SeriesDecimator::pointsOf(s, 0, 2, out, options);
  checkPoints(out, "0:0 2:10", "the integral's first and last point");

  out.clear();
  options.integralOffset = 1;
  options.integralScale = 0.5;
  SeriesDecimator::pointsOf(s, 0, 2, out, options);
  checkPoints(out, "0:1 2:6", "offset and scale");
}

static void testAppend()
{
  // buckets of 100 ms: 0..99 (3 readings), 100..199 (1), 300..399 (2)
  const RecordingSeries s = seriesOf({ { 0, 1 }, { 40, 3 }, { 80, 2 }, { 100, 7 }, { 310, 4 }, { 390, 4.5 } });
  QList<QPointF> out;
  check(SeriesDecimator::append(s, 0, s.count() - 1, 100, out) == 2, "the last bucket has two points");
  checkPoints(out, "0:1 0.04:3 0.1:7 0.31:4 0.39:4.5", "bucket by bucket");

  out.clear();
  check(SeriesDecimator::append(s, 0, 3, 100, out) == 1, "up to index 3: the last bucket has one point");
  checkPoints(out, "0:1 0.04:3 0.1:7", "a part of the series");

  out.clear();
  check(SeriesDecimator::append(s, 0, s.count() - 1, 1, out) == 1 && out.size() == s.count(),
        "1 ms buckets: every reading is a point");

  out.clear();
  check(SeriesDecimator::append(s, 3, 2, 100, out) == 0 && out.isEmpty(), "an empty span");

  // a bucket's points do not depend on where the window begins
  out.clear();
  const int start = SeriesDecimator::bucketStart(s, 2, 100);
  SeriesDecimator::append(s, start, s.count() - 1, 100, out);
  checkPoints(out, "0:1 0.04:3 0.1:7 0.31:4 0.39:4.5", "from the bucket's start: the same points");
}

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);
  testBucketSize();
  testBucketStart();
  testOneBucket();
  testGaps();
  testHidden();
  testIntegral();
  testAppend();
  return failed ? 1 : 0;
}
