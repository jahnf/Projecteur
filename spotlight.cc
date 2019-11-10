// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "spotlight.h"

#include <QDirIterator>
#include <QSocketNotifier>
#include <QTimer>
#include <QVarLengthArray>

#include <functional>
#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <unistd.h>
#include <vector>

namespace {
  struct Device {
    const quint16 vendorId;
    const quint16 productId;
    const bool isBluetooth;
  };

  // List of supported devices
  const std::vector<Device> supportedDevices {
    {0x46d, 0xc53e, false},  // Logitech Spotlight (USB)
    {0x46d, 0xb503, true},   // Logitech Spotlight (Bluetooth)
  };
}

Spotlight::Spotlight(QObject* parent)
  : QObject(parent)
  , m_activeTimer(new QTimer(this))
{
  m_activeTimer->setSingleShot(true);
  m_activeTimer->setInterval(600);

  connect(m_activeTimer, &QTimer::timeout, [this](){
    m_spotActive = false;
    emit spotActiveChanged(false);
  });

  // Try to find an already attached device(s) and connect to it.
  connectDevices();
  setupDevEventInotify();
}

Spotlight::~Spotlight()
{
}

bool Spotlight::anySpotlightDeviceConnected() const
{
  for (const auto& i : m_eventNotifiers)
  {
    if (i.second && i.second->isEnabled())
      return true;
  }

  return false;
}

QStringList Spotlight::connectedDevices() const
{
  QStringList devices;
  for (const auto& i : m_eventNotifiers)
  {
    if (i.second && i.second->isEnabled())
      devices.push_back(i.first);
  }
  return devices;
}

int Spotlight::connectDevices()
{
  int count = 0;
  QDirIterator it("/dev/input", QDir::System);
  while (it.hasNext())
  {
    it.next();
    if (it.fileName().startsWith("event"))
    {
      const auto found = m_eventNotifiers.find(it.filePath());
      if (found != m_eventNotifiers.end() && found->second && found->second->isEnabled()) {
        continue;
      }

      if (connectSpotlightDevice(it.filePath()) == ConnectionResult::Connected) {
        ++count;
      }
    }
  }
  return count;
}

/// Connect to devicePath if readable and if it is a spotlight device
Spotlight::ConnectionResult Spotlight::connectSpotlightDevice(const QString& devicePath)
{
  const int evfd = ::open(devicePath.toLocal8Bit().constData(), O_RDONLY, 0);
  if (evfd < 0) {
    return ConnectionResult::CouldNotOpen;
  }

  struct input_id id{};
  ioctl(evfd, EVIOCGID, &id);

  const auto it = std::find_if(supportedDevices.cbegin(), supportedDevices.cend(),
  [&id](const Device& d) {
    return (id.vendor == d.vendorId) && (id.product == d.productId);
  });

  if (it == supportedDevices.cend())
  {
    ::close(evfd);
    return ConnectionResult::NotASpotlightDevice;
  }

  unsigned long bitmask = 0;
  const int len = ioctl(evfd, EVIOCGBIT(0, sizeof(bitmask)), &bitmask);
  if (len < 0)
  {
    ::close(evfd);
    return ConnectionResult::NotASpotlightDevice;
  }

  const bool hasRelEv = !!(bitmask & (1 << EV_REL));
  if (!hasRelEv) { return ConnectionResult::NotASpotlightDevice; }

  if(id.bustype == BUS_BLUETOOTH)
  {
    // Known issue (at least on Ubuntu systems):
    // Bluetooth devices get detected fine with the current approach with inotify and /dev/input
    // but the corresponding /dev/input/event* device from the Bluetooth Spotlight HID device
    // is still available even if bluetooth is disabled -> The application still thinks that
    // a spotlight device connected, therefore showing a 'false' connected state in the
    // Preferences dialog. Possible future counter measure would be to monitor the
    // bluetooth connection of Uniq & Phys addresses via the Linux Bluetooth API

    // Bluetooth Spotlight device example
    /*
    I: Bus=0005 Vendor=046d Product=b503 Version=0032
    N: Name="SPOTLIGHT"
    P: Phys=C4:8E:8F:FA:37:0E
    S: Sysfs=/devices/virtual/misc/uhid/0005:046D:B503.0008/input/input55
    U: Uniq=C3:A0:BB:30:C1:7B
    H: Handlers=sysrq kbd mouse3 event17
    B: PROP=0
    B: EV=10001f
    B: KEY=3007f 0 0 483ffff17aff32d bf54444600000000 ffff0001 130f938b17c007 ffe77bfad9415fff febeffdfffefffff fffffffffffffffe
    B: REL=1c3
    B: ABS=100000000
    B: MSC=10
    */
  }

  const bool anyConnectedBefore = anySpotlightDeviceConnected();
  m_eventNotifiers[devicePath].reset(new QSocketNotifier(evfd, QSocketNotifier::Read));
  QSocketNotifier* const notifier = m_eventNotifiers[devicePath].data();

  connect(notifier, &QSocketNotifier::destroyed, [notifier, devicePath]() {
    ::close(static_cast<int>(notifier->socket()));
  });

  connect(notifier, &QSocketNotifier::activated, [this, notifier, devicePath](int fd)
  {
    struct input_event ev;
    const auto sz = ::read(fd, &ev, sizeof(ev));
    if (sz == sizeof(ev) && ev.type & EV_REL) // only for relative mouse events
    {
      if (!m_activeTimer->isActive()) {
        m_spotActive = true;
        emit spotActiveChanged(true);
      }
      m_activeTimer->start();
    }
    else if (sz == -1)
    {
      // Error, e.g. if the usb device was unplugged...
      notifier->setEnabled(false);
      emit disconnected(devicePath);
      if (!anySpotlightDeviceConnected()) {
        emit anySpotlightDeviceConnectedChanged(false);
      }
      QTimer::singleShot(0, [this, devicePath](){ m_eventNotifiers[devicePath].reset(); });
    }
  });

  emit connected(devicePath);
  if (!anyConnectedBefore) {
    emit anySpotlightDeviceConnectedChanged(true);
  }
  return ConnectionResult::Connected;
}

