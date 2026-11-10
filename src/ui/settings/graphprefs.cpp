//======================================================================
// File:		graphprefs.cpp
// Author:	Matthias Toussaint
// Created:	Sat Oct 19 15:27:46 CEST 2002
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

#include "ui/colorbutton.h"
#include "ui/engnumbervalidator.h"
#include "ui/views/graphwidget.h"
#include "ui/settings/graphprefs.h"
#include "core/settings.h"

#define MINUTE_SECS   60
#define HOUR_SECS     60*60
#define DAY_SECS      60*60*24

namespace
{
QStringList timeUnits()
{
  return { GraphPrefs::tr("Seconds"), GraphPrefs::tr("Minutes"), GraphPrefs::tr("Hours"), GraphPrefs::tr("Days") };
}

QStringList lineModes()
{
  return { GraphPrefs::tr("No line"), GraphPrefs::tr("Solid line"), GraphPrefs::tr("Dotted line") };
}

QStringList pointModes()
{
  return { GraphPrefs::tr("No points"), GraphPrefs::tr("Circle"), GraphPrefs::tr("Square"), GraphPrefs::tr("Diamond"),
           GraphPrefs::tr("X"), GraphPrefs::tr("Large Circle"), GraphPrefs::tr("Large Square"),
           GraphPrefs::tr("Large Diamond"), GraphPrefs::tr("Large X") };
}

const char *kNumberHint = QT_TRANSLATE_NOOP("GraphPrefs", "Values take a suffix: m, u, n, p, k, M, G, T (10k = 10000, "
                                                          "100m = 0.1).");
}

