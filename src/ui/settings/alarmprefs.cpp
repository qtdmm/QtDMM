// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/settings/alarmprefs.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProcess>
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>
#include <QToolButton>
#include <QVBoxLayout>

#include "ui/alarmsound.h"
#include "ui/engnumbervalidator.h"
#include "core/settings.h"

AlarmPrefs::AlarmPrefs(QWidget *parent) : SettingsPage(parent)
{
  m_label = tr("Alarms");
  m_description = tr("Alarms watch the reading and tell you when it leaves the range you expect.");
  m_iconName = "notifications";

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  // which meter's alarms, with more than one open
  auto *deviceRow = new QHBoxLayout;
  m_deviceLabel = new QLabel(tr("&Device:"), this);
  m_device = new QComboBox(this);
  m_device->setObjectName("ui_alarmDevice");
  m_deviceLabel->setBuddy(m_device);
  deviceRow->addWidget(m_deviceLabel);
  deviceRow->addWidget(m_device, 1);
  layout->addLayout(deviceRow);
  connect(m_device, qOverload<int>(&QComboBox::activated), this, [this](int index)
  {
    m_current = qBound(0, index, int(m_devices.size()) - 1);
    refresh();
  });
  m_devices << Device();
  m_list = new QListWidget(this);
  m_list->setToolTip(tr("Each alarm watches a reading of the device. It raises when its condition has held for the "
                        "given time and clears once the reading is back beyond the hysteresis. Untick an "
                        "alarm to keep it without it firing. An alarm can show a banner over the display, "
                        "play a sound, open a popup, run a program or start a recording."));
  layout->addWidget(m_list, 1);
  // the empty list says what to do
  m_empty = new QLabel(tr("No alarms yet. Add one with Add..."), m_list->viewport());
  m_empty->setAlignment(Qt::AlignCenter);
  m_empty->setEnabled(false);
  m_empty->setAttribute(Qt::WA_TransparentForMouseEvents);
  auto *emptyLayout = new QVBoxLayout(m_list->viewport());
  emptyLayout->addWidget(m_empty);
  connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) { edit(); });
  connect(m_list, &QListWidget::itemChanged, this, [this](QListWidgetItem *item)
  {
    toggle(m_list->row(item), item->checkState() == Qt::Checked);
  });
  connect(m_list, &QListWidget::itemSelectionChanged, this, [this]
  {
    const bool any = !m_list->selectedItems().isEmpty();
    m_edit->setEnabled(any);
    m_remove->setEnabled(any);
  });

  auto *buttons = new QHBoxLayout;
  m_add = new QPushButton(tr("&Add..."), this);
  m_edit = new QPushButton(tr("&Edit..."), this);
  m_remove = new QPushButton(tr("&Remove"), this);
  m_edit->setEnabled(false);
  m_remove->setEnabled(false);
  connect(m_add, &QPushButton::clicked, this, &AlarmPrefs::add);
  connect(m_edit, &QPushButton::clicked, this, &AlarmPrefs::edit);
  connect(m_remove, &QPushButton::clicked, this, &AlarmPrefs::remove);
  buttons->addWidget(m_add);
  buttons->addWidget(m_edit);
  buttons->addWidget(m_remove);
  buttons->addStretch(1);
  layout->addLayout(buttons);

  m_blink = new QCheckBox(tr("Coloured displays &blink"), this);
  m_blink->setToolTip(tr("The display and the analog meter switch between the alarm's colour and their own "
                         "once a second, for alarms that colour them."));
  layout->addWidget(m_blink);
}

bool AlarmPrefs::blink() const
{
  return m_blink->isChecked();
}

void AlarmPrefs::setValueUnit(int id, const QString &unit)
{
  if (m_units.value(id) == unit && m_units.contains(id))
    return;
  m_units[id] = unit;
  m_devices[0].units = m_units;
  refresh();
}

void AlarmPrefs::setValueNames(const QStringList &names)
{
  m_valueNames = names;
  m_devices[0].valueNames = names;
  refresh();
}

AlarmPrefs::Device AlarmPrefs::device(const QString &source) const
{
  const QList<Device> all = m_source ? m_source() : QList<Device>{ Device() };
  for (Device d : all)
    if (d.source == source)
    {
      if (source.isEmpty())
      {
        d.units = m_units;
        d.valueNames = m_valueNames;
      }
      return d;
    }
  return Device();
}

