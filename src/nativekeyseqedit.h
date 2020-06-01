// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

// _Note_: This is custom implementation similar to QKeySequenceEdit. Unfortunately QKeySequence
// and QKeySequenceEdit do not support native key codes, which are needed if we want to
// emit key sequences on a configured input from a 'spotlight' device.
//
// There is also not public API in Qt that allows us to map Qt Keycodes back to system key codes
// and vice versa.

#include "deviceinput.h"

#include <QKeySequence>
#include <QWidget>

// -------------------------------------------------------------------------------------------------
class QStyleOptionFrame;

// -------------------------------------------------------------------------------------------------
class NativeKeySequence
{
public:
  NativeKeySequence() = default;
  NativeKeySequence(NativeKeySequence&&) = default;
  NativeKeySequence(const NativeKeySequence&) = default;
  NativeKeySequence(QKeySequence&&, KeyEventSequence&&);

  NativeKeySequence& operator=(NativeKeySequence&&) = default;
  NativeKeySequence& operator=(const NativeKeySequence&) = default;
  bool operator==(const NativeKeySequence& other) const;
  bool operator!=(const NativeKeySequence& other) const;


  auto count() const { return m_keySequence.count(); }
  bool empty() const { return count() == 0; }
  const auto& keySequence() const { return m_keySequence; }
  const auto& nativeSequence() const { return m_nativeSequence; }

  void clear();

  void swap(NativeKeySequence& other);

private:
  QKeySequence m_keySequence;
  KeyEventSequence m_nativeSequence;
};

// -------------------------------------------------------------------------------------------------
class NativeKeySeqEdit : public QWidget
{
  Q_OBJECT

public:
  NativeKeySeqEdit(QWidget* parent = nullptr);

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
  static int translateModifiers(Qt::KeyboardModifiers state);
  void recordKeyPressEvent(QKeyEvent* e);
  void reset();

  NativeKeySequence m_nativeSequence;
  std::vector<int> m_recordedKeys;
  std::vector<int> m_nativeModifiers;
  QKeySequence m_recordedSequence;
  KeyEventSequence m_recordedEvents;
  QTimer* m_timer = nullptr;
  int m_lastKey = -1;
  bool m_recording = false;
};
