// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// GenLcdWidget: the frequency's seven digits with its prefix, the rows of
// a block (duty for a pulse, symmetry for a ramp, blank for the others),
// nothing overlapping at several sizes for one and two channels, SET while
// set and ERR, every shape drawn. TEST_DISPLAY_DUMP=<dir> writes PNGs for
// visual review.

#include <QApplication>
#include <QDir>
#include <QImage>

#include "ui/views/genlcdwidget.h"
#include "testutil.h"

namespace
{
using Shape = GenLcdWidget::Shape;

QImage render(GenLcdWidget &w, const QSize &size)
{
  w.resize(size);
  QImage img(size, QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::transparent);
  w.render(&img, QPoint(), QRegion(), QWidget::DrawChildren);
  return img;
}

void dump(GenLcdWidget &w, const QSize &size, const QString &name)
{
  const QString dir = qEnvironmentVariable("TEST_DISPLAY_DUMP");
  if (dir.isEmpty())
    return;
  QDir().mkpath(dir);
  render(w, size).save(dir + "/gen_" + name + ".png");
}

QList<GenLcdWidget::Channel> channels(int n)
{
  QList<GenLcdWidget::Channel> list;
  for (int k = 0; k < n; ++k)
  {
    GenLcdWidget::Channel c;
    c.label = QString("CH%1").arg(k + 1);
    c.color = FunctionGenerator::defaultColor(k);
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

  // the frequency: seven digits, the prefix that keeps it below 1000
  using P = std::pair<QString, QString>;
  check(GenLcdWidget::frequencyText(1000) == P("1.000000", "kHz"), "1 kHz");
  check(GenLcdWidget::frequencyText(25e6) == P("25.00000", "MHz"), "25 MHz");
  check(GenLcdWidget::frequencyText(440) == P("440.0000", "Hz"), "440 Hz");
  check(GenLcdWidget::frequencyText(0.1) == P("0.100000", "Hz"), "0.1 Hz");
  check(GenLcdWidget::frequencyText(999999) == P("999.9990", "kHz"), "just below 1 MHz");
  check(GenLcdWidget::frequencyText(GenLcdWidget::kNone).first.isEmpty(), "none: blank");

  // the rows
  {
    GenLcdWidget w;
    w.setChannels(channels(1));
    w.setValues(0, 1000, 2.0, -0.5, 25, 60);
    check(w.text(0, GenLcdWidget::Frequency) == "1.000000" && w.frequencyUnit(0) == "kHz", "frequency row");
    check(w.text(0, GenLcdWidget::Amplitude) == "2.000" && w.text(0, GenLcdWidget::Offset) == "-0.500",
          "amplitude and offset");
    check(w.text(0, GenLcdWidget::Extra).isEmpty() && w.shapeWord(0) == "SINE", "a sine: no duty, no symmetry");
    w.setShape(0, Shape::Pulse);
    check(w.text(0, GenLcdWidget::Extra) == "25.0" && w.shapeWord(0) == "PULSE", "a pulse: its duty");
    w.setShape(0, Shape::Ramp);
    check(w.text(0, GenLcdWidget::Extra) == "60.0", "a ramp: its symmetry");
    w.setShape(0, Shape::Arbitrary, "HaverSine");
    check(w.shapeWord(0) == "HAVERSIN", "another shape: its name, eight letters");
  }

  // nothing overlapping, one and two channels, at several sizes
  for (int n = 1; n <= 2; ++n)
  {
    GenLcdWidget w;
    w.setChannels(channels(n));
    const QSize hint = w.sizeHint();
    for (const QSize &size : { hint, QSize(hint.width() / 2, hint.height()), QSize(hint.width() * 2, hint.height()),
                               w.minimumSizeHint() })
    {
      w.resize(size);
      const QList<GenLcdWidget::Block> blocks = w.layout();
      bool ok = blocks.size() == n;
      for (int k = 0; k < int(blocks.size()) && ok; ++k)
      {
        const GenLcdWidget::Block &b = blocks[k];
        ok = inside(b.cell, b.flags) && inside(b.cell, b.freq) && inside(b.cell, b.extra) && inside(b.freq, b.freqText)
             && inside(b.amp, b.ampText) && inside(b.offset, b.offsetText) && inside(b.extra, b.extraText)
             && !b.ampText.intersects(b.offsetText) && b.freqH > b.smallH && b.smallH > 0 && b.extraH > 0;
        for (int j = 0; j < k && ok; ++j)
          ok = !blocks[j].cell.intersects(b.cell);
      }
      check(ok, QString("%1 channel(s) at %2x%3: rows inside their block, apart").arg(n).arg(size.width()).arg(size.height()));
    }
    w.resize(hint);
    const QRectF cell = w.layout().first().cell;
    const double aspect = cell.width() / cell.height();
    check(aspect > GenLcdWidget::kBlockMinAspect * 0.9 && aspect < GenLcdWidget::kBlockMaxAspect * 1.1,
          QString("%1 channel(s): a block %2 wide per height").arg(n).arg(aspect, 0, 'f', 2));
  }

  // SET while set, then off; ERR
  {
    GenLcdWidget w;
    w.setChannels(channels(1));
    w.setHold(150);
    w.markSet(0);
    check(w.setShown(0), "SET after a turn");
    check(waitFor([&] { return !w.setShown(0); }, 1000), "SET gone after the hold");
    w.setChannelError(0, "No answer");
    check(!render(w, w.sizeHint()).isNull(), "ERR drawn");
  }

  // every shape drawn, each on its own image that differs from the others
  {
    GenLcdWidget w;
    w.setChannels(channels(1));
    w.setValues(0, 10e3, 1.0, 0.0, 30, 50);
    w.setOutput(0, true);
    QList<QImage> images;
    for (Shape s : { Shape::Sine, Shape::Square, Shape::Ramp, Shape::Pulse, Shape::Arbitrary })
    {
      w.setShape(0, s, "Sinc");
      images << render(w, w.sizeHint());
      dump(w, w.sizeHint(), FunctionGenerator::shapeName(s));
    }
    bool apart = true;
    for (int i = 0; i < int(images.size()); ++i)
      for (int j = 0; j < i; ++j)
        apart = apart && images[i] != images[j];
    check(apart, "each shape looks different");
    GenLcdWidget two;
    two.setChannels(channels(2));
    two.setValues(0, 1000, 2, 0);
    two.setValues(1, 2.5e6, 0.5, 1.25);
    two.setShape(1, Shape::Square);
    two.setOutput(1, true);
    dump(two, two.sizeHint(), "dual");
  }

  if (failed == 0)
    qInfo() << "All generator LCD tests passed.";
  return failed == 0 ? 0 : 1;
}
