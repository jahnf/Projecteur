// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md

// Virtal Device to emit customized events from Projecteur device
// The spotlight.cc grabs mouse inputs from Logitech Spotlight device.
// This module is used when the input events are supposed to be forwarded to the system.

# pragma once

#include <cstdint>

// Device that can act as virtual keyboard and mouse
class VirtualDevice
{
  public:
    VirtualDevice();
    ~VirtualDevice();

    enum class DeviceStatus { UinputNotFound, UinputAccessDenied, CouldNotCreate, Connected };
    DeviceStatus getDeviceStatus() const;
    bool isDeviceCreated() { return (m_deviceStatus == DeviceStatus::Connected); }
    void emitEvent(uint16_t type, uint16_t code, int val);
    void emitEvent(struct input_event ie, bool remove_timestamp = false);
    void mouseLeftClick();

  private:
    int m_uinpFd = -1;
    DeviceStatus m_deviceStatus;

    DeviceStatus setupVirtualDevice();
};
