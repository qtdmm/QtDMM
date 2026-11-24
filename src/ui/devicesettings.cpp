//======================================================================
// File:		devicesettings.cpp
// Author:	Matthias Toussaint
// Created:	Sat Oct 19 15:26:51 CEST 2002
//----------------------------------------------------------------------
// This file is part of QtDMM.
//
// QtDMM is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// QtDMM is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with QtDMM.  If not, see <http://www.gnu.org/licenses/>.
//----------------------------------------------------------------------
// Copyright (c) 2002 Matthias Toussaint
// SPDX-License-Identifier: GPL-3.0-or-later
//======================================================================

#include <QtGui>
#include <QtWidgets>
#include <QSerialPortInfo>
#include <iostream>
#include <algorithm>
#include <vector>

#include "ui/devicesettings.h"
#include "device/devicetype.h"
#include "device/discovery/discovery.h"
#include "device/metersetup.h"
#include "device/protocols.h"
#include "device/victronble.h"
#ifdef QTDMM_WITH_BLE
#include "device/transports/ble.h"
#include "device/transports/blegatt.h"
#endif
#include "core/sampletypes.h"
#include "core/siprefix.h"
#include "decoders.h"

DeviceSettings::DeviceSettings(QWidget *parent) : QWidget(parent)
{
  setupUi(this);
  m_portlist = new QStringListModel(this);
  port->setModel(m_portlist);

  setupComboBoxModel();
  // the rates of microcontroller boards above the meters' 19200
  for (const char *rate : { "38400", "57600", "74880", "115200", "230400", "250000", "460800", "500000", "921600",
                            "1000000", "2000000" })
    baudRate->addItem(QString::fromLatin1(rate));

  message2->hide();
  ui_bleGroup->hide();
  ui_sigrokGroup->hide();
  connect(ui_sigrokDriver, &QLineEdit::textChanged, this, &DeviceSettings::updateSigrokHint);
  connect(ui_bleKey, &QLineEdit::textChanged, this, &DeviceSettings::updateBleHint);
  connect(ui_bleAddress, &QComboBox::currentTextChanged, this, &DeviceSettings::updateBleHint);
  // a microcontroller's values: the unit of each, from its label or chosen
  {
    m_unitsGroup = new QGroupBox(tr("Values"), this);
    m_unitsGroup->setObjectName("ui_unitsGroup");
    auto *grid = new QGridLayout(m_unitsGroup);
    for (int i = 0; i < kMaxValues; ++i)
    {
      m_unitLabels[size_t(i)] = new QLabel(m_unitsGroup);
      auto *combo = new QComboBox(m_unitsGroup);
      combo->setObjectName(QString("ui_unit%1").arg(i + 1));
      combo->addItem(tr("Automatic"), QString());
      for (const QString &unit : DecoderTextLines::units())
        combo->addItem(unit == QLatin1String("Ohm") ? QStringLiteral("\u03A9") : unit, unit);
      combo->addItem(tr("No unit"), QString::fromLatin1(DecoderTextLines::kNoUnit));
      combo->setToolTip(tr("The unit of value %1. Automatic: the unit its label names (\"V:\", \"mA:\"), else none. "
                           "A unit chosen here takes the number as sent.").arg(i + 1));
      m_unitLabels[size_t(i)]->setBuddy(combo);
      m_unitCombos[size_t(i)] = combo;
      grid->addWidget(m_unitLabels[size_t(i)], i, 0);
      grid->addWidget(combo, i, 1);
    }
    grid->setColumnStretch(2, 1);
    verticalLayout_2->insertWidget(verticalLayout_2->indexOf(ui_advancedBox) + 1, m_unitsGroup);
    m_unitsGroup->hide();
    setValueNames({});
  }
  // no frames: a section title above each group, shown with it
  for (QGroupBox *group : findChildren<QGroupBox *>())
    sectionTitle(group);
  showPortParameters(false);

  // any edit may complete the fields or break them (isComplete())
  for (QLineEdit *e : findChildren<QLineEdit *>())
    connect(e, &QLineEdit::textChanged, this, &DeviceSettings::changed);
  for (QComboBox *c : findChildren<QComboBox *>())
    connect(c, &QComboBox::currentTextChanged, this, &DeviceSettings::changed);

  m_path = QDir::currentPath();
}

QStringList DeviceSettings::models() const
{
  QStringList names;
  for (const auto &cfg : m_models)
    names << cfg.name;
  return names;
}

void DeviceSettings::setDescriptionFilesVisible(bool visible)
{
  for (QWidget *w : std::initializer_list<QWidget *>{ ui_hint, ui_filename, ui_load, ui_save })
    w->setVisible(visible);
}

void DeviceSettings::setPortVisible(bool visible)
{
  m_portVisible = visible;
  updateConnectionMode();
}

bool DeviceSettings::isComplete() const
{
  const bool manual = ui_vendor->currentIndex() == 0;
  if (manual)
    return m_manualAllowed && !port->currentText().trimmed().isEmpty();
  const int modelIdx = ui_model->currentIndex();
  if (modelIdx < 0 || modelIdx >= static_cast<int>(m_currentVendorModels.size()))
    return false;
  if (isSigrokMeter())
    return !ui_sigrokDriver->text().trimmed().isEmpty();
  const QString address = ui_bleAddress->currentText().section(' ', 0, 0).trimmed();
  if (isGatt() || isBtHome())
    return !address.isEmpty();
  if (isBluetooth())
    return !address.isEmpty() && VictronBle::keyFromHex(ui_bleKey->text()).size() == 16;
  return !port->currentText().trimmed().isEmpty();
}


