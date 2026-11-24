// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/dialogs/adddevicedlg.h"

#include <QtWidgets>

#include "core/devicelibrary.h"
#include "device/devicetype.h"
#include "device/discovery/discovery.h"
#include "device/protocols.h"
#include "ui/devicesettings.h"
#include "ui/devicesummary.h"
#include "ui/devicesymbol.h"
#include "ui/dialogs/formulanodedlg.h"
#include "ui/dialogs/generatornodedlg.h"
#include "ui/dialogs/simpsunodedlg.h"
#include "ui/dialogs/integralnodedlg.h"
#include "ui/tilebutton.h"

#ifdef QTDMM_WITH_BLE
#include <QBluetoothLocalDevice>
#endif

namespace
{
const QSize kTileSize(180, 120);
const QSize kTargetSize(230, 140);

// the picture of what a find can be, when all its models share maker and
// kind; else none (the symbol of the way)
QIcon symbolOf(const QStringList &models, QSize box, qreal dpr)
{
  QString vendor;
  DeviceType::Type type = DeviceType::Type();
  bool first = true;
  for (const QString &model : models)
    for (const DmmDecoder::DMMInfo &info : DmmDecoder::getDeviceConfigurations())
    {
      if (!DmmDecoder::sameModel(model, info.name))
        continue;
      if (!first && (info.vendor != vendor || DeviceType::of(info) != type))
        return QIcon();
      vendor = info.vendor;
      type = DeviceType::of(info);
      first = false;
    }
  return first ? QIcon() : QIcon(DeviceSymbol::pixmap(vendor, type, box, dpr));
}
}

namespace
{
// a find: its title in bold, the lines below it as they are
class CardDelegate : public QStyledItemDelegate
{
public:
  using QStyledItemDelegate::QStyledItemDelegate;
  void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override
  {
    QStyleOptionViewItem o(option);
    initStyleOption(&o, index);
    const QStringList lines = index.data(Qt::DisplayRole).toString().split('\n');
    o.text.clear();
    const QStyle *style = o.widget->style();
    style->drawControl(QStyle::CE_ItemViewItem, &o, p, o.widget);
    // right of the picture, the row's height (the style's text rect is as
    // high as the text taken away)
    const int margin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &o, o.widget) + 1;
    const QRect area = o.rect.adjusted(o.decorationSize.width() + 3 * margin, 0, -margin, 0);
    const QFontMetrics fm(o.font);
    QFont bold(o.font);
    bold.setBold(true);
    p->save();
    p->setPen(o.palette.color(QPalette::Normal, o.state & QStyle::State_Selected ? QPalette::HighlightedText : QPalette::Text));
    if (const QVariant colour = index.data(Qt::ForegroundRole); colour.isValid() && !(o.state & QStyle::State_Selected))
      p->setPen(colour.value<QBrush>().color());
    int y = area.top() + (area.height() - fm.lineSpacing() * int(lines.size())) / 2;
    for (int i = 0; i < lines.size(); ++i)
    {
      p->setFont(i == 0 ? bold : o.font);
      p->drawText(QRect(area.left(), y, area.width(), fm.lineSpacing()), Qt::AlignLeft | Qt::AlignVCenter,
                  QFontMetrics(p->font()).elidedText(lines[i], Qt::ElideRight, area.width()));
      y += fm.lineSpacing();
    }
    p->restore();
  }
};
}

