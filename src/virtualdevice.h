// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md

// Virtal Device to emit customized events from Projecteur device
// The spotlight.cc grabs mouse inputs from Logitech Spotlight device.
// This module is used when the input events are supposed to be forwarded to the system.

# pragma once

#include <linux/uinput.h>
#include <unistd.h>

#include <QDebug>

// Device that can act as virtual keyboard and mouse
class VirtualDevice{
  private:
    struct uinput_user_dev uinp;
    int uinp_fd;
    bool deviceCreated = false;

    void setupVirtualDevice();


  public:
    VirtualDevice();
    ~VirtualDevice();

    bool isDeviceCreated(){ return deviceCreated; }
    void emitEvent(uint16_t type, uint16_t code, int val);
    void emitEvent(struct input_event ie, bool remove_timestamp=false);
    void mouseLeftClick();
};