DeviceSettings::~DeviceSettings()
{
}

void DeviceSettings::selectFormat(FrameFormat::DataFormat df)
{
  const int idx = protocolCombo->findData(int(df));
  protocolCombo->setCurrentIndex(idx >= 0 ? idx : 0);
}

void DeviceSettings::setupComboBoxModel()
{
  // one combo entry per row of the protocol table, the enum value as data
  protocolCombo->clear();
  for (const ProtocolInfo &p : protocols())
    protocolCombo->addItem(QCoreApplication::translate("Protocols", p.description), int(p.id));

  fillModels();

  connect(ui_model->lineEdit(), &QLineEdit::editingFinished, this, [this]()
  {
    QString text = ui_model->currentText();

    // already the current selection within the current vendor's list?
    for (int i = 0; i < ui_model->count(); ++i)
    {
      if (ui_model->itemText(i).compare(text, Qt::CaseInsensitive) == 0)
      {
        ui_model->setCurrentIndex(i);
        on_ui_model_activated(i);
        return;
      }
    }

    // otherwise search the full device list and switch vendor if needed
    for (size_t i = 0; i < m_models.size(); ++i)
    {
      if (m_models[i].name.compare(text, Qt::CaseInsensitive) == 0)
      {
        int vendorIdx = ui_vendor->findText(m_models[i].vendor);
        if (vendorIdx >= 0)
        {
          ui_vendor->setCurrentIndex(vendorIdx);
          populateModelsForVendor(m_models[i].vendor);
          int modelIdx = ui_model->findText(m_models[i].name);
          ui_model->setCurrentIndex(modelIdx);
          on_ui_model_activated(modelIdx);
        }
        return;
      }
    }

    ui_model->setCurrentIndex(-1);
  });
}