GraphPrefs::GraphPrefs(QWidget *parent) : SettingsPage(parent)
{
  m_label = tr("Graph");
  m_description = tr("Colours and lines, the axes and the integration curve.");
  m_iconName = "office-chart-line";

  auto *validator = new EngNumberValidator(this);
  const QString numberHint = tr(kNumberHint);
  auto colour = [this](const char *name, const QString &tip)
  {
    auto *b = new ColorButton(this);
    b->setObjectName(name);
    b->setToolTip(tip);
    return b;
  };
  auto number = [&](const char *name, const QString &tip)
  {
    auto *e = new QLineEdit(this);
    e->setObjectName(name);
    e->setValidator(validator);
    e->setToolTip(tip + "<p>" + numberHint);
    e->setMaximumWidth(fontMetrics().horizontalAdvance('0') * 12);
    return e;
  };
  auto combo = [this](const char *name, const QStringList &items)
  {
    auto *c = new QComboBox(this);
    c->setObjectName(name);
    c->addItems(items);
    return c;
  };
  auto spin = [this](const char *name, int min, int max)
  {
    auto *sp = new QSpinBox(this);
    sp->setObjectName(name);
    sp->setRange(min, max);
    return sp;
  };

  m_form = createForm();

  // the graphs' colours; the colour buttons below are the variant Custom
  // (cursor and thresholds always)
  addSection(m_form, tr("Colours"));
  m_variant = new QComboBox(this);
  m_variant->setObjectName("ui_variant");
  for (auto v : { GraphWidget::Neutral, GraphWidget::ScopeBlue, GraphWidget::PhosphorGreen, GraphWidget::PhosphorAmber,
                  GraphWidget::ChartRecorder, GraphWidget::Custom })
    m_variant->addItem(v == GraphWidget::Custom ? tr("Custom")
                                             : v == GraphWidget::Neutral ? tr("Neutral: follows the design")
                                                                      : GraphWidget::variantTitle(v),
                       GraphWidget::variantName(v));
  m_variant->setToolTip(tr("The colours of the graph: background, grid, lettering and curves. Custom sets "
                           "them one by one. A graph's context menu can choose other colours for that graph "
                           "only."));
  m_form->addRow(tr("S&cheme:"), m_variant);
  ui_dataColor = colour("ui_dataColor", tr("The colour of the curve."));
  ui_bgColor = colour("ui_bgColor", tr("The background of the graph."));
  ui_gridColor = colour("ui_gridColor", tr("The grid and its lettering."));
  // the own colours side by side: they come and go with Custom
  m_ownColours = new QWidget(this);
  {
    auto *grid = new QHBoxLayout(m_ownColours);
    grid->setContentsMargins(0, 0, 0, 0);
    auto add = [&](const QString &text, ColorButton *b)
    {
      auto *l = new QLabel(text, m_ownColours);
      l->setBuddy(b);
      grid->addWidget(l);
      grid->addWidget(b);
    };
    add(tr("&Data"), ui_dataColor);
    grid->addSpacing(fontMetrics().horizontalAdvance('x') * 2);
    add(tr("&Background"), ui_bgColor);
    grid->addSpacing(fontMetrics().horizontalAdvance('x') * 2);
    add(tr("&Grid"), ui_gridColor);
    grid->addStretch(1);
  }
  m_form->addRow(QString(), m_ownColours);
  ui_cursorColor = colour("ui_cursorColor", tr("The line where the readings come in."));
  ui_startColor = colour("ui_startColor", tr("The line of the start threshold."));
  m_form->addRow(tr("Cu&rsor:"), ui_cursorColor);
  m_form->addRow(tr("&Start threshold:"), ui_startColor);

  addSection(m_form, tr("Line and points"));
  ui_lineMode = combo("ui_lineMode", lineModes());
  ui_lineWidth = spin("ui_lineWidth", 1, 5);
  ui_lineWidth->setToolTip(tr("The width of the line."));
  m_form->addRow(tr("&Line:"), row({ ui_lineMode, ui_lineWidth }));
  ui_pointMode = combo("ui_pointMode", pointModes());
  m_form->addRow(tr("&Points:"), ui_pointMode);

  addSection(m_form, tr("Y axis"));
  autoScaleBut = new QRadioButton(tr("Au&tomatic"), this);
  autoScaleBut->setObjectName("autoScaleBut");
  autoScaleBut->setToolTip(tr("The axis follows the readings."));
  ui_includeZero = new QCheckBox(tr("Include &zero line"), this);
  ui_includeZero->setObjectName("ui_includeZero");
  ui_includeZero->setToolTip(tr("The automatic axis always shows zero."));
  m_form->addRow(QString(), row({ autoScaleBut, ui_includeZero }));
  manualScaleBut = new QRadioButton(tr("Fi&xed"), this);
  manualScaleBut->setObjectName("manualScaleBut");
  manualScaleBut->setToolTip(tr("The axis runs from the minimum to the maximum below."));
  m_form->addRow(QString(), manualScaleBut);
  ui_scaleMin = number("ui_scaleMin", tr("The bottom of the axis."));
  ui_scaleMax = number("ui_scaleMax", tr("The top of the axis."));
  // the two sit in different rows: one group keeps them exclusive
  auto *scaleGroup = new QButtonGroup(this);
  scaleGroup->addButton(autoScaleBut);
  scaleGroup->addButton(manualScaleBut);
  m_form->addRow(tr("M&inimum:"), ui_scaleMin);
  m_form->addRow(tr("&Maximum:"), ui_scaleMax);

  addSection(m_form, tr("Time axis"));
  ui_winSize = spin("ui_winSize", 1, 99999);
  ui_winSize->setToolTip(tr("How much of the recording the graph shows at once; also how much Live keeps "
                            "when the recording has no length. The time buttons under the graph set it too."));
  sizeUnit = combo("sizeUnit", timeUnits());
  m_form->addRow(tr("&Visible:"), row({ ui_winSize, sizeUnit }));
  ui_crosshair = new QCheckBox(tr("Cross&hair cursor"), this);
  ui_crosshair->setObjectName("ui_crosshair");
  ui_crosshair->setToolTip(tr("Lines through the mouse pointer, with the time and the value."));
  m_form->addRow(QString(), ui_crosshair);
  // the statistics beside every graph: which side
  ui_statsSide = combo("ui_statsSide", { tr("Right of the graph"), tr("Left of the graph") });
  ui_statsSide->setToolTip(tr("Where the statistics stand beside the graph (the cursors, from A to B, the "
                              "window shown); the Σ button hides them."));
  m_form->addRow(tr("Statistics:"), ui_statsSide);

  // the integration curve: a section of its own, open while it is shown
  ui_showInt = new QCheckBox(tr("I&ntegration curve"), this);
  ui_showInt->setObjectName("ui_showInt");
  ui_showInt->setToolTip(tr("A second curve: the running integral of the reading, scaled and offset."));
  addSection(m_form, ui_showInt);
  ui_intValue = combo("ui_intValue", { tr("Value 1 (main display)"), tr("Value 2 (second display, if the meter has one)") });
  ui_intValue->setToolTip(tr("The value the curve integrates. Without a second display there is no curve for value 2."));
  m_form->addRow(tr("Appl&y to:"), ui_intValue);
  ui_intColor = colour("ui_intColor", tr("The colour of the integration curve."));
  ui_intThresholdColor = colour("ui_intThresholdColor", tr("The colour of the integration threshold."));
  auto *intColours = row({ ui_intColor, new QLabel(tr("Threshold"), this), ui_intThresholdColor });
  m_form->addRow(tr("Colo&ur:"), intColours);
  ui_intLineMode = combo("ui_intLineMode", lineModes());
  ui_intLineWidth = spin("ui_intLineWidth", 1, 5);
  ui_intLineWidth->setToolTip(tr("The width of the line."));
  auto *intLine = row({ ui_intLineMode, ui_intLineWidth });
  m_form->addRow(tr("Lin&e:"), intLine);
  ui_intPointMode = combo("ui_intPointMode", pointModes());
  m_form->addRow(tr("P&oints:"), ui_intPointMode);
  ui_intScale = number("ui_intScale", tr("Scaling factor for the integration curve."));
  ui_intOffset = number("ui_intOffset", tr("Offset of the integration curve."));
  ui_intThreshold = number("ui_intThreshold", tr("Below this value the integral starts again at zero."));
  m_form->addRow(tr("Sc&ale:"), ui_intScale);
  m_form->addRow(tr("O&ffset:"), ui_intOffset);
  m_form->addRow(tr("Reset belo&w:"), ui_intThreshold);
  m_intRows = { ui_intValue, intColours, intLine, ui_intPointMode, ui_intScale, ui_intOffset, ui_intThreshold };

  // the combo boxes line up
  int width = 0;
  for (QComboBox *c : findChildren<QComboBox *>())
    width = qMax(width, c->sizeHint().width());
  for (QComboBox *c : { m_variant, ui_lineMode, ui_pointMode, ui_intValue, ui_intLineMode, ui_intPointMode })
    c->setMinimumWidth(width);

  alignLabels(m_form);
  connect(m_variant, &QComboBox::currentIndexChanged, this, &GraphPrefs::updateRows);
  connect(manualScaleBut, &QRadioButton::toggled, this, &GraphPrefs::updateRows);
  connect(ui_showInt, &QCheckBox::toggled, this, &GraphPrefs::updateRows);
  updateRows();
}

