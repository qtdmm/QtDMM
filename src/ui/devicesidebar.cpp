// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/devicesidebar.h"

#include <QContextMenuEvent>
#include <QDropEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QTimer>

#include "core/devicelibrary.h"

namespace
{
// an instance item: running or not
constexpr int kRunning = Qt::UserRole + 1;

// the state of the device in use, as the status line's dot: readings
// coming in or not; the others get an empty one, so the names line up
QIcon dot(const QColor &color)
{
  QPixmap pixmap(12, 12);
  pixmap.fill(Qt::transparent);
  if (color.isValid())
  {
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawEllipse(QRectF(1, 1, 10, 10));
  }
  return QIcon(pixmap);
}
}

DeviceSidebar::DeviceSidebar(DeviceLibrary *library, QWidget *parent)
  : QTreeWidget(parent)
  , m_library(library)
{
  setObjectName("ui_deviceSidebar");
  setHeaderHidden(true);
  // the name, and the reading of an instance at the right
  setColumnCount(2);
  header()->setStretchLastSection(false);
  header()->setSectionResizeMode(0, QHeaderView::Stretch);
  header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  setRootIsDecorated(true);
  setDragDropMode(QAbstractItemView::InternalMove);
  setSelectionMode(QAbstractItemView::SingleSelection);
  // renamed with F2 (keyPressEvent) or the context menu, not by a click
  setEditTriggers(QAbstractItemView::NoEditTriggers);
  // only the node takes entries: they cannot become children of each other
  invisibleRootItem()->setFlags(Qt::ItemIsEnabled);
  m_devices = new QTreeWidgetItem(this, { tr("My devices") });
  m_devices->setFlags(Qt::ItemIsEnabled | Qt::ItemIsDropEnabled);
  QFont bold = m_devices->font(0);
  bold.setBold(true);
  m_devices->setFont(0, bold);
  m_devices->setFirstColumnSpanned(true);
  m_devices->setExpanded(true);
  m_instances = new QTreeWidgetItem(this, { tr("Instances") });
  m_instances->setFlags(Qt::ItemIsEnabled);
  m_instances->setFont(0, bold);
  m_instances->setFirstColumnSpanned(true);
  m_instances->setExpanded(true);

  connect(this, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item)
  {
    if (item && item == m_addItem)
    {
      Q_EMIT addDeviceRequested();
      return;
    }
    const QString id = idOf(item);
    if (!id.isEmpty() && id != m_current)
      Q_EMIT switchRequested(id);
    // a click brings a running instance to the front; starting a stopped
    // one takes a double click or its menu - no new window by a stray click
    const QString instance = instanceOf(item);
    if (!instance.isEmpty() && instance != m_ownInstance && instanceRunning(item))
      Q_EMIT instanceRequested(instance);
  });
  connect(this, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item)
  {
    const QString instance = instanceOf(item);
    if (!instance.isEmpty() && instance != m_ownInstance && !instanceRunning(item))
      Q_EMIT instanceRequested(instance);
  });
  // renamed in place
  connect(this, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item)
  {
    if (m_filling)
      return;
    if (item->parent() == m_instances)
    {
      // the new name only counts once it is checked; till then the old one
      const QString from = item->data(0, Qt::UserRole).toString();
      const QString to = item->text(0).trimmed();
      {
        QSignalBlocker block(this);
        item->setText(0, from);
      }
      if (!to.isEmpty() && to != from)
      {
        m_selectInstance = to;   // stays selected under its new name
        Q_EMIT renameInstanceRequested(from, to);
      }
      return;
    }
    const QString id = idOf(item);
    if (id.isEmpty())
      return;
    const std::optional<MyDevice> d = m_library->find(id);
    const QString name = item->text(0).trimmed();
    if (d && !name.isEmpty() && name != d->name)
      m_library->rename(id, name);
    else if (d)
    {
      QSignalBlocker block(this);
      item->setText(0, d->name);
    }
  });
  // later: a change in the middle of a drop must not take the items away
  connect(m_library, &DeviceLibrary::changed, this, &DeviceSidebar::fill, Qt::QueuedConnection);
  fill();
}

QString DeviceSidebar::idOf(const QTreeWidgetItem *item) const
{
  return item && item != m_addItem && item->parent() == m_devices ? item->data(0, Qt::UserRole).toString() : QString();
}

QString DeviceSidebar::instanceOf(const QTreeWidgetItem *item) const
{
  if (item && item->parent() && item->parent()->parent() == m_instances)
    item = item->parent();
  return item && item->parent() == m_instances ? item->data(0, Qt::UserRole).toString() : QString();
}

bool DeviceSidebar::instanceRunning(const QTreeWidgetItem *item) const
{
  const QTreeWidgetItem *own = instanceItem(instanceOf(item));
  return own && own->data(0, kRunning).toBool();
}

