// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// ControlBar, the meter's keys under the display: which keys a meter has,
// what a click sends, holding MIN/MAX leaves it, and the states the
// readings report.
#include <QApplication>
#include <QDebug>
#include <QFrame>
#include <QSignalSpy>
#include <QTest>
#include <QToolButton>

#include "core/reading.h"
#include "ui/controlbar.h"
#include "testutil.h"

static QToolButton *key(ControlBar &bar, const QString &name)
{
  for (QToolButton *b : bar.findChildren<QToolButton *>())
    if (b->property("key").toString() == name)
      return b;
  return nullptr;
}

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  ControlBar bar;
  bar.show();
  bar.setConnected(true);

  // --- 1. the keys the meter has: no Hz/%, MIN/MAX and PEAK on the UT60BT,
  //        on the UT61x+ ---
  bar.setProtocol(FrameFormat::UniTiDMM, "UT60BT");
  check(key(bar, "hold") && !key(bar, "hold")->isHidden(), "UT60BT: HOLD shown");
  check(key(bar, "lamp") && !key(bar, "lamp")->isHidden(), "UT60BT: LIGHT shown");
  check(key(bar, "select1") && !key(bar, "select1")->isHidden(), "UT60BT: SELECT shown");
  check(key(bar, "peak")->isHidden() && key(bar, "minmax")->isHidden() && key(bar, "select2")->isHidden(),
        "UT60BT: no PEAK, MIN/MAX, Hz/%");
  check(key(bar, "fn_v")->parentWidget()->isHidden(), "UT60BT: no function keys, their group hidden");
  bar.setProtocol(FrameFormat::UniTUT202BT, "UT202BT");
  check(!key(bar, "fn_v")->isHidden() && !key(bar, "fn_a")->isHidden() && !key(bar, "fn_ohm")->isHidden()
          && !key(bar, "fn_ncv")->isHidden() && !key(bar, "range")->isHidden() && !key(bar, "hold")->isHidden(),
        "UT202BT: V~, A~, Ω, NCV/PEAK, RANGE, HOLD shown");
  check(key(bar, "auto")->isHidden() && key(bar, "lamp")->isHidden() && key(bar, "peak")->isHidden()
          && key(bar, "select1")->parentWidget()->isHidden() && key(bar, "rel")->parentWidget()->isHidden()
          && !key(bar, "hold")->parentWidget()->isHidden(),
        "UT202BT: no AUTO, LIGHT, PEAK; SELECT and REL groups hidden, HOLD's shown");
  {
    // A~ held: inrush; a short click: A~
    QSignalSpy keys(&bar, &ControlBar::keyPressed);
    QTest::mouseClick(key(bar, "fn_a"), Qt::LeftButton);
    QTest::mousePress(key(bar, "fn_a"), Qt::LeftButton);
    QTest::qWait(ControlBar::kLongPressMs + 100);
    QTest::mouseRelease(key(bar, "fn_a"), Qt::LeftButton);
    check(keys.size() == 2 && keys[0][0].toString() == "fn_a" && keys[1][0].toString() == "fn_a_long",
          "UT202BT A~: click fn_a, held fn_a_long");
    check(!key(bar, "fn_a")->isCheckable() && key(bar, "fn_v")->styleSheet().contains("#e03c31"),
          "UT202BT: function keys are plain keys, V~ red");
  }
  bar.setProtocol(FrameFormat::UniTiENV, "UT363BT");
  check(!key(bar, "hold")->isHidden() && !key(bar, "max")->isHidden() && !key(bar, "avg")->isHidden()
          && !key(bar, "unit")->isHidden() && key(bar, "unit2")->isHidden(),
        "UT363BT: HOLD, MAX, AVG, UNIT shown, no °C/°F");
  check(key(bar, "min")->isHidden() && key(bar, "weighting")->isHidden() && key(bar, "minmax")->parentWidget()->isHidden()
          && key(bar, "select1")->parentWidget()->isHidden(),
        "UT363BT: no MIN, FAST/SLOW; the MIN/MAX and SELECT groups hidden");
  bar.setProtocol(FrameFormat::UniTiENV, "UT353BT *");
  check(!key(bar, "min")->isHidden() && !key(bar, "weighting")->isHidden() && key(bar, "unit2")->isHidden()
          && key(bar, "avg")->isHidden(),
        "UT353BT: MIN and FAST/SLOW, no °C/°F, no AVG");
  {
    Reading avg;
    avg.flags = SampleFlag::Avg;
    bar.showReading(avg);
    check(key(bar, "avg")->isChecked() && !key(bar, "max")->isChecked() && !key(bar, "min")->isChecked(),
          "iENV states: AVG");
    bar.showReading(Reading());
  }
  bar.setProtocol(FrameFormat::UniTUT61Plus, "UT61E+");
  check(!key(bar, "peak")->isHidden() && !key(bar, "minmax")->isHidden() && !key(bar, "select2")->isHidden(),
        "UT61x+: PEAK, MIN/MAX, Hz/% shown");
  check(key(bar, "max")->parentWidget()->isHidden() && key(bar, "unit")->parentWidget()->isHidden(),
        "UT61x+: no iENV keys");

  // SELECT and HOLD in the colours of the meter's keys
  check(key(bar, "select1")->styleSheet().contains("#ffc61c") && key(bar, "hold")->styleSheet().contains("#3c9aff")
          && key(bar, "range")->styleSheet().isEmpty(),
        "colours: SELECT yellow, HOLD blue, the others plain");

  // --- 2. a click sends the key; MIN/MAX held while on leaves it ---
  QSignalSpy sent(&bar, &ControlBar::keyPressed);
  QTest::mouseClick(key(bar, "hold"), Qt::LeftButton);
  QTest::mouseClick(key(bar, "minmax"), Qt::LeftButton);
  check(sent.size() == 2 && sent[0][0].toString() == "hold" && sent[1][0].toString() == "minmax",
        "click: hold, minmax");
  QToolButton *minmax = key(bar, "minmax");
  check(minmax->isChecked(), "minmax: shown on after the click");
  QTest::mousePress(minmax, Qt::LeftButton);
  QTest::qWait(ControlBar::kLongPressMs + 100);
  QTest::mouseRelease(minmax, Qt::LeftButton);
  check(sent.size() == 3 && sent[2][0].toString() == "minmax_off", "minmax held: minmax_off, got " + sent.last()[0].toString());
  check(!minmax->isChecked(), "minmax held: off");
  // held while off: just MIN/MAX
  QTest::mousePress(minmax, Qt::LeftButton);
  QTest::qWait(ControlBar::kLongPressMs + 100);
  QTest::mouseRelease(minmax, Qt::LeftButton);
  check(sent.size() == 4 && sent[3][0].toString() == "minmax", "minmax held while off: minmax");

  // --- 3. the states come from the readings, not from the clicks ---
  Reading r;
  r.hold = true;
  r.range = "MANU";
  r.flags = SampleFlag::Relative | SampleFlag::Min;
  bar.showReading(r);
  check(key(bar, "hold")->isChecked() && !key(bar, "auto")->isChecked() && key(bar, "rel")->isChecked()
          && key(bar, "minmax")->isChecked() && !key(bar, "peak")->isChecked(),
        "states: HOLD, MANU, REL, MIN");
  r = Reading();
  r.range = "AUTO";
  bar.showReading(r);
  check(!key(bar, "hold")->isChecked() && key(bar, "auto")->isChecked() && !key(bar, "rel")->isChecked()
          && !key(bar, "minmax")->isChecked(),
        "states: all off, AUTO");
  // a second value says nothing about the keys
  Reading second;
  second.id = 1;
  second.hold = true;
  bar.showReading(second);
  check(!key(bar, "hold")->isChecked(), "states: a secondary value is ignored");

  // --- 3b. another meter at the same size: the groups placed anew, none
  //         over another, each as wide as its keys (they overlapped until
  //         the window was resized) ---
  {
    ControlBar other;
    other.setConnected(true);
    other.setProtocol(FrameFormat::ZotekBLE, "ZT-5B *");
    other.resize(700, 80);
    other.show();
    QTest::qWait(20);
    other.setProtocol(FrameFormat::UniTiDMM, "UT60BT");
    QTest::qWait(20);
    QList<QWidget *> shown;
    for (QFrame *g : other.findChildren<QFrame *>(Qt::FindDirectChildrenOnly))
      if (!g->isHidden())
        shown << g;
    bool apart = !shown.isEmpty();
    for (int i = 0; i < shown.size(); ++i)
    {
      apart = apart && shown[i]->width() == shown[i]->sizeHint().width();
      for (int j = i + 1; j < shown.size(); ++j)
        apart = apart && !shown[i]->geometry().intersects(shown[j]->geometry());
    }
    check(apart, QString("another meter at the same size: %1 groups apart").arg(shown.size()));
  }

  // a picture to look at: QTDMM_SHOT=<dir> writes the bar per meter
  if (qEnvironmentVariableIsSet("QTDMM_SHOT"))
  {
    const QString dir = qEnvironmentVariable("QTDMM_SHOT");
    bar.resize(bar.sizeHint());
    struct Shot { FrameFormat::DataFormat format; const char *model; const char *name; };
    for (const Shot &shot : { Shot{ FrameFormat::UniTiDMM, "UT60BT", "ut60bt" }, Shot{ FrameFormat::UniTUT61Plus, "UT61E+", "ut61plus" },
                              Shot{ FrameFormat::UniTUT202BT, "UT202BT", "ut202bt" },
                              Shot{ FrameFormat::UniTiENV, "UT333BT", "ut333bt" } })
    {
      const auto [format, model, name] = shot;
      bar.setProtocol(format, model);
      bar.resize(bar.sizeHint());
      QTest::qWait(50);
      bar.grab().save(dir + "/controlbar_" + name + ".png");
    }
  }

  // --- 4. not connected: no keys ---
  bar.setConnected(false);
  check(!key(bar, "hold")->isEnabled(), "disconnected: keys off");

  if (failed)
  {
    qWarning() << failed << "ControlBar check(s) failed";
    return 1;
  }
  qInfo() << "All ControlBar tests passed.";
  return 0;
}
