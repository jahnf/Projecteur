// This file is part of Projecteur - https://github.com/jahnf/projecteur
// - See LICENSE.md and README.md
#pragma once

#include <QObject>
#include <QPixmap>
#include <QtDBus>

class QScreen;

class LinuxDesktop : public QObject
{
  Q_OBJECT

public:
  enum class Type : uint8_t { KDE, Gnome, Sway, Other };

  explicit LinuxDesktop(QObject* parent = nullptr);

  bool isWayland() const { return m_wayland; };
  Type type() const { return m_type; };

  QPixmap grabScreen(QScreen* screen) const;

private:
  bool m_wayland = false;
  Type m_type = Type::Other;

  QPixmap grabScreenWayland(QScreen* screen) const;
};

/*
 * Proxy class for interface org.freedesktop.portal.Request
 */
class OrgFreedesktopPortalRequestInterface : public QDBusAbstractInterface
{
    Q_OBJECT
public:
    OrgFreedesktopPortalRequestInterface(const QString& service,
                                         const QString& path,
                                         const QDBusConnection& connection,
                                         QObject* parent = nullptr);

    ~OrgFreedesktopPortalRequestInterface();

public Q_SLOTS:
    inline QDBusPendingReply<> Close()
    {
        QList<QVariant> argumentList;
        return asyncCallWithArgumentList(QStringLiteral("Close"), argumentList);
    }

Q_SIGNALS: // SIGNALS
    void Response(uint response, QVariantMap results);
};

namespace org {
namespace freedesktop {
namespace portal {
typedef ::OrgFreedesktopPortalRequestInterface Request;
}
}
}
