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
  sidebar.setOpenDevices({ DeviceSidebar::OpenDevice { tfa, "TFA", "800 ppm", true } });
  check(sidebar.deviceItem(tfa)->font(0).bold() && !bench->font(0).bold(), "the one connected is bold");
  {
    // the dot: green while readings come in, grey when they stop
    const QImage on = sidebar.deviceItem(tfa)->icon(0).pixmap(16).toImage();
    sidebar.setOpenDevices({ DeviceSidebar::OpenDevice { tfa, "TFA", "800 ppm", false } });
    const QImage off = sidebar.deviceItem(tfa)->icon(0).pixmap(16).toImage();
    check(on != plain && on != off && off != plain, "the dot: green, grey, none when not connected");
    sidebar.setOpenDevices({ DeviceSidebar::OpenDevice { tfa, "TFA", "800 ppm", true } });
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

  // 7. the open devices: the active one bold, one not connected grey, a double
  //    click shows the front panel, the others closed by their menu
  {
    QList<DeviceSidebar::OpenDevice> rows;
    rows << DeviceSidebar::OpenDevice { "w", "Bench UT61E", "12.01 V DC", true }
         << DeviceSidebar::OpenDevice { "p", "Power", "", false }
         << DeviceSidebar::OpenDevice { "u", "PSU", "OL", true };
    rows[0].current = true;
    rows[1].connected = false;
    sidebar.setOpenDevices(rows);
    check(sidebar.openNode()->childCount() == 3, "three open devices");
    QTreeWidgetItem *own = sidebar.openItem("w");
    QTreeWidgetItem *p = sidebar.openItem("p");
    check(own && own->font(0).bold() && !p->font(0).bold(), "the active one is bold");
    check(p->foreground(0).color() != own->foreground(0).color(), "one not connected greyed");
    // another one active: it is bold
    rows[0].current = false;
    rows[2].current = true;
    sidebar.setOpenDevices(rows);
    check(!own->font(0).bold() && sidebar.openItem("u")->font(0).bold(), "another active one bold");
    // one value in the graph: its curve's colour beside the device's dot, none without
    {
      auto curveAt = [&](const QString &id)
      {
        const QImage icon = sidebar.openItem(id)->icon(0).pixmap(sidebar.iconSize()).toImage();
        return icon.pixelColor(icon.width() - icon.height() / 2 - 2, icon.height() / 2);
      };
      rows[2].values = { DeviceSidebar::Value { 0, "Voltage DC (V)", "OL", QColor(Qt::magenta) } };
      sidebar.setOpenDevices(rows);
      check(curveAt("u") == QColor(Qt::magenta) && sidebar.openItem("u")->toolTip(0).contains("In the graph")
              && sidebar.openItem("u")->childCount() == 0,
            "one value in the graph: its colour on the device's row");
      rows[2].values.first().color = QColor();
      sidebar.setOpenDevices(rows);
      check(curveAt("u").alpha() == 0, "not in the graph: no colour");
    }
    // a double click shows its front panel
    QSignalSpy panel(&sidebar, &DeviceSidebar::frontPanelRequested);
    Q_EMIT sidebar.itemDoubleClicked(sidebar.openItem("u"), 0);
    check(panel.size() == 1 && panel.first().first().toString() == "u", "double click: its front panel");
    check(own && own->text(0) == "Bench UT61E" && own->text(1) == "12.01 V DC", "name and reading");
    check(p && !(p->flags() & Qt::ItemIsEditable) && !(own->flags() & Qt::ItemIsEditable), "not renamed here");
    check(sidebar.openNode()->flags() & Qt::ItemIsDropEnabled, "an entry may be dropped onto the open devices");

    // the values change in place, the items stay
    rows[0].value = "12.02 V DC";
    sidebar.setOpenDevices(rows);
    check(sidebar.openItem("p") == p && own->text(1) == "12.02 V DC", "updated in place");
    rows.removeAt(1);
    sidebar.setOpenDevices(rows);
    check(!sidebar.openItem("p") && sidebar.openNode()->childCount() == 2, "closed one gone");
  }

  // 7b. the values below each open device: a row each with the reading,
  //     the curve's colour while in the graph, the menu per value
  {
    QList<DeviceSidebar::OpenDevice> rows;
    rows << DeviceSidebar::OpenDevice { "w", "Victron SmartShunt", "13.29 V DC", true, {} }
         << DeviceSidebar::OpenDevice { "u", "Bench", "OL", false, {} };
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
    check(sidebar.valueOf(own).source == "-", "a device row with value rows is no value");
    QTreeWidgetItem *current = sidebar.valueItem(1, "w");
    check(current && current->text(0) == "2  Current DC" && current->text(1) == "-14.9 A", "the row of value 2");
    check(sidebar.valueItem(0, "w")->toolTip(0).contains("graph") && !current->toolTip(0).contains("graph"),
          "the graph's values say so");
    rows[0].values[1].text = "-15.0 A";
    sidebar.setOpenDevices(rows);
    check(sidebar.valueItem(1, "w") == current && current->text(1) == "-15.0 A", "the reading in place");
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
    check(own->childCount() == 0 && !sidebar.valueItem(0, "w"), "one value: no rows");
    check(sidebar.valueOf(own) == ValueRef(0, "device:w"), "one value: the device row is its main value");
    // where the row is short: unit and coupling, the full name in the tooltip
    rows[0].values = values;
    rows[0].values[1].shortName = "A DC";
    sidebar.setOpenDevices(rows);
    check(sidebar.valueItem(1, "w")->text(0) == "2  A DC" && sidebar.valueItem(1, "w")->toolTip(0) == "Current DC",
          "the short name in the row, the full one in the tooltip");
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
    drops.setOpenDevices({ DeviceSidebar::OpenDevice { "u", "PSU", "OL", true } });
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
    check(s->text(0) == "SCPI" && s->text(1) == "5025" && s->data(1, Qt::UserRole).toString() == "localhost:5025",
          "SCPI row: " + s->text(1));
    check(node->child(1)->text(1) == "off", "HiSLIP off");
    check(!(s->flags() & Qt::ItemIsEditable) && !(node->flags() & Qt::ItemIsDropEnabled), "not renamed, no drops");
    int settings = 0;
    QObject::connect(&sidebar, &DeviceSidebar::serviceSettingsRequested, [&] { ++settings; });
    Q_EMIT sidebar.itemDoubleClicked(node->child(1), 0);
    check(settings == 1, "double click: settings");
    hislip.running = true;
    hislip.value = "4880";
    sidebar.setServices({ scpi, hislip });
    check(node->child(0) == s && node->child(1)->text(1) == "4880", "updated in place");
    sidebar.setServices({ scpi });
    check(node->childCount() == 1, "one less");
  }

  // 9. the workspaces used last: on top, folded, hidden without any; a
  // double click or Enter loads one, not a gone one
  {
    QTreeWidgetItem *node = sidebar.workspacesNode();
    check(sidebar.indexOfTopLevelItem(node) == 0, "Workspaces on top, before My devices");
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
    sidebar.setCurrentWorkspace("Bench test");
    check(node->text(1) == "Bench test" && !node->isFirstColumnSpanned(), "the workspace in use at the right of the node");
    sidebar.setCurrentWorkspace(QString());
    check(node->text(1).isEmpty() && node->isFirstColumnSpanned(), "none in use: the node across");
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
