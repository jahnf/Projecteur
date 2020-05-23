// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include <cstdint>
#include <memory>
#include <tuple>
#include <vector>

#include <linux/input.h>

#include <QObject>

class QTimer;
class VirtualDevice;

/// This is basically the input_event struct from linux/input.h without the time member
struct DeviceInputEvent
{
  DeviceInputEvent() = default;
  DeviceInputEvent(uint16_t type, uint16_t code, uint32_t value) : type(type), code(code), value(value) {}
  DeviceInputEvent(const input_event& ie) : type(ie.type), code(ie.code), value(ie.value) {}
  DeviceInputEvent(const DeviceInputEvent&) = default;
  DeviceInputEvent(DeviceInputEvent&&) = default;

  DeviceInputEvent& operator=(const DeviceInputEvent&) = default;
  DeviceInputEvent& operator=(DeviceInputEvent&&) = default;

  uint16_t type;
  uint16_t code;
  uint32_t value;

  bool operator==(const DeviceInputEvent& o) const {
    return std::tie(type,code,value) == std::tie(o.type,o.code,o.value);
  }

  bool operator==(const input_event& o) const {
    return std::tie(type,code,value) == std::tie(o.type,o.code,o.value);
  }

  bool operator<(const DeviceInputEvent& o) const {
    return std::tie(type,code,value) < std::tie(o.type,o.code,o.value);
  }

  bool operator<(const input_event& o) const {
    return std::tie(type,code,value) < std::tie(o.type,o.code,o.value);
  }
};

/// KeyEvent is a sequence of DeviceInputEvent.
using KeyEvent = std::vector<DeviceInputEvent>;

/// KeyEventSequence is a sequence of KeyEvents.
using KeyEventSequence = std::vector<KeyEvent>;


// Placeholder class
class InputMapper : public QObject
{
  Q_OBJECT

public:
  InputMapper(std::shared_ptr<VirtualDevice> virtualDevice, QObject* parent = nullptr);
  ~InputMapper();

  void resetState(); // Reset any stored sequence state.

  // input_events = complete sequence including SYN event
  void addEvents(struct input_event input_events[], size_t num);

  bool recordingMode() const;
  void setRecordingMode(bool recording);

  std::shared_ptr<VirtualDevice> virtualDevice() const;

signals:
  void recordingModeChanged(bool recording);

private:
  void sequenceTimeout();

private:
  struct Impl;
  std::unique_ptr<Impl> impl;
  QTimer* m_seqTimer = nullptr;
};
