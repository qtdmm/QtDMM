//======================================================================
// File:		recorderprefs.cpp
// Author:	Matthias Toussaint
// Created:	Sat Oct 19 14:59:20 CEST 2002
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
//======================================================================


#include <QtGui>
#include <QtWidgets>
#include <limits>

#include "ui/engnumbervalidator.h"
#include "ui/settings/recorderprefs.h"
#include  "core/settings.h"

#define MINUTE_SECS   60
#define HOUR_SECS     60*60
#define DAY_SECS      60*60*24

RecorderPrefs::RecorderPrefs(QWidget *parent) : SettingsPage(parent)
{
  m_label = tr("Recording");
  m_description = tr("How often a reading is kept and when the recording starts.");
  m_iconName = "media-record";

  auto *validator = new EngNumberValidator(this);
  const QString numberHint = tr("Values take a suffix: m, u, n, p, k, M, G, T (10k = 10000, 100m = 0.1).");
  const QStringList units = { tr("Seconds"), tr("Minutes"), tr("Hours"), tr("Days") };

  m_form = createForm();

  addSection(m_form, tr("Sampling"));
  sampleEvery = new QSpinBox(this);
  sampleEvery->setObjectName("sampleEvery");
  sampleEvery->setRange(1, 99999);
  sampleEvery->setToolTip(tr("QtDMM records every reading of the meter with its time. This is the grid of "
                             "the export: one row per period, the mean of the readings in it."));
  ui_sampleUnit = new QComboBox(this);
  ui_sampleUnit->setObjectName("ui_sampleUnit");
  ui_sampleUnit->addItems(QStringList { tr("1/10 Seconds") } + units);
  m_form->addRow(tr("Sample &every:"), row({ sampleEvery, ui_sampleUnit }));

  addSection(m_form, tr("Start"));
  manualBut = new QRadioButton(tr("&By hand"), this);
  manualBut->setObjectName("manualBut");
  manualBut->setToolTip(tr("The Record button starts and stops the recording."));
  m_form->addRow(QString(), manualBut);

  predefinedBut = new QRadioButton(tr("At a &clock time"), this);
  predefinedBut->setObjectName("predefinedBut");
  predefinedBut->setToolTip(tr("The recording starts from Live at this time of day."));
  ui_startTime = new QTimeEdit(this);
  ui_startTime->setObjectName("ui_startTime");
  ui_startTime->setDisplayFormat("HH:mm:ss");
  m_form->addRow(QString(), row({ predefinedBut, ui_startTime }));

  triggerBut = new QRadioButton(tr("At a t&hreshold"), this);
  triggerBut->setObjectName("triggerBut");
  triggerBut->setToolTip(tr("The recording starts from Live when the reading crosses the threshold: rising "
                            "above it or falling below it."));
  ui_edge = new QComboBox(this);
  ui_edge->setObjectName("ui_edge");
  ui_edge->addItems({ tr("rising above"), tr("falling below") });
  ui_raisingThreshold = new QLineEdit(this);
  ui_raisingThreshold->setObjectName("ui_raisingThreshold");
  ui_fallingThreshold = new QLineEdit(this);
  ui_fallingThreshold->setObjectName("ui_fallingThreshold");
  for (QLineEdit *e : { ui_raisingThreshold, ui_fallingThreshold })
  {
    e->setValidator(validator);
    e->setToolTip(tr("The threshold; a line in the graph shows it and can be dragged.") + "<p>" + numberHint);
    e->setMaximumWidth(fontMetrics().horizontalAdvance('0') * 12);
  }
  // with more than one value: which one the threshold watches (setTriggerChoices())
  ui_triggerValue = new QComboBox(this);
  ui_triggerValue->setObjectName("ui_triggerValue");
  ui_triggerValue->setToolTip(tr("The value the threshold watches; the recording keeps all values."));
  ui_triggerValue->setVisible(false);
  connect(ui_triggerValue, &QComboBox::activated, this, [this]
  {
    m_triggerValue = ValueRef::fromString(ui_triggerValue->currentData().toString());
  });
  m_thresholdRow = row({ ui_triggerValue, ui_edge, ui_raisingThreshold, ui_fallingThreshold });
  m_form->addRow(QString(), row({ triggerBut, m_thresholdRow }));

  // the options sit in rows of their own: one group keeps them exclusive
  auto *startGroup = new QButtonGroup(this);
  startGroup->addButton(manualBut);
  startGroup->addButton(predefinedBut);
  startGroup->addButton(triggerBut);

  ui_preTrigger = new QCheckBox(tr("Pre-&trigger"), this);
  ui_preTrigger->setObjectName("ui_preTrigger");
  ui_preTrigger->setToolTip(tr("A recording started by the threshold reaches back by this time: QtDMM keeps "
                               "the readings while it waits, and a green mark shows where the trigger came."));
  ui_preTriggerTime = new QSpinBox(this);
  ui_preTriggerTime->setObjectName("ui_preTriggerTime");
  ui_preTriggerTime->setRange(1, 99999);
  ui_preTriggerUnit = new QComboBox(this);
  ui_preTriggerUnit->setObjectName("ui_preTriggerUnit");
  ui_preTriggerUnit->addItems(units);
  m_preRow = row({ ui_preTrigger, ui_preTriggerTime, ui_preTriggerUnit });
  m_form->addRow(QString(), m_preRow);

  // the threshold's fields sit in the line of the option; the pre-trigger
  // belongs to the threshold start
  m_preRow->setContentsMargins(QApplication::style()->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth) + 6, 0, 0, 0);

  alignLabels(m_form);
  connect(predefinedBut, &QRadioButton::toggled, this, &RecorderPrefs::updateRows);
  connect(triggerBut, &QRadioButton::toggled, this, &RecorderPrefs::updateRows);
  connect(ui_edge, &QComboBox::currentIndexChanged, this, &RecorderPrefs::updateRows);
  connect(ui_preTrigger, &QCheckBox::toggled, this, &RecorderPrefs::updateRows);
  manualBut->setChecked(true);
  updateRows();
}

