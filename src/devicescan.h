// This file is part of Projecteur - https://github.com/jahnf/projecteur
// - See LICENSE.md and README.md
#pragma once

#include "device-defs.h"

#include <QMetaType>
#include <QStringList>

#include <vector>
#include <tuple>

// -------------------------------------------------------------------------------------------------
struct SupportedDevice
{
  quint16 vendorId;
  quint16 productId;
  bool isBluetooth = false;
  QString name = {};
};

// -------------------------------------------------------------------------------------------------
struct DeviceId
{
  uint16_t vendorId = 0;
  uint16_t productId = 0;
  BusType busType = BusType::Unknown;
  QString phys; // should be sufficient to differentiate between two devices of the same type
                // - not tested, don't have two devices of any type currently.

  inline bool operator==(const DeviceId& rhs) const {
    return std::tie(vendorId, productId, busType, phys) == std::tie(rhs.vendorId, rhs.productId, rhs.busType, rhs.phys);
  }

  inline bool operator!=(const DeviceId& rhs) const {
    return std::tie(vendorId, productId, busType, phys) != std::tie(rhs.vendorId, rhs.productId, rhs.busType, rhs.phys);
  }

  inline bool operator<(const DeviceId& rhs) const {
    return std::tie(vendorId, productId, busType, phys) < std::tie(rhs.vendorId, rhs.productId, rhs.busType, rhs.phys);
  }
};
Q_DECLARE_METATYPE(DeviceId);

// -------------------------------------------------------------------------------------------------
namespace DeviceScan
{
  struct SubDevice { // Structure for device scan results
    enum class Type : uint8_t { Unknown, Event, Hidraw };
    QString deviceFile;
    QString phys;
    Type type = Type::Unknown;
    bool hasRelativeEvents = false;
    bool deviceReadable = false;
    bool deviceWritable = false;
  };

  struct Device { // Structure for device scan results
    const QString& getName() const { return userName.size() ? userName : name; }
    QString name;
    QString userName;
    DeviceId id;
    std::vector<SubDevice> subDevices;
  };

  struct ScanResult {
    std::vector<Device> devices;
    quint16 numDevicesReadable = 0;
    quint16 numDevicesWritable = 0;
    QStringList errorMessages;
  };

  /// Scan for supported devices and check if they are accessible
  ScanResult getDevices(const std::vector<SupportedDevice>& additionalDevices = {});
}
