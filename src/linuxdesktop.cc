// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "linuxdesktop.h"

#include <QApplication>
#include <QDebug>
#include <QDesktopWidget>
#include <QDir>
#include <QFile>
#include <QProcessEnvironment>
#include <QScreen>

#if HAS_Qt5_DBus
#include <QDBusInterface>
#include <QDBusReply>
#endif

namespace {
  // -----------------------------------------------------------------------------------------------
  QPixmap grabScreenDBusGnome()
  {
    const auto filepath = QDir::temp().absoluteFilePath("000_projecteur_zoom_screenshot.png");
    QDBusInterface interface(QStringLiteral("org.gnome.Shell"),
                             QStringLiteral("/org/gnome/Shell/Screenshot"),
                             QStringLiteral("org.gnome.Shell.Screenshot"));
    QDBusReply<bool> reply = interface.call(QStringLiteral("Screenshot"), false, false, filepath);
    
    if (reply.value()) 
    {
      QPixmap pm(filepath);
      QFile::remove(filepath);
      return pm;
    }
    qDebug() << "Error: Screenshot via GNOME DBus interface failed.";
    return QPixmap();
  }

  // -----------------------------------------------------------------------------------------------
  QPixmap grabScreenDBusKde()
  {
    QDBusInterface interface(QStringLiteral("org.kde.KWin"),
                             QStringLiteral("/Screenshot"),
                             QStringLiteral("org.kde.kwin.Screenshot"));
    QDBusReply<QString> reply = interface.call(QStringLiteral("screenshotFullscreen"));
    QPixmap pm(reply.value());
    if (!pm.isNull()) {
      QFile::remove(reply.value());
    } else {
      qDebug() << "Error: Screenshot via KDE DBus interface failed.";
    }
    return pm;
  }

  // -----------------------------------------------------------------------------------------------
  QPixmap grabScreenVirtualDesktop(QScreen* screen)
  {
    QRect g;
    for (const auto screen : QGuiApplication::screens()) {
      g = g.united(screen->geometry());
    }

    QPixmap pm(QApplication::primaryScreen()->grabWindow(
                 QApplication::desktop()->winId(), g.x(), g.y(), g.width(), g.height()));

    if (!pm.isNull())  
    {
      pm.setDevicePixelRatio(screen->devicePixelRatio());
      return pm.copy(screen->geometry());
    }

    return pm;
  }
} // end anonymous namespace

LinuxDesktop::LinuxDesktop(QObject* parent)
  : QObject(parent)
{
  const auto env = QProcessEnvironment::systemEnvironment();
  { // check for Kde and Gnome
    const auto kdeFullSession = env.value(QStringLiteral("KDE_FULL_SESSION"));
    const auto gnomeSessionId = env.value(QStringLiteral("GNOME_DESKTOP_SESSION_ID"));
    const auto desktopSession = env.value(QStringLiteral("DESKTOP_SESSION"));
    const auto xdgCurrentDesktop = env.value(QStringLiteral("XDG_CURRENT_DESKTOP"));
    if (gnomeSessionId.size() || xdgCurrentDesktop.contains("Gnome", Qt::CaseInsensitive)) {
      m_type = LinuxDesktop::Type::Gnome;
    } 
    else if (kdeFullSession.size() || desktopSession == "kde-plasma") {
      m_type = LinuxDesktop::Type::KDE;
    }
  }

  { // check for wayland session
    const auto waylandDisplay = env.value(QStringLiteral("WAYLAND_DISPLAY"));
    const auto xdgSessionType = env.value(QStringLiteral("XDG_SESSION_TYPE"));
    m_wayland = (xdgSessionType == "wayland") 
                || waylandDisplay.contains("wayland", Qt::CaseInsensitive);
  }
}

QPixmap LinuxDesktop::grabScreen(QScreen* screen) const
{
  if (screen == nullptr) 
    return QPixmap();
  
  if (isWayland()) 
    return grabScreenWayland(screen);
  
  if (QApplication::desktop()->isVirtualDesktop()) 
    return grabScreenVirtualDesktop(screen);
 
  // everything else.. usually X11
  return screen->grabWindow(0);
}

QPixmap LinuxDesktop::grabScreenWayland(QScreen* screen) const
{
  QPixmap pm;
  switch (type()) 
  {
  case LinuxDesktop::Type::Gnome: 
    pm = grabScreenDBusGnome(); 
    break;
  case LinuxDesktop::Type::KDE: 
    pm = grabScreenDBusKde(); 
    break;
  default:
    qDebug() << "Warning: Currently zoom on Wayland is only supported via DBus on KDE and GNOME."; 
  }
  return pm.isNull() ? pm : pm.copy(screen->geometry());
}