RecorderPrefs::~RecorderPrefs()
{
}

void RecorderPrefs::updateRows()
{
  ui_startTime->setVisible(predefinedBut->isChecked());
  m_thresholdRow->setVisible(triggerBut->isChecked());
  ui_raisingThreshold->setVisible(ui_edge->currentIndex() == 0);
  ui_fallingThreshold->setVisible(ui_edge->currentIndex() == 1);
  showRow(m_form, m_preRow, triggerBut->isChecked());
  ui_preTriggerTime->setEnabled(ui_preTrigger->isChecked());
  ui_preTriggerUnit->setEnabled(ui_preTrigger->isChecked());
}

void RecorderPrefs::defaultsSLOT()
{
  sampleEvery->setValue(m_cfg->getInt("Sample/rate", 1));
  ui_sampleUnit->setCurrentIndex(m_cfg->getInt("Sample/rate-unit", 1));
  m_lengthValue = m_cfg->getInt("Sample/time", 500);
  m_lengthUnit = qBound(0, m_cfg->getInt("Sample/time-unit"), 3);

  RecordingStore::StartMode mode = static_cast<RecordingStore::StartMode>(m_cfg->getInt("Start/mode"));
  if (mode == RecordingStore::Time)
    predefinedBut->setChecked(true);
  else if (mode == RecordingStore::Raising || mode == RecordingStore::Falling)
    triggerBut->setChecked(true);
  else
    manualBut->setChecked(true);
  // the edge of a start by hand is kept for the threshold, as before
  if (mode == RecordingStore::Falling)
    ui_edge->setCurrentIndex(1);
  else if (mode == RecordingStore::Raising)
    ui_edge->setCurrentIndex(0);

  ui_startTime->setTime(QTime(qBound(0, m_cfg->getInt("Start/hour"), 23), qBound(0, m_cfg->getInt("Start/minute"), 59),
                              qBound(0, m_cfg->getInt("Start/second"), 59)));
  ui_raisingThreshold->setText(m_cfg->getString("Start/raising-threshold", "0.0"));
  ui_fallingThreshold->setText(m_cfg->getString("Start/falling-threshold", "0.0"));
  ui_preTrigger->setChecked(m_cfg->getBool("Start/pre-trigger", false));
  ui_preTriggerTime->setValue(m_cfg->getInt("Start/pre-trigger-time", 10));
  ui_preTriggerUnit->setCurrentIndex(m_cfg->getInt("Start/pre-trigger-unit", 0));
  m_triggerValue = ValueRef::fromString(m_cfg->getString("Start/trigger-value", "0"));
  ui_triggerValue->setCurrentIndex(qMax(0, ui_triggerValue->findData(m_triggerValue.toString())));
  updateRows();
}

void RecorderPrefs::factoryDefaultsSLOT()
{
  // the recording length is not on the page: it stays
  sampleEvery->setValue(1);
  ui_sampleUnit->setCurrentIndex(1);

  manualBut->setChecked(true);
  ui_edge->setCurrentIndex(0);
  ui_startTime->setTime(QTime(0, 0));
  ui_raisingThreshold->setText("0.0");
  ui_fallingThreshold->setText("0.0");
  ui_preTrigger->setChecked(false);
  ui_preTriggerTime->setValue(10);
  ui_preTriggerUnit->setCurrentIndex(0);
  m_triggerValue = ValueRef();
  ui_triggerValue->setCurrentIndex(0);
  updateRows();
}

