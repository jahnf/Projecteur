// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include <QObject>

#include <memory>
#include <map>

#include "enum-helper.h"
#include "devicescan.h"

class InputMapper;
class QTimer;
class VirtualDevice;

enum class DeviceFlag : uint32_t {
  NoFlags = 0,
  NonBlocking    = 1 << 0,
  SynEvents      = 1 << 1,
  RepEvents      = 1 << 2,
  RelativeEvents = 1 << 3,
};
ENUM(DeviceFlag, DeviceFlags)

/// Class to notify the application if the Logitech Spotlight and other supported devices
/// are connected and sending mouse move events. Used to turn the applications spot on or off.
class Spotlight : public QObject
{
  Q_OBJECT

public:
  struct Options {
    bool enableUInput = true; // enable virtual uinput device
    QList<SupportedDevice> additionalDevices;
  };

  explicit Spotlight(QObject* parent, Options options);
  virtual ~Spotlight();

  bool spotActive() const { return m_spotActive; }
  bool anySpotlightDeviceConnected() const;

  struct ConnectedDeviceInfo {
    DeviceId id;
    QString name;
  };

  uint32_t connectedDeviceCount() const;
  QList<ConnectedDeviceInfo> connectedDevices() const;

signals:
  void error(const QString& errMsg);
  void deviceConnected(const DeviceId& id, const QString& name);
  void deviceDisconnected(const DeviceId& id, const QString& name);
  void subDeviceConnected(const DeviceId& id, const QString& name, const QString& path);
  void subDeviceDisconnected(const DeviceId& id, const QString& name, const QString& path);
  void anySpotlightDeviceConnectedChanged(bool connected);
  void spotActiveChanged(bool isActive);

private:
  enum class ConnectionResult { CouldNotOpen, NotASpotlightDevice, Connected };
  ConnectionResult connectSpotlightDevice(const QString& devicePath, bool verbose = false);

  struct DeviceConnection;
  struct ConnectionDetails;
  using DevicePath = QString;
  using ConnectionMap = std::map<DevicePath, std::shared_ptr<DeviceConnection>>;

  std::shared_ptr<DeviceConnection> openEventDevice(const QString& devicePath, const DeviceId& devId);
  bool addInputEventHandler(std::shared_ptr<DeviceConnection> connection);

  bool setupDevEventInotify();
  int connectDevices();
  void removeDeviceConnection(const QString& devicePath);
  void onDeviceDataAvailable(int fd, DeviceConnection& connection);

  const Options m_options;
  std::map<DeviceId, std::unique_ptr<ConnectionDetails>> m_deviceConnections;

  QTimer* m_activeTimer = nullptr;
  QTimer* m_connectionTimer = nullptr;
  bool m_spotActive = false;
  std::shared_ptr<VirtualDevice> m_virtualDevice;
};
