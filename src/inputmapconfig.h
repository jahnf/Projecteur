// This file is part of Projecteur - https://github.com/jahnf/
// - See LICENSE.md and README.md
#pragma once

#include "device-defs.h"
#include "deviceinput.h"

#include <QAbstractTableModel>
#include <QPointer>
#include <QTableView>

// -------------------------------------------------------------------------------------------------

class ActionTypeDelegate;
class InputSeqDelegate;

// -------------------------------------------------------------------------------------------------
/// Item for the input map model.
struct InputMapModelItem {
  KeyEventSequence deviceSequence;
  std::shared_ptr<Action> action;
  bool isDuplicate = false;
};

// -------------------------------------------------------------------------------------------------
/// Input map configuration table model.
class InputMapConfigModel : public QAbstractTableModel
{
  Q_OBJECT

public:
  enum Roles { InputSeqRole = Qt::UserRole + 1, ActionTypeRole, NativeSeqRole };
  enum Columns { InputSeqCol = 0, ActionTypeCol, ActionCol, ColumnsCount};

  InputMapConfigModel(InputMapper* im, const DeviceId& dId, QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;

  void removeConfigItemRows(std::vector<int> rows);
  int addNewItem(std::shared_ptr<Action> action);

  const InputMapModelItem& configData(const QModelIndex& index) const;
  void setInputSequence(const QModelIndex& index, const KeyEventSequence& kes);
  void setKeySequence(const QModelIndex& index, const NativeKeySequence& ks);
  void setItemActionType(const QModelIndex& index, Action::Type type);

  InputMapper* inputMapper() const;
  void setInputMapper(InputMapper* im);

  InputMapConfig configuration() const;
  void setConfiguration(const InputMapConfig& config);

  const DeviceId& deviceId() const;
  void setDeviceId(const DeviceId& dId);

private:
  void configureInputMapper();
  void removeConfigItemRows(int fromRow, int toRow);
  void updateDuplicates();

  DeviceId m_currentDeviceId;
  QPointer<InputMapper> m_inputMapper;
  QVector<InputMapModelItem> m_configItems;
  std::map<KeyEventSequence, int> m_duplicates;
};

// -------------------------------------------------------------------------------------------------
/// Input map configuration view.
struct InputMapConfigView : public QTableView
{
  Q_OBJECT

public:
  InputMapConfigView(QWidget* parent = nullptr);

  void setModel(QAbstractItemModel* model) override;

protected:
  void keyPressEvent(QKeyEvent* e) override;

private:
  ActionTypeDelegate* m_actionTypeDelegate = nullptr;
};

