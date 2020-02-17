// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "virtualdevice.h"

#include "logging.h"

#include <fcntl.h>
#include <linux/uinput.h>
#include <unistd.h>

LOGGING_CATEGORY(virtualdevice, "virtualdevice")

namespace  {
  constexpr char uinputDeviceLocation[] = "/dev/uinput";
  constexpr char uinputDeviceName[] = "Projecteur_input_device";
  constexpr uint16_t virtualVendorId = 0xfeed;
  constexpr uint16_t virtualProductId = 0xc0de;
  constexpr uint16_t virtualVersionId = 1;

  class VirtualDevice_ : public QObject {}; // for i18n and logging
}

VirtualDevice::VirtualDevice()
  : m_deviceStatus(setupVirtualDevice())
{}

VirtualDevice::~VirtualDevice()
{
  if (isDeviceCreated())
  {
    ioctl(m_uinpFd, UI_DEV_DESTROY);
    ::close(m_uinpFd);
    logDebug(virtualdevice) << VirtualDevice_::tr("uinput Device Closed");
  }
}

VirtualDevice::DeviceStatus VirtualDevice::getDeviceStatus() const {
  return m_deviceStatus;
}

// Setup uinput device that can send mouse and keyboard events. Logs the result too.
VirtualDevice::DeviceStatus VirtualDevice::setupVirtualDevice()
{
  if (isDeviceCreated())
    return DeviceStatus::Connected;

  // Currently this upinput code uses the 'old' version,
  // We can check the version and we can switch to the new interface if version is >=5
  // see https://www.kernel.org/doc/html/v4.13/input/uinput.html

  // Open the input device
  if (access(uinputDeviceLocation, F_OK) == -1) {
    logWarn(virtualdevice) << VirtualDevice_::tr("File not found: %1").arg(uinputDeviceLocation);
    logWarn(virtualdevice) << VirtualDevice_::tr("Please check if uinput kernel module is loaded");
    return DeviceStatus::UinputNotFound;
  }

  m_uinpFd = ::open(uinputDeviceLocation, O_WRONLY | O_NDELAY);
  if (m_uinpFd < 0) {
    logWarn(virtualdevice) << VirtualDevice_::tr("Unable to open: %1").arg(uinputDeviceLocation);
    logWarn(virtualdevice) << VirtualDevice_::tr("Please check if current user has write access");
    return DeviceStatus::UinputAccessDenied;
  }

  struct uinput_user_dev uinp {};
  strncpy(uinp.name, uinputDeviceName, UINPUT_MAX_NAME_SIZE);
  uinp.id.bustype = BUS_USB;
  uinp.id.vendor = virtualVendorId;
  uinp.id.product = virtualProductId;
  uinp.id.version = virtualVersionId;

  // Setup the uinput device
  // TODO Are the following Key and Event bits sufficient? Do we need more? (see all in Linux's input-event-codes.h)
  ioctl(m_uinpFd, UI_SET_EVBIT, EV_KEY);
  ioctl(m_uinpFd, UI_SET_EVBIT, EV_REL);
  ioctl(m_uinpFd, UI_SET_RELBIT, REL_X);
  ioctl(m_uinpFd, UI_SET_RELBIT, REL_Y);

  for (int i = 0; i < 256; ++i) {
    ioctl(m_uinpFd, UI_SET_KEYBIT, i);
  }

  ioctl(m_uinpFd, UI_SET_KEYBIT, BTN_MOUSE);
  ioctl(m_uinpFd, UI_SET_KEYBIT, BTN_TOUCH);
  ioctl(m_uinpFd, UI_SET_KEYBIT, BTN_LEFT);
  ioctl(m_uinpFd, UI_SET_KEYBIT, BTN_MIDDLE);
  ioctl(m_uinpFd, UI_SET_KEYBIT, BTN_RIGHT);
  ioctl(m_uinpFd, UI_SET_KEYBIT, BTN_FORWARD);
  ioctl(m_uinpFd, UI_SET_KEYBIT, BTN_BACK);

  // Create input device into input sub-system
  const auto bytesWritten = write(m_uinpFd, &uinp, sizeof(uinp));
  if ((bytesWritten != sizeof(uinp)) || (ioctl(m_uinpFd, UI_DEV_CREATE)))
  {
    ::close(m_uinpFd);
    logWarn(virtualdevice) << VirtualDevice_::tr("Unable to create Virtual (UINPUT) device.");
    return DeviceStatus::CouldNotCreate;
  }

  // Log the device name
  char sysfs_device_name[16]{};
  ioctl(m_uinpFd, UI_GET_SYSNAME(sizeof(sysfs_device_name)), sysfs_device_name);
  logInfo(virtualdevice) << VirtualDevice_::tr("Created uinput device: %1")
                            .arg(QString("/sys/devices/virtual/input/%1").arg(sysfs_device_name));

  return DeviceStatus::Connected;
}

// Public methods to emit event from the device
void VirtualDevice::emitEvent(uint16_t type, uint16_t code, int val)
{
  // If no virtual device is present then do not emit the event.
  if (!isDeviceCreated())
    return;

  input_event ie {{}, type, code, val};
  emitEvent(std::move(ie), true);
}

void VirtualDevice::emitEvent(struct input_event ie, bool remove_timestamp)
{
  // If no virtual device is present then do not emit the event.
  if (!isDeviceCreated())
    return;

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