AddDeviceDlg::AddDeviceDlg(DeviceLibrary *library, QWidget *parent)
  : QDialog(parent)
  , m_library(library)
{
  setWindowTitle(tr("Add device"));

  auto *top = new QVBoxLayout(this);
  m_title = new QLabel(this);
  QFont bold = m_title->font();
  bold.setBold(true);
  bold.setPointSizeF(bold.pointSizeF() * 1.2);
  m_title->setFont(bold);
  auto *titleRow = new QHBoxLayout;
  titleRow->addWidget(m_title, 1);
  m_kindSearch = new QLineEdit(this);
  m_kindSearch->setObjectName("ui_kindSearch");
  m_kindSearch->setPlaceholderText(tr("Search"));
  m_kindSearch->setClearButtonEnabled(true);
  m_kindSearch->setMaximumWidth(200);
  m_kindSearch->hide();
  connect(m_kindSearch, &QLineEdit::textChanged, this, &AddDeviceDlg::searchKinds);
  m_kindSearch->installEventFilter(this);
  titleRow->addWidget(m_kindSearch);
  top->addLayout(titleRow);
  m_pages = new QStackedWidget(this);
  top->addWidget(m_pages, 1);

  // page 1: one tile per kind of connection, a generator and the
  // calculated values; a click goes on
  auto *connections = new QWidget(m_pages);
  auto *tiles = new QGridLayout(connections);
  const struct { Connection id; const char *icon; QString text; QString toolTip; } kinds[] = {
    { Cable,     "drive-removable-media-usb",    tr("&Cable") + '\n' + tr("UT61E, UT803, TFA ..."),
      tr("A meter on a USB cable, a USB-serial adapter or a serial port") },
    { Bluetooth, "preferences-system-bluetooth", tr("Bl&uetooth") + '\n' + tr("UT60BT, Victron"),
      tr("A meter or a Victron device over Bluetooth LE") },
    { Network,   "network-server",               tr("Net&work") + '\n' + tr("qtdmm-bridge"),
      tr("A meter at another computer, through qtdmm-bridge or another RFC 2217 server") },
    { Sigrok,    "qtdmm-sigrok",                 tr("sig&rok") + '\n' + tr("Bench meters"),
      tr("A bench meter read through sigrok-cli") },
    { SimKinds,  "qtdmm-simulated",              tr("&Simulated") + '\n' + tr("Test signals, power supply"),
      tr("Simulated devices to try QtDMM without hardware: test signals (sine, square, noise, a formula over "
         "time) or a power supply") },
    { NodeKinds, "qtdmm-calculated",             tr("C&alculated") + '\n' + tr("Formula, integral ..."),
      tr("Values calculated from those of your devices: a formula, an integral ...") },
  };
  int column = 0;
  for (const auto &kind : kinds)
  {
    QToolButton *button = tile(kind.icon, kind.text, kind.toolTip);
    button->setObjectName(QString("ui_connection%1").arg(int(kind.id)));
    const Connection id = kind.id;
    connect(button, &QToolButton::clicked, this, [this, id] { chooseConnection(id); });
    tiles->addWidget(button, 1 + column / 3, 1 + column % 3);   // centred: stretch around
    ++column;
    if (id == Bluetooth)
    {
      m_bluetoothTile = button;
      m_bluetoothTip = kind.toolTip;
    }
  }
  // no adapter, switched off, a build without Bluetooth: the tile off, its
  // tooltip says why; on again when the adapter is switched on
  m_bluetoothCheck = &BleDiscoverer::unavailable;
  updateBluetoothTile();
#ifdef QTDMM_WITH_BLE
  auto *adapter = new QBluetoothLocalDevice(this);
  connect(adapter, &QBluetoothLocalDevice::hostModeStateChanged, this, &AddDeviceDlg::updateBluetoothTile);
#endif
  tiles->setRowStretch(0, 1);
  tiles->setRowStretch(3, 1);
  tiles->setColumnStretch(0, 1);
  tiles->setColumnStretch(4, 1);
  m_pages->addWidget(connections);

  // page 2: the search for the chosen connection, every port found, and
  // the port field
  auto *port = new QWidget(m_pages);
  auto *portLayout = new QVBoxLayout(port);
  portLayout->setContentsMargins(0, 0, 0, 0);
  auto *searchRow = new QHBoxLayout;
  m_progress = new QProgressBar(port);
  m_progress->setRange(0, 0);
  m_progress->setTextVisible(false);
  m_searchAgain = new QPushButton(tr("Search a&gain"), port);
  searchRow->addWidget(m_progress, 1);
  searchRow->addStretch(0);
  searchRow->addWidget(m_searchAgain, 0, Qt::AlignRight);
  portLayout->addLayout(searchRow);
  connect(m_searchAgain, &QPushButton::clicked, this, &AddDeviceDlg::startSearch);
  m_cards = new QListWidget(port);
  m_cards->setObjectName("ui_cards");
  m_cards->setIconSize(QSize(32, 32));
  m_cards->setItemDelegate(new CardDelegate(m_cards));
  m_cards->setSpacing(2);
  portLayout->addWidget(m_cards, 1);
  connect(m_cards, &QListWidget::itemClicked, this, [this](QListWidgetItem *item)
  {
    chooseCandidate(item->data(Qt::UserRole).toString());
  });
  connect(m_cards, &QListWidget::itemDoubleClicked, this, [this] { next(); });
  m_hint = new QLabel(port);
  m_hint->setWordWrap(true);
  m_fix = new QPushButton(tr("How to &fix..."), port);
  m_fix->hide();
  auto *hintRow = new QHBoxLayout;
  hintRow->addWidget(m_hint, 1);
  hintRow->addWidget(m_fix);
  portLayout->addLayout(hintRow);
  connect(m_fix, &QPushButton::clicked, this, [this]
  {
    const int row = chosenCandidate();
    if (row < 0)
      return;
    QMessageBox box(QMessageBox::Information, tr("How to fix"), m_found[row].problem, QMessageBox::Close, this);
    box.setInformativeText(m_found[row].fix);
    box.setTextInteractionFlags(Qt::TextSelectableByMouse);
    box.exec();
  });
  auto *portRow = new QFormLayout;
  m_port = new QLineEdit(port);
  m_port->setObjectName("ui_port");
  portRow->addRow(tr("Po&rt:"), m_port);
  portLayout->addLayout(portRow);
  connect(m_port, &QLineEdit::textChanged, this, [this]
  {
    // the card of the port typed or chosen, none for another one
    const int row = chosenCandidate();
    m_cards->setCurrentRow(row);
    updateHint();
    updateButtons();
  });
  m_pages->addWidget(port);

  m_makeDiscoverers = [](Connection connection) -> QList<Discoverer *>
  {
    if (connection == Cable)
      return { new UsbDiscoverer, new UsbTmcDiscoverer, new SerialDiscoverer };
    if (connection == Bluetooth)
      return { new BleDiscoverer };
    if (connection == Network)
      return { new BridgeDiscoverer };
    return {};
  };

  // page 3: the name, then the settings of the meter
  auto *device = new QWidget(m_pages);
  auto *deviceLayout = new QVBoxLayout(device);
  deviceLayout->setContentsMargins(0, 0, 0, 0);
  auto *nameRow = new QFormLayout;
  m_name = new QLineEdit(device);
  m_name->setObjectName("ui_name");
  m_name->setToolTip(tr("The name in My devices"));
  nameRow->addRow(tr("Na&me:"), m_name);
  deviceLayout->addLayout(nameRow);
  m_settings = new DeviceSettings(device);
  m_settings->layout()->setContentsMargins(0, 0, 0, 0);
  m_settings->setDescriptionFilesVisible(false);
  // the port comes from page 2
  m_settings->setPortVisible(false);
  deviceLayout->addWidget(m_settings, 1);
  connect(m_settings, &DeviceSettings::changed, this, &AddDeviceDlg::suggestName);
  connect(m_settings, &DeviceSettings::changed, this, &AddDeviceDlg::updateButtons);
  connect(m_name, &QLineEdit::textChanged, this, &AddDeviceDlg::updateButtons);
  m_pages->addWidget(device);

  // page 4: the device as set up - its picture, name, kind and connection -
  // then where it goes; a click finishes
  auto *target = new QWidget(m_pages);
  auto *targetLayout = new QVBoxLayout(target);
  auto *line1 = new QFrame(target);
  auto *line2 = new QFrame(target);
  m_summary = new DeviceSummary(target);
  line1->setFrameShape(QFrame::HLine);
  line1->setFrameShadow(QFrame::Sunken);
  line2->setFrameShape(QFrame::HLine);
  line2->setFrameShadow(QFrame::Sunken);
  targetLayout->addWidget(line1);
  targetLayout->addSpacing(fontMetrics().height()/2);
  targetLayout->addWidget(m_summary);
  targetLayout->addSpacing(fontMetrics().height()/2);
  targetLayout->addWidget(line2);
  targetLayout->addStretch(1);
  auto *targets = new QHBoxLayout;
  targetLayout->addStretch(1);
  targetLayout->addLayout(targets);
  targetLayout->addStretch(1);
  m_connect = tile("network-connect", tr("&Connect") + '\n' + tr("into My devices and connect at once"),
                   tr("Keeps it in My devices and reads it at once: with a front panel, recorded, in formulas "
                      "and over SCPI"));
  m_connect->setObjectName("ui_connect");
  m_connect->setFixedSize(kTargetSize);
  m_saveOnly = tile("document-save", tr("&Save only") + '\n' + tr("into My devices, without connecting"),
                    tr("Keeps it in My devices; Connect in its context menu reads it later"));
  m_saveOnly->setObjectName("ui_saveOnly");
  m_saveOnly->setFixedSize(kTargetSize);
  connect(m_connect, &QToolButton::clicked, this, [this] { finish(Connect); });
  connect(m_saveOnly, &QToolButton::clicked, this, [this] { finish(SaveOnly); });
  targets->addStretch();
  targets->addWidget(m_connect);
  targets->addWidget(m_saveOnly);
  targets->addStretch();
  m_pages->addWidget(target);

  // page 5: a logic node's settings (its NodeEditor), then page 4
  m_nodePage = new QWidget(m_pages);
  auto *nodeLayout = new QVBoxLayout(m_nodePage);
  nodeLayout->setContentsMargins(0, 0, 0, 0);
  m_pages->addWidget(m_nodePage);

  // page 6, after the tile Calculated: the logic nodes with inputs, in
  // groups, what the one marked does beside them; a double click goes on
  auto *kindPage = new QWidget(m_pages);
  auto *kindLayout = new QHBoxLayout(kindPage);
  kindLayout->setContentsMargins(0, 0, 0, 0);
  m_kindList = new QTreeWidget(kindPage);
  m_kindList->setObjectName("ui_kindList");
  m_kindList->setHeaderHidden(true);
  m_kindList->setRootIsDecorated(false);
  m_kindList->setItemsExpandable(false);
  m_kindList->setIndentation(fontMetrics().height());
  m_kindList->setIconSize(QSize(24, 24));
  // each kind with the tile that leads to it
  const struct { Connection tile; Connection id; const char *icon; QString group; QString name; QString about; } nodeKinds[] = {
    { SimKinds, Simulated, "qtdmm-simulated", tr("Simulate"), tr("Test signals"),
      tr("<p>Up to four values on a clock, each a shape between a minimum and a maximum - sine, triangle, "
         "square, sawtooth, random, a discharge - plus noise, or a formula over time.</p>"
         "<p><b>Example:</b> a sine between 0 and 10 V every 20 s to try the graph and the alarms.</p>") },
    { SimKinds, SimulatedPsu, "qtdmm-simulated", tr("Simulate"), tr("Power supply"),
      tr("<p>A power supply with one to four channels, operated on its front: a knob for the voltage and "
         "OUT for each channel. Each channel's voltage is a value as a meter's.</p>"
         "<p>Ideal at first; rise and fall time, overshoot, ripple, noise and drift make it a worse one.</p>"
         "<p><b>Example:</b> a dual supply to try a setup before the real one is there.</p>") },
    { NodeKinds, Calculated, "qtdmm-calculated", tr("Calculate"), tr("Formula"),
      tr("<p>Computes up to four values from the readings of your devices with formulas of your own: "
         "a power from a voltage and a current, a resistance, a dew point.</p>"
         "<p><b>Inputs:</b> one or more devices, each with all its values (<tt>v1</tt>, <tt>v1.2</tt> ...).</p>"
         "<p><b>Example:</b> <tt>v1*v2</tt> in W - a supply's voltage times a load's current is the "
         "power the load takes.</p>") },
    { NodeKinds, Integral, "qtdmm-integration", tr("Calculate"), tr("Integral"),
      tr("<p>Sums one value up over time, from a start you choose: when a recording starts, when the value "
         "rises above a threshold, or by hand.</p>"
         "<p><b>Input:</b> one value of one device.</p>"
         "<p><b>Example:</b> a load's power in W gives the energy it took in Wh, a battery's current in A "
         "the charge in Ah.</p>") },
  };
  QHash<QString, QTreeWidgetItem *> groups;
  for (const auto &kind : nodeKinds)
  {
    QTreeWidgetItem *&group = groups[kind.group];
    if (!group)
    {
      group = new QTreeWidgetItem(m_kindList, { kind.group });
      group->setFlags(Qt::ItemIsEnabled);
      QFont font = m_kindList->font();
      font.setBold(true);
      group->setFont(0, font);
    }
    auto *item = new QTreeWidgetItem(group, { kind.name });
    item->setIcon(0, QIcon::fromTheme(kind.icon));
    item->setData(0, Qt::UserRole, int(kind.id));
    item->setData(0, Qt::UserRole + 1, kind.about);
    item->setData(0, Qt::UserRole + 2, int(kind.tile));
  }
  m_kindList->expandAll();
  kindLayout->addWidget(m_kindList, 2);
  auto *aboutArea = new QScrollArea(kindPage);
  aboutArea->setWidgetResizable(true);
  m_kindAbout = new QLabel(aboutArea);
  m_kindAbout->setObjectName("ui_kindAbout");
  m_kindAbout->setWordWrap(true);
  m_kindAbout->setTextFormat(Qt::RichText);
  m_kindAbout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  m_kindAbout->setMargin(fontMetrics().height() / 2);
  aboutArea->setWidget(m_kindAbout);
  kindLayout->addWidget(aboutArea, 3);
  connect(m_kindList, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *item)
  {
    m_kindAbout->setText(item && item->parent()
                           ? "<h3>" + item->text(0).toHtmlEscaped() + "</h3>" + item->data(0, Qt::UserRole + 1).toString()
                           : QString());
    updateButtons();
  });
  connect(m_kindList, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item)
  {
    if (item->parent())
      next();
  });
  m_pages->addWidget(kindPage);

  auto *buttons = new QDialogButtonBox(this);
  m_back = buttons->addButton(tr("< &Back"), QDialogButtonBox::ActionRole);
  m_next = buttons->addButton(tr("&Next >"), QDialogButtonBox::ActionRole);
  m_next->setDefault(true);
  buttons->addButton(QDialogButtonBox::Cancel);
  connect(m_back, &QPushButton::clicked, this, &AddDeviceDlg::back);
  connect(m_next, &QPushButton::clicked, this, &AddDeviceDlg::next);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  top->addWidget(buttons);

  // the kinds of the tile Calculated until another one leads there
  searchKinds(QString());
  showPage(ConnectionPage);
  resize(660, 560);
}

