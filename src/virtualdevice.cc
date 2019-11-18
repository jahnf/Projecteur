// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "virtualdevice.h"

#include <fcntl.h>
#include <unistd.h>

#include <QDebug>

class VirtualDevice;

VirtualDevice::VirtualDevice() {
  deviceStatus = setupVirtualDevice();
}

VirtualDevice::~VirtualDevice() {
  if (isDeviceCreated()) {
    ioctl(uinp_fd, UI_DEV_DESTROY);
    close(uinp_fd);
    qDebug("uinput Device Closed");
  }
}

VirtualDevice::DeviceStatus VirtualDevice::getDeviceStatus(){
  return deviceStatus;
}

// Setup uinput device that can send mouse and keyboard events. Logs the result too.
VirtualDevice::DeviceStatus VirtualDevice::setupVirtualDevice() {
  if (isDeviceCreated())
    return DeviceStatus::Connected;

  // Open the input device
  if (access("/dev/uinput", F_OK) == -1) {
    qDebug("File not found /dev/uinput");
    return DeviceStatus::UinputNotFound;
  }
  uinp_fd = open("/dev/uinput", O_WRONLY | O_NDELAY);
  if (uinp_fd < 0) {
    qDebug("Unable to open /dev/uinput");
    return DeviceStatus::UinputAccessDenied;
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

  for (int i=0; i < 256; i++) {
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
    close(uinp_fd);
    qDebug("Unable to create Virtual (UINPUT) device.");
    return DeviceStatus::CouldNotCreate;
  }

  // Log the device name
  char sysfs_device_name[16];
  ioctl(uinp_fd, UI_GET_SYSNAME(sizeof(sysfs_device_name)), sysfs_device_name);
  qDebug("uinput device: /sys/devices/virtual/input/%s", sysfs_device_name);

  return DeviceStatus::Connected;
}

// Public methods to emit event from the device
void VirtualDevice::emitEvent(u_int16_t type, u_int16_t code, int val) {
  // If no virtual device is present then do not emit the event.
  if (!isDeviceCreated())
    return;

  struct input_event ie;

  ie.type = type;
  ie.code = code;
  ie.value = val;

  emitEvent(ie, true);
}

void VirtualDevice::emitEvent(struct input_event ie, bool remove_timestamp) {
  // If no virtual device is present then do not emit the event.
  if (!isDeviceCreated())
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
