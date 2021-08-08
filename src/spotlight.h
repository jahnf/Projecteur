// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include <QObject>

#include <map>
#include <memory>
#include <vector>

#include "devicescan.h"
#include "deviceinput.h"

class QTimer;
class Settings;
class VirtualDevice;

// -----------------------------------------------------------------------------------------------
struct HoldButtonStatus {
  enum class HoldButtonType : uint8_t { None, Next, Back };

  void setButton(HoldButtonType b){ _button = b; _numEvents=0; };
  auto getButton() const { return _button; }
  int numEvents() const { return _numEvents; };
  void addEvent(){ _numEvents++; };
  void reset(){ setButton(HoldButtonType::None); };
  auto keyEventSeq() {
    using namespace ReservedKeyEventSequence;
    switch (_button){
      case HoldButtonType::Next:
        return NextHoldInfo.keqEventSeq;
      case HoldButtonType::Back:
        return BackHoldInfo.keqEventSeq;
      case HoldButtonType::None:
        return KeyEventSequence();
      }
    return KeyEventSequence();
  };

private:
  HoldButtonType _button = HoldButtonType::None;
  unsigned long _numEvents = 0;
};

/// Class handling spotlight device connections and indicating if a device is sending
/// sending mouse move events.
class Spotlight : public QObject
{
  Q_OBJECT

public:
  struct Options {
    bool enableUInput = true; // enable virtual uinput device
    std::vector<SupportedDevice> additionalDevices;
  };

  explicit Spotlight(QObject* parent, Options options, Settings* settings);
  virtual ~Spotlight();

  bool spotActive() const { return m_spotActive; }
  void setSpotActive(bool active);

  struct ConnectedDeviceInfo {
    DeviceId id;
    QString name;
  };

  bool anySpotlightDeviceConnected() const;
  uint32_t connectedDeviceCount() const;
  std::vector<ConnectedDeviceInfo> connectedDevices() const;
  std::shared_ptr<DeviceConnection> deviceConnection(const DeviceId& deviceId);

signals:
  void deviceConnected(const DeviceId& id, const QString& name);
  void deviceDisconnected(const DeviceId& id, const QString& name);
  void deviceActivated(const DeviceId& id, const QString& name);
  void subDeviceConnected(const DeviceId& id, const QString& name, const QString& path);
  void subDeviceDisconnected(const DeviceId& id, const QString& name, const QString& path);
  void anySpotlightDeviceConnectedChanged(bool connected);
  void spotActiveChanged(bool isActive);

private:
  enum class ConnectionResult { CouldNotOpen, NotASpotlightDevice, Connected };
  ConnectionResult connectSpotlightDevice(const QString& devicePath, bool verbose = false);

  bool addInputEventHandler(std::shared_ptr<SubEventConnection> connection);
  bool addHidppInputHandler(std::shared_ptr<SubHidppConnection> connection);

  bool setupDevEventInotify();
  int connectDevices();
  void removeDeviceConnection(const QString& devicePath);
  void onEventDataAvailable(int fd, SubEventConnection& connection);
  void onHidppDataAvailable(int fd, SubHidppConnection& connection);

  const Options m_options;
  std::map<DeviceId, std::shared_ptr<DeviceConnection>> m_deviceConnections;

  QTimer* m_activeTimer = nullptr;
  QTimer* m_connectionTimer = nullptr;
  bool m_spotActive = false;
  std::shared_ptr<VirtualDevice> m_virtualDevice;
  Settings* m_settings = nullptr;
  HoldButtonStatus m_holdButtonStatus;
};
