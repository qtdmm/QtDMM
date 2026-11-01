// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// AlarmManager: every condition, minimum duration, hysteresis, acknowledge,
// the no-readings watchdog, JSON round trip and the description text.

#include <QtCore>

#include "core/alarm.h"
#include "ui/engnumbervalidator.h"
#include "testutil.h"

struct Log
{
  QStringList events;
  Log(AlarmManager &m)
  {
    QObject::connect(&m, &AlarmManager::raised, [this](int i, const Alarm &a, double v) { events << QString("raised %1 %2 %3").arg(i).arg(a.name).arg(v); });
    QObject::connect(&m, &AlarmManager::cleared, [this](int i, const Alarm &a) { events << QString("cleared %1 %2").arg(i).arg(a.name); });
  }
};

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);

  // --- 1. conditions ---
  Alarm low; low.name = "low"; low.condition = Alarm::Below; low.a = 5;
  Alarm high; high.name = "high"; high.condition = Alarm::Above; high.a = 12;
  Alarm out; out.name = "out"; out.condition = Alarm::Outside; out.a = 11; out.b = 13;
  Alarm in; in.name = "in"; in.condition = Alarm::Inside; in.a = 1; in.b = 2;
  Alarm ol; ol.name = "ol"; ol.condition = Alarm::Overload;
  check(low.matches(4.9, false) && !low.matches(5, false) && !low.matches(4, true), "below");
  check(high.matches(12.1, false) && !high.matches(12, false), "above");
  check(out.matches(10.9, false) && out.matches(13.1, false) && !out.matches(12, false), "outside");
  check(in.matches(1.5, false) && !in.matches(2.5, false), "inside");
  check(ol.matches(0, true) && !ol.matches(0, false), "overload");
  Alarm swapped = out; swapped.a = 13; swapped.b = 11;
  check(swapped.matches(10.9, false) && !swapped.matches(12, false), "bounds in either order");

  // --- 2. raise at once, clear with hysteresis ---
  {
    AlarmManager m;
    Log log(m);
    Alarm h = low; h.hysteresis = 0.5;
    m.setAlarms({h});
    m.feed(6, false, 0);
    check(log.events.isEmpty() && m.state(0) == AlarmManager::Off, "6 V: off");
    m.feed(4.9, false, 1000);
    check(log.events == QStringList{"raised 0 low 4.9"} && m.state(0) == AlarmManager::Raised, "4.9 V: raised at once (no duration)");
    m.feed(5.2, false, 2000);
    check(log.events.size() == 1 && m.state(0) == AlarmManager::Raised, "5.2 V: still raised, inside the hysteresis band");
    m.feed(5.5, false, 3000);
    check(log.events.last() == "cleared 0 low" && m.state(0) == AlarmManager::Off, "5.5 V: cleared (5 + 0.5)");
    m.feed(4, false, 4000);
    check(log.events.size() == 3 && log.events.last().startsWith("raised"), "raises again");
  }

  // --- 3. minimum duration suppresses spikes ---
  {
    AlarmManager m;
    Log log(m);
    Alarm d = high; d.seconds = 2;
    m.setAlarms({d});
    m.feed(13, false, 0);
    check(m.state(0) == AlarmManager::Pending && log.events.isEmpty(), "pending, not raised");
    m.feed(13, false, 1500);
    check(log.events.isEmpty(), "1.5 s: still pending");
    m.feed(11, false, 1800);
    check(m.state(0) == AlarmManager::Off, "spike over: back to off");
    m.feed(13, false, 2000);
    m.feed(13, false, 4100);
    check(log.events == QStringList{"raised 0 high 13"}, "raised after 2 s of holding");
  }

  // --- 4. acknowledge: banner may go, state clears only with the condition ---
  {
    AlarmManager m;
    Log log(m);
    m.setAlarms({low});
    m.feed(1, false, 0);
    check(m.anyRaised(), "raised");
    m.acknowledge(0);
    check(m.state(0) == AlarmManager::Acknowledged && !m.anyRaised(), "acknowledged");
    m.feed(1, false, 1000);
    check(log.events.size() == 1, "no second raise while acknowledged and still low");
    m.feed(9, false, 2000);
    check(log.events.last() == "cleared 0 low", "cleared when the reading is back");
  }

  // --- 5. no readings ---
  {
    AlarmManager m;
    Log log(m);
    Alarm nr; nr.name = "silence"; nr.condition = Alarm::NoReadings; nr.seconds = 3;
    m.setAlarms({nr});
    m.feed(1, false, 0);
    m.tick(2000);
    check(log.events.isEmpty(), "2 s silence: nothing");
    m.tick(3100);
    check(log.events == QStringList{"raised 0 silence 1"}, "3 s silence: raised");
    m.tick(5000);
    check(log.events.size() == 1, "raised once");
    m.feed(2, false, 6000);
    check(log.events.last() == "cleared 0 silence", "a reading clears it");
  }

  // --- 5b. silence before the first reading: only a connected meter can be
  // silent. Without this the alarm raised on the first tick after the start
  // (epoch as the time base) and ran its actions - recorder, command.
  {
    AlarmManager m;
    Log log(m);
    Alarm nr; nr.name = "silence"; nr.condition = Alarm::NoReadings; nr.seconds = 60;
    m.setAlarms({nr});
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    m.tick(now);
    m.tick(now + 3600 * 1000);
    check(log.events.isEmpty(), "not connected, never a reading: no alarm");

    m.setConnected(true, now);
    m.tick(now + 59000);
    check(log.events.isEmpty(), "connected 59 s ago: not yet");
    m.tick(now + 60000);
    check(log.events == QStringList{"raised 0 silence 0"}, "connected 60 s ago without a reading: raised");

    m.setConnected(false, now + 61000);
    check(log.events.last() == "cleared 0 silence", "disconnecting clears it");
    m.tick(now + 7200 * 1000);
    check(log.events.size() == 2, "and it stays quiet while disconnected");

    // after reconnecting the clock starts again, and a reading takes over
    m.setConnected(true, now + 100000);
    m.feed(1, false, now + 110000);
    m.tick(now + 165000);
    check(log.events.size() == 2, "silence is measured from the last reading");
    m.tick(now + 171000);
    check(log.events.last() == "raised 0 silence 1", "60 s after that reading: raised");
  }

  // --- 6. disabled alarms and edits ---
  {
    AlarmManager m;
    Log log(m);
    Alarm off = low; off.enabled = false;
    m.setAlarms({off, high});
    m.feed(1, false, 0);
    check(log.events.isEmpty(), "disabled alarm does not raise");
    m.feed(20, false, 100);
    check(log.events == QStringList{"raised 1 high 20"}, "second alarm has index 1");
    Alarm high2 = high; high2.message = "edited";
    m.setAlarms({high2});
    check(m.state(0) == AlarmManager::Raised, "a raised alarm keeps its state through an edit (by name)");
    m.setAlarms({});
    check(!m.anyRaised(), "no alarms, nothing raised");
  }

  // --- 7. JSON round trip and description ---
  {
    Alarm a = out; a.seconds = 2.5; a.hysteresis = 0.1; a.message = "Voltage odd"; a.color = QColor("#123456");
    a.sound = "alarm_crit"; a.repeat = Alarm::Times; a.repeatCount = 5; a.clearSound = true; a.popup = true; a.command = "notify-send %n %v%u"; a.disconnect = true; a.recorder = Alarm::RecorderStart; a.markTable = false;
    const QString json = Alarm::listToJson({a, low});
    const QList<Alarm> back = Alarm::listFromJson(json);
    check(back.size() == 2, "two alarms back");
    if (back.size() == 2)
    {
      const Alarm &r = back[0];
      check(r.name == "out" && r.condition == Alarm::Outside && r.a == 11 && r.b == 13 && r.seconds == 2.5 && r.hysteresis == 0.1, "condition round trip");
      check(r.message == "Voltage odd" && r.color == QColor("#123456") && r.popup && r.command == "notify-send %n %v%u" && r.disconnect
            && r.recorder == Alarm::RecorderStart && !r.markTable && r.markGraph, "actions round trip");
      check(r.sound == "alarm_crit" && r.repeat == Alarm::Times && r.repeatCount == 5 && r.clearSound, "sound round trip");
      check(back[1].condition == Alarm::Below && back[1].sound == "beep" && back[1].repeat == Alarm::Once && !back[1].clearSound && back[1].banner, "defaults survive");
    }
    Alarm plain = low; plain.tintDisplay = false;
    check(!Alarm::listFromJson(Alarm::listToJson({plain})).value(0).tintDisplay, "tint off round trip");
    check(Alarm::listFromJson(R"([{"name":"old","condition":"below","a":5}])").value(0).tintDisplay,
          "an alarm stored before the option colours the display");
    check(Alarm::listFromJson(R"([{"name":"old","beep":false}])").value(0).sound.isEmpty()
          && Alarm::listFromJson(R"([{"name":"old","beep":true}])").value(0).sound == "beep", "beep before the sounds");
    check(Alarm::listFromJson("not json").isEmpty(), "junk gives no alarms");
    check(low.describe("V") == "below 5V", "describe: " + low.describe("V"));
    check(a.describe("V") == "outside 11V to 13V for 2.5 s", "describe range+duration: " + a.describe("V"));
    Alarm nr; nr.condition = Alarm::NoReadings; nr.seconds = 10;
    check(nr.describe("V") == "no readings for 10 s", "describe silence: " + nr.describe("V"));
    Alarm milli = low; milli.a = 0.0047;
    check(milli.describe("A") == "below 4.7mA", "describe with SI prefix: " + milli.describe("A"));
    // a threshold keeps its digits: the dialog reads its fields back, and a
    // banner that says "below 4.8V" for an alarm raising at 4.7512 is a lie
    Alarm exact = low; exact.a = 4.7512;
    check(exact.describe("V") == "below 4.7512V", "describe keeps the digits: " + exact.describe("V"));
    Alarm ohms = low; ohms.a = 1234;
    check(ohms.describe("Ohm") == "below 1.234kOhm", "... with a prefix: " + ohms.describe("Ohm"));
    for (double v : {4.7512, 1234.0, 0.04712, -2.5e-6, 0.0})
      check(qFuzzyCompare(EngNumberValidator::value(EngNumberValidator::engText(v)) + 1.0, v + 1.0),
            QString("engText(%1) reads back as %2").arg(v).arg(EngNumberValidator::value(EngNumberValidator::engText(v))));
  }

  // --- 8. the display's colour: the alarm raised last, also acknowledged, until it clears ---
  {
    AlarmManager m;
    Alarm red = low; red.name = "red"; red.color = QColor("#d82222"); red.hysteresis = 0;
    Alarm blue = high; blue.name = "blue"; blue.color = QColor("#2222d8");
    Alarm quiet = out; quiet.name = "quiet"; quiet.tintDisplay = false;
    m.setAlarms({red, blue, quiet});
    m.feed(8, false, 0);
    check(!m.displayColor().isValid(), "nothing raised: no colour");
    m.feed(4, false, 1000);
    check(m.displayColor() == red.color, "red raised: red");
    m.acknowledgeAll();
    check(m.displayColor() == red.color, "acknowledged: still red");
    // a second alarm on another condition: a list of two where both hold
    Alarm blue2 = blue; blue2.condition = Alarm::Below; blue2.a = 3;
    m.setAlarms({red, blue2, quiet});
    m.feed(2, false, 2000);
    check(m.state(1) == AlarmManager::Raised && m.displayColor() == blue.color, "blue raised last: blue");
    m.feed(4, false, 3000);
    check(m.state(1) == AlarmManager::Off && m.displayColor() == red.color, "blue cleared: red again");
    m.feed(14, false, 4000);   // red clears; quiet (outside 11..13) is raised since 4 V, it does not colour
    check(m.state(0) == AlarmManager::Off && m.state(2) != AlarmManager::Off && !m.displayColor().isValid(),
          "red cleared, quiet raised: no colour");
  }

  if (failed == 0)
    qInfo() << "All alarm tests passed.";
  else
    qWarning() << failed << "alarm test(s) failed.";
  // --- 9. an alarm on another value of the meter: only that value's
  //        readings count; "no readings" stays the meter's silence ---
  {
    AlarmManager m;
    Log log(m);
    Alarm humid;
    humid.name = "humid";
    humid.condition = Alarm::Above;
    humid.a = 60;
    humid.value = ValueRef(2);
    Alarm silent;
    silent.name = "silent";
    silent.condition = Alarm::NoReadings;
    silent.seconds = 5;
    m.setAlarms({humid, silent});
    m.setConnected(true, 0);
    m.feed(800, false, 0, 0);    // CO2 800 ppm: not the humidity
    m.feed(22, false, 0, 1);
    check(log.events.isEmpty(), "value 3: the main value's 800 is not above 60 %");
    m.feed(65, false, 1000, 2);
    check(log.events == QStringList{"raised 0 humid 65"}, "value 3: 65 % raises it, got " + log.events.join(", "));
    m.feed(50, false, 2000, 2);
    check(log.events.last() == "cleared 0 humid", "value 3: 50 % clears it");
    // the other values come, the main one does not: the meter is silent
    for (qint64 t = 3000; t <= 9000; t += 1000)
    {
      m.feed(50, false, t, 2);
      m.tick(t);
    }
    check(log.events.last().startsWith("raised 1 silent"), "no readings: the main value's, got " + log.events.last());
    m.feed(800, false, 9500, 0);
    check(log.events.last() == "cleared 1 silent", "no readings: ends with the main value");
  }
  // --- an alarm on another meter's value: only that meter's readings count ---
  {
    AlarmManager m;
    Log log(m);
    Alarm far;
    far.name = "far";
    far.condition = Alarm::Above;
    far.a = 10;
    far.value = ValueRef(1, "device:ab12cd34");
    m.setAlarms({far});
    m.feed(20, false, 0, 1, "instance:default");
    m.feed(20, false, 0, 1);
    check(log.events.isEmpty(), "another meter's value 2: this meter's 20 does not raise it");
    m.feed(20, false, 1000, 1, "device:ab12cd34");
    check(log.events == QStringList{"raised 0 far 20"}, "its own meter's 20 raises it, got " + log.events.join(", "));
  }
  {
    Alarm al;
    al.value = ValueRef(1);
    check(Alarm::fromJson(al.toJson()).value == ValueRef(1) && al.toJson()["value"].isDouble()
            && !Alarm().toJson().contains("value"),
          "JSON: the value kept as a number, the main one not written");
    check(Alarm::fromJson(QJsonObject{ { "value", 3 } }).value == ValueRef(3), "JSON: the fourth value kept");
    check(Alarm::fromJson(QJsonObject{ { "value", 7 } }).value == ValueRef(3), "JSON: a value beyond the fourth is the fourth");
    al.value = ValueRef(1, "device:ab12cd34");
    check(Alarm::fromJson(al.toJson()).value == al.value && al.toJson()["value"].toString() == "1@device:ab12cd34",
          "JSON: another meter's value as text");
  }

  // --- alarms from someone else's file: no program, the port kept open ---
  {
    Alarm run;
    run.name = "Fridge";
    run.command = "touch /tmp/marker";
    run.disconnect = true;
    Alarm quiet;
    quiet.name = "Quiet";
    bool removed = false;
    const QList<Alarm> back = Alarm::listFromJson(Alarm::listWithoutPrograms(Alarm::listToJson({ run, quiet }), &removed));
    check(removed && back.size() == 2 && back[0].name == "Fridge" && back[0].command.isEmpty() && !back[0].disconnect,
          "file alarms: the program and the disconnect left out, the alarm kept");
    Alarm::listWithoutPrograms(Alarm::listToJson({ quiet }), &removed);
    check(!removed, "file alarms: none with a program: nothing said");
    const bool none = Alarm::listWithoutPrograms(QString(), &removed).isEmpty();
    check(none && !removed, "file alarms: none at all: nothing said");
  }

  // --- the name in messages: default names already say "Alarm" ---
  check(Alarm::title("Alarm 1") == "Alarm 1", "default name is not doubled: " + Alarm::title("Alarm 1"));
  check(Alarm::title("High voltage") == "Alarm High voltage", "own name gets the word: " + Alarm::title("High voltage"));
  check(Alarm::title("alarm for the fridge") == "alarm for the fridge", "a name starting with the word is kept");

  return failed == 0 ? 0 : 1;
}
