// This file is part of Projecteur - https://github.com/jahnf/projecteur
// - See LICENSE.md and README.md

// Virtual Device to emit custom events from Projecteur.
// The spotlight.cc grabs mouse inputs from Logitech Spotlight device.
// This module is used when the input events are supposed to be forwarded to the system.

# pragma once

#include <QString>

#include <cstdint>
#include <memory>
#include <vector>

/// Device that can act as virtual keyboard or mouse
class VirtualDevice
{
private:
  struct Token;
  int m_uinpFd = -1;
  QString m_userName;
  QString m_deviceName;

public:
  enum class Type {
    Mouse,
    Keyboard
  };

  /// Return a VirtualDevice shared_ptr or an empty shared_ptr if the creation fails.
  static std::shared_ptr<VirtualDevice> create(Type deviceType,
                                               const char* name = "Projecteur_input_device",
                                               uint16_t virtualVendorId = 0xfeed,
                                               uint16_t virtualProductId = 0xc0de,
                                               uint16_t virtualVersionId = 1,
                                               const char* location = "/dev/uinput");

  VirtualDevice(Token, int fd, const char* name, const char* sysfs_name);
  ~VirtualDevice();

  void emitEvents(const struct input_event[], size_t num);
  void emitEvents(const std::vector<struct input_event>& events);
};
