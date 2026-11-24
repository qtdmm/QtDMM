// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// PsuLcdWidget: the grid by channel count, the block proportions, the rows
// by capability, nothing overlapping on a narrow panel, the power from U·I,
// the setpoint shown for a while, a blank cell, per-block alarm colour and
// verdict. TEST_DISPLAY_DUMP=<dir> writes PNGs for visual review.

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QImage>

#include "ui/views/psulcdwidget.h"
#include "testutil.h"

namespace
{
QImage render(PsuLcdWidget &w, const QSize &size)
{
  w.resize(size);
  QImage img(size, QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::transparent);
  w.render(&img, QPoint(), QRegion(), QWidget::DrawChildren);
  return img;
}

void dump(PsuLcdWidget &w, const QSize &size, const QString &name)
{
  const QString dir = qEnvironmentVariable("TEST_DISPLAY_DUMP");
  if (dir.isEmpty())
    return;
  QDir().mkpath(dir);
  render(w, size).save(dir + "/psu_" + name + ".png");
}

QList<PsuLcdWidget::Channel> channels(int n, bool current)
{
  const QColor colors[] = { QColor(0xe0, 0xa0, 0x00), QColor(0x20, 0x90, 0xd0), QColor(0xd0, 0x40, 0x80),
                            QColor(0x40, 0xa0, 0x40) };
  QList<PsuLcdWidget::Channel> list;
  for (int k = 0; k < n; ++k)
  {
    PsuLcdWidget::Channel c;
    c.label = QString("CH%1").arg(k + 1);
    c.color = colors[k];
    c.current = current;
    list << c;
  }
  return list;
}

bool inside(const QRectF &outer, const QRectF &inner)
{
  return inner.isEmpty() || outer.adjusted(-0.5, -0.5, 0.5, 0.5).contains(inner);
}
}

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  // the grid
  check(PsuLcdWidget::grid(1) == QSize(1, 1) && PsuLcdWidget::grid(2) == QSize(2, 1)
          && PsuLcdWidget::grid(3) == QSize(2, 2) && PsuLcdWidget::grid(4) == QSize(2, 2),
        "grid: 1x1, 2x1, 2x2, 2x2");

  // proportions, rows, nothing overlapping: every count, with and without a
  // current, at the size hint and on narrow and wide panels
  for (int n = 1; n <= 4; ++n)
    for (bool current : { false, true })
    {
      PsuLcdWidget w;
      w.setChannels(channels(n, current));
      w.setChannelVerdict(n - 1, true, LimitTest::Pass, 0.4);
      const QString what = QString("%1 channel(s)%2").arg(n).arg(current ? " with I" : "");
      check(w.hasCurrentRows() == current, what + ": rows by capability");
      const QSize hint = w.sizeHint();
      for (const QSize &size : { hint, QSize(hint.width() / 2, hint.height()), QSize(hint.width() * 2, hint.height()),
                                 w.minimumSizeHint() })
      {
        w.resize(size);
        const QList<PsuLcdWidget::Block> blocks = w.layout();
        bool ok = blocks.size() == n;
        for (int k = 0; k < int(blocks.size()) && ok; ++k)
        {
          const PsuLcdWidget::Block &b = blocks[k];
          ok = inside(b.cell, b.flags) && inside(b.cell, b.u) && inside(b.cell, b.i) && inside(b.cell, b.p)
               && inside(b.cell, b.verdict) && inside(b.u, b.uText) && inside(b.i, b.iText) && inside(b.p, b.pText)
               && (b.i.isEmpty() != current) && (b.p.isEmpty() != current) && b.uH > 0
               && (!current || (b.iH > 0 && b.iH < b.uH && b.pH > 0 && b.pH <= b.iH))
               && (b.verdict.isEmpty() || !b.verdict.intersects(b.u));
          for (int j = 0; j < k && ok; ++j)
            ok = !blocks[j].cell.intersects(b.cell);
        }
        check(ok, QString("%1 at %2x%3: rows and numbers inside their block, blocks apart")
                    .arg(what).arg(size.width()).arg(size.height()));
      }
      // a block's proportions in the range, at the size hint
      w.resize(hint);
      const QRectF cell = w.layout().first().cell;
      const double aspect = cell.width() / cell.height();
      const double lo = current ? PsuLcdWidget::kBlockMinAspect : PsuLcdWidget::kBlockMinAspectU;
      const double hi = current ? PsuLcdWidget::kBlockMaxAspect : PsuLcdWidget::kBlockMaxAspectU;
      check(aspect > lo * 0.9 && aspect < hi * 1.1,
            QString("%1: a block %2 wide per height (%3..%4)").arg(what).arg(aspect, 0, 'f', 2).arg(lo).arg(hi));
    }

  // values, power, a blank cell
  {
    PsuLcdWidget w;
    w.setChannels(channels(2, true));
    w.setValues(0, 5.0, 2.4);
    w.setValues(1, 12.0, PsuLcdWidget::kNone, 7.5);
    check(w.text(0, 0) == "5.00" && w.text(0, 1) == "2.400" && w.text(0, 2) == "12.00",
          "U, I, and P from U·I: " + w.text(0, 0) + " " + w.text(0, 1) + " " + w.text(0, 2));
    check(w.text(1, 1).isEmpty() && w.text(1, 2) == "7.50", "no I: a blank cell, the device's P shown: " + w.text(1, 2));
    w.setValues(1, 12.0);
    check(w.text(1, 2).isEmpty(), "neither P nor I: P blank");
    w.setOutput(0, true);
    w.setMode(0, PsuLcdWidget::Mode::CV);
    dump(w, w.sizeHint(), "dual");
  }

  // the setpoint while the knob turns, then the measured value again
  {
    PsuLcdWidget w;
    w.setChannels(channels(1, false));
    w.setSetpointHold(150);
    w.setValues(0, 3.3);
    w.showSetpoint(0, 7.5);
    check(w.showsSetpoint(0) && w.text(0, 0) == "7.50", "turning: the setpoint " + w.text(0, 0));
    dump(w, w.sizeHint(), "setpoint");
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < 400)
      QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    check(!w.showsSetpoint(0) && w.text(0, 0) == "3.30", "after the hold: the measured value " + w.text(0, 0));
    check(!w.showsSetpoint(5) && w.text(5, 0).isEmpty(), "no such channel: nothing");
  }

  // per block: an alarm's colour, the verdict, stale
  {
    PsuLcdWidget w;
    w.setChannels(channels(4, true));
    for (int k = 0; k < 4; ++k)
      w.setValues(k, 1.5 * (k + 1), 0.25 * (k + 1));
    const QColor red(0xd0, 0x20, 0x20);
    w.setChannelAlert(1, red);
    w.setChannelVerdict(2, true, LimitTest::High, 12.5);
    w.setChannelStale(3, true);
    const QSize size = w.sizeHint();
    const QImage img = render(w, size);
    const QList<PsuLcdWidget::Block> blocks = w.layout();
    // a point in the block's lower left, away from digits and label
    auto corner = [&](int k)
  {
    const QRectF c = blocks[k].cell;
    return img.pixelColor(QPointF(c.left() + c.height() * 0.1, c.bottom() - c.height() * 0.025).toPoint());
  };
    check(corner(1) == red && corner(0) != red, "an alarm colours its own block only");
    check(!blocks[2].verdict.isEmpty() && blocks[0].verdict.isEmpty(), "the verdict's column in its own block only");
    int dark = 0;
    for (int y = 0; y < img.height(); ++y)
      for (int x = 0; x < img.width(); ++x)
        dark += qAlpha(img.pixel(x, y)) > 0 && qGray(img.pixel(x, y)) < 60;
    check(dark > 1000, QString("renders: %1 dark pixels").arg(dark));
    dump(w, size, "quad");
    w.setChannels(channels(3, false));
    dump(w, w.sizeHint(), "triple_u");
    for (const LcdPanel::LcdVariant v : { LcdPanel::BacklightBlue, LcdPanel::Amber, LcdPanel::HighContrast })
    {
      w.setLcdVariant(v);
      dump(w, w.sizeHint(), "triple_u_" + LcdPanel::lcdVariantName(v));
    }
  }

  if (failed == 0)
    qInfo() << "All power supply LCD tests passed.";
  return failed == 0 ? 0 : 1;
}
