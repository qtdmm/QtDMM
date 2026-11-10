// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The sidebar "Devices" without a main window: My devices in their order,
// the one in use marked, a click to switch, rename in place, a change of
// the library shown; when the sidebar is shown at the start and after the
// assistant; the dialog "Settings..." of an entry; the node "Open devices"
// with its readings and this window's values.

#include <QtWidgets>
#include <QTemporaryDir>
#include <QTest>
#include <QSignalSpy>
#include <QDropEvent>
#include <QMimeData>

#include "core/devicelibrary.h"
#include "device/meterconnection.h"
#include "ui/devicesettings.h"
#include "ui/devicesidebar.h"
#include "ui/devicespecs.h"
#include "ui/dialogs/devicesettingsdlg.h"
#include "testutil.h"

int main(int argc, char **argv)
{
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  QTemporaryDir dir;
  DeviceLibrary library(dir.path());
  const QString ut61e = library.add("Bench UT61E", { { "DMM/model", "Uni-Trend UT61E" },
                                                     { "Port settings/device", "SERIAL /dev/ttyUSB0" } });
  const QString tfa = library.add("TFA", { { "DMM/model", "TFA AIRCO2NTROL MINI" } });
  const QString shunt = library.add("Battery", { { "DMM/model", "Victron SmartShunt" } });

  // 1. the entries in their order, the ones connected marked
  DeviceSidebar sidebar(&library);
  check(sidebar.deviceIds() == QStringList({ ut61e, tfa, shunt }), "entries in their order");
  check(sidebar.devicesNode()->text(0) == "My devices", "the node");
  QTreeWidgetItem *bench = sidebar.deviceItem(ut61e);
  check(bench && bench->text(0) == "Bench UT61E", "the name");
  check(bench && bench->toolTip(0).contains("/dev/ttyUSB0"), "the place in the tooltip: " + bench->toolTip(0));
  check(bench && !(bench->flags() & Qt::ItemIsDropEnabled), "an entry takes no other");
  const QImage plain = sidebar.deviceItem(tfa)->icon(0).pixmap(16).toImage();
  sidebar.setOpenDevices({ DeviceSidebar::OpenDevice { tfa, "TFA", true } });
  check(sidebar.deviceItem(tfa)->font(0).bold() && !bench->font(0).bold(), "the one on the bench is bold");
  {
    // no dot here (the bench has it): the picture stays as it was
    const QImage on = sidebar.deviceItem(tfa)->icon(0).pixmap(16).toImage();
    sidebar.setOpenDevices({ DeviceSidebar::OpenDevice { tfa, "TFA", false } });
    const QImage off = sidebar.deviceItem(tfa)->icon(0).pixmap(16).toImage();
    check(on == plain && off == plain && !plain.isNull(), "My devices: its picture, no dot");
    sidebar.setOpenDevices({ DeviceSidebar::OpenDevice { tfa, "TFA", true } });
  }

  // 2. a click on a device does nothing (its menu connects it); a double
  //    click is info and settings
  QStringList info;
  QObject::connect(&sidebar, &DeviceSidebar::settingsRequested, [&](const QString &id) { info << id; });
  Q_EMIT sidebar.itemClicked(bench, 0);
  QTest::qWait(QApplication::doubleClickInterval() + 100);
  bench = sidebar.deviceItem(ut61e);   // the sidebar may fill anew from the library's file
  check(info.isEmpty() && !sidebar.isConnected(ut61e) && sidebar.isConnected(tfa),
        "a click does nothing; the one connected is, the other not");
  {
    Q_EMIT sidebar.itemDoubleClicked(bench, 0);
    check(info == QStringList({ ut61e }), "double click: info and settings " + info.join(','));
    check(sidebar.deviceItem(tfa)->toolTip(0).contains("Connected")
            && sidebar.deviceItem(tfa)->toolTip(0).contains("Double-click")
            && !bench->toolTip(0).contains("Connected"),
          "the tooltip: connected, how to read more: " + sidebar.deviceItem(tfa)->toolTip(0));
  }

  // the last row adds a device; it is no device itself
  QTreeWidgetItem *add = sidebar.devicesNode()->child(sidebar.devicesNode()->childCount() - 1);
  int adds = 0;
  QObject::connect(&sidebar, &DeviceSidebar::addDeviceRequested, [&] { adds++; });
  Q_EMIT sidebar.itemClicked(add, 0);
  check(add->text(0).startsWith('+') && adds == 1 && info.size() == 1, "the last row asks for Add device");
  check(!(add->flags() & (Qt::ItemIsDragEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable)), "the add row stays put");

  // 3. rename in place; an empty name is not taken
  sidebar.deviceItem(shunt)->setText(0, "House battery");
  check(library.find(shunt)->name == "House battery", "renamed in place");
  sidebar.deviceItem(shunt)->setText(0, "  ");
  check(library.find(shunt)->name == "House battery", "an empty name is not taken");

  // 4. a change of the library (here or another QtDMM) is shown
  library.move(shunt, 0);
  QCoreApplication::processEvents();
  check(sidebar.deviceIds() == QStringList({ shunt, ut61e, tfa }), "new order shown");
  library.remove(tfa);
  QCoreApplication::processEvents();
  check(sidebar.deviceIds() == QStringList({ shunt, ut61e }), "removed entry gone");
  check(sidebar.deviceItem(shunt)->text(0) == "House battery", "new name shown");

  // 5. when it is shown
  check(!DeviceSidebar::shownAtStart(QVariant(), 1), "one device: closed");
  check(DeviceSidebar::shownAtStart(QVariant(), 2), "two devices, never set: open");
  check(!DeviceSidebar::shownAtStart(QVariant(false), 3), "closed by the user: stays closed");
  check(DeviceSidebar::shownAtStart(QVariant(true), 1), "opened by the user: stays open");
  check(DeviceSettingsDlg::sampleRate({}) == "-", "sample rate: none");
  check(DeviceSettingsDlg::sampleRate({ 1000 }) == "-", "sample rate: one frame");
  check(DeviceSettingsDlg::sampleRate({ 0, 500, 1000, 1500, 2000 }) == QLocale().toString(2.0, 'f', 1) + " Hz", "sample rate: 2 Hz");
  check(DeviceSettingsDlg::sampleRate({ 0, 2000, 4000 }) == QLocale().toString(0.5, 'f', 2) + " Hz", "sample rate: below 1 Hz");
  check(DeviceSidebar::opensAfterAdd(1, 2), "the second device opens it");
  check(!DeviceSidebar::opensAfterAdd(0, 1), "the first does not");
  check(!DeviceSidebar::opensAfterAdd(2, 3), "the third does not");
  check(!DeviceSidebar::opensAfterAdd(2, 2), "a known one changed does not");

  // 6. "Settings..." of an entry: its keys, OK only when complete
  {
    DeviceSettingsDlg dlg("Battery");
    QPushButton *ok = dlg.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
    dlg.load({ { "DMM/model", "Victron SmartShunt" }, { "Port settings/ble-address", "AA:BB:CC:DD:EE:FF" } });
    check(!ok->isEnabled(), "Victron without key: no OK");
    dlg.load({ { "DMM/model", "Victron SmartShunt" }, { "Port settings/ble-address", "AA:BB:CC:DD:EE:FF" },
               { "Port settings/ble-key", "00112233445566778899aabbccddeeff" } });
    check(ok->isEnabled(), "complete: OK");
    check(dlg.keys().value("DMM/model") == "Victron SmartShunt", "keys: the model");
  }
  {
    // Info first: the device at a glance, its state, the maker's website, the help
    DeviceSettingsDlg dlg("Bench");
    dlg.load({ { "DMM/model", "Uni-Trend UT61E" }, { "Port settings/device", "SERIAL /dev/ttyUSB0" } });
    check(dlg.currentTab() == DeviceSettingsDlg::InfoTab, "the dialog opens on Info");
    const QString summary = dlg.findChild<QLabel *>("ui_deviceInfo")->text();
    check(summary.contains("Bench") && summary.contains("UT61E") && summary.contains("/dev/ttyUSB0"),
          "Info: name, model, connection: " + summary);
    int asked = 0;
    dlg.setStatus([&] { ++asked; return QString("<p>In use in this window.</p>"); });
    check(asked == 1 && dlg.statusText().contains("In use"), "Info: the state at once");
    // a second, and a busy CI runner may be late with it
    QElapsedTimer waited;
    waited.start();
    while (asked < 2 && waited.elapsed() < 3000)
      QTest::qWait(50);
    check(asked >= 2, QString("Info: the state again each second, asked %1 times").arg(asked));
    check(dlg.linksText().contains("https://www.uni-trend.com/") && dlg.linksText().contains("help:supported-devices.md"),
          "Info: the maker's website and the help: " + dlg.linksText());
    QString page;
    QObject::connect(&dlg, &DeviceSettingsDlg::helpRequested, [&](const QString &p) { page = p; });
    Q_EMIT dlg.findChild<QLabel *>("ui_deviceLinks")->linkActivated("help:supported-devices.md");
    check(page == "supported-devices.md", "Info: the help link asks for its page");
    // the sample rate: only with a connection, from its frames while the dialog is open
    check(!dlg.statusText().contains("Sample rate"), "Info: no sample rate without a connection");
    MeterConnection dmm(nullptr);
    MeterConnection *source = nullptr;
    dlg.setFrameSource([&] { return source; });
    source = &dmm;
    dlg.setFrameSource([&] { return source; });
    check(dlg.statusText().contains("Sample rate: -"), "Info: sample rate - before two frames: " + dlg.statusText());
    for (int i = 0; i < 3; ++i)
    {
      Q_EMIT dmm.frame(QByteArray("x"), 0, std::nullopt);
      QTest::qWait(100);
    }
    dlg.setFrameSource([&] { return source; });
    check(dlg.statusText().contains(" Hz") && !dlg.statusText().contains("Sample rate: -"),
          "Info: sample rate from the frames: " + dlg.statusText());
    source = nullptr;
    dlg.setFrameSource([&] { return source; });
    check(!dlg.statusText().contains("Sample rate") && !QObject::disconnect(&dmm, &MeterConnection::frame, &dlg, nullptr),
          "Info: no counting after the connection went");
    dlg.showTab(DeviceSettingsDlg::SettingsTab);
    check(dlg.currentTab() == DeviceSettingsDlg::SettingsTab && dlg.settings()->isVisibleTo(&dlg), "the Settings tab");
    check(!dlg.hasData(), "UT61E: no specifications, no Data tab");
    dlg.showTab(DeviceSettingsDlg::DataTab);
    check(dlg.currentTab() == DeviceSettingsDlg::InfoTab, "no Data tab: Info instead");
  }
  {
    // Data: the manual's specifications (dmm-tools), also for a twin and the
    // Bluetooth adapter's entry
    DeviceSettingsDlg dlg("Bench");
    dlg.load({ { "DMM/model", "Uni-Trend UT803" }, { "Port settings/device", "SERIAL /dev/ttyUSB0" } });
    check(dlg.hasData(), "UT803: a Data tab");
    dlg.showTab(DeviceSettingsDlg::DataTab);
    const QString data = dlg.findChild<QTextBrowser *>("ui_deviceData")->toPlainText();
    check(dlg.currentTab() == DeviceSettingsDlg::DataTab && data.contains("DC Voltage") && data.contains("600mV")
            && data.contains("dmm-tools"), "UT803 data: " + data.left(200));
    check(DeviceSpecs::find("ANENG", "V05B *").value("device") == "zt5b"
            && DeviceSpecs::find("Uni-Trend", "UT61E+ (UT-D07B Bluetooth) *").value("device") == "ut61eplus"
            && DeviceSpecs::find("Uni-Trend", "UT61E").isEmpty(), "specifications by model, twin and adapter");
    const QJsonObject hp = DeviceSpecs::find("AOPUTTRIVER", "AP-570C-APP *");
    check(hp.value("device") == "hp570c-app" && DeviceSpecs::html(hp).contains("1000A")
            && DeviceSpecs::html(hp).contains("FCC ID") && !DeviceSpecs::html(hp).contains("dmm-tools"),
          "HP-570C-APP: from its manual, not dmm-tools");
    check(DeviceSpecs::html(DeviceSpecs::find("CMT", "3503 *")).contains("Cosinus"), "CMT 3503: from its data sheet");
    check(DeviceSpecs::html(DeviceSpecs::find("Uni-Trend", "UT363BT")).contains("0~30 m/s")
            && DeviceSpecs::html(DeviceSpecs::find("Uni-Trend", "UT383BT")).contains("0~9999 Lux"),
          "UT363BT, UT383BT: from their manuals");
    check(DeviceSpecs::html(DeviceSpecs::find("OWON", "CM2100B")).contains("100.0A"), "CM2100B: from its spec sheet");
    // Connect at the bottom left: only once set up, greyed while connected
    check(dlg.connectButton()->isHidden(), "no Connect without setConnect()");
    bool connected = false;
    int connects = 0;
    dlg.setConnect([&] { return connected; }, [&] { ++connects; connected = true; });
    check(!dlg.connectButton()->isHidden() && dlg.connectButton()->isEnabled(), "Connect: there, enabled");
    dlg.connectButton()->click();
    check(connects == 1 && !dlg.connectButton()->isEnabled(), "Connect: connects, then greyed");
  }

  // 7. the open devices: none bold (no active device any more), one not
  //    connected grey, a double click shows the front panel, the others
  //    closed by their menu; the one chosen for Shift+F8
  {
    QList<DeviceSidebar::OpenDevice> rows;
    rows << DeviceSidebar::OpenDevice { "w", "Bench UT61E", true }
         << DeviceSidebar::OpenDevice { "p", "Power", false }
         << DeviceSidebar::OpenDevice { "u", "PSU", true };
    rows[1].connected = false;
    sidebar.setOpenDevices(rows);
    check(sidebar.openNode()->childCount() == 3, "three open devices");
    QTreeWidgetItem *own = sidebar.openItem("w");
    QTreeWidgetItem *p = sidebar.openItem("p");
    check(own && !own->font(0).bold() && !p->font(0).bold(), "none bold");
    check(p->foreground(0).color() != own->foreground(0).color(), "one not connected greyed");
    sidebar.clearSelection();
    check(sidebar.chosenDevice().isEmpty(), "nothing chosen");
    sidebar.setCurrentItem(p);
    check(sidebar.chosenDevice() == "p", "the device chosen on the bench: " + sidebar.chosenDevice());
    sidebar.devicesNode()->setExpanded(true);
    sidebar.setCurrentItem(sidebar.deviceItem(ut61e));
    check(sidebar.chosenDevice() == ut61e, "or in My devices: " + sidebar.chosenDevice());
    // one value: a row of its own with the curve's colour, none without;
    // the device's row has no marks
    {
      auto curveAt = [&](const QString &id)
      {
        // the marks of where it is shown: the curve's stroke first
        const QImage marks = sidebar.valueItem(0, id)->data(2, Qt::UserRole + 5).value<QPixmap>().toImage();
        return marks.isNull() ? QColor(Qt::transparent) : marks.pixelColor(marks.height() / 2, marks.height() / 2);
      };
      rows[2].values = { DeviceSidebar::Value { 0, "Voltage DC (V)", "OL", QColor(Qt::magenta) } };
      rows[2].values.first().views = { "Graph" };
      sidebar.setOpenDevices(rows);
      check(sidebar.openItem("u")->childCount() == 1 && curveAt("u") == QColor(Qt::magenta)
              && sidebar.openItem("u")->data(2, Qt::UserRole + 5).isNull(),
            "one value in the graph: its colour on its own row");
      check(sidebar.valueItem(0, "u")->text(0) == "Voltage DC (V)" && sidebar.valueItem(0, "u")->text(1).isEmpty(),
            "the name with the unit, no reading");
      check(sidebar.valueItem(0, "u")->toolTip(2).contains("Graph") && !sidebar.valueItem(0, "u")->toolTip(0).contains("OL"),
            "the windows in the marks' tooltip, no reading in any");
      rows[2].values.first().color = QColor();
      sidebar.setOpenDevices(rows);
      check(curveAt("u").alpha() == 0, "not in the graph: no colour");
      // a double click on the value row shows its device's front panel
      QSignalSpy panel(&sidebar, &DeviceSidebar::frontPanelRequested);
      Q_EMIT sidebar.itemDoubleClicked(sidebar.valueItem(0, "u"), 0);
      check(panel.size() == 1 && panel.first().first().toString() == "u", "double click on a value: the front panel");
    }
    // a double click shows its front panel
    QSignalSpy panel(&sidebar, &DeviceSidebar::frontPanelRequested);
    Q_EMIT sidebar.itemDoubleClicked(sidebar.openItem("u"), 0);
    check(panel.size() == 1 && panel.first().first().toString() == "u", "double click: its front panel");
    check(own && own->text(0) == "Bench UT61E" && own->text(1).isEmpty() && p->text(1).isEmpty(),
          "the name, no reading, no state");
    check(p && !(p->flags() & Qt::ItemIsEditable) && !(own->flags() & Qt::ItemIsEditable), "not renamed here");
    check(sidebar.openNode()->flags() & Qt::ItemIsDropEnabled, "an entry may be dropped onto the open devices");

    // a change in place, the items stay
    rows[0].name = "Bench";
    sidebar.setOpenDevices(rows);
    check(sidebar.openItem("p") == p && own->text(0) == "Bench", "updated in place");
    rows.removeAt(1);
    sidebar.setOpenDevices(rows);
    check(!sidebar.openItem("p") && sidebar.openNode()->childCount() == 2, "closed one gone");
  }

  // 7b. the values below each open device: a row each with its name,
  //     the curve's colour while in the graph, the menu per value
  {
    QList<DeviceSidebar::OpenDevice> rows;
    rows << DeviceSidebar::OpenDevice { "w", "Victron SmartShunt", true, {} }
         << DeviceSidebar::OpenDevice { "u", "Bench", false, {} };
    const QList<DeviceSidebar::Value> values { { 0, "Voltage DC", "13.29 V", QColor("#3b82f6") },
                                               { 1, "Current DC", "-14.9 A", QColor() },
                                               { 2, "Value 3", "", QColor() } };
    rows[0].values = values;
    rows[1].values = values;
    sidebar.setOpenDevices(rows);
    QTreeWidgetItem *own = sidebar.openItem("w");
    check(own && own->childCount() == 3, QString("three values, got %1 rows").arg(own ? own->childCount() : -1));
    check(sidebar.openItem("u")->childCount() == 3, "the values of one beside it too");
    const ValueRef beside = sidebar.valueOf(sidebar.valueItem(2, "u"));
    check(beside == ValueRef(2, "device:u"), "a value beside it by its source: " + beside.toString());
    check(sidebar.valueOf(sidebar.valueItem(1, "w")) == ValueRef(1, "device:w"), "each by its source");
    check(sidebar.valueOf(own).source == "-", "a device row is no value");
    QTreeWidgetItem *current = sidebar.valueItem(1, "w");
    check(current && current->text(0) == "Current DC" && current->text(1).isEmpty(), "the row of value 2, no reading");
    auto marked = [](const QTreeWidgetItem *item) { return !item->data(2, Qt::UserRole + 5).isNull(); };
    check(marked(sidebar.valueItem(0, "w")) && !marked(current), "the graph's values say so");
    rows[0].values[1].color = QColor(Qt::red);
    sidebar.setOpenDevices(rows);
    check(sidebar.valueItem(1, "w") == current && marked(current), "the marks in place");
    own->setExpanded(false);
    sidebar.setOpenDevices(rows);
    check(!own->isExpanded(), "folded away it stays so");
    own->setExpanded(true);
    int menuFor = -1;
    QObject::connect(&sidebar, &DeviceSidebar::valueMenuRequested,
                     [&](const ValueRef &v, const QPoint &) { menuFor = v.id; });
    const QRect r = sidebar.visualItemRect(current);
    QContextMenuEvent menu(QContextMenuEvent::Mouse, r.center(), sidebar.viewport()->mapToGlobal(r.center()));
    QCoreApplication::sendEvent(sidebar.viewport(), &menu);
    check(menuFor == 1, QString("the menu of value 2, got %1").arg(menuFor));
    rows[0].values = values.mid(0, 1);
    sidebar.setOpenDevices(rows);
    check(own->childCount() == 1 && sidebar.valueItem(0, "w"), "one value: a row of its own");
    check(sidebar.valueOf(own).source == "-" && sidebar.valueOf(sidebar.valueItem(0, "w")) == ValueRef(0, "device:w"),
          "one value: its row is the value, not the device's");
    check(sidebar.valueItem(0, "w")->toolTip(0) == "Value 1: Voltage DC", "the number in the tooltip");
  }

  // 7c. dragged between the nodes: an entry onto the connected devices
  //     connects it, a connected one back onto My devices closes it
  {
    struct DropSidebar : DeviceSidebar
    {
      using DeviceSidebar::DeviceSidebar;
      using DeviceSidebar::dropEvent;
    };
    DropSidebar drops(&library);
    drops.setOpenDevices({ DeviceSidebar::OpenDevice { "u", "PSU", true } });
    drops.devicesNode()->setExpanded(true);
    drops.resize(400, 800);
    drops.show();
    QSignalSpy opened(&drops, &DeviceSidebar::openBesideRequested);
    QSignalSpy closed(&drops, &DeviceSidebar::closeRequested);
    auto dropOnto = [&](QTreeWidgetItem *from, QTreeWidgetItem *onto)
    {
      drops.setCurrentItem(from);
      QMimeData mime;
      QDropEvent drop(drops.visualItemRect(onto).center(), Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
      drops.dropEvent(&drop);
    };
    const QStringList order = drops.deviceIds();
    dropOnto(drops.deviceItem(ut61e), drops.openNode());
    check(opened.size() == 1 && opened.first().first().toString() == ut61e, "an entry dropped there connects");
    dropOnto(drops.openItem("u"), drops.devicesNode());
    check(closed.size() == 1 && closed.first().first().toString() == "u", "a connected one dropped back closes");
    check(drops.deviceIds() == order && drops.openItem("u"), "the items stay where they are");
  }

  // 8. the services: port or off, the address to copy, settings by a double click
  {
    DeviceSidebar::Service scpi { "scpi", "SCPI", "5025", "localhost:5025", "tip", true, true };
    DeviceSidebar::Service hislip { "hislip", "HiSLIP", "off", "", "off tip", false, false };
    sidebar.setServices({ scpi, hislip });
    QTreeWidgetItem *node = sidebar.servicesNode();
    check(node->childCount() == 2, "two services");
    QTreeWidgetItem *s = node->child(0);
    check(s->text(0) == "SCPI" && s->text(2) == "5025" && s->data(1, Qt::UserRole).toString() == "localhost:5025",
          "SCPI row: " + s->text(2));
    check(node->child(1)->text(2) == "off", "HiSLIP off");
    check(!(s->flags() & Qt::ItemIsEditable) && !(node->flags() & Qt::ItemIsDropEnabled), "not renamed, no drops");
    int settings = 0;
    QObject::connect(&sidebar, &DeviceSidebar::serviceSettingsRequested, [&] { ++settings; });
    Q_EMIT sidebar.itemDoubleClicked(node->child(1), 0);
    check(settings == 1, "double click: settings");
    hislip.running = true;
    hislip.value = "4880";
    sidebar.setServices({ scpi, hislip });
    check(node->child(0) == s && node->child(1)->text(2) == "4880", "updated in place");
    sidebar.setServices({ scpi });
    check(node->childCount() == 1, "one less");
  }

  // 9. the workspaces used last: on top, folded, hidden without any; a
  // double click or Enter loads one, not a gone one
  {
    QTreeWidgetItem *node = sidebar.workspacesNode();
    // on top after the open project's node, which hides without one
    check(sidebar.indexOfTopLevelItem(node) == 1 && sidebar.indexOfTopLevelItem(sidebar.projectNode()) == 0
            && sidebar.projectNode()->isHidden(),
          "Workspaces on top after the project, before My devices");
    check(node->isHidden(), "hidden without workspaces");
    QFile file(QDir(dir.path()).filePath("Bench.qtdmm-workspace"));
    check(file.open(QIODevice::WriteOnly), "a workspace file");
    file.close();
    const QString gone = QDir(dir.path()).filePath("gone.qtdmm-workspace");
    sidebar.setWorkspaces({ { file.fileName(), {}, {} }, { gone, {}, {} } });
    check(!node->isHidden() && !node->isExpanded(), "shown, folded");
    check(node->childCount() == 2 && node->child(0)->text(0) == "Bench", "the name without the suffix");
    check(node->child(0)->toolTip(0) == QDir::toNativeSeparators(file.fileName()), "the path in the tooltip");
    sidebar.setWorkspaces({ { file.fileName(), "Bench test", "<b>Bench test</b>" }, { gone, {}, {} } });
    check(node->child(0)->text(0) == "Bench test" && node->child(0)->toolTip(0) == "<b>Bench test</b>",
          "a workspace's name and its tooltip");
    sidebar.setCurrentWorkspace("Bench test", "<b>Bench test</b>");
    check(sidebar.openNode()->text(0) == "Bench · Bench test" && node->text(1).isEmpty()
            && sidebar.openNode()->toolTip(0) == "<b>Bench test</b>",
          "the workspace in use after Bench, not at the recent ones: " + sidebar.openNode()->text(0));
    sidebar.setCurrentWorkspace(QString());
    check(sidebar.openNode()->text(0) == "Bench", "none in use: Bench alone");
    check(node->child(1)->toolTip(0).contains("not found"), "a gone one says so");
    check(!(node->flags() & Qt::ItemIsDropEnabled) && !(node->child(0)->flags() & Qt::ItemIsDragEnabled),
          "no drag and drop");
    QStringList loaded;
    QObject::connect(&sidebar, &DeviceSidebar::workspaceRequested, [&](const QString &f) { loaded << f; });
    Q_EMIT sidebar.itemDoubleClicked(node->child(1), 0);
    check(loaded.isEmpty(), "a gone one loads no more");
    Q_EMIT sidebar.itemDoubleClicked(node->child(0), 0);
    check(loaded == QStringList({ file.fileName() }), "double click loads");
    node->setExpanded(true);
    sidebar.setCurrentItem(node->child(0));
    QTest::keyClick(&sidebar, Qt::Key_Return);
    check(loaded.size() == 2, "Enter loads");
    // the open project: its benches, the one in use in bold, "+ New bench..."
    QTreeWidgetItem *project = sidebar.projectNode();
    sidebar.setProject("Ripple", { { file.fileName(), "Bench test", {} }, { gone, {}, {} } }, file.fileName());
    check(!project->isHidden() && project->text(0).contains("Ripple") && project->childCount() == 3
            && project->child(0)->font(0).bold() && !project->child(1)->font(0).bold(),
          "the project's benches, the one in use bold, then New bench: " + project->text(0));
    int newBench = 0;
    QObject::connect(&sidebar, &DeviceSidebar::newBenchRequested, [&] { ++newBench; });
    Q_EMIT sidebar.itemClicked(project->child(2), 0);
    check(newBench == 1, "+ New bench... asks for a new bench");
    const qsizetype before = loaded.size();
    Q_EMIT sidebar.itemDoubleClicked(project->child(0), 0);
    Q_EMIT sidebar.itemDoubleClicked(project->child(2), 0);
    check(loaded.size() == before + 1 && loaded.last() == file.fileName(),
          "a double click loads a bench of the project, not New bench");
    loaded.removeLast();
    sidebar.setProject(QString(), {}, QString());
    check(project->isHidden() && project->childCount() == 0, "no project: no node");
    Q_EMIT sidebar.itemClicked(node->child(0), 0);
    check(loaded.size() == 2, "a single click does nothing");
    sidebar.setWorkspaces({});
    check(node->isHidden() && node->childCount() == 0, "hidden again");
  }

  if (failed)
    qWarning() << failed << "check(s) failed";
  else
    qInfo() << "all checks passed";
  return failed ? 1 : 0;
}
