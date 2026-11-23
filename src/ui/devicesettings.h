//======================================================================
// File:		devicesettings.h
// Author:	Matthias Toussaint
// Created:	Sat Oct 19 15:08:57 CEST 2002
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

#pragma once

class QGroupBox;

#include <QtSerialPort>
#include <vector>

#include "ui_uidevicesettings.h"
#include "device/frameformat.h"
#include "device/dmmdecoder.h"
#include "device/transports/calc.h"
#include <QTimer>
#include <array>
#include <functional>


/// The settings of one meter: vendor/model choice (from the registered
/// DmmDecoder::DMMInfo entries), port and the serial parameters, which
/// become editable in manual mode, or the group of a Bluetooth, sigrok,
/// virtual or calculated meter. Descriptions can be saved to and loaded
/// from .cfg files.
///
/// The widget works on a map of meter keys (Settings::isMeterKey()), not on
/// the Settings: the Multimeter page fills it from the instance, a dialog
/// could fill it from an entry of My devices.
class DeviceSettings : public QWidget, private Ui::UIDeviceSettings
{
  Q_OBJECT
public:
  DeviceSettings(QWidget *parent = nullptr);
  ~DeviceSettings();

  /// Fills the fields from @p keys; a key missing there gets its default.
  void           load(const QVariantMap &keys);
  /// The fields as meter keys, the counterpart of load().
  QVariantMap    keys() const;
  /// The ports offered in the port list (Transport::availablePorts() and the
  /// custom ports); the port of load() is added when missing.
  void           setPorts(const QStringList &ports);
  /// The port parameters and the protocol are shown: set by hand (a known
  /// model brings its own, so they are hidden).
  bool           portParametersShown() const;
  /// Offers only the models @p filter accepts (all when empty); @p manual
  /// false disables "Manual settings". Call before load().
  void           setModelFilter(std::function<bool(const DmmDecoder::DMMInfo &)> filter, bool manual);
  /// The names of the models offered, sorted.
  QStringList    models() const;
  /// The page's hint line and the buttons for DMM description files (.cfg),
  /// which the assistant leaves out.
  void           setDescriptionFilesVisible(bool visible);
  /// Shows the port row of a cable meter, or not (the assistant asks for
  /// the port on a page of its own).
  void           setPortVisible(bool visible);
  /// Everything needed is filled in: a model, and its port, address and key,
  /// formula or sigrok driver.
  bool           isComplete() const;
  /// Path of sigrok-cli, for the hint and the test of a sigrok meter.
  void           setSigrokExe(const QString &exe) { m_sigrokExe = exe; }