void AlarmPrefs::setCurrentDevice(const QString &source)
{
  for (int i = 0; i < m_devices.size(); ++i)
    if (m_devices[i].source == source)
    {
      m_current = i;
      m_device->setCurrentIndex(i);
      refresh();
      return;
    }
}

void AlarmPrefs::defaultsSLOT()
{
  // the meter shown stays, if it is still open
  const QString shown = m_devices.value(m_current).source;
  if (m_source)
    m_devices = m_source();
  // no device connected: alarms belong to one, nothing to edit here
  const bool none = m_source && m_devices.isEmpty();
  if (m_devices.isEmpty() || !m_source)
  {
    m_devices = { Device() };
    if (!none)
      m_devices[0].alarms = Alarm::listFromJson(m_cfg->getString("Alarms/list"));
  }
  m_add->setEnabled(!none);
  m_list->setEnabled(!none);
  m_empty->setText(none ? tr("Alarms belong to a device: connect one first.") : tr("No alarms yet. Add one with Add..."));
  m_devices[0].units = m_units;
  m_devices[0].valueNames = m_valueNames;
  m_changed.clear();
  m_device->clear();
  for (const Device &d : std::as_const(m_devices))
    m_device->addItem(d.name);
  m_device->setVisible(m_devices.size() > 1);
  m_deviceLabel->setVisible(m_devices.size() > 1);
  m_current = 0;
  setCurrentDevice(shown);
  m_blink->setChecked(m_cfg->getBool("Alarms/blink", false));
  refresh();
}

void AlarmPrefs::factoryDefaultsSLOT()
{
  for (Device &d : m_devices)
  {
    if (!d.alarms.isEmpty())
      m_changed.insert(d.source);
    d.alarms.clear();
  }
  m_blink->setChecked(false);
  refresh();
}

void AlarmPrefs::applySLOT()
{
  // the alarms go to the meters, which keep them
  for (const Device &d : std::as_const(m_devices))
  {
    if (!m_changed.contains(d.source))
      continue;
    if (m_apply)
      m_apply(d.source, d.alarms);
    else if (d.source.isEmpty())
      m_cfg->setString("Alarms/list", Alarm::listToJson(d.alarms));
  }
  m_changed.clear();
  m_cfg->setBool("Alarms/blink", m_blink->isChecked());
}

void AlarmPrefs::refresh()
{
  const Device &dev = current();
  const int row = m_list->currentRow();
  m_list->blockSignals(true);
  m_list->clear();
  for (const Alarm &al : dev.alarms)
  {
    // with more than one value: which one it watches
    const QString what = dev.valueNames.size() > 1 && al.condition != Alarm::NoReadings
                           ? tr("value %1: ").arg(al.value.id + 1) : QString();
    auto *item = new QListWidgetItem(QString("%1  —  %2%3").arg(al.name, what, al.describe(dev.units.value(al.value.id))),
                                     m_list);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(al.enabled ? Qt::Checked : Qt::Unchecked);
    QPixmap swatch(12, 12);
    swatch.fill(al.color);
    item->setIcon(QIcon(swatch));
  }
  m_list->blockSignals(false);
  m_empty->setVisible(dev.alarms.isEmpty());
  if (row >= 0 && row < m_list->count())
    m_list->setCurrentRow(row);
}

void AlarmPrefs::add()
{
  Device &dev = current();
  Alarm al;
  al.name = alarmName(int(dev.alarms.size()) + 1);
  AlarmDlg dlg(al, dev.units, dev.valueNames, this);
  if (dlg.exec() == QDialog::Accepted)
  {
    dev.alarms << dlg.alarm();
    m_changed.insert(dev.source);
    refresh();
    m_list->setCurrentRow(int(dev.alarms.size()) - 1);
  }
}

void AlarmPrefs::edit()
{
  Device &dev = current();
  const int row = m_list->currentRow();
  if (row < 0 || row >= dev.alarms.size())
    return;
  AlarmDlg dlg(dev.alarms[row], dev.units, dev.valueNames, this);
  if (dlg.exec() == QDialog::Accepted)
  {
    dev.alarms[row] = dlg.alarm();
    m_changed.insert(dev.source);
    refresh();
  }
}

