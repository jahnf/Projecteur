// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "linuxdesktop.h"

#include <QProcessEnvironment>

LinuxDesktop::LinuxDesktop()
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
