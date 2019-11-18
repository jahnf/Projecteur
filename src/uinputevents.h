// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
// Virtal Device to emit customized events from Projecteur device
// The spotlight.cc grabs mouse inputs from Logitech Spotlight device.
// This module is used when the input events are supposed to be forwarded to the system.
# pragma once

#include <iostream>
#include <memory>
#include <linux/uinput.h>
#include <unistd.h>

#include <QDebug>

using namespace std;

class uinputEvents;

class uinputEvents{
  private:
    static int uinp_fd;
    // Device that can act as virtual keyboard and mouse
    struct uinput_user_dev uinp;
    uinputEvents(){}

  public:
    static shared_ptr<uinputEvents> getInstance() {
      static shared_ptr<uinputEvents> s_instance{new uinputEvents};
      // Try to setup the device. If it fails exit.
      if (s_instance->setup_uinputDevice() != 1)
         exit(1);
      return s_instance;
    }
    ~uinputEvents() {
      ioctl(uinp_fd, UI_DEV_DESTROY);
      close(uinp_fd);
      qDebug("uinput Device Closed");
    }

    void emitEvent(uint16_t type, uint16_t code, int val);
    void emitEvent(struct input_event ie, bool remove_timestamp=false);
    int setup_uinputDevice();
    uinputEvents(uinputEvents const&) = delete;
    void operator=(uinputEvents const&) = delete;
    void mouseLeftClick();
};