void RecorderPrefs::applySLOT()
{
  m_cfg->setInt("Sample/rate", sampleEvery->value());
  m_cfg->setInt("Sample/rate-unit", ui_sampleUnit->currentIndex());
  m_cfg->setInt("Sample/time", m_lengthValue);
  m_cfg->setInt("Sample/time-unit", m_lengthUnit);

  m_cfg->setInt("Start/mode", sampleMode());
  m_cfg->setInt("Start/hour", ui_startTime->time().hour());
  m_cfg->setInt("Start/minute", ui_startTime->time().minute());
  m_cfg->setInt("Start/second", ui_startTime->time().second());
  m_cfg->setString("Start/raising-threshold", ui_raisingThreshold->text());
  m_cfg->setString("Start/falling-threshold", ui_fallingThreshold->text());
  m_cfg->setBool("Start/pre-trigger", ui_preTrigger->isChecked());
  m_cfg->setInt("Start/pre-trigger-time", ui_preTriggerTime->value());
  m_cfg->setInt("Start/pre-trigger-unit", ui_preTriggerUnit->currentIndex());
  m_cfg->setString("Start/trigger-value", m_triggerValue.toString());
}

ValueRef RecorderPrefs::triggerValue() const
{
  return m_triggerValue;
}

void RecorderPrefs::setTriggerChoices(const QList<TriggerChoice> &choices)
{
  ui_triggerValue->clear();
  for (const TriggerChoice &c : choices)
    ui_triggerValue->addItem(c.name, c.value.toString());
  if (ui_triggerValue->findData(m_triggerValue.toString()) < 0)
    ui_triggerValue->addItem(tr("%1 (not connected)").arg(m_triggerValue.toString()), m_triggerValue.toString());
  ui_triggerValue->setCurrentIndex(qMax(0, ui_triggerValue->findData(m_triggerValue.toString())));
  ui_triggerValue->setVisible(ui_triggerValue->count() > 1);
}

RecordingStore::StartMode RecorderPrefs::sampleMode() const
{
  if (predefinedBut->isChecked())
    return RecordingStore::Time;
  if (triggerBut->isChecked())
    return ui_edge->currentIndex() == 1 ? RecordingStore::Falling : RecordingStore::Raising;
  return RecordingStore::Manual;
}

int RecorderPrefs::sampleStep() const
{
  int thenthOfSec = sampleEvery->value();

  switch (ui_sampleUnit->currentIndex())
  {
    case 1: return thenthOfSec * 10;
    case 2: return thenthOfSec * 10 * MINUTE_SECS;
    case 3: return thenthOfSec * 10 * HOUR_SECS;
    case 4: return thenthOfSec * 10 * DAY_SECS;
  }
  return thenthOfSec;
}

int RecorderPrefs::sampleLength() const
{
  static const int unitSeconds[] = { 1, MINUTE_SECS, HOUR_SECS, DAY_SECS };
  const qint64 tenths = qint64(m_lengthValue) * unitSeconds[m_lengthUnit] * 10;
  return int(qMin<qint64>(tenths, std::numeric_limits<int>::max()));
}

void RecorderPrefs::setLength(int value, int unit)
{
  m_lengthValue = qMax(0, value);
  m_lengthUnit = qBound(0, unit, 3);
}

int RecorderPrefs::preTrigger() const
{
  if (!ui_preTrigger->isChecked())
    return 0;
  static const int unitSeconds[] = { 1, MINUTE_SECS, HOUR_SECS, DAY_SECS };
  const qint64 ms = qint64(ui_preTriggerTime->value()) * unitSeconds[qBound(0, ui_preTriggerUnit->currentIndex(), 3)] * 1000;
  return int(qMin<qint64>(ms, std::numeric_limits<int>::max()));
}

double RecorderPrefs::fallingThreshold() const
{
  return EngNumberValidator::value(ui_fallingThreshold->text());
}

double RecorderPrefs::raisingThreshold() const
{
  return EngNumberValidator::value(ui_raisingThreshold->text());
}

void RecorderPrefs::setThreshold(double value)
{
  if (ui_edge->currentIndex() == 0)
    ui_raisingThreshold->setText(EngNumberValidator::engText(value));
  else
    ui_fallingThreshold->setText(EngNumberValidator::engText(value));
}

QTime RecorderPrefs::startTime() const
{
  return ui_startTime->time();
}