QTreeWidgetItem *DeviceSidebar::instanceItem(const QString &id) const
{
  for (int i = 0; i < m_instances->childCount(); ++i)
    if (m_instances->child(i)->data(0, Qt::UserRole).toString() == id)
      return m_instances->child(i);
  return nullptr;
}

void DeviceSidebar::setInstances(const QList<Instance> &instances, const QString &own)
{
  if (state() == QAbstractItemView::EditingState)
    return;
  m_filling = true;
  m_ownInstance = own;
  QStringList ids, shown;
  for (const Instance &instance : instances)
    ids << instance.id;
  for (int i = 0; i < m_instances->childCount(); ++i)
    shown << m_instances->child(i)->data(0, Qt::UserRole).toString();
  if (ids != shown)
  {
    const QString selected = m_selectInstance.isEmpty() ? instanceOf(currentItem()) : m_selectInstance;
    qDeleteAll(m_instances->takeChildren());
    for (const Instance &instance : instances)
    {
      auto *item = new QTreeWidgetItem(m_instances, { instance.id });
      item->setData(0, Qt::UserRole, instance.id);
      item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
      new QTreeWidgetItem(item);
      item->child(0)->setFirstColumnSpanned(true);   // a long name needs no room for a value
      if (instance.id == selected)
        setCurrentItem(item);
    }
  }
  m_selectInstance.clear();
  const QColor grey = palette().color(QPalette::Disabled, QPalette::WindowText);
  for (int i = 0; i < instances.size(); ++i)
  {
    const Instance &instance = instances[i];
    QTreeWidgetItem *item = m_instances->child(i);
    // renaming and deleting are for stopped ones; "default" has no name to change
    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (!instance.running && instance.id != QLatin1String("default"))
      flags |= Qt::ItemIsEditable;
    item->setFlags(flags);
    item->setData(0, kRunning, instance.running);
    item->setText(1, instance.value);
    item->setToolTip(0, instance.device.isEmpty() ? instance.id : QString("%1 · %2").arg(instance.id, instance.device));
    item->setForeground(1, instance.running ? palette().color(QPalette::Text) : grey);
    QFont font = item->font(0);
    font.setBold(instance.id == own);
    item->setFont(0, font);
    item->setIcon(0, dot(instance.active ? QColor(0x22, 0xaa, 0x22) : instance.running ? grey : QColor()));
    QTreeWidgetItem *device = item->child(0);
    device->setText(0, instance.device);
    device->setFlags(Qt::ItemIsEnabled);
    device->setForeground(0, grey);
    device->setHidden(instance.device.isEmpty());
    item->setExpanded(true);
  }
  m_filling = false;
}

void DeviceSidebar::fill()
{
  // a name being typed (Rename) would go: later, once it is done
  if (state() == QAbstractItemView::EditingState)
  {
    QTimer::singleShot(250, this, &DeviceSidebar::fill);
    return;
  }
  m_filling = true;
  const QString selected = idOf(currentItem());
  qDeleteAll(m_devices->takeChildren());
  m_addItem = nullptr;
  for (const MyDevice &d : m_library->list())
  {
    auto *item = new QTreeWidgetItem(m_devices, { d.name });
    item->setData(0, Qt::UserRole, d.id);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEditable);
    item->setFirstColumnSpanned(true);
    item->setToolTip(0, d.where().isEmpty() ? d.model() : QString("%1 · %2").arg(d.model(), d.where()));
    if (d.id == selected)
      setCurrentItem(item);
  }
  // a device more: the assistant, from where the devices are
  m_addItem = new QTreeWidgetItem(m_devices, { tr("+ Add device...") });
  m_addItem->setFlags(Qt::ItemIsEnabled);
  m_addItem->setFirstColumnSpanned(true);
  m_addItem->setForeground(0, palette().color(QPalette::Disabled, QPalette::WindowText));
  m_addItem->setToolTip(0, tr("Set up another meter (the assistant)."));
  m_devices->setExpanded(true);
  updateMarks();
  m_filling = false;
}

void DeviceSidebar::setCurrent(const QString &id, bool active)
{
  if (id == m_current && active == m_active)
    return;
  m_current = id;
  m_active = active;
  m_filling = true;
  updateMarks();
  m_filling = false;
}

void DeviceSidebar::updateMarks()
{
  for (int i = 0; i < m_devices->childCount(); ++i)
  {
    QTreeWidgetItem *item = m_devices->child(i);
    if (item == m_addItem)
      continue;
    const bool current = idOf(item) == m_current;
    QFont font = item->font(0);
    font.setBold(current);
    item->setFont(0, font);
    item->setIcon(0, dot(!current  ? QColor()
                         : m_active ? QColor(0x22, 0xaa, 0x22)
                                    : palette().color(QPalette::Disabled, QPalette::WindowText)));
  }
}

