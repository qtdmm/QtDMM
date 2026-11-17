// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The settings pages after the redesign (26.2): the keys go through a page
// unchanged, fields that depend on a choice come with it, and the
// recording length - asked when Record is pressed, no longer on a page -
// survives OK and a page reset.
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QRadioButton>
#include <QLabel>
#include <QSpinBox>
#include <QTemporaryDir>

#include "core/settings.h"
#include "ui/dialogs/recordlengthdlg.h"
#include "ui/settings/alarmprefs.h"
#include "ui/settings/generalprefs.h"
#include "ui/settings/graphprefs.h"
#include "ui/settings/guiprefs.h"
#include "ui/settings/recorderprefs.h"
#include "ui/views/lcdwidget.h"
#include "testutil.h"

template <typename T> static T *child(QWidget &page, const char *name)
{
  T *w = page.findChild<T *>(name);
  check(w, QString("no %1").arg(name));
  return w;
}

// shown on the page (with the row around it)
static bool shown(QWidget *w)
{
  return w && w->isVisible();
}

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  QTemporaryDir dir;

  // --- 1. the keys go through the pages unchanged: loaded, applied, saved
  //        - values off the defaults, so a page that dropped one shows ---
  {
    Settings cfg("pages", dir.path());
    const QList<QPair<QString, QVariant>> keys = {
      { "Alert/unsaved-file", false }, { "Save/window-pos", false }, { "Icons/text-label", true },
      { "Port settings/sigrok_exe", QString("/opt/sigrok-cli") },
      { "Windows/design", QString("silver") }, { "Display/display-bar", false }, { "Display/display-min-max", true },
      { "Display/lcd", QString("amber") },
      { "Meter/scale-mode", 2 }, { "Meter/style", 0 }, { "Meter/ballistics", false }, { "Meter/red-zone", 75 },
      { "Graph/variant", QString("custom") }, { "Graph/line-mode", 2 }, { "Graph/point-mode", 3 },
      { "Graph/line-width", 4 }, { "Graph/crosshair-cursor", false },
      { "Window/size", 45 }, { "Window/size-unit", 1 },
      { "Sample/rate", 5 }, { "Sample/rate-unit", 2 }, { "Sample/time", 90 }, { "Sample/time-unit", 1 },
      { "Start/mode", int(RecordingStore::Falling) }, { "Start/hour", 13 }, { "Start/minute", 14 }, { "Start/second", 15 },
      { "Start/raising-threshold", QString("1.5") }, { "Start/falling-threshold", QString("-3") },
      { "Start/pre-trigger", true }, { "Start/pre-trigger-time", 7 }, { "Start/pre-trigger-unit", 1 },
      { "Start/trigger-value", QString("2@device:ab12cd34") } };
    for (const auto &[key, value] : keys)
    {
      if (value.typeId() == QMetaType::Bool)
        cfg.setBool(key, value.toBool());
      else if (value.typeId() == QMetaType::Int)
        cfg.setInt(key, value.toInt());
      else
        cfg.setString(key, value.toString());
    }
    cfg.save();

    GeneralPrefs general;
    GuiPrefs gui;
    GraphPrefs graph;
    RecorderPrefs recorder;
    for (SettingsPage *p : std::initializer_list<SettingsPage *>{ &general, &gui, &graph, &recorder })
    {
      p->setCfg(&cfg);
      p->defaultsSLOT();
    }
    // the keys away, then back from the pages
    Settings blank("pages_out", dir.path());
    for (SettingsPage *p : std::initializer_list<SettingsPage *>{ &general, &gui, &graph, &recorder })
    {
      p->setCfg(&blank);
      p->applySLOT();
    }
    blank.save();
    for (const auto &[key, value] : keys)
    {
      const QString got = value.typeId() == QMetaType::Bool ? QString::number(blank.getBool(key, !value.toBool()))
                          : value.typeId() == QMetaType::Int ? QString::number(blank.getInt(key, -99))
                                                             : blank.getString(key, "-");
      const QString want = value.typeId() == QMetaType::Bool ? QString::number(value.toBool()) : value.toString();
      check(got == want, QString("keys: %1 came back as %2, expected %3").arg(key, got, want));
    }

    // a design chosen in the menu is what OK on the page keeps
    gui.setDesign("dark");
    gui.applySLOT();
    blank.save();
    check(blank.getString("Windows/design", "-") == "dark", "gui: the menu's design after OK, got " + blank.getString("Windows/design", "-"));
    // the LCD colours of a config from before the choice: blue, or Custom with a tint of its own
    {
      Settings before("pages_lcd", dir.path());
      GuiPrefs page;
      page.setCfg(&before);
      page.defaultsSLOT();
      check(page.lcdVariant() == LcdWidget::BacklightBlue, "gui: LCD colours of an old config: backlight blue");
      before.setColor("Display/display-background", QColor(0x80, 0xc0, 0xff));
      before.save();
      page.defaultsSLOT();
      check(page.lcdVariant() == LcdWidget::Custom, "gui: an old config with a tint of its own: Custom");
      page.factoryDefaultsSLOT();
      check(page.lcdVariant() == LcdWidget::BacklightBlue, "gui: factory defaults: backlight blue");
    }

    // what the dialog reads
    check(graph.windowSeconds() == 45 * 60, "graph: window");
    check(recorder.sampleLength() == 90 * 60 * 10 && recorder.sampleMode() == RecordingStore::Falling
            && recorder.startTime() == QTime(13, 14, 15) && recorder.preTrigger() == 7 * 60 * 1000,
          "recorder: length, start or pre-trigger");
    // the value the threshold watches: chosen among the values of every
    // meter connected; one not connected now stays chosen
    auto *triggerValue = child<QComboBox>(recorder, "ui_triggerValue");
    recorder.setTriggerChoices({ { ValueRef(0), "1: V" }, { ValueRef(1), "2: A" }, { ValueRef(0, "device:b"), "Sim B · W" } });
    check(!triggerValue->isHidden() && triggerValue->count() == 4
            && triggerValue->currentText().contains("not connected")
            && recorder.triggerValue() == ValueRef::fromString(triggerValue->currentData().toString()),
          "recorder: the stored value of a meter not connected stays chosen: " + triggerValue->currentText());
    triggerValue->setCurrentIndex(2);
    Q_EMIT triggerValue->activated(2);
    check(recorder.triggerValue() == ValueRef(0, "device:b"), "recorder: another meter's value: "
            + recorder.triggerValue().toString());
    recorder.setTriggerChoices({ { ValueRef(0), "1: V" }, { ValueRef(0, "device:b"), "Sim B · W" } });
    check(triggerValue->count() == 2 && triggerValue->currentData().toString() == "0@device:b",
          "recorder: still chosen when the list changes");

    // --- 2. what comes with a choice ---
    graph.show();
    recorder.show();
    QApplication::processEvents();
    check(shown(child<QWidget>(graph, "ui_dataColor")), "graph: Custom shows the own colours");
    child<QComboBox>(graph, "ui_variant")->setCurrentIndex(0);
    check(!shown(child<QWidget>(graph, "ui_dataColor")), "graph: the own colours go with Custom");
    check(shown(child<QWidget>(graph, "ui_cursorColor")), "graph: the cursor colour stays");

    check(shown(child<QWidget>(recorder, "ui_fallingThreshold")) && !shown(child<QWidget>(recorder, "ui_raisingThreshold"))
            && shown(child<QWidget>(recorder, "ui_preTrigger")) && !shown(child<QWidget>(recorder, "ui_startTime")),
          "recorder: the falling threshold and the pre-trigger with the threshold start");
    child<QRadioButton>(recorder, "manualBut")->setChecked(true);
    check(!shown(child<QWidget>(recorder, "ui_fallingThreshold")), "recorder: by hand shows no threshold");
    check(!shown(child<QWidget>(recorder, "ui_preTrigger")), "recorder: by hand shows no pre-trigger");
    child<QRadioButton>(recorder, "predefinedBut")->setChecked(true);
    check(shown(child<QWidget>(recorder, "ui_startTime")), "recorder: the clock time comes with its start");

    // --- 3. the recording length: not on the page, kept by it ---
    recorder.setLength(3, 2);
    recorder.factoryDefaultsSLOT();
    check(recorder.lengthValue() == 3 && recorder.lengthUnit() == 2 && recorder.sampleLength() == 3 * 3600 * 10,
          "length: a page reset keeps it");
    recorder.applySLOT();
    blank.save();
    check(blank.getInt("Sample/time") == 3 && blank.getInt("Sample/time-unit") == 2, "length: OK writes it back");
    recorder.setLength(0, 0);
    check(recorder.sampleLength() == 0, "length: 0 records until stopped");
  }

  // --- 4. the window Record asks with ---
  {
    RecordLengthDlg dlg(25, 1);
    check(dlg.value() == 25 && dlg.unit() == 1, "length dialog: the last length ready");
    dlg.findChild<QRadioButton *>("ui_untilStop")->setChecked(true);
    check(dlg.value() == 0, "length dialog: until Stop is 0");
    dlg.findChild<QSpinBox *>("ui_length")->setValue(3);
    check(dlg.value() == 3, "length dialog: typing a length chooses it");
    RecordLengthDlg open(0, 0, "starts at 12:00");
    check(open.value() == 0 && open.findChild<QRadioButton *>("ui_untilStop")->isChecked()
            && open.findChild<QLabel *>("ui_intro")->text().contains("12:00"),
          "length dialog: until Stop ready, the start hint shown");
  }

  // --- 5. an alarm on another value: the dialog chooses it, the thresholds
  //        say its unit; a meter with one value has no choice ---
  {
    const QMap<int, QString> units{ { 0, "ppm" }, { 1, "C" }, { 2, "%" } };
    Alarm al;
    al.condition = Alarm::Above;
    AlarmDlg one(al, units);
    // the condition, the sound, its repeat and the recording: no fifth one
    check(one.findChildren<QComboBox *>().size() == 4, "alarm: one value, no choice of value");
    AlarmDlg dlg(al, units, { "1: CO2 (ppm)", "2: Temperature (°C)", "3: Humidity (%)" });
    QComboBox *value = nullptr;
    for (QComboBox *c : dlg.findChildren<QComboBox *>())
      if (c->count() == 3 && c->itemText(0).startsWith("1: CO2"))
        value = c;
    check(value != nullptr, "alarm: three values to choose from");
    auto threshold = [&]
    {
      for (QLabel *l : dlg.findChildren<QLabel *>())
        if (l->text().startsWith("&Threshold"))
          return l->text();
      return QString();
    };
    check(threshold() == "&Threshold (ppm):", "alarm: the main value's unit at first, got " + threshold());
    if (value)
      value->setCurrentIndex(2);
    check(threshold() == "&Threshold (%):" && dlg.alarm().value == ValueRef(2), "alarm: value 3 in %, got " + threshold());
  }

  if (failed)
    qWarning() << failed << "settings page test(s) failed.";
  else
    qInfo() << "settings pages: all passed";
  return failed ? 1 : 0;
}