bool Spotlight::setupDevEventInotify()
{
  int fd = -1;
#if defined(IN_CLOEXEC)
  fd = inotify_init1(IN_CLOEXEC);
#endif
  if (fd == -1)
  {
    fd = inotify_init();
    if (fd == -1) {
       return false; // TODO: error msg - without it we cannot detect attachting of new devices
    }
  }
  fcntl(fd, F_SETFD, FD_CLOEXEC);
  const int wd = inotify_add_watch(fd, "/dev/input", IN_CREATE | IN_DELETE);

  if (wd < 0) {
    return false; // TODO: error msg - without it we cannot detect attachting of new devices
  }

  const auto notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
  connect(notifier, &QSocketNotifier::activated, [this](int fd)
  {
    int bytesAvaibable = 0;
    if (ioctl(fd, FIONREAD, &bytesAvaibable) < 0 || bytesAvaibable <= 0) {
      return; // Error or no bytes available
    }
    QVarLengthArray<char, 2048> buffer(bytesAvaibable);
    const auto bytesRead = read(fd, buffer.data(), static_cast<size_t>(bytesAvaibable));
    const char* at = buffer.data();
    const char* const end = at + bytesRead;
    while (at < end)
    {
      const auto event = reinterpret_cast<const inotify_event*>(at);

      if ((event->mask & (IN_CREATE )) && QString(event->name).startsWith("event"))
      {
        const auto devicePath = QString("/dev/input/").append(event->name);
        tryConnect(devicePath, 100, 4);
      }
      at += sizeof(inotify_event) + event->len;
    }
  });

  connect(notifier, &QSocketNotifier::destroyed, [notifier]() {
    ::close(static_cast<int>(notifier->socket()));
  });
  return true;
}

// Usually the devices are not fully ready when added to the device file system
// We'll try to check several times if the first try fails.
// Not elegant - but needs very little code, easy to maintain and no need for
// linux udev message parsing
void Spotlight::tryConnect(const QString& devicePath, int msec, int retries)
{
  --retries;
  QTimer::singleShot(msec, [this, devicePath, retries, msec]() {
    if (connectSpotlightDevice(devicePath) == ConnectionResult::CouldNotOpen) {
      if (retries == 0) return;
      tryConnect(devicePath, msec+100, retries);
    }
  });
}

