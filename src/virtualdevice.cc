// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "virtualdevice.h"

#include <fcntl.h>

#include <QDebug>

class VirtualDevice;
int VirtualDevice::uinp_fd;

VirtualDevice::VirtualDevice(){
  uinp_fd = -1;
  setupVirtualDevice();
}

VirtualDevice::~VirtualDevice(){
  if (deviceCreated) {
    ioctl(uinp_fd, UI_DEV_DESTROY);
    close(uinp_fd);
    qDebug("uinput Device Closed");
  }
}

// Setup uinput device that can send mouse and keyboard events. Logs the result too.
void VirtualDevice::setupVirtualDevice() {
  int i=0;
  if (deviceCreated)
    return;

  // Open the input device
  uinp_fd = open("/dev/uinput", O_WRONLY | O_NDELAY);
  if (uinp_fd < 0) {
    qDebug("Unable to open /dev/uinput\n");
    deviceCreated = false;
    return;
  }

  memset(&uinp,0,sizeof(uinp));
  // Intialize the UINPUT device to NULL
  strncpy(uinp.name, "Projecteur_input_device", UINPUT_MAX_NAME_SIZE);
  uinp.id.bustype = BUS_USB;

  // Setup the uinput device
  ioctl(uinp_fd, UI_SET_EVBIT, EV_KEY);
  ioctl(uinp_fd, UI_SET_EVBIT, EV_REL);
  ioctl(uinp_fd, UI_SET_RELBIT, REL_X);
  ioctl(uinp_fd, UI_SET_RELBIT, REL_Y);

  for (i=0; i < 256; i++) {
    ioctl(uinp_fd, UI_SET_KEYBIT, i); }

  ioctl(uinp_fd, UI_SET_KEYBIT, BTN_MOUSE);
  ioctl(uinp_fd, UI_SET_KEYBIT, BTN_TOUCH);

  ioctl(uinp_fd, UI_SET_KEYBIT, BTN_MOUSE);
  ioctl(uinp_fd, UI_SET_KEYBIT, BTN_LEFT);
  ioctl(uinp_fd, UI_SET_KEYBIT, BTN_MIDDLE);
  ioctl(uinp_fd, UI_SET_KEYBIT, BTN_RIGHT);
  ioctl(uinp_fd, UI_SET_KEYBIT, BTN_FORWARD);
  ioctl(uinp_fd, UI_SET_KEYBIT, BTN_BACK);

  // Create input device into input sub-system
  write(uinp_fd, &uinp, sizeof(uinp));
  if (ioctl(uinp_fd, UI_DEV_CREATE)) {
    qDebug("Unable to create Virtual (UINPUT) device.");
    deviceCreated = false;
    return;
  }

  // Log the device name
  char sysfs_device_name[UINPUT_MAX_NAME_SIZE];
  ioctl(uinp_fd, UI_GET_SYSNAME(sizeof(sysfs_device_name)), sysfs_device_name);
  qDebug("uinput device: /sys/devices/virtual/input/%s", sysfs_device_name);

  deviceCreated = true;
}

// Public methods to emit event from the device
void VirtualDevice::emitEvent(uint16_t type, uint16_t code, int val) {
  // If no virtual device is present then do not emit the event.
  if (!deviceCreated)
    return;

  struct input_event ie;

  ie.type = type;
  ie.code = code;
  ie.value = val;

  emitEvent(ie, true);
}

void VirtualDevice::emitEvent(struct input_event ie, bool remove_timestamp)
{
  // If no virtual device is present then do not emit the event.
  if (!deviceCreated)
    return;

  if (remove_timestamp) {
    // timestamp values below are ignored
    ie.time.tv_sec = 0;
    ie.time.tv_usec = 0;
  }

  write(uinp_fd, &ie, sizeof(ie));
}

// Simulate mouse clicks
void VirtualDevice::mouseLeftClick(){
  emitEvent(EV_KEY, BTN_LEFT, 1);
  emitEvent(EV_SYN, SYN_REPORT, 0);
  emitEvent(EV_KEY, BTN_LEFT, 0);
  emitEvent(EV_SYN, SYN_REPORT, 0);
}
