// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/dialogs/logdlg.h"

#include <QAbstractTableModel>
#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QStandardPaths>
#include <QTableView>
#include <QToolButton>
#include <QVBoxLayout>

namespace
{
enum Column { TimeColumn, ScopeColumn, LevelColumn, MessageColumn, Columns };

QString levelText(AppLog::Level level)
{
  switch (level)
  {
    case AppLog::Debug:   return LogDlg::tr("Debug");
    case AppLog::Info:    return LogDlg::tr("Info");
    case AppLog::Warning: return LogDlg::tr("Warning");
    case AppLog::Error:   return LogDlg::tr("Error");
  }
  return QString();
}
}

/// The log's lines; they come in queued, added() may come from any thread.
class LogModel : public QAbstractTableModel
{
public:
  LogModel(AppLog *log, QObject *parent) : QAbstractTableModel(parent), m_max(log->maxLines())
  {
    // connected first: a line in both is told by its number
    connect(log, &AppLog::added, this, &LogModel::append, Qt::QueuedConnection);
    m_entries = log->entries();
    if (!m_entries.isEmpty())
      m_last = m_entries.last().number;
  }
  int rowCount(const QModelIndex &parent = QModelIndex()) const override { return parent.isValid() ? 0 : int(m_entries.size()); }
  int columnCount(const QModelIndex &parent = QModelIndex()) const override { return parent.isValid() ? 0 : Columns; }
  QVariant data(const QModelIndex &index, int role) const override
  {
    const AppLog::Entry &e = m_entries[index.row()];
    if (role == Qt::DisplayRole)
      switch (index.column())
      {
        case TimeColumn:  return e.time.toString(QStringLiteral("hh:mm:ss.zzz"));
        case ScopeColumn: return e.scope;
        case LevelColumn: return levelText(e.level);
        default:          return e.message;
      }
    if (role == Qt::ToolTipRole)
      return index.column() == TimeColumn ? e.time.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz")) : e.message;
    if (role == Qt::ForegroundRole && e.level >= AppLog::Warning)
      return e.level == AppLog::Error ? QColor(0xe0, 0x50, 0x50) : QColor(0xd0, 0x90, 0x00);
    return QVariant();
  }
  QVariant headerData(int section, Qt::Orientation orientation, int role) const override
  {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
      return QVariant();
    const QString titles[] = { LogDlg::tr("Time"), LogDlg::tr("Scope"), LogDlg::tr("Level"), LogDlg::tr("Message") };
    return titles[section];
  }
  const AppLog::Entry &entry(int row) const { return m_entries[row]; }

private:
  void append(const AppLog::Entry &entry)
  {
    if (entry.number <= m_last)
      return;
    m_last = entry.number;
    const int row = int(m_entries.size());
    beginInsertRows(QModelIndex(), row, row);
    m_entries << entry;
    endInsertRows();
    if (m_entries.size() > m_max)
    {
      beginRemoveRows(QModelIndex(), 0, 0);
      m_entries.removeFirst();
      endRemoveRows();
    }
  }

  QList<AppLog::Entry> m_entries;
  qint64 m_last = 0;
  int    m_max;
};

/// The levels ticked.
class LogFilter : public QSortFilterProxyModel
{
public:
  using QSortFilterProxyModel::QSortFilterProxyModel;
  void setShown(AppLog::Level level, bool on)
  {
    m_hidden[level] = !on;
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();   // invalidateFilter() is deprecated from there on
    endFilterChange();
#else
    invalidateFilter();
#endif
  }
  bool has(int row, const QString &text) const
  {
    const AppLog::Entry &e = static_cast<LogModel *>(sourceModel())->entry(mapToSource(index(row, 0)).row());
    return e.message.contains(text, Qt::CaseInsensitive) || e.scope.contains(text, Qt::CaseInsensitive);
  }

protected:
  bool filterAcceptsRow(int row, const QModelIndex &) const override
  {
    return !m_hidden[static_cast<LogModel *>(sourceModel())->entry(row).level];
  }

private:
  bool m_hidden[4] = { true, false, false, false };   // Debug is for looking into a fault
};