QToolButton *AddDeviceDlg::tile(const QString &icon, const QString &text, const QString &toolTip)
{
  auto *button = new TileButton(this);
  button->setIcon(QIcon::fromTheme(icon));
  button->setIconSize(QSize(48, 48));
  button->setText(text);
  button->setToolTip(toolTip);
  button->setFixedSize(kTileSize);
  return button;
}

void AddDeviceDlg::setPorts(const QStringList &ports)
{
  m_settings->setPorts(ports);
}

void AddDeviceDlg::setSigrokExe(const QString &exe)
{
  m_settings->setSigrokExe(exe);
}

bool AddDeviceDlg::offers(Connection connection, const DmmDecoder::DMMInfo &info)
{
  const ProtocolInfo *protocol = protocolInfo(info.protocol);
  const bool bluetooth = info.baud == 0 && protocol && QLatin1String(protocol->transport) == QLatin1String("Bluetooth LE");
  const bool sigrok = info.protocol == FrameFormat::Sigrok && !info.sigrokDriver.isEmpty();
  switch (connection)
  {
    case Bluetooth: return bluetooth;
    case Sigrok:    return sigrok;
    case Simulated:
    case SimulatedPsu:
    case Calculated:
    case Integral:
    case NodeKinds:
    case SimKinds:  return false;   // logic nodes, on the node page
    case Cable:
    case Network:   return !bluetooth && !sigrok;   // the bridge passes a cable on
  }
  return false;
}