// The models offered: all registered ones, or those the filter lets through
// (the assistant offers only the models of the chosen connection).
void DeviceSettings::fillModels()
{
  ui_vendor->clear();
  ui_vendor->insertItem(-1, tr("Manual settings"));
  ui_vendor->addItem(tr("All vendors"));
  // index 0 means manual settings throughout; without them it stays, disabled
  if (auto *model = qobject_cast<QStandardItemModel *>(ui_vendor->model()))
    model->item(0)->setEnabled(m_manualAllowed);

  std::vector<DmmDecoder::DMMInfo> configs = DmmDecoder::getDeviceConfigurations();

  // Sortieren nach dem Namen
  std::sort(configs.begin(), configs.end(), [](const auto& a, const auto& b) {
    return a.name < b.name;
  });

  m_models.clear();
  QStringList vendors;
  for (const auto& cfg : configs) {
    if (m_filter && !m_filter(cfg))
      continue;
    m_models.push_back(cfg);
    if (!vendors.contains(cfg.vendor))
      vendors.append(cfg.vendor);
  }

  vendors.sort(Qt::CaseInsensitive);
  ui_vendor->addItems(vendors);

  // The completer searches the full, unfiltered device list (all vendors),
  // so typing a known model name still finds it directly - on selection the
  // editingFinished handler switches the vendor combo to match.
  QStringList allNames;
  for (const auto& cfg : m_models)
    allNames.append(cfg.name);

  if (!m_completerNames)
  {
    m_completerNames = new QStringListModel(this);
    QCompleter *completer = new QCompleter(m_completerNames, this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    ui_model->setCompleter(completer);
  }
  m_completerNames->setStringList(allNames);
}

void DeviceSettings::setModelFilter(std::function<bool(const DmmDecoder::DMMInfo &)> filter, bool manual)
{
  m_filter = std::move(filter);
  m_manualAllowed = manual;
  fillModels();
}

void DeviceSettings::populateModelsForVendor(const QString &vendor)
{
  ui_model->clear();

  m_currentVendorModels.clear();
  for (const auto& cfg : m_models)
    if (cfg.vendor == vendor)
      m_currentVendorModels.push_back(cfg);

  std::sort(m_currentVendorModels.begin(), m_currentVendorModels.end(), [](const auto& a, const auto& b) {
    return a.model < b.model;
  });

  for (const auto& cfg : m_currentVendorModels)
    addModelItem(cfg);
}

void DeviceSettings::addModelItem(const DmmDecoder::DMMInfo &cfg)
{
  ui_model->addItem(cfg.name);
  ui_model->setItemData(ui_model->count() - 1, DeviceType::displayName(DeviceType::of(cfg)), Qt::ToolTipRole);
}

void DeviceSettings::populateAllModels()
{
  ui_model->clear();

  // m_models is already sorted by name (vendor+model) in setupComboBoxModel().
  m_currentVendorModels = m_models;

  for (const auto& cfg : m_currentVendorModels)
    addModelItem(cfg);
}

void DeviceSettings::on_ui_vendor_activated(int id)
{
  if (id == 0)
  {
    ui_model->clear();
    m_currentVendorModels.clear();
    enterManualMode();
  }
  else if (id == 1)
  {
    populateAllModels();
    if (!m_currentVendorModels.empty())
    {
      ui_model->setCurrentIndex(0);
      on_ui_model_activated(0);
    }
  }
  else
  {
    populateModelsForVendor(ui_vendor->itemText(id));
    if (!m_currentVendorModels.empty())
    {
      ui_model->setCurrentIndex(0);
      on_ui_model_activated(0);
    }
  }
}


void DeviceSettings::setPorts(const QStringList &ports)
{
  const QString current = port->currentText();
  m_portlist->setStringList(ports);
  if (!current.isEmpty())
    port->setCurrentText(current);
}

void DeviceSettings::load(const QVariantMap &keys)
{
  m_loaded = keys;
  auto cfgString = [&keys](const QString &key, const QString &def = QString()) { return keys.value(key, def).toString(); };
  auto cfgInt    = [&keys](const QString &key, int def = 0) { return keys.value(key, def).toInt(); };
  auto cfgBool   = [&keys](const QString &key, bool def = false) { return keys.value(key, def).toBool(); };

  // a port that is not there now (unplugged, a /dev/serial/by-id name
  // from "My devices") stays the choice
  QString device = cfgString("Port settings/device");
  if (device.section(' ', 0, 0).startsWith("ble", Qt::CaseInsensitive))
    device.clear();   // a Bluetooth meter: no serial port to keep
  QStringList list = m_portlist->stringList();
  if (!device.isEmpty() && !list.contains(device))
  {
    list.append(device);
    m_portlist->setStringList(list);
  }
  if (!device.isEmpty())
    port->setCurrentText(device);
  ui_sigrokConn->setCurrentText(cfgString("Port settings/sigrok-conn"));
  ui_sigrokOptions->setText   (cfgString("Port settings/sigrok-options"));
  ui_bleAddress->setCurrentText(cfgString("Port settings/ble-address"));
  ui_bleKey->setText          (cfgString("Port settings/ble-key"));
  // filled again from the loaded choice: widgets still filled for the same
  // model kept the choice of before (a Victron always showed the voltage
  // after a takeover, a typed sigrok driver went back to the model's). The
  // Victron combos stay empty until the model below fills them: filled now,
  // for the model of before, they mapped the loaded choice onto its fields
  ui_sigrokDriver->setProperty("model", QVariant());
  ui_bleMain->setProperty("readoutType", QVariant());
  ui_bleMain->clear();
  ui_bleSecond->clear();
  ui_bleThird->clear();
  ui_bleFourth->clear();
  for (int i = 0; i < kMaxValues; ++i)
  {
    const int idx = m_unitCombos[size_t(i)]->findData(cfgString(QString("DMM/unit-%1").arg(i + 1)).trimmed());
    m_unitCombos[size_t(i)]->setCurrentIndex(qMax(0, idx));
  }
  baudRate->setCurrentText    (cfgString("Port settings/baud"));
  bitsCombo->setCurrentText   (cfgString("Port settings/bits", "7"));
  stopBitsCombo->setCurrentText(cfgString("Port settings/stop-bits", "1"));
  parityCombo->setCurrentIndex(cfgInt("Port settings/parity"));   // stored as index by applySLOT()
  selectDisplay(cfgString("DMM/display", "4000"));
  ui_externalSetup->setChecked(cfgBool("DMM/external-setup", false));

  uirts->setChecked(cfgBool("DMM/rts", true));
  uidtr->setChecked(cfgBool("DMM/dtr", false));

  selectFormat(MeterSetup::formatFromSetting(cfgString("DMM/data-format", "Metex14")));
  ui_framesPerReading->setValue(cfgInt("DMM/number-of-values", 1));

  QString model = cfgString("DMM/model");

  ui_vendor->setCurrentIndex(0);
  ui_model->clear();
  m_currentVendorModels.clear();

  for (const auto& cfg : m_models)
  {
    if (DmmDecoder::sameModel(model, cfg.name))
    {
      int vendorIdx = ui_vendor->findText(cfg.vendor);
      if (vendorIdx >= 0)
      {
        ui_vendor->setCurrentIndex(vendorIdx);
        populateModelsForVendor(cfg.vendor);
        int modelIdx = ui_model->findText(cfg.name);
        ui_model->setCurrentIndex(modelIdx);
      }
      break;
    }
  }

  if (ui_vendor->currentIndex() == 0)
    enterManualMode();
  else
    on_ui_model_activated(ui_model->currentIndex());
  // a known model brings its port parameters and protocol: nothing to set,
  // but for a microcontroller the baud rate saved
  if (m_dmmInfo.baudChoosable() && cfgInt("Port settings/baud") > 0)
    baudRate->setCurrentText(cfgString("Port settings/baud"));
  showPortParameters(ui_vendor->currentIndex() == 0 || m_dmmInfo.baudChoosable());
}

void DeviceSettings::setValueNames(const QStringList &names)
{
  for (int i = 0; i < kMaxValues; ++i)
  {
    const QString name = names.value(i);
    m_unitLabels[size_t(i)]->setText(name.isEmpty() ? tr("Value %1:").arg(i + 1) : tr("Value %1 (%2):").arg(i + 1).arg(name));
  }
}

void DeviceSettings::showPortParameters(bool show)
{
  m_portParameters = show;
  ui_advancedBox->setVisible(show);
}

bool DeviceSettings::portParametersShown() const
{
  return m_portParameters;
}

namespace
{
/// Shows and hides a group's section title with the group.
class SectionFollower : public QObject
{
public:
  SectionFollower(QWidget *group, QLabel *title) : QObject(group), m_title(title) {}
  bool eventFilter(QObject *, QEvent *event) override
  {
    if (event->type() == QEvent::ShowToParent)
      m_title->show();
    else if (event->type() == QEvent::HideToParent)
      m_title->hide();
    return false;
  }

private:
  QLabel *m_title;
};
}

void DeviceSettings::sectionTitle(QGroupBox *group)
{
  // the frame goes; the title becomes a bold label above, as on the pages
  // of the settings dialog
  const QString title = group->title();
  group->setTitle(QString());
  group->setFlat(true);
  group->setStyleSheet(QStringLiteral("QGroupBox { border: none; margin: 0; padding: 0; }"));
  auto *box = qobject_cast<QBoxLayout *>(group->parentWidget() ? group->parentWidget()->layout() : nullptr);
  if (title.isEmpty() || !box)
    return;
  auto *label = new QLabel(title, group->parentWidget());
  label->setObjectName(group->objectName() + "_title");
  QFont f = label->font();
  f.setBold(true);
  label->setFont(f);
  label->setContentsMargins(0, label->fontMetrics().height() / 2, 0, 0);
  box->insertWidget(box->indexOf(group), label);
  label->setVisible(!group->isHidden());
  group->installEventFilter(new SectionFollower(group, label));
}

QVariantMap DeviceSettings::keys() const
{
  QVariantMap keys;
  // a Bluetooth meter has no port: the port field still held the one of the
  // meter before, and My devices showed it (the key stays in its own entry)
  const QString address = ui_bleAddress->currentText().section(' ', 0, 0).trimmed();
  keys.insert("Port settings/sigrok-conn", ui_sigrokConn->currentText().trimmed());
  keys.insert("Port settings/sigrok-options", ui_sigrokOptions->text().trimmed());
  keys.insert("Port settings/ble-address", ui_bleAddress->currentText().trimmed());
  // only a Victron device has a key: another meter would carry it on into
  // its own entry of My devices
  keys.insert("Port settings/ble-key", isBluetooth() && !isGatt() && !isBtHome() ? ui_bleKey->text().trimmed() : QString());
  keys.insert("Port settings/ble-main", ui_bleMain->currentData().toString());
  keys.insert("Port settings/ble-second", ui_bleSecond->currentData().toString());
  keys.insert("Port settings/ble-third", ui_bleThird->currentData().toString());
  keys.insert("Port settings/ble-fourth", ui_bleFourth->currentData().toString());
  keys.insert("Port settings/baud", baudRate->currentText());
  // only a microcontroller's, and only a chosen one: the other meters'
  // entries stay as they were (an extra key would read as a change)
  if (ui_vendor->currentIndex() != 0 && m_dmmInfo.unitsChoosable())
    for (int i = 0; i < kMaxValues; ++i)
      if (const QString unit = m_unitCombos[size_t(i)]->currentData().toString(); !unit.isEmpty())
        keys.insert(QString("DMM/unit-%1").arg(i + 1), unit);
  keys.insert("Port settings/bits", bitsCombo->currentText());
  keys.insert("Port settings/stop-bits", stopBitsCombo->currentText());
  keys.insert("Port settings/parity", parityCombo->currentIndex());

  keys.insert("DMM/display", displayCombo->currentText());
  keys.insert("DMM/external-setup", ui_externalSetup->isChecked());

  keys.insert("DMM/data-format", FrameFormat::toString(format()));
  keys.insert("DMM/number-of-values", ui_framesPerReading->value());
  const int modelIdx = ui_model->currentIndex();
  const bool haveModel = ui_vendor->currentIndex() != 0
                         && modelIdx >= 0
                         && modelIdx < static_cast<int>(m_currentVendorModels.size());
  keys.insert("DMM/model", haveModel ? m_currentVendorModels[modelIdx].name : "Manual");

  keys.insert("DMM/rts", uirts->isChecked());
  keys.insert("DMM/dtr", uidtr->isChecked());

  // a Bluetooth meter its address, a sigrok meter its driver (typed or the
  // model's), not the port of the meter before
  if (isGatt())
    keys.insert("Port settings/device", "blegatt " + address);
  else if (isBluetooth())
    keys.insert("Port settings/device", "ble " + address);
  else if (isSigrokMeter())
  {
    keys.insert("Port settings/device", "SIGROK " + ui_sigrokDriver->text().trimmed());
    keys.insert("Port settings/device", MeterSetup::fromKeys(keys).device);
  }
  else
    keys.insert("Port settings/device", port->currentText());
  return keys;
}

void DeviceSettings::on_ui_externalSetup_toggled()
{
  if (ui_vendor->currentIndex() == 0)
  {
    baudRate->setDisabled(ui_externalSetup->isChecked());
    bitsCombo->setDisabled(ui_externalSetup->isChecked());
    stopBitsCombo->setDisabled(ui_externalSetup->isChecked());
    parityCombo->setDisabled(ui_externalSetup->isChecked());
  }
}

bool DeviceSettings::isSigrokMeter() const
{
  return ui_vendor->currentIndex() != 0 && MeterSetup::isSigrok(m_dmmInfo);
}

bool DeviceSettings::isBluetooth() const
{
  return ui_vendor->currentIndex() != 0 && MeterSetup::isBluetooth(m_dmmInfo);
}

bool DeviceSettings::isGatt() const
{
  return ui_vendor->currentIndex() != 0 && MeterSetup::isGatt(m_dmmInfo);
}

bool DeviceSettings::isBtHome() const
{
  return ui_vendor->currentIndex() != 0 && m_dmmInfo.protocol == FrameFormat::BTHome;
}

// What a model's way of connecting shows: the port and its line settings,
// the protocol, the Bluetooth or sigrok group, a microcontroller's units.
void DeviceSettings::updateConnectionMode()
{
  const bool ble = isBluetooth();
  const bool sigrok = isSigrokMeter();
  ButtonGroup11->setVisible(m_portVisible && !ble && !sigrok);
  ui_serialBox->setVisible(!ble && !sigrok);
  m_unitsGroup->setVisible(ui_vendor->currentIndex() != 0 && m_dmmInfo.unitsChoosable() && !ble && !sigrok);
  ui_bleGroup->setVisible(ble);
  // neither key nor a choice of values: a GATT meter, a BTHome sensor
  const bool gatt = isGatt() || isBtHome();
  for (QWidget *w : std::initializer_list<QWidget *>{ ui_bleKeyLabel, ui_bleKey, ui_bleMainLabel, ui_bleMain,
                                                      ui_bleSecondLabel, ui_bleSecond, ui_bleThirdLabel, ui_bleThird,
                                                      ui_bleFourthLabel, ui_bleFourth })
    w->setVisible(ble && !gatt);
  if (ble)
    updateBleHint();
  ui_sigrokGroup->setVisible(sigrok);
  if (sigrok)
  {
    // the model's driver, unless the user typed another one for this model
    // (then it is in the loaded port string)
    if (ui_sigrokDriver->property("model").toString() != m_dmmInfo.model)
    {
      ui_sigrokDriver->setProperty("model", m_dmmInfo.model);
      const QString stored = m_loaded.value("Port settings/device").toString();
      QString driver = m_dmmInfo.sigrokDriver;
      if (stored.section(' ', 0, 0).compare("sigrok", Qt::CaseInsensitive) == 0
          && DmmDecoder::sameModel(m_loaded.value("DMM/model").toString(), m_dmmInfo.vendor + " " + m_dmmInfo.model))
        driver = stored.section(' ', 1).section(':', 0, 0).trimmed();
      ui_sigrokDriver->setText(driver.isEmpty() ? m_dmmInfo.sigrokDriver : driver);
    }
    if (ui_sigrokConn->count() == 0)
    {
      // serial ports first; USB-TMC and LAN are typed in
      QStringList ports;
      for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts())
        ports << info.systemLocation();
      const QString current = ui_sigrokConn->currentText();
      ui_sigrokConn->addItems(ports);
      ui_sigrokConn->setCurrentText(current);
    }
    updateSigrokHint();
  }
  if (ble)
  {
    updateBleFields();
    updateBleHint();
  }
}