void AlarmPrefs::remove()
{
  Device &dev = current();
  const int row = m_list->currentRow();
  if (row < 0 || row >= dev.alarms.size())
    return;
  dev.alarms.removeAt(row);
  m_changed.insert(dev.source);
  refresh();
}

void AlarmPrefs::toggle(int row, bool on)
{
  Device &dev = current();
  if (row >= 0 && row < dev.alarms.size())
  {
    dev.alarms[row].enabled = on;
    m_changed.insert(dev.source);
  }
}

// ---------------------------------------------------------------------------

AlarmDlg::AlarmDlg(const Alarm &alarm, const QMap<int, QString> &units, const QStringList &valueNames, QWidget *parent)
  : QDialog(parent), m_units(units), m_unit(units.value(alarm.value.id)), m_valueId(alarm.value.id),
    m_valueSource(alarm.value.source), m_colorValue(alarm.color)
{
  setWindowTitle(tr("Alarm"));
  auto *layout = new QVBoxLayout(this);

  auto *top = new QFormLayout;
  m_name = new QLineEdit(alarm.name, this);
  m_enabled = new QCheckBox(tr("&Enabled"), this);
  m_enabled->setChecked(alarm.enabled);
  auto *nameRow = new QHBoxLayout;
  nameRow->addWidget(m_name, 1);
  nameRow->addWidget(m_enabled);
  auto *nameLabel = new QLabel(tr("&Name:"), this);
  nameLabel->setBuddy(m_name);
  top->addRow(nameLabel, nameRow);
  layout->addLayout(top);

  // --- condition ---
  auto *cond = new QGroupBox(tr("Condition"), this);
  auto *cf = new QFormLayout(cond);
  // the meter's value it watches, when it has more than one
  if (valueNames.size() > 1 || alarm.value.id > 0)
  {
    m_value = new QComboBox(cond);
    for (int id = 0; id < qMax(int(valueNames.size()), alarm.value.id + 1); ++id)
      m_value->addItem(valueNames.value(id, tr("Value %1").arg(id + 1)), id);
    m_value->setCurrentIndex(m_value->findData(alarm.value.id));
    cf->addRow(tr("&Value:"), m_value);
    connect(m_value, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]
    {
      m_valueId = m_value->currentData().toInt();
      m_unit = m_units.value(m_valueId);
      updateFields();
    });
  }
  m_condition = new QComboBox(cond);
  m_condition->addItem(tr("Reading below"), Alarm::Below);
  m_condition->addItem(tr("Reading above"), Alarm::Above);
  m_condition->addItem(tr("Reading outside a range"), Alarm::Outside);
  m_condition->addItem(tr("Reading inside a range"), Alarm::Inside);
  m_condition->addItem(tr("Overload (OL)"), Alarm::Overload);
  m_condition->addItem(tr("No readings for a while"), Alarm::NoReadings);
  m_condition->setCurrentIndex(m_condition->findData(alarm.condition));
  cf->addRow(tr("&Condition:"), m_condition);
  auto *validator = new EngNumberValidator(this);
  m_a = new QLineEdit(EngNumberValidator::engText(alarm.a), cond);
  m_a->setValidator(validator);
  m_aLabel = new QLabel(cond);
  cf->addRow(m_aLabel, m_a);
  m_b = new QLineEdit(EngNumberValidator::engText(alarm.b), cond);
  m_b->setValidator(validator);
  m_bLabel = new QLabel(cond);
  m_bLabel->setBuddy(m_b);
  cf->addRow(m_bLabel, m_b);
  m_seconds = new QLineEdit(QString::number(alarm.seconds), cond);
  m_seconds->setValidator(new QDoubleValidator(0, 1e6, 1, this));
  m_secondsLabel = new QLabel(cond);
  cf->addRow(m_secondsLabel, m_seconds);
  m_hysteresis = new QLineEdit(EngNumberValidator::engText(alarm.hysteresis), cond);
  m_hysteresis->setValidator(validator);
  m_hystLabel = new QLabel(cond);
  m_hystLabel->setBuddy(m_hysteresis);
  m_hysteresis->setToolTip(tr("The reading must come back by this much before the alarm clears - keeps it from "
                              "flickering when the reading sits at the threshold."));
  cf->addRow(m_hystLabel, m_hysteresis);
  layout->addWidget(cond);

  // --- actions ---
  auto *act = new QGroupBox(tr("Actions"), this);
  auto *af = new QFormLayout(act);
  m_message = new QLineEdit(alarm.message, act);
  m_message->setPlaceholderText(tr("the condition in words"));
  af->addRow(tr("&Message:"), m_message);
  m_color = new QPushButton(act);
  m_color->setFlat(false);
  auto paintColor = [this]
  {
    QPixmap pm(40, 14);
    pm.fill(m_colorValue);
    m_color->setIcon(QIcon(pm));
    m_color->setText(m_colorValue.name());
  };
  paintColor();
  connect(m_color, &QPushButton::clicked, this, [this, paintColor]
  {
    const QColor c = QColorDialog::getColor(m_colorValue, this, tr("Alarm colour"));
    if (c.isValid())
    {
      m_colorValue = c;
      paintColor();
    }
  });
  af->addRow(tr("Co&lour:"), m_color);
  m_banner = new QCheckBox(tr("Banner over the display (with Acknowledge)"), act);
  m_banner->setChecked(alarm.banner);
  m_tint = new QCheckBox(tr("Display and analog meter in the alarm's colour"), act);
  m_tint->setToolTip(tr("While the alarm is raised, also after Acknowledge, until the reading is back. With "
                        "several alarms the one raised last gives the colour."));
  m_tint->setChecked(alarm.tintDisplay);
  m_popup = new QCheckBox(tr("Popup window"), act);
  m_popup->setChecked(alarm.popup);
  m_raise = new QCheckBox(tr("Bring the QtDMM window to the front"), act);
  m_raise->setChecked(alarm.raiseWindow);
  m_markGraph = new QCheckBox(tr("Mark in the graph"), act);
  m_markGraph->setChecked(alarm.markGraph);
  m_markTable = new QCheckBox(tr("Mark in the readings table"), act);
  m_markTable->setChecked(alarm.markTable);
  for (QCheckBox *cb : {m_banner, m_tint, m_popup, m_raise, m_markGraph, m_markTable})
    af->addRow(QString(), cb);

  // the sound: none, the system's beep, a built-in one or a WAV file
  m_preview = new AlarmSound([](int) { return false; }, this);
  m_sound = new QComboBox(act);
  m_sound->setObjectName("ui_alarmSound");
  m_sound->addItem(tr("none"), QString());
  m_sound->addItem(tr("System beep"), QStringLiteral("beep"));