bool AddDeviceDlg::hasPortPage(Connection connection)
{
  return connection == Cable || connection == Bluetooth || connection == Network;
}

bool AddDeviceDlg::isNode(Connection connection)
{
  return connection == Simulated || connection == SimulatedPsu || connection == Calculated || connection == Integral;
}

void AddDeviceDlg::chooseConnection(Connection connection)
{
  m_connection = connection;
  if (connection == NodeKinds || connection == SimKinds)
  {
    // the tile's kinds only; the search anew
    if (m_kindTile != connection)
    {
      m_kindTile = connection;
      m_kindSearch->clear();
      searchKinds(QString());
    }
    if (markedKind() == NodeKinds && !nodeKinds().isEmpty())
      markKind(nodeKinds().constFirst());
    showPage(KindPage);
    return;
  }
  // a logic node: its settings on a page of their own, a new editor each time
  if (isNode(connection))
  {
    markKind(connection);
    delete m_editor;
    m_editor = m_makeEditor ? m_makeEditor(connection) : nullptr;
    if (!m_editor)
    {
      if (connection == Simulated)
      {
        auto *e = new GeneratorNodeEditor;
        e->load(tr("Simulated"), GeneratorNode());
        m_editor = e;
      }
      else if (connection == SimulatedPsu)
      {
        auto *e = new SimPsuNodeEditor;
        e->load(tr("Power supply"), SimPsuNode());
        m_editor = e;
      }
      else if (connection == Integral)
      {
        auto *e = new IntegralNodeEditor;
        e->load(QString(), IntegralNode());
        m_editor = e;
      }
      else
      {
        auto *e = new FormulaNodeEditor;
        e->load(tr("Calculated"), FormulaNode());
        m_editor = e;
      }
    }
    m_editor->setParent(m_nodePage);
    m_nodePage->layout()->addWidget(m_editor);
    connect(m_editor, &NodeEditor::validChanged, this, &AddDeviceDlg::updateButtons);
    showPage(NodePage);
    return;
  }
  const bool manual = connection == Cable || connection == Network;
  m_settings->setModelFilter([connection](const DmmDecoder::DMMInfo &info) { return offers(connection, info); }, manual);
  QVariantMap keys;
  if (!manual && !m_settings->models().isEmpty())
    keys.insert("DMM/model", m_settings->models().first());
  m_settings->load(keys);
  m_name->clear();
  m_name->setModified(false);
  m_model.clear();
  m_known.clear();
  suggestName();
  if (hasPortPage(connection))
  {
    m_port->clear();
    m_port->setPlaceholderText(connection == Bluetooth ? tr("Bluetooth address, e.g. AA:BB:CC:DD:EE:FF")
                               : connection == Network ? tr("host:port, e.g. 192.168.1.20:4711")
                                                       : tr("e.g. /dev/ttyUSB0 or COM3"));
    showPage(PortPage);
    startSearch();
  }
  else
    showPage(DevicePage);
}