void DeviceSettings::enterManualMode()
{
  ui_filename->setDisabled(false);
  ui_save->setDisabled(false);
  ui_load->setDisabled(false);

  baudRate->setDisabled(false);
  ui_protocol->setDisabled(false);
  ui_baudLabel->setDisabled(false);
  ui_bitsLabel->setDisabled(false);
  ui_stopLabel->setDisabled(false);
  ui_displayLabel->setDisabled(false);
  ui_parityLabel->setDisabled(false);
  bitsCombo->setDisabled(false);
  displayCombo->setDisabled(false);
  stopBitsCombo->setDisabled(false);
  parityCombo->setDisabled(false);
  ui_framesPerReading->setDisabled(false);
  ui_externalSetup->setDisabled(false);
  uirts->setDisabled(false);
  uidtr->setDisabled(false);

  message->show();
  message2->hide();
  showPortParameters(true);   // nothing to take from a model

  m_dmmInfo.name = "custom";
  m_dmmInfo.baud = baudRate->currentText().toInt();
  m_dmmInfo.protocol = format();
  m_dmmInfo.bits =  bitsCombo->currentText().toInt();
  m_dmmInfo.stopBits = stopBitsCombo->currentText().toInt();
  m_dmmInfo.parity = parityCombo->currentIndex();
  m_dmmInfo.display = displayCombo->currentText().toInt();
  m_dmmInfo.framesPerReading = ui_framesPerReading->value();
  m_dmmInfo.externalSetup = ui_externalSetup->isChecked();
  m_dmmInfo.rts = uirts->isChecked();
  m_dmmInfo.dtr = uidtr->isChecked();

  updateConnectionMode();
}

