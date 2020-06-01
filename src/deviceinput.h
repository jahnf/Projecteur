// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include <memory>

#include <QDataStream>
#include <QObject>
#include <QKeySequence>

class VirtualDevice;

// -------------------------------------------------------------------------------------------------
/// This is basically the input_event struct from linux/input.h without the time member
struct DeviceInputEvent
{
  DeviceInputEvent() = default;
  DeviceInputEvent(uint16_t type, uint16_t code, uint32_t value) : type(type), code(code), value(value) {}
  DeviceInputEvent(const struct input_event& ie);
  DeviceInputEvent(const DeviceInputEvent&) = default;
  DeviceInputEvent(DeviceInputEvent&&) = default;

  DeviceInputEvent& operator=(const DeviceInputEvent&) = default;
  DeviceInputEvent& operator=(DeviceInputEvent&&) = default;

  uint16_t type;
  uint16_t code;
  int32_t  value;

  bool operator==(const DeviceInputEvent& o) const;
  bool operator==(const struct input_event& o) const;
  bool operator<(const DeviceInputEvent& o) const;
  bool operator<(const struct input_event& o) const;
};

// -------------------------------------------------------------------------------------------------
QDataStream& operator<<(QDataStream& s, const DeviceInputEvent& die);
QDataStream& operator>>(QDataStream& s, DeviceInputEvent& die);

// -------------------------------------------------------------------------------------------------
/// KeyEvent is a sequence of DeviceInputEvent.
using KeyEvent = std::vector<DeviceInputEvent>;

/// KeyEventSequence is a sequence of KeyEvents.
using KeyEventSequence = std::vector<KeyEvent>;
Q_DECLARE_METATYPE(KeyEventSequence);

// -------------------------------------------------------------------------------------------------
template<typename T>
QDataStream& operator<<(QDataStream& s, const std::vector<T>& container)
{
  s << quint32(container.size());
  for (const auto& item : container) {
    s << item;
  }
  return s;
}

template<typename T>
QDataStream& operator>>(QDataStream& s, std::vector<T>& container)
{
  quint32 size{};
  s >> size;
  container.resize(size);
  for (quint64 i = 0; i < size; ++i) {
    s >> container[i];
  }
  return s;
}

// -------------------------------------------------------------------------------------------------
QString& operator<<(QString& s, const KeyEventSequence& kes);
const QString& operator<<(QString&& s, const KeyEventSequence& kes);
const QString& operator>>(const QString& s, KeyEventSequence& kes);

// -------------------------------------------------------------------------------------------------
QDebug operator<<(QDebug debug, const DeviceInputEvent &ie);
QDebug operator<<(QDebug debug, const KeyEvent &ke);

// -------------------------------------------------------------------------------------------------
class NativeKeySequence
{
public:
  NativeKeySequence();
  NativeKeySequence(NativeKeySequence&&) = default;
  NativeKeySequence(const NativeKeySequence&) = default;
  NativeKeySequence(QKeySequence&& ks, KeyEventSequence&& kes);

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

  friend QDataStream& operator>>(QDataStream& s, NativeKeySequence& ks) {
    return s >> ks.m_keySequence >> ks.m_nativeSequence;
  }

  friend QDataStream& operator<<(QDataStream& s, const NativeKeySequence& ks) {
    return s << ks.m_keySequence << ks.m_nativeSequence;
  }

private:
  QKeySequence m_keySequence;
  KeyEventSequence m_nativeSequence;
};
Q_DECLARE_METATYPE(NativeKeySequence);

// -------------------------------------------------------------------------------------------------
struct MappedInputAction
{
  bool operator==(const MappedInputAction& o) const { return sequence == o.sequence; }

  // For now this can only be a mapped key sequence
  // TODO This action could also be sth like toggle the zoom...
  NativeKeySequence sequence;
};
Q_DECLARE_METATYPE(MappedInputAction);

QDataStream& operator>>(QDataStream& s, MappedInputAction& mia);
QDataStream& operator<<(QDataStream& s, const MappedInputAction& mia);

// -------------------------------------------------------------------------------------------------
class InputMapConfig : public std::map<KeyEventSequence, MappedInputAction>{};

// -------------------------------------------------------------------------------------------------
class InputMapper : public QObject
{
  Q_OBJECT

public:
  InputMapper(std::shared_ptr<VirtualDevice> virtualDevice, QObject* parent = nullptr);
  ~InputMapper();

  void resetState(); // Reset any stored sequence state.

  // input_events = complete sequence including SYN event
  void addEvents(const struct input_event input_events[], size_t num);

  bool recordingMode() const;
  void setRecordingMode(bool recording);

  int keyEventInterval() const;
  void setKeyEventInterval(int interval);

  std::shared_ptr<VirtualDevice> virtualDevice() const;

  void setConfiguration(const InputMapConfig& config);
  void setConfiguration(InputMapConfig&& config);
  const InputMapConfig& configuration() const;

signals:
  void configurationChanged();
  void recordingModeChanged(bool recording);
  void keyEventRecorded(const KeyEvent&);
  // Right befor first key event recorded:
  void recordingStarted();
  // After key sequence interval timer timout or max sequence length reached
  void recordingFinished(bool canceled); // canceled if recordingMode was set to false instead of interval time out

private:
  struct Impl;
  std::unique_ptr<Impl> impl;
};