GraphPrefs::~GraphPrefs()
{
}

void GraphPrefs::updateRows()
{
  showRow(m_form, m_ownColours, variant() == GraphWidget::variantName(GraphWidget::Custom));
  ui_includeZero->setEnabled(autoScaleBut->isChecked());
  showRow(m_form, ui_scaleMin, manualScaleBut->isChecked());
  showRow(m_form, ui_scaleMax, manualScaleBut->isChecked());
  for (QWidget *w : m_intRows)
    showRow(m_form, w, ui_showInt->isChecked());
}

QString GraphPrefs::variant() const
{
  return m_variant->currentData().toString();
}

void GraphPrefs::defaultsSLOT()
{
  ui_bgColor->setColor(m_cfg->getColor("Graph/background"));
  ui_gridColor->setColor(m_cfg->getColor("Graph/grid", Qt::gray));
  ui_dataColor->setColor(m_cfg->getColor("Graph/data", Qt::blue));
  ui_cursorColor->setColor(m_cfg->getColor("Graph/cursor", Qt::black));
  ui_startColor->setColor(m_cfg->getColor("Graph/start-trigger", Qt::magenta));
  ui_intColor->setColor(m_cfg->getColor("Graph/integration", Qt::darkBlue));
  ui_intThresholdColor->setColor(m_cfg->getColor("Graph/integration-threshold", Qt::darkBlue));
  // a config from before the variants, whose colours were set here, keeps
  // them as Custom
  QString variant = m_cfg->getString("Graph/variant");
  if (variant.isEmpty())
    variant = m_cfg->getString("Graph/background").isEmpty() ? "scope" : "custom";
  m_variant->setCurrentIndex(qMax(0, m_variant->findData(variant)));
  ui_lineMode->setCurrentIndex(m_cfg->getInt("Graph/line-mode", 1));
  ui_pointMode->setCurrentIndex(m_cfg->getInt("Graph/point-mode"));
  ui_crosshair->setChecked(m_cfg->getBool("Graph/crosshair-cursor", true));
  ui_statsSide->setCurrentIndex(m_cfg->getString("Graph/stats-side", "right") == "left" ? 1 : 0);
  ui_lineWidth->setValue(m_cfg->getInt("Graph/line-width", 2));

  if (m_cfg->getBool("Scale/automatic", true))
    autoScaleBut->setChecked(true);
  else
    manualScaleBut->setChecked(true);
  ui_includeZero->setChecked(m_cfg->getBool("Scale/automatic-include-zero", false));
  ui_scaleMin->setText(m_cfg->getString("Scale/minimum", "-3.999"));
  ui_scaleMax->setText(m_cfg->getString("Scale/maximum", "3.999"));
  ui_winSize->setValue(m_cfg->getInt("Window/size", 600));
  sizeUnit->setCurrentIndex(m_cfg->getInt("Window/size-unit"));

  ui_intLineMode->setCurrentIndex(m_cfg->getInt("Graph/int-line-mode"));
  ui_intPointMode->setCurrentIndex(m_cfg->getInt("Graph/int-point-mode", 1));
  ui_showInt->setChecked(m_cfg->getBool("Graph/show-integration"));
  ui_intValue->setCurrentIndex(qBound(0, m_cfg->getInt("Graph/int-value"), 1));
  ui_intScale->setText(m_cfg->getString("Graph/int-scale", "1.0"));
  ui_intOffset->setText(m_cfg->getString("Graph/int-offset", "0.0"));
  ui_intThreshold->setText(m_cfg->getString("Graph/int-threshold", "0.0"));
  ui_intLineWidth->setValue(m_cfg->getInt("Graph/int-line-width", 2));
  updateRows();
}