void DeviceSettings::on_ui_model_activated(int id)
{
  if (id < 0 || id >= static_cast<int>(m_currentVendorModels.size()))
    return;

  // a model brings its port parameters and protocol: nothing to show, also
  // after manual settings had shown them
  showPortParameters(false);
  ui_filename->setDisabled(true);
  ui_save->setDisabled(true);
  ui_load->setDisabled(true);

  baudRate->setDisabled(true);
  ui_protocol->setDisabled(true);
  ui_baudLabel->setDisabled(true);
  ui_bitsLabel->setDisabled(true);
  ui_stopLabel->setDisabled(true);
  ui_displayLabel->setDisabled(true);
  ui_parityLabel->setDisabled(true);
  bitsCombo->setDisabled(true);
  displayCombo->setDisabled(true);
  stopBitsCombo->setDisabled(true);
  parityCombo->setDisabled(true);
  ui_framesPerReading->setDisabled(true);
  ui_externalSetup->setDisabled(true);
  uirts->setDisabled(true);
  uidtr->setDisabled(true);

  message->hide();
  if (ui_model->itemText(id).contains('*'))   // "Model *": settings from chip data, unconfirmed
    message2->show();
  else
    message2->hide();

  m_dmmInfo = m_currentVendorModels[id];

  baudRate->setCurrentText(QString::number(m_currentVendorModels[id].baud));
  selectFormat(m_currentVendorModels[id].protocol);
  bitsCombo->setCurrentText(QString::number(m_currentVendorModels[id].bits));
  stopBitsCombo->setCurrentText(QString::number(m_currentVendorModels[id].stopBits));
  parityCombo->setCurrentIndex(m_currentVendorModels[id].parity);
  selectDisplay(QString::number(m_currentVendorModels[id].display));
  ui_framesPerReading->setValue(m_currentVendorModels[id].framesPerReading);
  ui_externalSetup->setChecked(m_currentVendorModels[id].externalSetup);
  uirts->setChecked(m_currentVendorModels[id].rts);
  uidtr->setChecked(m_currentVendorModels[id].dtr);

  ui_filename->setText("");
  // a microcontroller: the rate its sketch sets, the rest from the model
  if (m_dmmInfo.baudChoosable())
  {
    showPortParameters(true);
    baudRate->setDisabled(false);
    ui_baudLabel->setDisabled(false);
  }
  updateConnectionMode();
}

