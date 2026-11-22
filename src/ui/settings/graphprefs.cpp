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
#include "ui/views/graphlook.h"
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
}

GraphPrefs::GraphPrefs(QWidget *parent) : SettingsPage(parent)
{
  m_label = tr("Graph");
  m_description = tr("Colours and lines, the time window and the statistics.");
  m_iconName = "office-chart-line";

  auto colour = [this](const char *name, const QString &tip)
  {
    auto *b = new ColorButton(this);
    b->setObjectName(name);
    b->setToolTip(tip);
    return b;
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

  // first: a graphics driver that draws the plots blank or flickering is
  // the first thing to rule out
  ui_openGl = new QCheckBox(tr("Draw the XY diagram, histogram and heatmap with Open&GL (from the next start)"), this);
  ui_openGl->setObjectName("ui_openGl");
  ui_openGl->setToolTip(tr("Experimental, off by default: with several plots open it can show artefacts and "
                           "raise the load. QtDMM takes it at its next start."));
  m_form->addRow(QString(), ui_openGl);
#ifdef QTDMM_NEOQCP
  // NeoQCP always draws on the GPU: nothing to choose
  m_form->setRowVisible(ui_openGl, false);
#endif

  // the graphs' colours; the colour buttons below are the variant Custom
  // (cursor and thresholds always)
  addSection(m_form, tr("Colours"));
  m_variant = new QComboBox(this);
  m_variant->setObjectName("ui_variant");
  for (GraphLook::Variant v : GraphLook::variants())
    m_variant->addItem(v == GraphLook::Custom ? tr("Custom")
                                              : v == GraphLook::Neutral ? tr("Neutral: follows the design")
                                                                        : GraphLook::title(v),
                       GraphLook::name(v));
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

  // the combo boxes line up
  int width = 0;
  for (QComboBox *c : findChildren<QComboBox *>())
    width = qMax(width, c->sizeHint().width());
  for (QComboBox *c : { m_variant, ui_lineMode, ui_pointMode })
    c->setMinimumWidth(width);

  alignLabels(m_form);
  connect(m_variant, &QComboBox::currentIndexChanged, this, &GraphPrefs::updateRows);
  updateRows();
}

GraphPrefs::~GraphPrefs()
{
}

void GraphPrefs::updateRows()
{
  showRow(m_form, m_ownColours, variant() == GraphLook::name(GraphLook::Custom));
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
  // a config from before the variants, whose colours were set here, keeps
  // them as Custom
  QString variant = m_cfg->getString("Graph/variant");
  if (variant.isEmpty())
    variant = m_cfg->getString("Graph/background").isEmpty() ? "scope" : "custom";
  m_variant->setCurrentIndex(qMax(0, m_variant->findData(variant)));
  ui_lineMode->setCurrentIndex(m_cfg->getInt("Graph/line-mode", 1));
  ui_pointMode->setCurrentIndex(m_cfg->getInt("Graph/point-mode"));
  ui_crosshair->setChecked(m_cfg->getBool("Graph/crosshair-cursor", true));
  ui_openGl->setChecked(m_cfg->getBool("Graph/openGl", false));
  ui_statsSide->setCurrentIndex(m_cfg->getString("Graph/stats-side", "left") == "left" ? 1 : 0);
  ui_lineWidth->setValue(m_cfg->getInt("Graph/line-width", 2));

  ui_winSize->setValue(m_cfg->getInt("Window/size", 600));
  sizeUnit->setCurrentIndex(m_cfg->getInt("Window/size-unit"));

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
  ui_openGl->setChecked(false);
  ui_statsSide->setCurrentIndex(0);

  ui_winSize->setValue(600);
  sizeUnit->setCurrentIndex(0);

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
  m_cfg->setBool("Graph/openGl", ui_openGl->isChecked());
  m_cfg->setString("Graph/stats-side", statsLeft() ? "left" : "right");

  m_cfg->setInt("Window/size", ui_winSize->value());
  m_cfg->setInt("Window/size-unit", sizeUnit->currentIndex());

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
