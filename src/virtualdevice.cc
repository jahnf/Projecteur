// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "virtualdevice.h"

#include "logging.h"

#include <fcntl.h>
#include <linux/uinput.h>
#include <unistd.h>

LOGGING_CATEGORY(virtualdevice, "virtualdevice")

namespace  {
  class VirtualDevice_ : public QObject {}; // for i18n and logging
}

struct VirtualDevice::Token {};

VirtualDevice::VirtualDevice(Token, int fd)
  : m_uinpFd(fd)
{}

VirtualDevice::~VirtualDevice()
{
  if (m_uinpFd >= 0)
  {
    ioctl(m_uinpFd, UI_DEV_DESTROY);
    ::close(m_uinpFd);
    logDebug(virtualdevice) << VirtualDevice_::tr("uinput Device Closed");
  }
}

// Setup uinput device that can send mouse and keyboard events.
std::unique_ptr<VirtualDevice> VirtualDevice::create(const char* name,
                                                     uint16_t virtualVendorId,
                                                     uint16_t virtualProductId,
                                                     uint16_t virtualVersionId,
                                                     const char* location)
{
  // Open the input device
  if (access(location, F_OK) == -1) {
    logWarn(virtualdevice) << VirtualDevice_::tr("File not found: %1").arg(location);
    logWarn(virtualdevice) << VirtualDevice_::tr("Please check if uinput kernel module is loaded");
    return std::unique_ptr<VirtualDevice>();
  }

  int fd = ::open(location, O_WRONLY | O_NDELAY);
  if (fd < 0) {
    logWarn(virtualdevice) << VirtualDevice_::tr("Unable to open: %1").arg(location);
    logWarn(virtualdevice) << VirtualDevice_::tr("Please check if current user has write access");
    return std::unique_ptr<VirtualDevice>();
  }

  struct uinput_user_dev uinp {};
  snprintf(uinp.name, sizeof(uinp.name), "%s", name);
  uinp.id.bustype = BUS_USB;
  uinp.id.vendor = virtualVendorId;
  uinp.id.product = virtualProductId;
  uinp.id.version = virtualVersionId;

  // Setup the uinput device
  // TODO Are the following Key and Event bits sufficient? Do we need more? (see all in Linux's input-event-codes.h)
  ioctl(fd, UI_SET_EVBIT, EV_KEY);
  ioctl(fd, UI_SET_EVBIT, EV_REL);
  ioctl(fd, UI_SET_RELBIT, REL_X);
  ioctl(fd, UI_SET_RELBIT, REL_Y);

  for (int i = 0; i < 256; ++i) {
    ioctl(fd, UI_SET_KEYBIT, i);
  }

  ioctl(fd, UI_SET_KEYBIT, BTN_MOUSE);
  ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH);
  ioctl(fd, UI_SET_KEYBIT, BTN_LEFT);
  ioctl(fd, UI_SET_KEYBIT, BTN_MIDDLE);
  ioctl(fd, UI_SET_KEYBIT, BTN_RIGHT);
  ioctl(fd, UI_SET_KEYBIT, BTN_FORWARD);
  ioctl(fd, UI_SET_KEYBIT, BTN_BACK);

  // Create input device into input sub-system
  const auto bytesWritten = write(fd, &uinp, sizeof(uinp));
  if ((bytesWritten != sizeof(uinp)) || (ioctl(fd, UI_DEV_CREATE)))
  {
    ::close(fd);
    logWarn(virtualdevice) << VirtualDevice_::tr("Unable to create Virtual (UINPUT) device.");
    return std::unique_ptr<VirtualDevice>();
  }

  // Log the device name
  char sysfs_device_name[16]{};
  ioctl(fd, UI_GET_SYSNAME(sizeof(sysfs_device_name)), sysfs_device_name);
  logInfo(virtualdevice) << VirtualDevice_::tr("Created uinput device: %1")
                            .arg(QString("/sys/devices/virtual/input/%1").arg(sysfs_device_name));

  return std::make_unique<VirtualDevice>(Token{}, fd);
}


// Public methods to emit event from the device
void VirtualDevice::emitEvent(uint16_t type, uint16_t code, int val)
{
  input_event ie {{}, type, code, val};
  emitEvent(std::move(ie), true);
}

void VirtualDevice::emitEvent(struct input_event ie, bool remove_timestamp)
{
  if (remove_timestamp) {
    // timestamp values below are ignored
    ie.time.tv_sec = 0;
    ie.time.tv_usec = 0;
  }

  const auto bytesWritten = write(m_uinpFd, &ie, sizeof(ie));
  if (bytesWritten != sizeof(ie)) {
    logError(virtualdevice) << VirtualDevice_::tr("Error while writing to virtual device.");
  }
}

// Simulate mouse clicks
void VirtualDevice::mouseLeftClick()
{
  emitEvent(EV_KEY, BTN_LEFT, 1);
  emitEvent(EV_SYN, SYN_REPORT, 0);
  emitEvent(EV_KEY, BTN_LEFT, 0);
  emitEvent(EV_SYN, SYN_REPORT, 0);
}
