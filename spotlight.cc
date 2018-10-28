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

  constexpr quint16 vendorId = 0x46d;
  constexpr quint16 usbProductId = 0xc53e;
  constexpr quint16 btProductId = 0xb503;

  if (id.vendor != vendorId || (id.product != usbProductId && id.product != btProductId))
  {
    ::close(evfd);
    return ConnectionResult::NotASpotlightDevice;
  }

  unsigned long bitmask = 0;
  const int len = ioctl(evfd, EVIOCGBIT(0, sizeof(bitmask)), &bitmask);
  if ( len < 0 )
  {
    ::close(evfd);
    return ConnectionResult::NotASpotlightDevice;
  }

  const bool hasRelEv = !!(bitmask & (1 << EV_REL));
  if (!hasRelEv) { return ConnectionResult::NotASpotlightDevice; }

  const bool anyConnectedBefore = anySpotlightDeviceConnected();
  m_eventNotifiers[devicePath].reset(new QSocketNotifier(evfd, QSocketNotifier::Read));
  QSocketNotifier* notifier = m_eventNotifiers[devicePath].data();

  connect(notifier, &QSocketNotifier::destroyed, [notifier, devicePath]() {
    ::close(static_cast<int>(notifier->socket()));
  });

  connect(notifier, &QSocketNotifier::activated, [this, notifier, devicePath](int fd)
  {
    struct input_event ev;
    auto sz = ::read(fd, &ev, sizeof(ev));
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
  if( fd == -1 )
  {
    fd = inotify_init();
    if( fd == -1 ) {
       return false; // TODO: error msg - without it we cannot detect attachting of new devices
    }
  }
  fcntl( fd, F_SETFD, FD_CLOEXEC );
  const int wd = inotify_add_watch( fd, "/dev/input", IN_CREATE | IN_DELETE );
  // TODO check if wd >=0... else error
  auto notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
  connect(notifier, &QSocketNotifier::activated, [this](int fd)
  {
    int bytesAvaibable = 0;
    if( ioctl( fd, FIONREAD, &bytesAvaibable ) < 0 || bytesAvaibable <= 0 ) {
      return; // Error or no bytes available
    }
    QVarLengthArray<char, 2048> buffer( bytesAvaibable );
    const auto bytesRead = read( fd, buffer.data(), static_cast<size_t>(bytesAvaibable) );
    const char* at = buffer.data();
    const char* const end = at + bytesRead;
    while( at < end )
    {
      const inotify_event* event = reinterpret_cast<const inotify_event*>( at );
      if( (event->mask & (IN_CREATE )) && QString(event->name).startsWith("event") )
      {
        const auto devicePath = QString("/dev/input/").append(event->name);

        // Usually the devices are not fully ready when added to the device file system
        // We'll try to check several times if the first try fails.
        // Not elegant - but needs very little code, easy to maintain and no need to parse
        // linux udev message parsing and similar
        static std::function<void(const QString&,int,int)> try_connect =
          [this](const QString& devicePath, int msec, int retries) {
            --retries;
            QTimer::singleShot(msec, [this, devicePath, retries, msec]() {
              if (connectSpotlightDevice(devicePath) == ConnectionResult::CouldNotOpen) {
                if( retries == 0) return;
                try_connect(devicePath, msec+100, retries);
              }
            });
          };
        try_connect(devicePath, 100, 4);
      }
      at += sizeof(inotify_event) + event->len;
    }

  });

  connect(notifier, &QSocketNotifier::destroyed, [notifier]() {
    ::close(static_cast<int>(notifier->socket()));
  });
  return true;
}