#ifdef QTDMM_WITH_MULTIMEDIA
  for (const auto &[name, shown] : AlarmSound::builtIn())
    m_sound->addItem(shown, name);
#endif
  if (m_sound->findData(alarm.sound) < 0)
    m_sound->addItem(QFileInfo(alarm.sound).fileName(), alarm.sound);
#ifdef QTDMM_WITH_MULTIMEDIA
  m_sound->addItem(tr("Choose file..."));
#endif
  m_sound->setCurrentIndex(m_sound->findData(alarm.sound));
  m_soundIndex = m_sound->currentIndex();
  auto *tryIt = new QToolButton(act);
  tryIt->setObjectName("ui_alarmSoundPlay");
  tryIt->setIcon(QIcon::fromTheme("media-playback-start"));
  tryIt->setToolTip(tr("Play the sound"));
  connect(tryIt, &QToolButton::clicked, this, [this]
  {
    const QString sound = m_sound->currentData().toString();
    if (!sound.isEmpty())
      m_preview->play(sound);
  });
  auto *soundRow = new QHBoxLayout;
  soundRow->setContentsMargins(0, 0, 0, 0);
  soundRow->addWidget(m_sound, 1);
  soundRow->addWidget(tryIt);
  auto *soundLabel = new QLabel(tr("S&ound:"), act);
  soundLabel->setBuddy(m_sound);
  af->addRow(soundLabel, soundRow);
  m_repeat = new QComboBox(act);
  m_repeat->addItem(tr("once"), Alarm::Once);
  m_repeat->addItem(tr("while the alarm is raised"), Alarm::WhileRaised);
  m_repeat->addItem(tr("times, while the alarm is raised"), Alarm::Times);
  m_repeat->setCurrentIndex(m_repeat->findData(alarm.repeat));
  m_repeat->setToolTip(tr("A second from start to start. Acknowledge ends it, and so does the reading coming back."));
  m_repeatCount = new QSpinBox(act);
  m_repeatCount->setRange(2, 99);
  m_repeatCount->setValue(qMax(2, alarm.repeatCount));
  auto *repeatRow = new QHBoxLayout;
  repeatRow->setContentsMargins(0, 0, 0, 0);
  repeatRow->addWidget(m_repeatCount);
  repeatRow->addWidget(m_repeat, 1);
  auto *repeatLabel = new QLabel(tr("Repe&at:"), act);
  repeatLabel->setBuddy(m_repeat);
  af->addRow(repeatLabel, repeatRow);
  m_clearSound = new QCheckBox(tr("Play \"O&K\" when the alarm clears"), act);
  m_clearSound->setChecked(alarm.clearSound);
  af->addRow(QString(), m_clearSound);
  connect(m_sound, qOverload<int>(&QComboBox::activated), this, &AlarmDlg::updateSound);
  connect(m_repeat, qOverload<int>(&QComboBox::currentIndexChanged), this, &AlarmDlg::updateSound);
  m_recorder = new QComboBox(act);
  m_recorder->addItem(tr("leave alone"), Alarm::RecorderNone);
  m_recorder->addItem(tr("start"), Alarm::RecorderStart);
  m_recorder->addItem(tr("stop"), Alarm::RecorderStop);
  m_recorder->setCurrentIndex(m_recorder->findData(alarm.recorder));
  af->addRow(tr("&Recording:"), m_recorder);
  m_command = new QLineEdit(alarm.command, act);
  m_command->setPlaceholderText(tr("none"));
  m_command->setToolTip(tr("Program to run when the alarm raises. %v is the value, %u its unit, %n the alarm's name."));
  auto *browse = new QToolButton(act);
  browse->setObjectName("ui_alarmCommandButton");
  browse->setIcon(QIcon::fromTheme("document-open"));
  browse->setToolTip(tr("Choose the program"));
  connect(browse, &QToolButton::clicked, this, [this]
  {
    // the file replaces the program, its arguments (%v, %u, %n) stay
    QStringList args = QProcess::splitCommand(m_command->text());
    const QString current = args.isEmpty() ? QString() : args.takeFirst();
    const QString dir = QFileInfo(current).isAbsolute() ? QFileInfo(current).absolutePath() : QString();
    const QString file = QFileDialog::getOpenFileName(this, tr("Program to run"), dir);
    if (file.isEmpty())
      return;
    QStringList parts{ file.contains(' ') ? '"' + file + '"' : file };
    for (const QString &a : std::as_const(args))
      parts << (a.contains(' ') ? '"' + a + '"' : a);
    m_command->setText(parts.join(' '));
  });
  auto *commandRow = new QHBoxLayout;
  commandRow->setContentsMargins(0, 0, 0, 0);
  commandRow->addWidget(m_command);
  commandRow->addWidget(browse);
  auto *commandLabel = new QLabel(tr("Run &program:"), act);
  commandLabel->setBuddy(m_command);   // Alt+P into the field, not the row
  af->addRow(commandLabel, commandRow);
  m_disconnect = new QCheckBox(tr("&Disconnect first"), act);
  m_disconnect->setChecked(alarm.disconnect);
  m_disconnect->setToolTip(tr("Close the port while the program runs and open it again when it has ended - "
                              "so the program can talk to the meter itself."));
  af->addRow(QString(), m_disconnect);
  connect(m_command, &QLineEdit::textChanged, m_disconnect, [this](const QString &text)
  {
    m_disconnect->setEnabled(!text.trimmed().isEmpty());
  });
  m_disconnect->setEnabled(!alarm.command.trimmed().isEmpty());
  layout->addWidget(act);

  auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(bb);

  connect(m_condition, qOverload<int>(&QComboBox::currentIndexChanged), this, &AlarmDlg::updateFields);
  updateFields();
  updateSound();
}

