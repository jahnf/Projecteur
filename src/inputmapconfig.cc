// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "inputmapconfig.h"

#include "inputseqedit.h"
#include "logging.h"

#include <QHeaderView>
#include <QKeyEvent>

// -------------------------------------------------------------------------------------------------
namespace  {
  const InputMapModelItem invalidItem_;
}

// -------------------------------------------------------------------------------------------------
InputMapConfigModel::InputMapConfigModel(QObject* parent)
  : InputMapConfigModel(nullptr, parent)
{}

// -------------------------------------------------------------------------------------------------
InputMapConfigModel::InputMapConfigModel(InputMapper* im, QObject* parent)
  : QAbstractTableModel(parent)
  , m_inputMapper(im)
{}

// -------------------------------------------------------------------------------------------------
int InputMapConfigModel::rowCount(const QModelIndex& parent) const
{
  return ( parent == QModelIndex() ) ? m_configItems.size() : 0;
}

// -------------------------------------------------------------------------------------------------
int InputMapConfigModel::columnCount(const QModelIndex& /*parent*/) const
{
  return ColumnsCount;
}

// -------------------------------------------------------------------------------------------------
Qt::ItemFlags InputMapConfigModel::flags(const QModelIndex &index) const
{
  if (index.column() == InputSeqCol)
    return (QAbstractTableModel::flags(index) | Qt::ItemIsEditable);
  else if (index.column() == ActionCol)
    return (QAbstractTableModel::flags(index) | Qt::ItemIsEditable);

  return QAbstractTableModel::flags(index) & ~Qt::ItemIsEditable;
}

// -------------------------------------------------------------------------------------------------
QVariant InputMapConfigModel::data(const QModelIndex& index, int role) const
{
  if (index.row() >= static_cast<int>(m_configItems.size()))
    return QVariant();

  if (index.column() == InputSeqCol && role == Roles::InputSeqRole) {
    return QVariant::fromValue(m_configItems[index.row()].sequence);
  }
  else if (index.column() == ActionCol && role == Qt::DisplayRole) {
    return m_configItems[index.row()].keySequence;
  }

  return QVariant();
}

// -------------------------------------------------------------------------------------------------
QVariant InputMapConfigModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
  {
    switch(section)
    {
    case InputSeqCol: return tr("Input Sequence");
    //case ActionTypeCol: return tr("Type");
    case ActionCol: return tr("Mapped Key(s)");
    }
  }
  else if (orientation == Qt::Vertical && role == Qt::DisplayRole)
  {
    return section;
  }
  return QVariant();
}

// -------------------------------------------------------------------------------------------------
const InputMapModelItem& InputMapConfigModel::configData(const QModelIndex& index) const
{
  if (index.row() >= static_cast<int>(m_configItems.size()))
    return invalidItem_;

  return m_configItems[index.row()];
}

// -------------------------------------------------------------------------------------------------
void InputMapConfigModel::removeConfigItemRows(int fromRow, int toRow)
{
  if (fromRow > toRow) return;

  beginRemoveRows(QModelIndex(), fromRow, toRow);
  for (int i = toRow; i >= fromRow && i < m_configItems.size(); --i) {
    m_configItems.removeAt(i);
  }
  endRemoveRows();
}

// -------------------------------------------------------------------------------------------------
int InputMapConfigModel::addConfigItem(const InputMapModelItem& cfg)
{
  const auto row = m_configItems.size();
  beginInsertRows(QModelIndex(), row, row);
  m_configItems.push_back(cfg);
  endInsertRows();
  return row;
}

// -------------------------------------------------------------------------------------------------
void InputMapConfigModel::removeConfigItemRows(std::vector<int> rows)
{
  if (rows.empty()) return;
  std::sort(rows.rbegin(), rows.rend());

  int seq_last = rows.front();
  int seq_first = seq_last;

  for (auto it = ++rows.cbegin(); it != rows.cend(); ++it)
  {
    if (seq_first - *it > 1)
    {
      removeConfigItemRows(seq_first, seq_last);
      seq_last = seq_first = *it;
    }
    else
    {
      seq_first = *it;
    }
  }

  removeConfigItemRows(seq_first, seq_last);
}