AddDeviceDlg::~AddDeviceDlg()
{
  stopProbe();
  stopSearch();
}

void AddDeviceDlg::setBluetoothCheck(std::function<QString()> check)
{
  m_bluetoothCheck = std::move(check);
  updateBluetoothTile();
}

void AddDeviceDlg::updateBluetoothTile()
{
  const QString why = m_bluetoothCheck ? m_bluetoothCheck() : QString();
  m_bluetoothTile->setEnabled(why.isEmpty());
  m_bluetoothTile->setToolTip(why.isEmpty() ? m_bluetoothTip : tr("Bluetooth cannot be used: %1").arg(why));
}

void AddDeviceDlg::startSearch()
{
  stopProbe();
  stopSearch();
  m_searchError.clear();
  m_probed.clear();
  m_found.clear();
  m_cards->clear();
  for (Discoverer *d : m_makeDiscoverers ? m_makeDiscoverers(m_connection) : QList<Discoverer *>())
  {
    d->setParent(this);
    connect(d, &Discoverer::found, this, &AddDeviceDlg::addCandidate);
    connect(d, &Discoverer::finished, this, [this, d]
    {
      if (!d->error().isEmpty())
        m_searchError = d->error();
      m_running.removeAll(d);
      d->deleteLater();
      updateHint();
    });
    m_running << d;
  }
  for (Discoverer *d : QList<Discoverer *>(m_running))
    d->start();
  updateHint();
}

void AddDeviceDlg::stopSearch()
{
  for (Discoverer *d : QList<Discoverer *>(m_running))
    d->stop();
}

// what the port field shows for a find: the port without "SERIAL", the
// Bluetooth address, host:port without "RFC2217"
static QString portText(const Candidate &c)
{
  if (c.kind == Candidate::Bluetooth)
    return c.keys.value("Port settings/ble-address").toString().section(' ', 0, 0);
  const QString device = c.keys.value("Port settings/device").toString();
  return device.startsWith(QLatin1String("SERIAL ")) || device.startsWith(QLatin1String("RFC2217 "))
           ? device.section(' ', 1) : device;
}

