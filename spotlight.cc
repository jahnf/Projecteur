// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "spotlight.h"

#include <QBuffer>
#include <QFile>
#include <QRegularExpression>
#include <QSocketNotifier>
#include <QTimer>

#include <fcntl.h>
#include <sys/socket.h>
#include <linux/input.h>
#include <linux/netlink.h>
#include <unistd.h>

#include <QDebug>

namespace {
  constexpr char inputDevicesFile[] = "/proc/bus/input/devices";

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
    connectDevice(device);
  }
}

Spotlight::~Spotlight()
{

}

bool Spotlight::connectDevice(const QString& devicePath)
{
  int evfd = ::open(devicePath.toLocal8Bit().constData(), O_RDONLY, 0);
  if (evfd < 0) // TODO: emit error message
    return false;

  m_deviceSocketNotifier.reset(new QSocketNotifier(evfd, QSocketNotifier::Read));
  connect(&*m_deviceSocketNotifier, &QSocketNotifier::activated, [this](int fd) {
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
      QTimer::singleShot(0, [this](){ m_deviceSocketNotifier.reset(); });
    }
  });

  connect(&*m_deviceSocketNotifier, &QSocketNotifier::destroyed, [this](){
    if(m_deviceSocketNotifier && m_deviceSocketNotifier->isEnabled()) {
      ::close(static_cast<int>(m_deviceSocketNotifier->socket()));
    }
  });
  return false;
}


//  const int buffersize = 16 * 1024 * 1024;
//  int retval;

//  struct sockaddr_nl snl;
//  memset(&snl, 0x00, sizeof(struct sockaddr_nl));
//  snl.nl_family = AF_NETLINK;
//  snl.nl_groups = NETLINK_KOBJECT_UEVENT;

//  int nlfd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
//  if (nlfd == -1) {
////    qWarning("error getting socket: %s", strerror(errno));
////    return false;
//    return;
//  }

//  /* set receive buffersize */
//  setsockopt(nlfd, SOL_SOCKET, SO_RCVBUFFORCE, &buffersize, sizeof(buffersize));
//  retval = bind(nlfd, reinterpret_cast<struct sockaddr*>(&snl), sizeof(struct sockaddr_nl));
//  if (retval < 0) {
////      qWarning("bind failed: %s", strerror(errno));
//      close(nlfd);
////      netlink_socket = -1;
////      return false;
//      return;
//  } else if (retval == 0) {
//    //from libudev-monitor.c
//    struct sockaddr_nl _snl;
//    socklen_t _addrlen;

//    /*
//     * get the address the kernel has assigned us
//     * it is usually, but not necessarily the pid
//     */
//    _addrlen = sizeof(struct sockaddr_nl);
//    retval = getsockname(nlfd, (struct sockaddr *)&_snl, &_addrlen);
//    if (retval == 0)
//      snl.nl_pid = _snl.nl_pid;
//  }
//  m_linuxUdevNotifier = new QSocketNotifier(nlfd, QSocketNotifier::Read, this);
//  connect(m_linuxUdevNotifier, &QSocketNotifier::activated, [this](int socket){
//    qDebug() << "______ NEW EVENT";
//    constexpr int bufsize = 2048*2;
//    QByteArray data(bufsize, 0);
//    auto len = ::read(m_linuxUdevNotifier->socket(), data.data(), bufsize);
//    qDebug("read fro socket %ld bytes", len);
//    data.resize(static_cast<int>(len));
//    data =data.replace(0, ' ').trimmed(); //In the original line each information is seperated by 0
//    QBuffer buf(&data);
//    buf.open(QIODevice::ReadOnly);
//    while(!buf.atEnd()) {
//      qDebug() << "--- " << buf.readLine().trimmed();
//    }
//  });
//  m_linuxUdevNotifier->setEnabled(true);