void AlarmDlg::updateSound()
{
  // the last entry without data is "Choose file...": a WAV file, before it
  if (m_sound->currentIndex() == m_sound->count() - 1 && m_sound->currentData().isNull())
  {
    const QString current = m_sound->itemData(m_soundIndex).toString();
    const QString file = QFileDialog::getOpenFileName(this, tr("Alarm sound"),
                                                      QFileInfo(current).isAbsolute() ? QFileInfo(current).absolutePath() : QString(),
                                                      tr("WAV files (*.wav);;All files (*)"));
    if (file.isEmpty())
      m_sound->setCurrentIndex(m_soundIndex);
    else
    {
      if (m_sound->findData(file) < 0)
        m_sound->insertItem(m_sound->count() - 1, QFileInfo(file).fileName(), file);
      m_sound->setCurrentIndex(m_sound->findData(file));
    }
  }
  m_soundIndex = m_sound->currentIndex();
  m_sound->setToolTip(QFileInfo(m_sound->currentData().toString()).isAbsolute() ? m_sound->currentData().toString() : QString());
  const bool on = !m_sound->currentData().toString().isEmpty();
  m_repeat->setEnabled(on);
  m_repeatCount->setVisible(m_repeat->currentData().toInt() == Alarm::Times);
  m_repeatCount->setEnabled(on);
}