void AddDeviceDlg::addCandidate(const Candidate &c)
{
  for (const Candidate &f : m_found)
    if (f.key == c.key && f.kind == c.kind)
      return;
  // recognised meters first, then the ports nothing is known of, each in
  // the order they came
  const bool recognised = !c.models.isEmpty();
  int at = int(m_found.size());
  if (recognised)
    for (int i = 0; i < m_found.size(); ++i)
      if (m_found[i].models.isEmpty())
      {
        at = i;
        break;
      }
  m_found.insert(at, c);

  // symbols of the sets QtDMM brings (assets/icons/sets)
  static const char *icons[] = { "qtdmm-dmm", "network-wired", "qtdmm-dmm", "network-server" };
  QString text = c.title + '\n' + c.detail;
  const QString known = m_library ? m_library->findByPlace(c.keys) : QString();
  if (const std::optional<MyDevice> d = m_library ? m_library->find(known) : std::nullopt)
    text += '\n' + tr("Already in My devices: %1").arg(d->name);
  const QString inUse = m_inUse.value(DeviceLibrary::place(c.keys));
  if (!inUse.isEmpty())
    text += '\n' + inUse;
  if (!c.problem.isEmpty())
    text += '\n' + c.problem;
  const QIcon symbol = symbolOf(c.models, m_cards->iconSize(), devicePixelRatioF());
  auto *item = new QListWidgetItem(symbol.isNull() ? QIcon::fromTheme(icons[c.kind]) : symbol, text);
  item->setData(Qt::UserRole, c.key);
  if (!c.problem.isEmpty())
    item->setForeground(palette().color(QPalette::Disabled, QPalette::Text));
  m_cards->insertItem(at, item);
  m_cards->setCurrentRow(chosenCandidate());
  updateHint();
}

QStringList AddDeviceDlg::candidates() const
{
  QStringList keys;
  for (const Candidate &c : m_found)
    keys << c.key;
  return keys;
}

void AddDeviceDlg::chooseCandidate(const QString &key)
{
  for (int i = 0; i < m_found.size(); ++i)
    if (m_found[i].key == key)
    {
      m_port->setText(portText(m_found[i]));
      if (m_found[i].probe != FrameFormat::Invalid && !m_probed.contains(key))
        startProbe(i);
    }
}

QStringList AddDeviceDlg::candidateModels(const QString &key) const
{
  for (const Candidate &c : m_found)
    if (c.key == key)
      return c.models;
  return {};
}

void AddDeviceDlg::startProbe(int row)
{
  stopProbe();
  // the scan and a connection get in each other's way
  stopSearch();
  const Candidate c = m_found[row];
  m_probed << c.key;
  m_probe = new FrameProbe(m_makeProbePort ? m_makeProbePort(c) : nullptr, c.probe, this);
  m_probe->setObjectName(c.key);
  m_probeHint = c.hint;
  m_found[row].hint = tr("Listening to the meter: its first reading tells which model it is ...");
  connect(m_probe, &FrameProbe::finished, this, [this, key = c.key](const QStringList &models)
  {
    for (Candidate &f : m_found)
    {
      if (f.key != key)
        continue;
      if (models.isEmpty())
        f.hint = tr("No reading came, so the model is not known: choose it on the next page.");
      else
      {
        // the models it names first, in their order
        QStringList rest = f.models;
        for (const QString &m : models)
          rest.removeAll(m);
        f.models = models + rest;
        QStringList names;
        for (const QString &m : models)
          names << QString(m).remove(" *");
        f.hint = tr("Its first reading says: %1.").arg(names.join(tr(" or ")));
        const QIcon symbol = symbolOf(models, m_cards->iconSize(), devicePixelRatioF());
        for (int i = 0; i < m_cards->count() && !symbol.isNull(); ++i)
          if (m_cards->item(i)->data(Qt::UserRole).toString() == key)
            m_cards->item(i)->setIcon(symbol);
      }
    }
    m_probe->deleteLater();
    m_probe = nullptr;
    updateHint();
  });
  m_probe->start();
  updateHint();
}

void AddDeviceDlg::stopProbe()
{
  if (!m_probe)
    return;
  // ended before it heard anything: as before, listened to again when chosen
  const QString key = m_probe->objectName();
  m_probed.removeAll(key);
  for (Candidate &f : m_found)
    if (f.key == key)
      f.hint = m_probeHint;
  delete m_probe;
  m_probe = nullptr;
}

void AddDeviceDlg::setPort(const QString &port)
{
  m_port->setText(port);
}

int AddDeviceDlg::chosenCandidate() const
{
  const QString port = m_port->text().trimmed();
  if (port.isEmpty())
    return -1;
  for (int i = 0; i < m_found.size(); ++i)
    if (portText(m_found[i]).compare(port, Qt::CaseInsensitive) == 0)
      return i;
  return -1;
}