  /// The DMMInfo of the chosen model, or the manual settings.
  DmmDecoder::DMMInfo dmmInfo() const { return m_dmmInfo; }
  QSerialPort::Parity parity() const;
  QSerialPort::DataBits bits() const;
  QSerialPort::StopBits stopBits() const;
  int            speed() const;
  /// Frames one reading consists of (FrameReader), the "Lines per reading"
  /// box: from the table for a model, as set for manual settings.
  int            framesPerReading() const;
  /// How many values the meter has: DMMInfo::valueCount() of the model, one
  /// per line for manual settings.
  int            valueCount() const;
  bool           externalSetup() const;
  bool           rts() const;
  bool           dtr() const;
  FrameFormat::DataFormat format() const;
  /// Display counts (4000, 6000, ...).
  int            display() const;
  /// Selects the display counts, adding the entry when the combo lacks it.
  void           selectDisplay(const QString &counts);
  /// Selects the protocol combo entry for @p df.
  void           selectFormat(FrameFormat::DataFormat df);
  QString        dmmName() const;
  /// The port entry as typed or chosen, e.g. "/dev/ttyUSB0" or "HID 0x1a86:0xe008 ...";
  /// for a calculated value "calc <unit> <formula>".
  QString        device() const;
  /// True while the model "QtDMM / Calculated value" is chosen.
  bool           isCalculated() const;
  /// True while the model "QtDMM / Simulated meter" is chosen.
  bool           isVirtual() const;
  /// Victron over Bluetooth LE: the Bluetooth group replaces the port.
  bool           isBluetooth() const;
  bool           isGatt() const;
  /// A bench meter read through sigrok-cli: the sigrok group replaces the port.
  bool           isSigrokMeter() const;
  /// The readings of the meters open here (DeviceSession::calcInputs()),
  /// shown as a hint below the formula.
  void           setCalcInputs(const CalcDevice::Inputs &inputs);

Q_SIGNALS:
  /// A field was edited or another model chosen.
  void           changed();

protected Q_SLOTS:
  void           on_ui_vendor_activated(int);
  void           on_ui_model_activated(int);
  /// Load a DMM description (.cfg).
  void           on_ui_load_clicked();
  /// Save the current settings as a DMM description (.cfg).
  void           on_ui_save_clicked();
  void           on_ui_externalSetup_toggled();
  /// Opens or closes "Advanced".
  void           showPortParameters(bool show);
  /// Turns a group box into a section: no frame, its title a bold label
  /// above it that comes and goes with it.
  void           sectionTitle(QGroupBox *group);
  /// Re-parses the formula and refreshes the hint (variables, live values, errors).
  void           updateCalcHint();
  /// Rebuilds the virtual meter's formula from the waveform fields.
  void           updateVirtualFormula();
  /// The rows of the simulated meter's values 2-4: those chosen shown, a
  /// formula that does not parse red.
  void           updateVirtualValues();
  /// Validates address and key, explains what is missing.
  void           updateBleHint();
  /// Fills the main/second/third/fourth value combos with the fields of the
  /// chosen model.
  void           updateBleFields();
  /// Five-second scan for Victron devices, fills the device combo.
  void           on_ui_bleScan_clicked();
  /// Checks sigrok-cli and the driver, explains what is missing.
  void           updateSigrokHint();
  /// sigrok-cli --scan with the current settings.
  void           on_ui_sigrokTest_clicked();

protected:
  QString        m_path;
  DmmDecoder::DMMInfo m_dmmInfo;
  QStringListModel *m_portlist;
  std::vector<DmmDecoder::DMMInfo> m_currentVendorModels;

  void setupComboBoxModel();
  /// The vendor and model lists from the registered models, through m_filter.
  void fillModels();
  void populateModelsForVendor(const QString &vendor);
  void populateAllModels();
  /// One entry of the model list, with the kind of instrument as its tooltip.
  void addModelItem(const DmmDecoder::DMMInfo &cfg);
  void enterManualMode();
  /// Shows the formula group instead of the port/protocol groups, or back.
  void updateCalcMode();

  CalcDevice::Inputs m_inputs;
  QTimer m_calcHintTimer;

private:
  /// The keys of the last load(): the Victron value choice falls back on
  /// them while the combos are still empty.
  QVariantMap    m_loaded;
  /// The models offered (all registered ones through m_filter), by name.
  std::vector<DmmDecoder::DMMInfo> m_models;
  std::function<bool(const DmmDecoder::DMMInfo &)> m_filter;
  bool           m_manualAllowed = true;
  bool           m_portVisible = true;
  bool           m_portParameters = false;   ///< showPortParameters()
  QStringListModel *m_completerNames = nullptr;   ///< what the model field completes
  QString        m_sigrokExe = QStringLiteral("sigrok-cli");
  /// The simulated meter's values 2-4 (values 1 is the waveform above).
  struct VirtualValue
  {
    QLabel    *label = nullptr;
    QLineEdit *unit = nullptr;
    QComboBox *coupling = nullptr;
    QLineEdit *formula = nullptr;
  };
  QSpinBox      *m_virtualValues = nullptr;   ///< how many values the simulated meter has
  std::array<VirtualValue, 3> m_virtualRows;
};
