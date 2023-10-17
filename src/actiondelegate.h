// This file is part of Projecteur - https://github.com/jahnf/projecteur
// - See LICENSE.md and README.md
#pragma once

#include <QStyledItemDelegate>

// -------------------------------------------------------------------------------------------------
struct Action;
class InputMapConfigModel;

// -------------------------------------------------------------------------------------------------
class ActionDelegate : public QStyledItemDelegate
{
  Q_OBJECT

public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter*, const QStyleOptionViewItem&, const QModelIndex&) const override;
  QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override;
  QWidget* createEditor(QWidget*, const QStyleOptionViewItem&, const QModelIndex&) const override;
  void setEditorData(QWidget* editor, const QModelIndex& index) const override;
  void setModelData(QWidget* editor, QAbstractItemModel*, const QModelIndex&) const override;
  void actionContextMenu(QWidget* parent, InputMapConfigModel* model, const QModelIndex& index,
                         const QPoint& globalPos);

protected:
  bool eventFilter(QObject* obj, QEvent* ev) override;

private:
  QWidget* createEditor(QWidget* parent, const Action* action) const;
  void commitAndCloseEditor(QWidget* editor);
  void commitAndCloseEditor_();
};

// -------------------------------------------------------------------------------------------------
class ActionTypeDelegate : public QStyledItemDelegate
{
  Q_OBJECT

public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter*, const QStyleOptionViewItem&, const QModelIndex&) const override;
  void actionContextMenu(QWidget* parent, InputMapConfigModel* model, const QModelIndex& index,
                         const QPoint& globalPos);

private:
  static int drawActionTypeSymbol(int startX, QPainter& p, const QStyleOptionViewItem& option,
                                  const QChar& symbol);
};