QTreeWidgetItem *DeviceSidebar::deviceItem(const QString &id) const
{
  for (int i = 0; i < m_devices->childCount(); ++i)
    if (idOf(m_devices->child(i)) == id)
      return m_devices->child(i);
  return nullptr;
}

QStringList DeviceSidebar::deviceIds() const
{
  QStringList ids;
  for (int i = 0; i < m_devices->childCount(); ++i)
    if (m_devices->child(i) != m_addItem)
      ids << idOf(m_devices->child(i));
  return ids;
}

void DeviceSidebar::dropEvent(QDropEvent *event)
{
  const QString id = idOf(currentItem());
  QTreeWidget::dropEvent(event);
  // the entry's new place in the node is its place in the list
  const int index = int(deviceIds().indexOf(id));
  if (!id.isEmpty() && index >= 0)
    m_library->move(id, index);
}

void DeviceSidebar::keyPressEvent(QKeyEvent *event)
{
  // F2 renames the device or the stopped instance, also from the device
  // line below an instance
  QTreeWidgetItem *item = currentItem();
  if (item && item->parent() && item->parent()->parent() == m_instances)
    item = item->parent();
  if (event->key() == Qt::Key_F2 && event->modifiers() == Qt::NoModifier && item
      && (item->flags() & Qt::ItemIsEditable))
  {
    editItem(item, 0);
    event->accept();
    return;
  }
  QTreeWidget::keyPressEvent(event);
}

void DeviceSidebar::instanceMenu(QTreeWidgetItem *item, const QPoint &pos)
{
  const QString id = instanceOf(item);
  if (item->parent() != m_instances)
    item = item->parent();
  const bool running = item->data(0, kRunning).toBool();
  QMenu menu(this);
  QAction *open = menu.addAction(running ? tr("Bring to &front") : tr("&Start"));
  open->setEnabled(id != m_ownInstance);
  menu.addSeparator();
  QAction *rename = menu.addAction(tr("&Rename"));
  QAction *remove = menu.addAction(QIcon::fromTheme("edit-delete"), tr("&Delete"));
  // a running instance keeps its name and its file
  rename->setEnabled(item->flags() & Qt::ItemIsEditable);
  remove->setEnabled(item->flags() & Qt::ItemIsEditable);
  if (running)
    for (QAction *a : { rename, remove })
      a->setToolTip(tr("Only for a stopped instance"));
  menu.setToolTipsVisible(true);
  QAction *chosen = menu.exec(pos);
  if (chosen == open)
    Q_EMIT instanceRequested(id);
  else if (chosen == rename)
    editItem(item);
  else if (chosen == remove
           && QMessageBox::question(this, tr("Delete instance"),
                                    tr("Delete the instance \"%1\" with its settings?").arg(id))
                == QMessageBox::Yes)
    Q_EMIT deleteInstanceRequested(id);
}

void DeviceSidebar::contextMenuEvent(QContextMenuEvent *event)
{
  QTreeWidgetItem *item = itemAt(event->pos());
  if (!instanceOf(item).isEmpty())
  {
    instanceMenu(item, event->globalPos());
    return;
  }
  const QString id = idOf(item);
  const std::optional<MyDevice> d = m_library->find(id);
  if (!d)
    return;
  QMenu menu(this);
  QAction *settings = menu.addAction(QIcon::fromTheme("configure"), tr("&Settings..."));
  QAction *rename = menu.addAction(tr("&Rename"));
  QAction *window = menu.addAction(QIcon::fromTheme("window-new"), tr("Open in a &new window"));
  menu.addSeparator();
  QAction *remove = menu.addAction(QIcon::fromTheme("list-remove"), tr("Re&move from My devices"));
  QAction *chosen = menu.exec(event->globalPos());
  if (chosen == settings)
    Q_EMIT settingsRequested(id);
  else if (chosen == rename)
    editItem(item);
  else if (chosen == window)
    Q_EMIT newWindowRequested(id);
  else if (chosen == remove
           && QMessageBox::question(this, tr("Remove device"), tr("Remove \"%1\" from My devices?").arg(d->name))
                == QMessageBox::Yes)
    m_library->remove(id);
}

bool DeviceSidebar::shownAtStart(const QVariant &stored, int devices, bool configured)
{
  if (!configured)
    return devices > 0;
  return stored.isValid() ? stored.toBool() : devices >= 2;
}

bool DeviceSidebar::opensAfterAdd(int devicesBefore, int devicesAfter)
{
  return devicesBefore < 2 && devicesAfter >= 2;
}
