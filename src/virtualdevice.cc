// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "virtualdevice.h"

#include "logging.h"

#include <fcntl.h>
#include <linux/uinput.h>
#include <unistd.h>

#include <QFileInfo>

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
std::shared_ptr<VirtualDevice> VirtualDevice::create(const char* name,
                                                     uint16_t virtualVendorId,
                                                     uint16_t virtualProductId,
                                                     uint16_t virtualVersionId,
                                                     const char* location)
{
  const QFileInfo fi(location);
  if (!fi.exists()) {
    logWarn(virtualdevice) << VirtualDevice_::tr("File not found: %1").arg(location);
    logWarn(virtualdevice) << VirtualDevice_::tr("Please check if uinput kernel module is loaded");
    return std::unique_ptr<VirtualDevice>();
  }

  const int fd = ::open(location, O_WRONLY | O_NDELAY);
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
  ioctl(fd, UI_SET_EVBIT, EV_SYN);
  ioctl(fd, UI_SET_EVBIT, EV_KEY);
  ioctl(fd, UI_SET_EVBIT, EV_REL);

  // Set all rel event code bits on virtual device
  for (int i = 0; i < REL_CNT; ++i) {
    ioctl(fd, UI_SET_RELBIT, i);
  }

  // Set all key code bits on virtual device
  for (int i = 1; i < KEY_CNT; ++i) {
    ioctl(fd, UI_SET_KEYBIT, i);
  }

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

  return std::make_shared<VirtualDevice>(Token{}, fd);
}

void VirtualDevice::emitEvents(const struct input_event input_events[], size_t num)
{
  if (const ssize_t sz = sizeof(input_event) * num) {
    const auto bytesWritten = write(m_uinpFd, input_events, sz);
    if (bytesWritten != sz) {
      logError(virtualdevice) << VirtualDevice_::tr("Error while writing to virtual device.");
    }
  }
}

void VirtualDevice::emitEvents(const std::vector<struct input_event>& events)
{
  if (const ssize_t sz = sizeof(input_event) * events.size()) {
    const auto bytesWritten = write(m_uinpFd, events.data(), sz);
    if (bytesWritten != sz) {
      logError(virtualdevice) << VirtualDevice_::tr("Error while writing to virtual device.");
    }
  }
}