void AddDeviceDlg::updateHint()
{
  m_progress->setVisible(!m_running.isEmpty());
  m_searchAgain->setEnabled(m_running.isEmpty());
  const int row = chosenCandidate();
  m_fix->setVisible(row >= 0 && !m_found[row].problem.isEmpty());
  if (row >= 0)
  {
    const Candidate &c = m_found[row];
    const QString known = m_library ? m_library->findByPlace(c.keys) : QString();
    const std::optional<MyDevice> d = m_library ? m_library->find(known) : std::nullopt;
    m_hint->setText(!c.problem.isEmpty() ? c.problem
                    : d                  ? tr("This is \"%1\" of My devices: the next page changes it, "
                                              "no second entry.").arg(d->name)
                                         : c.hint);
  }
  else if (!m_running.isEmpty())
    m_hint->setText(tr("Searching ..."));
  else if (m_found.isEmpty() && !m_searchError.isEmpty())
    m_hint->setText(tr("The search failed (%1). Or type the address.").arg(m_searchError));
  else if (m_found.isEmpty())
    m_hint->setText(m_connection == Bluetooth
                    ? tr("Nothing found. Is the meter's Bluetooth on and no other program connected to it? "
                         "Or type its address.")
                    : m_connection == Network
                    ? tr("No bridge found. Is qtdmm-bridge running with --mdns in this network? "
                         "Or type host:port of the bridge or another RFC 2217 server.")
                    : tr("Nothing found. Is the meter switched on and its cable plugged in? Or type the port."));
  else
    m_hint->setText(m_port->text().trimmed().isEmpty() ? tr("Choose where the meter is, or type the port.") : QString());
}

void AddDeviceDlg::takePort()
{
  stopProbe();
  stopSearch();
  const int row = chosenCandidate();
  const QString port = m_port->text().trimmed();
  QVariantMap found;
  QString model;
  if (row >= 0)
  {
    found = m_found[row].keys;
    if (!m_found[row].models.isEmpty())
      model = m_found[row].models.first();
  }
  else if (m_connection == Bluetooth)
    found.insert("Port settings/ble-address", port);
  else
    found.insert("Port settings/device", port.contains(' ')                             ? port
                                         : m_connection == Network                          ? "RFC2217 " + port
                                         : port.startsWith(QLatin1String("/dev/usbtmc")) ? "USBTMC " + port
                                                                                            : "SERIAL " + port);

  m_known = m_library ? m_library->findByPlace(found) : QString();
  const std::optional<MyDevice> entry = m_library ? m_library->find(m_known) : std::nullopt;
  QVariantMap keys = found;
  if (entry)
  {
    // the entry as it is, at the place found now
    keys = entry->keys;
    for (const char *key : { "Port settings/device", "Port settings/ble-address" })
      if (!found.value(key).toString().isEmpty())
        keys.insert(key, found.value(key));
  }
  else if (!model.isEmpty())
    keys.insert("DMM/model", model);
  else if (m_connection == Bluetooth && !m_settings->models().isEmpty())
    keys.insert("DMM/model", m_settings->models().first());
  m_settings->load(keys);
  m_model.clear();
  if (entry)
  {
    m_name->setText(entry->name);
    m_name->setModified(true);   // its name, not one after the model
    m_model = keys.value("DMM/model").toString();
  }
  else
  {
    m_name->clear();
    m_name->setModified(false);
    suggestName();
  }
}

QString AddDeviceDlg::knownDevice() const
{
  const std::optional<MyDevice> entry = m_library ? m_library->find(m_known) : std::nullopt;
  if (!entry)
    return QString();
  // another model than the entry's at this place: another meter, a new entry
  const QString model = m_settings->keys().value("DMM/model").toString();
  return QString(entry->model()).remove(" *") == QString(model).remove(" *") ? entry->id : QString();
}

QList<AddDeviceDlg::Connection> AddDeviceDlg::nodeKinds() const
{
  QList<Connection> kinds;
  for (QTreeWidgetItemIterator it(m_kindList, QTreeWidgetItemIterator::NotHidden); *it; ++it)
    if ((*it)->parent())
      kinds << static_cast<Connection>((*it)->data(0, Qt::UserRole).toInt());
  return kinds;
}

AddDeviceDlg::Connection AddDeviceDlg::markedKind() const
{
  const QTreeWidgetItem *item = m_kindList->currentItem();
  return item && item->parent() && !item->isHidden() ? static_cast<Connection>(item->data(0, Qt::UserRole).toInt())
                                                     : NodeKinds;
}

void AddDeviceDlg::markKind(Connection kind)
{
  for (QTreeWidgetItemIterator it(m_kindList); *it; ++it)
    if ((*it)->parent() && (*it)->data(0, Qt::UserRole).toInt() == int(kind))
      m_kindList->setCurrentItem(*it);
}

void AddDeviceDlg::searchKinds(const QString &text)
{
  const QString wanted = text.trimmed();
  for (int g = 0; g < m_kindList->topLevelItemCount(); ++g)
  {
    QTreeWidgetItem *group = m_kindList->topLevelItem(g);
    bool any = false;
    for (int i = 0; i < group->childCount(); ++i)
    {
      QTreeWidgetItem *item = group->child(i);
      QTextDocument about;
      about.setHtml(item->data(0, Qt::UserRole + 1).toString());
      // the kinds of the tile that led here, those the search finds
      const bool shown = item->data(0, Qt::UserRole + 2).toInt() == int(m_kindTile)
                         && (wanted.isEmpty() || item->text(0).contains(wanted, Qt::CaseInsensitive)
                             || about.toPlainText().contains(wanted, Qt::CaseInsensitive));
      item->setHidden(!shown);
      any = any || shown;
    }
    group->setHidden(!any);
  }
  // the mark on a kind still shown
  if (markedKind() == NodeKinds && !nodeKinds().isEmpty())
    markKind(nodeKinds().constFirst());
  else if (nodeKinds().isEmpty())
  {
    m_kindList->setCurrentItem(nullptr);
    m_kindAbout->setText(tr("Nothing found for \"%1\".").arg(wanted.toHtmlEscaped()));
  }
  updateButtons();
}

