// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include <map>

#include <QByteArray>



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
  // -----------------------------------------------------------------------------------------------
  namespace Bytes {
    constexpr uint8_t SHORT_MSG           = 0x10;
    constexpr uint8_t LONG_MSG            = 0x11;

    constexpr uint8_t MSG_TO_USB_RECEIVER = 0xff;
    constexpr uint8_t MSG_TO_SPOTLIGHT    = 0x01; // Spotlight is first device on the receiver (bluetooth also uses this code)

    constexpr uint8_t SHORT_GET_FEATURE   = 0x81;
    constexpr uint8_t SHORT_SET_FEATURE   = 0x80;

    constexpr uint8_t SHORT_WIRELESS_NOTIFICATION_CODE = 0x41;
  }

  /// Used for Bluetooth connections
  QByteArray shortToLongMsg(const QByteArray& shortMsg);

  /// Returns if msg is a valid hidpp message
  bool isValidMessage(const QByteArray& msg);
  bool isValidShortMessage(const QByteArray& msg);
  bool isValidLongMessage(const QByteArray& msg);

  bool isMessageForUsb(const QByteArray& msg);

  // Class to get and store Set of supported features for a HID++ 2.0 device
  class FeatureSet
  {
  public:
    void setHIDDeviceFileDescriptor(int fd) { m_fdHIDDevice = fd; }
    uint8_t getFeatureIndex(FeatureCode fc) const;
    bool supportFeatureCode(FeatureCode fc) const;
    auto getFeatureCount() const { return m_featureTable.size(); }
    uint8_t getRandomFunctionCode(uint8_t functionCode) const { return (functionCode | m_softwareIDBits); }
    void populateFeatureTable();

  private:
    uint8_t getFeatureIndexFromDevice(FeatureCode fc);
    uint8_t getFeatureCountFromDevice(uint8_t featureSetID);
    QByteArray getFirmwareVersionFromDevice();
    QByteArray getResponseFromDevice(const QByteArray &expectedBytes);

    std::map<uint16_t, uint8_t> m_featureTable;
    int m_fdHIDDevice = -1;
    uint8_t m_softwareIDBits = (rand() & 0x0f);
  };

} //end of HIDPP namespace