LogDlg::LogDlg(AppLog *log, QWidget *parent) :
  QDialog(parent, Qt::Window),
  m_log(log)
{
  setObjectName("ui_logDlg");
  setWindowTitle(tr("QtDMM log"));
  resize(860, 420);

  auto *top = new QVBoxLayout(this);
  auto *bar = new QHBoxLayout;
  top->addLayout(bar);
  m_model = new LogModel(log, this);
  m_filter = new LogFilter(this);
  m_filter->setSourceModel(m_model);
  for (AppLog::Level level : { AppLog::Debug, AppLog::Info, AppLog::Warning, AppLog::Error })
  {
    auto *box = new QCheckBox(levelText(level), this);
    box->setChecked(level != AppLog::Debug);
    connect(box, &QCheckBox::toggled, this, [this, level](bool on) { m_filter->setShown(level, on); });
    bar->addWidget(box);
    m_levels.insert(level, box);
  }
  bar->addSpacing(fontMetrics().height());
  m_search = new QLineEdit(this);
  m_search->setObjectName("ui_logSearch");
  m_search->setPlaceholderText(tr("Search"));
  m_search->setClearButtonEnabled(true);
  bar->addWidget(m_search, 1);
  auto *previous = new QToolButton(this);
  previous->setArrowType(Qt::UpArrow);
  previous->setToolTip(tr("Previous match"));
  auto *next = new QToolButton(this);
  next->setArrowType(Qt::DownArrow);
  next->setToolTip(tr("Next match (Enter)"));
  bar->addWidget(previous);
  bar->addWidget(next);
  m_notFound = new QLabel(tr("Not found"), this);
  m_notFound->setVisible(false);
  bar->addWidget(m_notFound);
  auto *save = new QPushButton(tr("&Export..."), this);
  save->setAutoDefault(false);
  save->setToolTip(tr("All lines kept, also those not shown: to attach to a report"));
  bar->addWidget(save);
  connect(previous, &QToolButton::clicked, this, [this] { search(false); });
  connect(next, &QToolButton::clicked, this, [this] { search(true); });
  connect(m_search, &QLineEdit::returnPressed, this, [this] { search(true); });
  connect(m_search, &QLineEdit::textChanged, m_notFound, &QWidget::hide);
  connect(save, &QPushButton::clicked, this, &LogDlg::exportLog);

  m_table = new QTableView(this);
  m_table->setObjectName("ui_logTable");
  m_table->setModel(m_filter);
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setSelectionMode(QAbstractItemView::SingleSelection);
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->setShowGrid(false);
  m_table->setWordWrap(false);
  m_table->verticalHeader()->hide();
  m_table->verticalHeader()->setDefaultSectionSize(fontMetrics().height() + 4);
  const int em = fontMetrics().averageCharWidth();
  m_table->setColumnWidth(TimeColumn, 15 * em);
  m_table->setColumnWidth(ScopeColumn, 20 * em);
  m_table->setColumnWidth(LevelColumn, 10 * em);
  m_table->horizontalHeader()->setStretchLastSection(true);
  top->addWidget(m_table, 1);

  // at the end it stays there, else a line looked at stays where it is
  auto follow = [this]
  {
    QScrollBar *bar = m_table->verticalScrollBar();
    if (bar->value() >= bar->maximum() - 1)
      m_table->scrollToBottom();
  };
  connect(m_filter, &QAbstractItemModel::rowsInserted, this, follow);
  m_table->scrollToBottom();
}

void LogDlg::setLevelShown(AppLog::Level level, bool on)
{
  m_levels.value(level)->setChecked(on);
}

int LogDlg::rows() const
{
  return m_filter->rowCount();
}

int LogDlg::currentRow() const
{
  return m_table->currentIndex().isValid() ? m_table->currentIndex().row() : -1;
}

bool LogDlg::find(const QString &text, bool forward)
{
  const int count = m_filter->rowCount();
  if (text.isEmpty() || count == 0)
    return false;
  const int step = forward ? 1 : -1;
  int row = currentRow() >= 0 ? currentRow() : (forward ? -1 : count);
  for (int i = 0; i < count; ++i)
  {
    row = (row + step + count) % count;
    if (m_filter->has(row, text))
    {
      m_table->selectRow(row);
      m_table->scrollTo(m_filter->index(row, 0));
      return true;
    }
  }
  return false;
}

QString LogDlg::fileName(const QDate &date)
{
  return QStringLiteral("qtdmm-%1.log").arg(date.toString(Qt::ISODate));
}

void LogDlg::search(bool forward)
{
  m_notFound->setVisible(!find(m_search->text(), forward) && !m_search->text().isEmpty());
}

void LogDlg::exportLog()
{
  const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  const QString file = QFileDialog::getSaveFileName(this, tr("Export log"),
                                                    QDir(dir).filePath(fileName(QDate::currentDate())),
                                                    tr("Log files (*.log)"));
  if (file.isEmpty())
    return;
  QString error;
  if (!m_log->write(file, &error))
    QMessageBox::warning(this, tr("Export log"), tr("Could not write %1: %2").arg(QDir::toNativeSeparators(file), error));
}