bool DeviceSettings::rts() const
{
  return uirts->isChecked();
}

bool DeviceSettings::dtr() const
{
  return uidtr->isChecked();
}

QSerialPort::Parity DeviceSettings::parity() const
{
  QSerialPort::Parity rValue;
  switch (parityCombo->currentIndex())
  {
    case 0:
      rValue = QSerialPort::NoParity;
      break;
    case 1:
      rValue = QSerialPort::EvenParity;
      break;
    case 2:
      rValue = QSerialPort::OddParity;
      break;
    default:
      qWarning() << "Wrong parity value. Using None";
      rValue = QSerialPort::NoParity;
      break;
  }
  return rValue;
}

QSerialPort::DataBits DeviceSettings::bits() const
{
  return static_cast<QSerialPort::DataBits>(5 + bitsCombo->currentIndex());
}

QSerialPort::StopBits DeviceSettings::stopBits() const
{
  return static_cast<QSerialPort::StopBits>(1 + stopBitsCombo->currentIndex());
}

int DeviceSettings::speed() const
{
  return baudRate->currentText().toInt();
}

bool DeviceSettings::externalSetup() const
{
  return ui_externalSetup->isChecked();
}

int DeviceSettings::framesPerReading() const
{
  return ui_framesPerReading->value();
}

int DeviceSettings::valueCount() const
{
  return MeterSetup::fromKeys(keys()).valueCount;
}

FrameFormat::DataFormat DeviceSettings::format() const
{
  return static_cast<FrameFormat::DataFormat>(protocolCombo->currentData().toInt());
}

// The counts combo is not editable; a value it does not list (a new model's
// counts, or a hand-edited config) would silently leave the previous entry
// selected - so unknown values are added instead.
void DeviceSettings::selectDisplay(const QString &counts)
{
  int idx = displayCombo->findText(counts);
  if (idx < 0)
  {
    displayCombo->addItem(counts);
    idx = displayCombo->count() - 1;
  }
  displayCombo->setCurrentIndex(idx);
}

int DeviceSettings::display() const
{
  return displayCombo->currentText().toInt();
}

QString DeviceSettings::dmmName() const
{
  return ui_model->currentText();
}

// The values a Victron model offers; the selection survives a model change
// when the new model has the same field, otherwise the defaults are taken.
void DeviceSettings::updateBleFields()
{
  const quint8 type = VictronBle::readoutTypeForModel(m_dmmInfo.model);
  // the combos are empty until the model is known (defaultsSLOT runs
  // before it), so the saved choice is the fallback
  QString main = ui_bleMain->currentData().toString();
  QString second = ui_bleSecond->currentData().toString();
  QString third = ui_bleThird->currentData().toString();
  QString fourth = ui_bleFourth->currentData().toString();
  if (main.isEmpty())
  {
    main = m_loaded.value("Port settings/ble-main").toString();
    second = m_loaded.value("Port settings/ble-second").toString();
    third = m_loaded.value("Port settings/ble-third").toString();
    fourth = m_loaded.value("Port settings/ble-fourth").toString();
  }
  const QList<VictronBle::Field> fields = VictronBle::fields(type);
  if (ui_bleMain->property("readoutType").toInt() == type && !fields.isEmpty())
    return;
  ui_bleMain->setProperty("readoutType", type);
  ui_bleMain->clear();
  QComboBox *const more[] = { ui_bleSecond, ui_bleThird, ui_bleFourth };
  for (QComboBox *c : more)
  {
    c->clear();
    c->addItem(tr("none"), "-");
  }
  for (const VictronBle::Field &f : fields)
  {
    const QString label = QCoreApplication::translate("VictronBle", f.label);
    ui_bleMain->addItem(label, QString::fromLatin1(f.id));
    for (QComboBox *c : more)
      c->addItem(label, QString::fromLatin1(f.id));
  }
  ui_bleMain->setCurrentIndex(qMax(0, ui_bleMain->findData(main)));
  // "none" (index 0) is a choice too; nothing chosen yet: the model's second
  // value, no third and fourth
  const int secondIndex = ui_bleSecond->findData(second);
  ui_bleSecond->setCurrentIndex(secondIndex >= 0 ? secondIndex : qMin(2, ui_bleSecond->count() - 1));
  ui_bleThird->setCurrentIndex(qMax(0, ui_bleThird->findData(third)));
  ui_bleFourth->setCurrentIndex(qMax(0, ui_bleFourth->findData(fourth)));
}