void AlarmDlg::updateFields()
{
  const auto c = Alarm::Condition(m_condition->currentData().toInt());
  const bool range = c == Alarm::Outside || c == Alarm::Inside;
  const bool value = c == Alarm::Below || c == Alarm::Above || range;
  // before the first reading the unit is not known: no unit rather than a guessed one
  if (m_unit.isEmpty())
  {
    m_aLabel->setText(range ? tr("&Lower bound:") : tr("&Threshold:"));
    m_bLabel->setText(tr("&Upper bound:"));
    m_hystLabel->setText(tr("&Hysteresis:"));
  }
  else
  {
    m_aLabel->setText(range ? tr("&Lower bound (%1):").arg(m_unit) : tr("&Threshold (%1):").arg(m_unit));
    m_bLabel->setText(tr("&Upper bound (%1):").arg(m_unit));
    m_hystLabel->setText(tr("&Hysteresis (%1):").arg(m_unit));
  }
  // "no readings" is the meter's silence: no value to choose
  if (m_value)
    m_value->setEnabled(c != Alarm::NoReadings);
  m_aLabel->setBuddy(m_a);
  m_a->setEnabled(value);
  m_aLabel->setEnabled(value);
  m_b->setEnabled(range);
  m_bLabel->setEnabled(range);
  m_secondsLabel->setText(c == Alarm::NoReadings ? tr("&Silence (s):") : tr("&For at least (s):"));
  m_secondsLabel->setBuddy(m_seconds);
  m_hysteresis->setEnabled(value);
  m_hystLabel->setEnabled(value);
}

Alarm AlarmDlg::alarm() const
{
  Alarm al;
  al.name = m_name->text().trimmed().isEmpty() ? tr("Alarm") : m_name->text().trimmed();
  al.enabled = m_enabled->isChecked();
  al.condition = Alarm::Condition(m_condition->currentData().toInt());
  al.value = al.condition == Alarm::NoReadings ? ValueRef() : ValueRef(m_valueId, m_valueSource);
  al.a = EngNumberValidator::value(m_a->text().isEmpty() ? "0" : m_a->text());
  al.b = EngNumberValidator::value(m_b->text().isEmpty() ? "0" : m_b->text());
  al.seconds = m_seconds->text().toDouble();
  al.hysteresis = EngNumberValidator::value(m_hysteresis->text().isEmpty() ? "0" : m_hysteresis->text());
  al.message = m_message->text().trimmed();
  al.color = m_colorValue;
  al.banner = m_banner->isChecked();
  al.tintDisplay = m_tint->isChecked();
  al.sound = m_sound->currentData().toString();
  al.repeat = Alarm::Repeat(m_repeat->currentData().toInt());
  al.repeatCount = m_repeatCount->value();
  al.clearSound = m_clearSound->isChecked();
  al.popup = m_popup->isChecked();
  al.raiseWindow = m_raise->isChecked();
  al.markGraph = m_markGraph->isChecked();
  al.markTable = m_markTable->isChecked();
  al.recorder = Alarm::RecorderAction(m_recorder->currentData().toInt());
  al.command = m_command->text().trimmed();
  al.disconnect = !al.command.isEmpty() && m_disconnect->isChecked();
  return al;
}
