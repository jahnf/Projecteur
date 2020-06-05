// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

// _Note_: This is custom implementation similar to QKeySequenceEdit. Unfortunately QKeySequence
// and QKeySequenceEdit do not support native key codes, which are needed if we want to
// emit key sequences via the uinput device.
//
// There is also no public API in Qt that allows us to map Qt Keycodes back to system key codes
// and vice versa.

#include "deviceinput.h"

#include <QKeySequence>
#include <QStyledItemDelegate>
#include <QWidget>

#include <set>

// -------------------------------------------------------------------------------------------------
class QStyleOptionFrame;

// -------------------------------------------------------------------------------------------------
class NativeKeySeqEdit : public QWidget
{
  Q_OBJECT

public:
  NativeKeySeqEdit(QWidget* parent = nullptr);
  virtual ~NativeKeySeqEdit();

  QSize sizeHint() const override;

  const NativeKeySequence& keySequence() const;
  void setKeySequence(const NativeKeySequence& nks);

  bool recording() const { return m_recording; }
  void setRecording(bool doRecord);

  void clear();

signals:
  void recordingChanged(bool);
  void keySequenceChanged(const NativeKeySequence& keySequence);
  void editingFinished(NativeKeySeqEdit*);

protected:
  void paintEvent(QPaintEvent* e) override;
  void mouseDoubleClickEvent(QMouseEvent* e) override;
  bool event(QEvent* e) override;
  void keyPressEvent(QKeyEvent* e) override;
  void keyReleaseEvent(QKeyEvent* e) override;
  void focusOutEvent(QFocusEvent* e) override;
  void initStyleOption(QStyleOptionFrame&) const;

private:
  static int getQtModifiers(Qt::KeyboardModifiers state);
  static uint16_t getNativeModifiers(const std::set<int>& modifiersPressed);
  void recordKeyPressEvent(QKeyEvent* e);
  void reset();

  NativeKeySequence m_nativeSequence;
  std::vector<int> m_recordedQtKeys;
  std::vector<uint16_t> m_recordedNativeModifiers;
  std::set<int> m_nativeModifiersPressed;
  KeyEventSequence m_recordedEvents;
  QTimer* m_timer = nullptr;
  int m_lastKey = -1;
  bool m_recording = false;
};

// -------------------------------------------------------------------------------------------------
class NativeKeySeqDelegate : public QStyledItemDelegate
{
  Q_OBJECT

public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter*, const QStyleOptionViewItem&, const QModelIndex&) const override;
  QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override;
  QWidget *createEditor(QWidget*, const QStyleOptionViewItem&, const QModelIndex&) const override;
  void setEditorData(QWidget* editor, const QModelIndex& index) const override;
  void setModelData(QWidget* editor, QAbstractItemModel*, const QModelIndex&) const override;

signals:
  void editingStarted() const;

protected:
  bool eventFilter(QObject* obj, QEvent* ev) override;

private:
  void commitAndCloseEditor(NativeKeySeqEdit* editor);
};