namespace
{
// What this sigrok-cli offers, asked once per executable path: its version
// line and the drivers of --list-supported. An empty version means it did
// not run.
struct SigrokCli
{
  QString version;
  QStringList drivers;
};

const SigrokCli &sigrokCli(const QString &exe)
{
  static QMap<QString, SigrokCli> cache;
  if (cache.contains(exe))
    return cache[exe];
  SigrokCli info;
  QProcess p;
  p.start(exe, {"--version"});
  if (p.waitForFinished(3000) && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0)
  {
    info.version = QString::fromUtf8(p.readAllStandardOutput()).section('\n', 0, 0).trimmed();
    QProcess l;
    l.start(exe, {"--list-supported"});
    if (l.waitForFinished(5000))
    {
      // "  scpi-dmm             SCPI DMM" lines under "Supported hardware drivers:"
      bool inDrivers = false;
      for (const QString &line : QString::fromUtf8(l.readAllStandardOutput()).split('\n'))
      {
        if (line.startsWith("Supported hardware drivers"))
          inDrivers = true;
        else if (!line.startsWith(' '))
          inDrivers = false;
        else if (inDrivers)
          info.drivers << line.trimmed().section(' ', 0, 0);
      }
    }
  }
  cache[exe] = info;
  return cache[exe];
}
}

void DeviceSettings::updateSigrokHint()
{
  const QString exe = m_sigrokExe;
  const SigrokCli &cli = sigrokCli(exe);
  const QString driver = ui_sigrokDriver->text().trimmed();
  QString hint;
  if (cli.version.isEmpty())
    hint = tr("%1 was not found or does not run. Install sigrok-cli, or set its path in the settings, "
              "page General.").arg(exe.toHtmlEscaped());
  else if (!driver.isEmpty() && !cli.drivers.isEmpty() && !cli.drivers.contains(driver))
    hint = tr("%1 has no driver \"%2\"; see sigrok-cli --list-supported.").arg(cli.version.toHtmlEscaped(), driver.toHtmlEscaped());
  else
    hint = tr("%1 found.").arg(cli.version.toHtmlEscaped());
  ui_sigrokHint->setText(hint);
  ui_sigrokTest->setEnabled(!cli.version.isEmpty());
}

void DeviceSettings::on_ui_sigrokTest_clicked()
{
  const QString exe = m_sigrokExe;
  const QString spec = device().section(' ', 1);   // "<driver>:conn=..."
  ui_sigrokTest->setEnabled(false);
  ui_sigrokHint->setText(tr("Running %1 --driver %2 --scan ...").arg(exe.toHtmlEscaped(), spec.toHtmlEscaped()));
  QCoreApplication::processEvents();
  QProcess p;
  p.start(exe, {"--driver", spec, "--scan"});
  // a serial SCPI scan runs through several timeouts before giving up;
  // keep the dialog alive meanwhile
  QElapsedTimer t;
  t.start();
  bool finished = false;
  while (!(finished = p.waitForFinished(100)) && t.elapsed() < 30000)
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
  if (!finished)
  {
    p.kill();
    p.waitForFinished(1000);
  }
  const QString out = QString::fromUtf8(p.readAllStandardOutput() + p.readAllStandardError()).trimmed();
  QString found;
  for (const QString &line : out.split('\n'))
    if (line.contains(spec.section(':', 0, 0) + ':') || line.contains(" - "))
      found = line.trimmed();
  if (!finished)
    ui_sigrokHint->setText(tr("No meter answered within 30 s (a serial port without a SCPI meter keeps sigrok-cli waiting)."));
  else if (p.exitCode() == 0 && !found.isEmpty())
    ui_sigrokHint->setText(tr("Found: %1").arg(found.toHtmlEscaped()));
  else
    ui_sigrokHint->setText(tr("No meter answered.") + (out.isEmpty() ? QString() : "<br><tt>" + out.toHtmlEscaped().replace('\n', "<br>") + "</tt>"));
  ui_sigrokTest->setEnabled(true);
}

// "ble <address> <key> <main> <second>", see BleAdvertisementDevice
void DeviceSettings::updateBleHint()
{
#ifndef QTDMM_WITH_BLE
  ui_bleScan->setEnabled(false);
  ui_bleScan->setToolTip(BleDiscoverer::unavailable());
  ui_bleHint->setText(tr("This QtDMM was built without Bluetooth support, so it cannot connect to this meter."));
  return;
#endif
  // no adapter, or switched off: no scan, the tooltip and the hint say why
  const QString unavailable = BleDiscoverer::unavailable();
  ui_bleScan->setEnabled(unavailable.isEmpty());
  ui_bleScan->setToolTip(unavailable.isEmpty() ? tr("Search for the device (a few seconds)") : unavailable);
  if (!unavailable.isEmpty())
  {
    ui_bleHint->setText(unavailable);
    return;
  }
  const QString address = ui_bleAddress->currentText().section(' ', 0, 0).trimmed();
  const bool keyOk = VictronBle::keyFromHex(ui_bleKey->text()).size() == 16;
  QStringList hints;
  if (address.isEmpty())
    hints << tr("Pick the device or type its Bluetooth address.");
  if (isGatt())
  {
    hints << tr("Switch the meter's Bluetooth on (it shows the Bluetooth symbol) before scanning or connecting.");
    ui_bleHint->setText(hints.join(' '));
    return;
  }
  if (isBtHome())
  {
    hints << tr("The board or sensor sends BTHome v2 without encryption; no key is needed. Its values come in the order it sends them.");
    ui_bleHint->setText(hints.join(' '));
    return;
  }
  if (!keyOk)
    hints << tr("The key is the 32-digit \"Encryption key\" VictronConnect shows under Product info, Instant readout via Bluetooth.");
  ui_bleHint->setText(hints.join(' '));
  ui_bleKey->setStyleSheet(keyOk || ui_bleKey->text().trimmed().isEmpty() ? QString() : QStringLiteral("color: red"));
}

