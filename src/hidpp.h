// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include <map>

#include <QByteArray>

#define HIDPP_SHORT_MSG                         0x10
#define HIDPP_LONG_MSG                          0x11

#define MSG_TO_USB_RECEIVER                     0xff
#define MSG_TO_SPOTLIGHT                        0x01   // Spotlight is first device on the receiver (bluetooth also uses this code)

#define HIDPP_SHORT_GET_FEATURE                 0x81
#define HIDPP_SHORT_SET_FEATURE                 0x80

#define HIDPP_SHORT_WIRELESS_NOTIFICATION_CODE  0x41



// Feature Codes important for Logitech Spotlight
enum class FeatureCode : uint16_t {
  Root                 = 0x0000,
  FeatureSet           = 0x0001,
  FirmwareVersion      = 0x0003,
  DeviceName           = 0x0005,
  Reset                = 0x0020,
  DFUControlSigned     = 0x00c2,
  BatteryStatus        = 0x1000,
  PresenterControl     = 0x1a00,
  Sensor3D             = 0x1a01,
  ReprogramControlsV4  = 0x1b04,
  WirelessDeviceStatus = 0x1db4,
  SwapCancelButton     = 0x2005,
  PointerSpeed         = 0x2205,
};

namespace HIDPP {
  /// Used for Bluetooth connections
  QByteArray shortToLongMsg(const QByteArray& shortMsg);
}

// Class to get and store Set of supported features for a HID++ 2.0 device
class FeatureSet
{
public:
  void setHIDDeviceFileDescriptor(int fd) { m_fdHIDDevice = fd; }
  uint8_t getFeatureID(FeatureCode fc);
  bool supportFeatureCode(FeatureCode fc);
  auto getFeatureCount() { return m_featureTable.size(); }
  void populateFeatureTable();

private:
  uint8_t getFeatureIDFromDevice(FeatureCode fc);
  uint8_t getFeatureCountFromDevice(uint8_t featureSetID);
  QByteArray getFirmwareVersionFromDevice();
  QByteArray getResponseFromDevice(const QByteArray& expectedBytes);

  std::map<uint16_t, uint8_t> m_featureTable;
  int m_fdHIDDevice = -1;
};