AddDeviceDlg::Page AddDeviceDlg::page() const
{
  return static_cast<Page>(m_pages->currentIndex());
}

bool AddDeviceDlg::canGoNext() const
{
  if (page() == PortPage)
  {
    const int row = chosenCandidate();
    return !m_port->text().trimmed().isEmpty() && (row < 0 || m_found[row].problem.isEmpty());
  }
  if (page() == NodePage)
    return m_editor && m_editor->valid();
  if (page() == KindPage)
    return markedKind() != NodeKinds;
  return page() == DevicePage && m_settings->isComplete() && !m_name->text().trimmed().isEmpty();
}

bool AddDeviceDlg::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == m_kindSearch && event->type() == QEvent::KeyPress)
  {
    auto *key = static_cast<QKeyEvent *>(event);
    switch (key->key())
    {
      case Qt::Key_Up:
      case Qt::Key_Down:
      case Qt::Key_PageUp:
      case Qt::Key_PageDown:
        QCoreApplication::sendEvent(m_kindList, event);
        return true;
      case Qt::Key_Return:
      case Qt::Key_Enter:
        if (m_next->isEnabled())
          m_next->click();
        return true;
      default:
        break;
    }
  }
  return QDialog::eventFilter(watched, event);
}

QVariantMap AddDeviceDlg::keys() const
{
  if (isNode(m_connection) && m_editor)
    return { { "DMM/model", m_editor->model() }, { "Node/json", m_editor->logicNode()->toJsonText() } };
  return m_settings->keys();
}

QString AddDeviceDlg::name() const
{
  if (isNode(m_connection) && m_editor)
    return m_editor->name().trimmed();
  return m_name->text().trimmed();
}

void AddDeviceDlg::back()
{
  if (page() == TargetPage)
    showPage(isNode(m_connection) ? NodePage : DevicePage);
  else if (page() == NodePage)
    showPage(KindPage);
  else if (page() == KindPage)
    showPage(ConnectionPage);
  else if (page() == DevicePage && hasPortPage(m_connection))
    showPage(PortPage);
  else if (page() == DevicePage || page() == PortPage)
  {
    stopProbe();
    stopSearch();
    showPage(ConnectionPage);
  }
}

void AddDeviceDlg::next()
{
  if (!canGoNext())
    return;
  if (page() == PortPage)
  {
    takePort();
    showPage(DevicePage);
  }
  else if (page() == KindPage)
    chooseConnection(markedKind());
  else
    showPage(TargetPage);
}

void AddDeviceDlg::showPage(Page page)
{
  m_pages->setCurrentIndex(page);
  switch (page)
  {
    case ConnectionPage:
      m_title->setText(tr("How is the meter connected?"));
      break;
    case KindPage:
      m_title->setText(m_kindTile == SimKinds ? tr("What is to be simulated?") : tr("What is to be calculated?"));
      break;
    case PortPage:       m_title->setText(tr("Where is it connected?")); break;
    case DevicePage:     m_title->setText(tr("Which meter is it?")); break;
    case TargetPage:     m_title->setText(tr("Connect it now?")); break;
    case NodePage:
      m_title->setText(m_connection == Simulated      ? tr("Which values to simulate?")
                       : m_connection == SimulatedPsu ? tr("Which power supply?")
                       : m_connection == Integral     ? tr("Which value to integrate?")
                                                  : tr("What to calculate?"));
      break;
  }
  if (page == TargetPage)
    updateDeviceInfo();
  if (page == DevicePage)
    m_name->setFocus();
  if (page == PortPage)
    m_port->setFocus();
  m_kindSearch->setVisible(page == KindPage);
  // typing filters at once; the arrows move in the list (eventFilter())
  if (page == KindPage)
    m_kindSearch->setFocus();
  updateButtons();
}

void AddDeviceDlg::updateButtons()
{
  m_back->setEnabled(page() != ConnectionPage);
  m_next->setVisible(page() == PortPage || page() == DevicePage || page() == NodePage || page() == KindPage);
  m_next->setEnabled(canGoNext());
}

void AddDeviceDlg::suggestName()
{
  if (m_name->isModified())
    return;
  const QString model = m_settings->keys().value("DMM/model").toString();
  if (model == m_model)
    return;
  m_model = model;
  const QString base = model == QLatin1String("Manual") ? tr("Meter")
                       : DmmDecoder::sameModel(model, QStringLiteral("QtDMM Simulated meter")) ? tr("Simulated meter")
                                                                                              : QString(model).remove(" *");
  m_name->setText(m_library ? m_library->uniqueName(base) : base);
}

void AddDeviceDlg::updateDeviceInfo()
{
  if (isNode(m_connection) && m_editor)
    m_summary->showNode(name(), m_editor->model());
  else
    m_summary->show(m_settings, m_name->text().trimmed());
}

void AddDeviceDlg::finish(Target target)
{
  stopSearch();
  m_target = target;
  accept();
}