void DeviceSettings::on_ui_bleScan_clicked()
{
#ifdef QTDMM_WITH_BLE
  ui_bleScan->setEnabled(false);
  const bool gatt = isGatt();
  const bool bthome = isBtHome();
  // a GATT meter may advertise seldom (HP-570C-APP)
  const int seconds = gatt ? 15 : 5;
  ui_bleHint->setText(gatt     ? tr("Scanning for %1 (%2 s)...").arg(m_dmmInfo.model.section(' ', 0, 0)).arg(seconds)
                      : bthome ? tr("Scanning for BTHome devices (%1 s)...").arg(seconds)
                               : tr("Scanning for Victron devices (%1 s)...").arg(seconds));
  QCoreApplication::processEvents();
  QString error;
  const QStringList found = gatt ? BleGattDevice::scan(m_dmmInfo.protocol, seconds * 1000, &error)
                                 : BleAdvertisementDevice::scan(seconds * 1000, &error, m_dmmInfo.protocol);
  const QString current = ui_bleAddress->currentText();
  ui_bleAddress->clear();
  ui_bleAddress->addItems(found);
  if (!current.isEmpty())
    ui_bleAddress->setCurrentText(current);   // keep what was configured, found or not
  ui_bleScan->setEnabled(true);
  if (found.isEmpty() && !error.isEmpty())
  {
    ui_bleHint->setText(tr("The search failed: %1").arg(error));
    return;
  }
  if (found.isEmpty())
  {
    ui_bleHint->setText(gatt     ? tr("No meter found. Is its Bluetooth switched on, and no other program connected to it?")
                        : bthome ? tr("No BTHome device found. Does the board send (BTHome v2, UUID 0xFCD2)?")
                                 : tr("No Victron device found. Is Bluetooth on, and Instant readout enabled on the device?"));
    return;
  }
#endif
  updateBleHint();
}

QString DeviceSettings::device() const
{
  return MeterSetup::fromKeys(keys()).device;
}

void DeviceSettings::on_ui_load_clicked()
{
  QString filename = QFileDialog::getOpenFileName(this, tr("Load DMM description"), m_path,	tr("DMM description (*.cfg)"));

  if (!filename.isNull())
  {
    QFileInfo info(filename);
    m_path = info.filePath();
    ui_filename->setText(info.fileName());

    QSettings cfg(filename, QSettings::IniFormat);

    port->setCurrentText(cfg.value("Port settings/device", "").toString());
    baudRate->setCurrentText(cfg.value("Port settings/baud", "9600").toString());
    bitsCombo->setCurrentText(cfg.value("Port settings/bits", "7").toString());
    stopBitsCombo->setCurrentText(cfg.value("Port settings/stop-bits", "2").toString());
    parityCombo->setCurrentIndex(cfg.value("Port settings/parity", 0).toInt());
    selectDisplay(cfg.value("DMM/display", "4000").toString());
    ui_externalSetup->setChecked(cfg.value("DMM/external-setup", false).toBool());
    selectFormat(MeterSetup::formatFromSetting(cfg.value("DMM/data-format", "Metex14")));
    ui_framesPerReading->setValue(cfg.value("DMM/number-of-values", 1).toInt());
    uirts->setChecked(cfg.value("DMM/rts", true).toBool());
    uidtr->setChecked(cfg.value("DMM/dtr", false).toBool());
  }
}

void DeviceSettings::on_ui_save_clicked()
{
  QString filename = QFileDialog::getSaveFileName(this, tr("Save DMM description"), m_path, tr("DMM description (*.cfg)"));
  if (!filename.isNull())
  {
    QFileInfo info(filename);
    m_path = info.filePath();
    ui_filename->setText(info.fileName());

    QSettings cfg(filename, QSettings::IniFormat);
    cfg.setValue("Port settings/device", port->currentText());
    cfg.setValue("Port settings/baud", baudRate->currentText());
    cfg.setValue("Port settings/bits", bitsCombo->currentText());
    cfg.setValue("Port settings/stop-bits", stopBitsCombo->currentText());
    cfg.setValue("Port settings/parity", parityCombo->currentIndex());

    cfg.setValue("DMM/display", displayCombo->currentText());
    cfg.setValue("DMM/external-setup", ui_externalSetup->isChecked());
    cfg.setValue("DMM/data-format", FrameFormat::toString(format()));
    cfg.setValue("DMM/number-of-values", ui_framesPerReading->value());

    cfg.setValue("DMM/rts", uirts->isChecked());
    cfg.setValue("DMM/dtr", uidtr->isChecked());
  }
}
