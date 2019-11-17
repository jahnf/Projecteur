// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "uinputevents.h"

#include <fcntl.h>

#include <QDebug>

class uinputEvents;
int uinputEvents::uinp_fd;

void uinputEvents::emitEvent(uint16_t type, uint16_t code, int val) {
  struct input_event ie;

  ie.type = type;
  ie.code = code;
  ie.value = val;
  // timestamp values below are ignored
  ie.time.tv_sec = 0;
  ie.time.tv_usec = 0;

  write(uinp_fd, &ie, sizeof(ie));
}

void uinputEvents::emitEvent(struct input_event ie)
{
  // timestamp values below are ignored
  ie.time.tv_sec = 0;
  ie.time.tv_usec = 0;

  write(uinp_fd, &ie, sizeof(ie));
}

int uinputEvents::setup_uinputDevice() {
  int i=0;
  if (uinp_fd > 0)
    return -1;

  // Open the input device
  uinp_fd = open("/dev/uinput", O_WRONLY | O_NDELAY);
  if (uinp_fd < 0) {
    qDebug("Unable to open /dev/uinput\n");
    return -1; }

  memset(&uinp,0,sizeof(uinp));
  // Intialize the uInput device to NULL
  strncpy(uinp.name, "Projecteur Input Device", UINPUT_MAX_NAME_SIZE);
  uinp.id.version = 5;
  uinp.id.bustype = BUS_USB;
  // Setup the uinput device
  ioctl(uinp_fd, UI_SET_EVBIT, EV_KEY);
  ioctl(uinp_fd, UI_SET_EVBIT, EV_REL);
  ioctl(uinp_fd, UI_SET_RELBIT, REL_X);
  ioctl(uinp_fd, UI_SET_RELBIT, REL_Y);

  for (i=0; i < 256; i++) {
    ioctl(uinp_fd, UI_SET_KEYBIT, i); }

  ioctl(uinp_fd, UI_SET_KEYBIT, BTN_MOUSE);
  ioctl(uinp_fd, UI_SET_KEYBIT, BTN_LEFT);
  ioctl(uinp_fd, UI_SET_KEYBIT, BTN_MIDDLE);
  ioctl(uinp_fd, UI_SET_KEYBIT, BTN_RIGHT);
  ioctl(uinp_fd, UI_SET_KEYBIT, BTN_FORWARD);
  ioctl(uinp_fd, UI_SET_KEYBIT, BTN_BACK);

  // Create input device into input sub-system
  write(uinp_fd, &uinp, sizeof(uinp));
  if (ioctl(uinp_fd, UI_DEV_CREATE)) {
    qDebug("Unable to create UINPUT device.");
    return -1;
  }

  // Log the device name
  char sysfs_device_name[16];
  ioctl(uinp_fd, UI_GET_SYSNAME(sizeof(sysfs_device_name)), sysfs_device_name);
  qDebug("uinput device: /sys/devices/virtual/input/%s\n", sysfs_device_name);

  return 1;
}

void uinputEvents::mouseLeftClick(){
  emitEvent(EV_KEY, 272, 1);
  emitEvent(EV_SYN, SYN_REPORT, 0);
  usleep(15000);
  emitEvent(EV_KEY, 272, 0);
  emitEvent(EV_SYN, SYN_REPORT, 0);
}
