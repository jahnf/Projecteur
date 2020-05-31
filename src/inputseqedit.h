// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include "deviceinput.h"

#include <QStyledItemDelegate>
#include <QWidget>

// -------------------------------------------------------------------------------------------------
class QKeySequenceEdit;
class QStyleOptionFrame;

// -------------------------------------------------------------------------------------------------
class InputSeqEdit : public QWidget
{
  Q_OBJECT

public:
  InputSeqEdit(QWidget* parent = nullptr);
  InputSeqEdit(InputMapper* im, QWidget* parent = nullptr);

  QSize sizeHint() const override;

  void setInputMapper(InputMapper* im);

  const KeyEventSequence& inputSequence() const;
  void setInputSequence(const KeyEventSequence& is);

  void clear();

signals:
  void inputSequenceChanged(const KeyEventSequence& inputSequence);
  void editingFinished(InputSeqEdit*);

protected:
  void paintEvent(QPaintEvent* e) override;
  void mouseDoubleClickEvent(QMouseEvent* e) override;
  void keyPressEvent(QKeyEvent* e) override;
  void keyReleaseEvent(QKeyEvent* e) override;
  void focusOutEvent(QFocusEvent* e) override;
  void initStyleOption(QStyleOptionFrame&) const;

private:
  InputMapper* m_inputMapper = nullptr;
  KeyEventSequence m_inputSequence;
  KeyEventSequence m_recordedSequence;
  uint8_t m_maxRecordingLength = 8; // = 8 KeyEvents, also equals 4 Button Presses (press + release)
};


// -------------------------------------------------------------------------------------------------
class InputSeqDelegate : public QStyledItemDelegate
{
  Q_OBJECT

public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter*, const QStyleOptionViewItem&, const QModelIndex&) const override;
  QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override;
  QWidget *createEditor(QWidget*, const QStyleOptionViewItem&, const QModelIndex&) const override;
  void setEditorData(QWidget* editor, const QModelIndex& index) const override;
  void setModelData(QWidget* editor, QAbstractItemModel*, const QModelIndex&) const override;

private:
  void commitAndCloseEditor(InputSeqEdit* editor);
};


// -------------------------------------------------------------------------------------------------
class KeySequenceDelegate : public QStyledItemDelegate
{
  Q_OBJECT

public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter*, const QStyleOptionViewItem&, const QModelIndex&) const override;
  QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override;
  QWidget *createEditor(QWidget*, const QStyleOptionViewItem&, const QModelIndex&) const override;
  void setEditorData(QWidget* editor, const QModelIndex& index) const override;
  void setModelData(QWidget* editor, QAbstractItemModel*, const QModelIndex&) const override;

private:
  void commitAndCloseEditor();
};

