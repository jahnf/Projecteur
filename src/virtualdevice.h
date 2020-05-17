// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md

// Virtal Device to emit customized events from Projecteur device
// The spotlight.cc grabs mouse inputs from Logitech Spotlight device.
// This module is used when the input events are supposed to be forwarded to the system.

# pragma once

#include <cstdint>
#include <memory>

// Device that can act as virtual keyboard and mouse
class VirtualDevice
{
private:
  struct Token;
  int m_uinpFd = -1;

public:
  // Return a VirtualDevice shared_ptr or an empty shared_ptr if the creation fails.
  static std::shared_ptr<VirtualDevice> create(const char* name = "Projecteur_input_device",
                                               uint16_t virtualVendorId = 0xfeed,
                                               uint16_t virtualProductId = 0xc0de,
                                               uint16_t virtualVersionId = 1,
                                               const char* location = "/dev/uinput");

  explicit VirtualDevice(Token, int fd);
  ~VirtualDevice();

  void emitEvent(uint16_t type, uint16_t code, int val);
  void emitEvent(struct input_event ie);
  void emitEvent(struct input_event[], size_t num);
};
