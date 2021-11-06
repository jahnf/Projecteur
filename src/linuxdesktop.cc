// This file is part of Projecteur - https://github.com/jahnf/projecteur
// - See LICENSE.md and README.md

#include "linuxdesktop.h"

#include "logging.h"

#include <QApplication>
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
  #include <QDesktopWidget>
#endif
#include <QDir>
#include <QFile>
#include <QProcessEnvironment>
#include <QScreen>
#include <QUuid>

#if HAS_Qt_DBus
#include <QDBusInterface>
#include <QDBusReply>
#endif

LOGGING_CATEGORY(desktop, "desktop")

namespace {
#if HAS_Qt_DBus
  // -----------------------------------------------------------------------------------------------
  // This function works. However, it is not user-friendly and automated.
  // Please see https://github.com/flatpak/xdg-desktop-portal/issues/649
  QPixmap grabScreenDBusXDGdesktop()
  {
    QDBusInterface interface(QStringLiteral("org.freedesktop.portal.Desktop"),
                             QStringLiteral("/org/freedesktop/portal/desktop"),
                             QStringLiteral("org.freedesktop.portal.Screenshot"));
    // unique token
    QString token = QUuid::createUuid().toString().remove('-').remove('{').remove('}');
    // premake interface
    auto* request = new OrgFreedesktopPortalRequestInterface(
                QStringLiteral("org.freedesktop.portal.Desktop"),
                "/org/freedesktop/portal/desktop/request/" +
                QDBusConnection::sessionBus().baseService().remove(':').replace('.', '_') +
                "/" + token,
                QDBusConnection::sessionBus(), NULL);

    QEventLoop loop;
    QPixmap pm;
    const auto gotSignal = [&pm, &loop](uint status, const QVariantMap& map) {
      if (status == 0)
      {
        QString uri = map.value("uri").toString().remove(0, 7);
        pm = QPixmap(uri);
        pm.setDevicePixelRatio(qApp->devicePixelRatio());
        QFile imgFile(uri);
        imgFile.remove();
      }
      loop.quit();
    };

    // prevent racy situations and listen before calling screenshot
    QMetaObject::Connection conn = QObject::connect(
                request, &org::freedesktop::portal::Request::Response, gotSignal);

    interface.call(QStringLiteral("Screenshot"),
                   "",
                   QMap<QString, QVariant>({ { "handle_token", QVariant(token) },
                                             { "interactive", QVariant(false) } }));

    loop.exec();
    QObject::disconnect(conn);
    request->Close().waitForFinished();
    request->deleteLater();

    if (pm.isNull())
    {
      logError(desktop) << LinuxDesktop::tr("Screenshot via DBus interface failed.");
      return QPixmap();
    }
    return pm;
  }
  // -----------------------------------------------------------------------------------------------
  // This function do not work in Gnome 41+. Remove this in future as grabScreenDBusXDGdesktop is
  // more universal way of capturing screen on wayland.
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
    logError(desktop) << LinuxDesktop::tr("Screenshot via GNOME DBus interface failed.");
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
      logError(desktop) << LinuxDesktop::tr("Screenshot via KDE DBus interface failed.");
    }
    return pm;
  }
#endif // HAS_Qt_DBus

  // -----------------------------------------------------------------------------------------------
  QPixmap grabScreenVirtualDesktop(QScreen* screen)
  {
    QRect g;
    for (const auto s : QGuiApplication::screens()) {
      g = g.united(s->geometry());
    }

    #if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    QPixmap pm(QApplication::primaryScreen()->grabWindow(
                 QApplication::desktop()->winId(), g.x(), g.y(), g.width(), g.height()));
    #else
    QPixmap pm(QApplication::primaryScreen()->grabWindow(0, g.x(), g.y(), g.width(), g.height()));
    #endif

    if (!pm.isNull())
    {
      pm.setDevicePixelRatio(screen->devicePixelRatio());
      return pm.copy(screen->geometry());
    }

    return pm;
  }
} // end anonymous namespace


OrgFreedesktopPortalRequestInterface::OrgFreedesktopPortalRequestInterface(
  const QString& service,
  const QString& path,
  const QDBusConnection& connection,
  QObject* parent)
  : QDBusAbstractInterface(service,
                           path,
                           "org.freedesktop.portal.Request",
                           connection,
                           parent)
{}

OrgFreedesktopPortalRequestInterface::~OrgFreedesktopPortalRequestInterface() {}

LinuxDesktop::LinuxDesktop(QObject* parent)
  : QObject(parent)
{
  const auto env = QProcessEnvironment::systemEnvironment();
  { // check for Kde and Gnome
    const auto kdeFullSession = env.value(QStringLiteral("KDE_FULL_SESSION"));
    const auto gnomeSessionId = env.value(QStringLiteral("GNOME_DESKTOP_SESSION_ID"));
    const auto xdgCurrentDesktop = env.value(QStringLiteral("XDG_CURRENT_DESKTOP"));

    if (gnomeSessionId.size() || xdgCurrentDesktop.contains("Gnome", Qt::CaseInsensitive)) {
      m_type = LinuxDesktop::Type::Gnome;
    }
    else if (kdeFullSession.size() || xdgCurrentDesktop.contains("kde-plasma", Qt::CaseInsensitive)) {
      m_type = LinuxDesktop::Type::KDE;
    }
    else if (xdgCurrentDesktop.contains(QLatin1String("sway"), Qt::CaseInsensitive)) {
      m_type = LinuxDesktop::Type::Sway;
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
  if (screen == nullptr) {
    return QPixmap();
  }

  if (isWayland()) {
    return grabScreenWayland(screen);
  }

  #if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
    const bool isVirtualDesktop = QApplication::primaryScreen()->virtualSiblings().size() > 1;
  #else
    const bool isVirtualDesktop = QApplication::desktop()->isVirtualDesktop();
  #endif

  if (isVirtualDesktop) {
    return grabScreenVirtualDesktop(screen);
  }

  // everything else.. usually X11
  return screen->grabWindow(0);
}

QPixmap LinuxDesktop::grabScreenWayland(QScreen* screen) const
{
#if HAS_Qt_DBus
  QPixmap pm;
  if (type() == LinuxDesktop::Type::Gnome)
  {
    pm = grabScreenDBusGnome();
  } else if (type() == LinuxDesktop::Type::KDE)
  {
    pm = grabScreenDBusKde();
  }

  // grabScreenDBusGnome may fail with Gnome 41+. Use xdg-desktop-portal
  // grab function for any wayland compositor. However this function is
  // not used as default as it is not user friendly. Please see
  // https://github.com/flatpak/xdg-desktop-portal/issues/649
  // If the PixelMap remain Null after this step then the compositor
  // is not supported.
  if (pm.isNull())
  {
    pm = grabScreenDBusXDGdesktop();
  }

  if (pm.isNull())
  {
    logWarning(desktop) << tr("Currently zoom on Wayland is only supported via DBus on KDE, GNOME and Sway.");
  }
  return pm.isNull() ? pm : pm.copy(screen->geometry());
#else
  Q_UNUSED(screen);
  logWarning(desktop) << tr("Projecteur was compiled without Qt DBus.");
  return QPixmap();
#endif
}