void GraphPrefs::factoryDefaultsSLOT()
{
  ui_bgColor->setColor(Qt::white);
  ui_gridColor->setColor(Qt::gray);
  ui_dataColor->setColor(Qt::blue);
  ui_cursorColor->setColor(Qt::black);
  ui_startColor->setColor(Qt::magenta);   // mt: removed .rgb()
  m_variant->setCurrentIndex(qMax(0, m_variant->findData(QString("scope"))));
  ui_lineMode->setCurrentIndex(1);
  ui_pointMode->setCurrentIndex(0);
  ui_lineWidth->setValue(2);
  ui_crosshair->setChecked(true);
  ui_statsSide->setCurrentIndex(0);

  autoScaleBut->setChecked(true);
  ui_includeZero->setChecked(false);
  ui_scaleMin->setText("-3.999");
  ui_scaleMax->setText("3.999");
  ui_winSize->setValue(600);
  sizeUnit->setCurrentIndex(0);

  ui_intColor->setColor(Qt::darkBlue);
  ui_intThresholdColor->setColor(Qt::darkBlue);
  ui_intLineMode->setCurrentIndex(0);
  ui_intPointMode->setCurrentIndex(1);
  ui_intLineWidth->setValue(2);
  ui_showInt->setChecked(false);
  ui_intValue->setCurrentIndex(0);
  ui_intScale->setText("1.0");
  ui_intOffset->setText("0.0");
  ui_intThreshold->setText("0.0");
  updateRows();
}

void GraphPrefs::applySLOT()
{
  m_cfg->setColor("Graph/background", ui_bgColor->color());
  m_cfg->setColor("Graph/grid", ui_gridColor->color());
  m_cfg->setColor("Graph/data", ui_dataColor->color());
  m_cfg->setColor("Graph/cursor", ui_cursorColor->color());
  m_cfg->setColor("Graph/start-trigger", ui_startColor->color());
  m_cfg->setString("Graph/variant", variant());
  m_cfg->setInt("Graph/line-width", ui_lineWidth->value());
  m_cfg->setInt("Graph/line-mode", ui_lineMode->currentIndex());
  m_cfg->setInt("Graph/point-mode", ui_pointMode->currentIndex());
  m_cfg->setBool("Graph/crosshair-cursor", ui_crosshair->isChecked());
  m_cfg->setString("Graph/stats-side", statsLeft() ? "left" : "right");

  m_cfg->setBool("Scale/automatic", automaticScale());
  m_cfg->setBool("Scale/automatic-include-zero", includeZero());
  m_cfg->setString("Scale/minimum", ui_scaleMin->text());
  m_cfg->setString("Scale/maximum", ui_scaleMax->text());
  m_cfg->setInt("Window/size", ui_winSize->value());
  m_cfg->setInt("Window/size-unit", sizeUnit->currentIndex());

  m_cfg->setColor("Graph/integration", ui_intColor->color());
  m_cfg->setColor("Graph/integration-threshold", ui_intThresholdColor->color());
  m_cfg->setInt("Graph/int-line-width", ui_intLineWidth->value());
  m_cfg->setInt("Graph/int-line-mode", ui_intLineMode->currentIndex());
  m_cfg->setInt("Graph/int-point-mode", ui_intPointMode->currentIndex());
  m_cfg->setBool("Graph/show-integration", ui_showInt->isChecked());
  m_cfg->setInt("Graph/int-value", ui_intValue->currentIndex());
  m_cfg->setString("Graph/int-scale", ui_intScale->text());
  m_cfg->setString("Graph/int-offset", ui_intOffset->text());
  m_cfg->setString("Graph/int-threshold", ui_intThreshold->text());
}

