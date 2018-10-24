// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "spotlight.h"

#include <QBuffer>
#include <QFile>
#include <QRegularExpression>
#include <QSocketNotifier>
#include <QTimer>
#include <QTextStream>

#include <fcntl.h>
#include <sys/socket.h>
#include <linux/input.h>
#include <linux/netlink.h>
#include <unistd.h>

namespace {
  constexpr char inputDevicesFile[] = "/proc/bus/input/devices";
  // Another possibility would be to scan /sys/bus/hid/devices/
  // check every device there for the vendor/product id
  // and also check the capabilites inside input/input*/capabilites...

  QString findAttachedSpotlightDevice()
  {
    QFile file(inputDevicesFile);
    if (!file.open(QIODevice::ReadOnly) || !file.isReadable())
      return QString();

    // With special files in /proc readLine will not work.
    QByteArray contents = file.readAll();
    QTextStream in(&contents, QIODevice::ReadOnly);

    while (!in.atEnd())
    {
      auto line = in.readLine();
      // Get Logitech USB Receiver that comes with the Spotlight device
      if (line.startsWith("I:") && line.contains("Vendor=046d Product=c53e"))
      {
        QString eventFile;
        // get next H: line
        while (!in.atEnd())
        {
          line = in.readLine();
          if (line.startsWith("H:"))
          {
            static const QRegularExpression eventRe("event\\d+");
            auto match = eventRe.match(line);
            if (match.hasMatch()) {
              eventFile = match.captured(0);
            }
          }
          // Spotlight device registers as one keyboard and one mouse device.
          // Select the correct one by EV field - not nice but works for now.
          else if (line.startsWith("B: EV=1f") && eventFile.size())
          {
            return QString("/dev/input/") + eventFile;
          }
          else if (line.isEmpty()) {
            break;
          }
        }
      }
    }
    return QString();
  }

  int indexOf(const void* needle, size_t needleSize,
              const QByteArray& haystack, size_t haystackSize)
  {
    if (needleSize == 0)
      return -1;

    const void* ptr = memmem(haystack.data(), haystackSize, needle, needleSize);
    if (ptr == nullptr) {
      return -1;
    }
    return static_cast<int>(reinterpret_cast<const char*>(ptr) - haystack.data());
  }
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

  // Try to find an already attached device and connect to it.
  auto device = findAttachedSpotlightDevice();
  if (device.size()) {
    connectToDevice(device);
  }

  setupUdevNotifier();
}

Spotlight::~Spotlight()
{
}

bool Spotlight::deviceConnected() const
{
  if (m_deviceSocketNotifier && m_deviceSocketNotifier->isEnabled()) {
    return true;
  }

  return false;
}

bool Spotlight::connectToDevice(const QString& devicePath)
{
  int evfd = ::open(devicePath.toLocal8Bit().constData(), O_RDONLY, 0);
  if (evfd < 0) // TODO: emit error message
    return false;

  m_deviceSocketNotifier.reset(new QSocketNotifier(evfd, QSocketNotifier::Read));
  connect(&*m_deviceSocketNotifier, &QSocketNotifier::activated, [this, devicePath](int fd)
  {
    static struct input_event ev;
    auto sz = ::read(fd, &ev, sizeof(ev));
    if (sz == sizeof(ev)) // for any kind of event from the device..
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
      m_deviceSocketNotifier->setEnabled(false);
      emit disconnected(devicePath);
      QTimer::singleShot(0, [this](){ m_deviceSocketNotifier.reset(); });
    }
  });

  connect(&*m_deviceSocketNotifier, &QSocketNotifier::destroyed, [this]() {
    if(m_deviceSocketNotifier) {
      ::close(static_cast<int>(m_deviceSocketNotifier->socket()));
    }
  });

  emit connected(devicePath);
  return true;
}

bool Spotlight::setupUdevNotifier()
{
  struct sockaddr_nl snl{};
  snl.nl_family = AF_NETLINK;
  snl.nl_groups = NETLINK_KOBJECT_UEVENT;

  int nlfd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
  if (nlfd == -1) {
    emit error(QString("Error creating socket: %1").arg(strerror(errno)));
    return false;
  }

  constexpr int min_buffersize = 2048;
  {
    int buffersize = 0; socklen_t s = sizeof(buffersize);
    int ret = getsockopt(nlfd, SOL_SOCKET, SO_RCVBUF, &buffersize, &s);
    if (ret == 0 && buffersize < min_buffersize * 2) {
      ret = setsockopt(nlfd, SOL_SOCKET, SO_RCVBUF, &min_buffersize, s);
    }
  }

  if (::bind(nlfd, reinterpret_cast<struct sockaddr*>(&snl), sizeof(struct sockaddr_nl)) != 0)
  {
    emit error(QString("Bind error: %1").arg(strerror(errno)));
    ::close(nlfd);
    return false;
  }

  struct sockaddr_nl _snl;
  socklen_t _addrlen =sizeof(struct sockaddr_nl);

  // Get the kernel assigned address
  if (getsockname(nlfd, reinterpret_cast<struct sockaddr*>(&_snl), &_addrlen) == 0) {
    snl.nl_pid = _snl.nl_pid;
  }

  m_linuxUdevNotifier.reset(new QSocketNotifier(nlfd, QSocketNotifier::Read));
  connect(&*m_linuxUdevNotifier, &QSocketNotifier::activated, [this](int socket) {
    static QByteArray data(min_buffersize, 0);
    const auto len = ::read(socket, data.data(), static_cast<size_t>(data.size()));
    if( len == -1 ) {
      m_linuxUdevNotifier->setEnabled(false);
      QTimer::singleShot(0, [this](){ m_linuxUdevNotifier.reset(); });
      return;
    }

    constexpr char actionStr[] = "ACTION=add";
    constexpr char vendorStr[] = "ID_VENDOR_ID=046d";
    constexpr char modelStr[] = "ID_MODEL_ID=c53e";
    constexpr char mouseStr[] = "INPUT_CLASS=mouse";
    constexpr char devStr[] = "DEVNAME=/dev/input/event";

    if (!deviceConnected())
    {
      if (indexOf(actionStr, sizeof(actionStr)-1, data, len) >= 0
          && indexOf(vendorStr, sizeof(vendorStr)-1, data, len) >= 0
          && indexOf(modelStr, sizeof(modelStr)-1, data, len) >= 0
          && indexOf(mouseStr, sizeof(mouseStr)-1, data, len) >= 0)
      {
        const int idx = indexOf(devStr, sizeof(devStr)-1, data, len);
        if( idx >= 0) {
          const auto newDevice = QString::fromUtf8(&data.data()[idx+8]);
          QTimer::singleShot(0, [this, newDevice](){
            connectToDevice(newDevice);
          });
        }
      }
    }
  });

  connect(&*m_linuxUdevNotifier, &QSocketNotifier::destroyed, [this]() {
    if(m_linuxUdevNotifier) {
      ::close(static_cast<int>(m_linuxUdevNotifier->socket()));
    }
  });

  return true;
}

