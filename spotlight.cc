#include "spotlight.h"

#include <QSocketNotifier>
#include <QBuffer>

#include <sys/socket.h>
#include <linux/netlink.h>
#include <unistd.h>

#include <QDebug>

Spotlight::Spotlight(QObject* parent)
  : QObject(parent)
{
  const int buffersize = 16 * 1024 * 1024;
  int retval;

  struct sockaddr_nl snl;
  memset(&snl, 0x00, sizeof(struct sockaddr_nl));
  snl.nl_family = AF_NETLINK;
  snl.nl_groups = NETLINK_KOBJECT_UEVENT;

  int nlfd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
  if (nlfd == -1) {
//    qWarning("error getting socket: %s", strerror(errno));
//    return false;
    return;
  }

    /* set receive buffersize */
    setsockopt(nlfd, SOL_SOCKET, SO_RCVBUFFORCE, &buffersize, sizeof(buffersize));
    retval = bind(nlfd, reinterpret_cast<struct sockaddr*>(&snl), sizeof(struct sockaddr_nl));
    if (retval < 0) {
//      qWarning("bind failed: %s", strerror(errno));
      close(nlfd);
//      netlink_socket = -1;
//      return false;
      return;
    } else if (retval == 0) {
      //from libudev-monitor.c
      struct sockaddr_nl _snl;
      socklen_t _addrlen;

      /*
       * get the address the kernel has assigned us
       * it is usually, but not necessarily the pid
       */
      _addrlen = sizeof(struct sockaddr_nl);
      retval = getsockname(nlfd, (struct sockaddr *)&_snl, &_addrlen);
      if (retval == 0)
        snl.nl_pid = _snl.nl_pid;
    }
    m_linuxUdevNotifier = new QSocketNotifier(nlfd, QSocketNotifier::Read, this);
    connect(m_linuxUdevNotifier, &QSocketNotifier::activated, [this](int socket){
      qDebug() << "______ NEW EVENT";
      constexpr int bufsize = 2048*2;
      QByteArray data(bufsize, 0);
      auto len = ::read(m_linuxUdevNotifier->socket(), data.data(), bufsize);
      qDebug("read fro socket %ld bytes", len);
      data.resize(static_cast<int>(len));
      data =data.replace(0, ' ').trimmed(); //In the original line each information is seperated by 0
      QBuffer buf(&data);
      buf.open(QIODevice::ReadOnly);
      while(!buf.atEnd()) {
        qDebug() << "--- " << buf.readLine().trimmed();
      }
    });
    m_linuxUdevNotifier->setEnabled(true);
}