QColor GraphPrefs::bgColor() const { return ui_bgColor->color(); }
QColor GraphPrefs::gridColor() const { return ui_gridColor->color(); }
QColor GraphPrefs::dataColor() const { return ui_dataColor->color(); }
QColor GraphPrefs::startColor() const { return ui_startColor->color(); }
QColor GraphPrefs::cursorColor() const { return ui_cursorColor->color(); }
int GraphPrefs::lineWidth() const { return ui_lineWidth->value(); }
int GraphPrefs::lineMode() const { return ui_lineMode->currentIndex(); }
int GraphPrefs::pointMode() const { return ui_pointMode->currentIndex(); }
bool GraphPrefs::crosshair() const { return ui_crosshair->isChecked(); }
bool GraphPrefs::statsLeft() const { return ui_statsSide->currentIndex() == 1; }

bool GraphPrefs::includeZero() const { return ui_includeZero->isChecked(); }
bool GraphPrefs::automaticScale() const { return autoScaleBut->isChecked(); }
double GraphPrefs::scaleMin() const { return EngNumberValidator::value(ui_scaleMin->text()); }
double GraphPrefs::scaleMax() const { return EngNumberValidator::value(ui_scaleMax->text()); }

bool GraphPrefs::showIntegration() const { return ui_showInt->isChecked(); }
QColor GraphPrefs::intColor() const { return ui_intColor->color(); }
QColor GraphPrefs::intThresholdColor() const { return ui_intThresholdColor->color(); }
int GraphPrefs::intLineWidth() const { return ui_intLineWidth->value(); }
int GraphPrefs::intLineMode() const { return ui_intLineMode->currentIndex(); }
int GraphPrefs::intPointMode() const { return ui_intPointMode->currentIndex(); }
int GraphPrefs::intValue() const { return ui_intValue->currentIndex(); }
double GraphPrefs::intScale() const { return EngNumberValidator::value(ui_intScale->text()); }
double GraphPrefs::intThreshold() const { return EngNumberValidator::value(ui_intThreshold->text()); }
double GraphPrefs::intOffset() const { return EngNumberValidator::value(ui_intOffset->text()); }

void GraphPrefs::setIntThreshold(double value)
{
  ui_intThreshold->setText(EngNumberValidator::engText(value));
}

void GraphPrefs::zoomInSLOT(double fac)
{
  double size = static_cast<double>(ui_winSize->value()) / fac;

  switch (sizeUnit->currentIndex())
  {
    case 0:
      if (size < 10.0)
        size = 10.0;
      break;
    case 1:
      if (size < 1.0)
      {
        size *= 60.0;
        sizeUnit->setCurrentIndex(0);
      }
      break;
    case 2:
      if (size < 1.0)
      {
        size *= 60.0;
        sizeUnit->setCurrentIndex(1);
      }
      break;
    case 3:
      if (size < 1.0)
      {
        size *= 24.0;
        sizeUnit->setCurrentIndex(2);
      }
      break;
  }

  ui_winSize->setValue(static_cast<int>(size));

}

void GraphPrefs::zoomOutSLOT(double fac)
{
  double size = static_cast<double>(ui_winSize->value()) * fac;

  switch (sizeUnit->currentIndex())
  {
    case 0:
      if (size > 600)
      {
        size /= 60.0;
        sizeUnit->setCurrentIndex(1);
      }
      break;
    case 1:
      if (size > 300)
      {
        size /= 60.0;
        sizeUnit->setCurrentIndex(2);
      }
      break;
    case 2:
      if (size > 48)
      {
        size /= 24.0;
        sizeUnit->setCurrentIndex(3);
      }
      break;
  }
  ui_winSize->setValue(static_cast<int>(size));
}

int GraphPrefs::windowSeconds() const
{
  int sec = ui_winSize->text().toInt();

  switch (sizeUnit->currentIndex())
  {
    case 1:
      sec *= MINUTE_SECS;
      break;
    case 2:
      sec *= HOUR_SECS;
      break;
    case 3:
      sec *= DAY_SECS;
      break;
  }
  return sec;
}

void GraphPrefs::setWindowSecondsSLOT(int seconds)
{
  const bool minutes = seconds >= 60 && seconds % 60 == 0;
  sizeUnit->setCurrentIndex(minutes ? 1 : 0);
  ui_winSize->setValue(minutes ? seconds / 60 : seconds);
}