// -------------------------------------------------------------------------------------------------
void InputMapConfigModel::setInputSequence(const QModelIndex& index, const KeyEventSequence& kes)
{
  if (index.row() < static_cast<int>(m_configItems.size()))
  {
    auto& c = m_configItems[index.row()];
    if (c.sequence != kes)
    {
      c.sequence = kes;
      emit dataChanged(index, index, {Qt::DisplayRole, Roles::InputSeqRole});
    }
  }
}

// -------------------------------------------------------------------------------------------------
void InputMapConfigModel::setKeySequence(const QModelIndex& index, const QKeySequence& ks)
{
  if (index.row() < static_cast<int>(m_configItems.size()))
  {
    auto& c = m_configItems[index.row()];
    if (c.keySequence != ks)
    {
      c.keySequence = ks;
      emit dataChanged(index, index, {Qt::DisplayRole, Roles::InputSeqRole});
    }
  }
}

// -------------------------------------------------------------------------------------------------
InputMapper* InputMapConfigModel::inputMapper() const
{
  return m_inputMapper;
}

// -------------------------------------------------------------------------------------------------
void InputMapConfigModel::setInputMapper(InputMapper* im)
{
  m_inputMapper = im;
}

// -------------------------------------------------------------------------------------------------
InputMapConfig InputMapConfigModel::configuration() const
{
  InputMapConfig config;

  for (const auto& item : m_configItems)
  {
    if (item.sequence.size() == 0) continue;
    if (item.keySequence.count() == 0) continue;
    config.emplace(item.sequence, MappedInputAction{item.keySequence});
  }

  return config;
}

// -------------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------------
InputMapConfigView::InputMapConfigView(QWidget* parent)
  : QTableView(parent)
{
  // verticalHeader()->setHidden(true);

  const auto imSeqDelegate = new InputSeqDelegate(this);
  setItemDelegateForColumn(InputMapConfigModel::InputSeqCol, imSeqDelegate);

  const auto keySeqDelegate = new KeySequenceDelegate(this);
  setItemDelegateForColumn(InputMapConfigModel::ActionCol, keySeqDelegate);

  setSelectionMode(QAbstractItemView::SelectionMode::ExtendedSelection);
  setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
  horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::Stretch);
}

// -------------------------------------------------------------------------------------------------
void InputMapConfigView::setModel(QAbstractItemModel* model)
{
  QTableView::setModel(model);

//  if (const auto imModel = qobject_cast<InputSeqMapConfigModel*>(model)) {
//    horizontalHeader()->setSectionResizeMode(InputSeqMapConfigModel::ActionTypeCol, QHeaderView::ResizeToContents);
//    horizontalHeader()->setSectionResizeMode(InputSeqMapConfigModel::ActionCol, QHeaderView::ResizeToContents);
//  }
}

//-------------------------------------------------------------------------------------------------
void InputMapConfigView::keyPressEvent(QKeyEvent* e)
{
  switch (e->key()) {
  case Qt::Key_Enter:
  case Qt::Key_Return:
    if (model()->flags(currentIndex()) & Qt::ItemIsEditable) {
      edit(currentIndex());
      return;
    }
    break;
  case Qt::Key_Delete:
    if (const auto imModel = qobject_cast<InputMapConfigModel*>(model()))
    switch (currentIndex().column())
    {
    case InputMapConfigModel::InputSeqCol:
      imModel->setInputSequence(currentIndex(), KeyEventSequence{});
      return;
    case InputMapConfigModel::ActionCol:
      imModel->setKeySequence(currentIndex(), QKeySequence());
      return;
    }
    break;
  case Qt::Key_Tab:
    e->ignore(); // Allow to change focus to other widgets in dialog.
    return;
  }

  QTableView::keyPressEvent(e);
}

