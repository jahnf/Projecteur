// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "hidpp.h"
#include "logging.h"

#include <unistd.h>

#include <QTime>

DECLARE_LOGGING_CATEGORY(hid)

// -------------------------------------------------------------------------------------------------
namespace {
  using HidppMsg = uint8_t[];

  template <typename C, size_t N>
  QByteArray make_QByteArray(const C(&a)[N]) {
    return {reinterpret_cast<const char*>(a),N};
  }

  class Hid_ : public QObject {}; // for i18n and logging
}

namespace HIDPP {
// -------------------------------------------------------------------------------------------------
QByteArray FeatureSet::getResponseFromDevice(const QByteArray& expectedBytes)
{
  if (m_fdHIDDevice == -1) return QByteArray();

  QByteArray readVal(20, 0);
  int timeOut = 4; // time out just in case device did not reply;
                   // 4 seconds time out is used by other prorams like Solaar.
  QTime timeOutTime = QTime::currentTime().addSecs(timeOut);
  while(true) {
    if(::read(m_fdHIDDevice, readVal.data(), readVal.length())) {
      if (readVal.mid(1, 3) == expectedBytes) return readVal;
      if (static_cast<uint8_t>(readVal.at(2)) == 0x8f) return readVal;  //Device not online
      if (QTime::currentTime() >= timeOutTime) return QByteArray();
    }
  }
}

// -------------------------------------------------------------------------------------------------
uint8_t FeatureSet::getFeatureIDFromDevice(FeatureCode fc)
{
  if (m_fdHIDDevice == -1) return 0x00;

  const uint8_t fSetLSB = static_cast<uint8_t>(static_cast<uint16_t>(fc) >> 8);
  const uint8_t fSetMSB = static_cast<uint8_t>(static_cast<uint16_t>(fc));

  const auto featureReqMessage = make_QByteArray(HidppMsg{
    HIDPP::Bytes::LONG_MSG, HIDPP::Bytes::MSG_TO_SPOTLIGHT, 0x00, 0x0d, fSetLSB, fSetMSB, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  });

  const auto res = ::write(m_fdHIDDevice, featureReqMessage.data(), featureReqMessage.size());
  if (res != featureReqMessage.size())
  {
    logDebug(hid) << Hid_::tr("Failed to write feature request message to device.");
    return 0x00;
  }

  const auto response = getResponseFromDevice(featureReqMessage.mid(1, 3));
  if (!response.length() || static_cast<uint8_t>(response.at(2)) == 0x8f) return 0x00;
  uint8_t featureID = static_cast<uint8_t>(response.at(4));

  return featureID;
}

// -------------------------------------------------------------------------------------------------
uint8_t FeatureSet::getFeatureCountFromDevice(uint8_t featureSetID)
{
  if (m_fdHIDDevice == -1) return 0x00;

  // Get Number of features (except Root Feature) supported
  const auto featureCountReqMessage = make_QByteArray(HidppMsg{
    HIDPP::Bytes::LONG_MSG, HIDPP::Bytes::MSG_TO_SPOTLIGHT, featureSetID, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  });

  const auto res = ::write(m_fdHIDDevice, featureCountReqMessage.data(), featureCountReqMessage.size());
  if (res != featureCountReqMessage.size())
  {
    logDebug(hid) << Hid_::tr("Failed to write feature count request message to device.");
    return 0x00;
  }

  const auto response = getResponseFromDevice(featureCountReqMessage.mid(1, 3));
  if (!response.length() || static_cast<uint8_t>(response.at(2)) == 0x8f) return 0x00;
  uint8_t featureCount = static_cast<uint8_t>(response.at(4));

  return featureCount;
}

// -------------------------------------------------------------------------------------------------
QByteArray FeatureSet::getFirmwareVersionFromDevice()
{
  if (m_fdHIDDevice == -1) return 0x00;

  // To get firmware details: first get Feature ID corresponding to Firmware feature code
  uint8_t fwID = getFeatureIDFromDevice(FeatureCode::FirmwareVersion);
  if (!fwID) return QByteArray();

  // Get the number of firmwares (Main HID++ application, BootLoader, or Hardware) now
  const auto fwCountReqMessage = make_QByteArray(HidppMsg{
    HIDPP::Bytes::LONG_MSG, HIDPP::Bytes::MSG_TO_SPOTLIGHT, fwID, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  });

  const auto res = ::write(m_fdHIDDevice, fwCountReqMessage.data(), fwCountReqMessage.size());
  if (res != fwCountReqMessage.size())
  {
    logDebug(hid) << Hid_::tr("Failed to write firmware count request message to device.");
    return 0x00;
  }

  const auto response = getResponseFromDevice(fwCountReqMessage.mid(1, 3));
  if (!response.length() || static_cast<uint8_t>(response.at(2)) == 0x8f) return QByteArray();
  const uint8_t fwCount = static_cast<uint8_t>(response.at(4));

  // The following info is not used currently; however, these commented lines are kept for future reference.
  // uint8_t connectionMode = static_cast<uint8_t>(response.at(10));
  // bool supportBluetooth = (connectionMode & 0x01);
  // bool supportBluetoothLE = (connectionMode & 0x02);  // true for Logitech Spotlight
  // bool supportUsbReceiver = (connectionMode & 0x04);  // true for Logitech Spotlight
  // bool supportUsbWired = (connectionMode & 0x08);
  // auto unitID = response.mid(5, 4);
  // auto modelIDs = response.mid(11, 8);
  // int count = 0;
  // if (supportBluetooth) { auto btmodelID = modelIDs.mid(count, 2); count += 2;}
  // if (supportBluetoothLE) { auto btlemodelID = modelIDs.mid(count, 2); count += 2;}
  // if (supportUsbReceiver) { auto wpmodelID = modelIDs.mid(count, 2); count += 2;}
  // if (supportUsbWired) { auto usbmodelID = modelIDs.mid(count, 2); count += 2;}


  // Iteratively find out firmware versions for all firmwares and get the firmware for main application
  for (uint8_t i = 0x00; i < fwCount; i++)
  {
    const auto fwVerReqMessage = make_QByteArray(HidppMsg{
      HIDPP::Bytes::LONG_MSG, HIDPP::Bytes::MSG_TO_SPOTLIGHT, fwID, 0x1d, i, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    });

    const auto res = ::write(m_fdHIDDevice, fwVerReqMessage.data(), fwVerReqMessage.length());
    if (res != fwCountReqMessage.size())
    {
      logDebug(hid) << Hid_::tr("Failed to write firmware request message to device (%1).")
                             .arg(int(i));
      return 0x00;
    }
    const auto fwResponse = getResponseFromDevice(fwVerReqMessage.mid(1, 3));
    if (!fwResponse.length() || static_cast<uint8_t>(fwResponse.at(2)) == 0x8f) return QByteArray();
    const auto fwType = (fwResponse.at(4) & 0x0f);  // 0 for main HID++ application, 1 for BootLoader, 2 for Hardware, 3-15 others
    const auto fwVersion = fwResponse.mid(5, 7);
    // Currently we are not interested in these details; however, these commented lines are kept for future reference.
    //auto firmwareName = fwVersion.mid(0, 3).data();
    //auto majorVesion = fwResponse.at(3);
    //auto MinorVersion = fwResponse.at(4);
    //auto build = fwResponse.mid(5);
    if (fwType == 0)
    {
      logDebug(hid) << "Main application firmware Version:" << fwVersion.toHex();
      return fwVersion;
    }
  }
  return QByteArray();
}

// -------------------------------------------------------------------------------------------------
void FeatureSet::populateFeatureTable()
{
  if (m_fdHIDDevice == -1) return;

  // Get the firmware version
  const auto firmwareVersion = getFirmwareVersionFromDevice();
  if (!firmwareVersion.length()) return;

  // TODO:: Read and write cache file (settings most probably)
  // if the firmware details match with cached file; then load the FeatureTable from file
  // else read the entire feature table from the device
  QByteArray cacheFirmwareVersion;  // currently a dummy variable for Firmware Version from cache file.

  if (firmwareVersion == cacheFirmwareVersion)
  {
    // TODO: load the featureSet from the cache file
  }
  else
  {
    // For reading feature table from device
    // first get featureID for FeatureCode::FeatureSet
    // then we can get the number of features supported by the device (except Root Feature)
    const uint8_t featureSetID = getFeatureIDFromDevice(FeatureCode::FeatureSet);
    if (!featureSetID) return;
    const uint8_t featureCount = getFeatureCountFromDevice(featureSetID);
    if (!featureCount) return;

    // Root feature is supported by all HID++ 2.0 device and has a featureID of 0 always.
    m_featureTable.insert({static_cast<uint16_t>(FeatureCode::Root), 0x00});

    // Read Feature Code for other featureIds from device.
    for (uint8_t featureId = 0x01; featureId <= featureCount; ++featureId)
    {
      const auto featureCodeReqMsg = make_QByteArray(HidppMsg{
        HIDPP::Bytes::LONG_MSG, HIDPP::Bytes::MSG_TO_SPOTLIGHT, featureSetID, 0x1d, featureId, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
      });
      const auto res = ::write(m_fdHIDDevice, featureCodeReqMsg.data(), featureCodeReqMsg.size());
      if (res != featureCodeReqMsg.size()) {
        logDebug(hid) << Hid_::tr("Failed to write feature code request message to device.");
        return;
      }

      const auto response = getResponseFromDevice(featureCodeReqMsg.mid(1, 3));
      if (!response.length() || static_cast<uint8_t>(response.at(2)) == 0x8f) {
        m_featureTable.clear();
        return;
      }
      const uint16_t featureCode = (static_cast<uint16_t>(response.at(4)) << 8) | static_cast<uint8_t>(response.at(5));
      const uint8_t featureType = static_cast<uint8_t>(response.at(6));
      const auto softwareHidden = (featureType & (1<<6));
      const auto obsoleteFeature = (featureType & (1<<7));
      if (!(softwareHidden) && !(obsoleteFeature)) m_featureTable.insert({featureCode, featureId});
    }
  }
}

// -------------------------------------------------------------------------------------------------
bool FeatureSet::supportFeatureCode(FeatureCode fc) const
{
  const auto featurePair = m_featureTable.find(static_cast<uint16_t>(fc));
  return (featurePair != m_featureTable.end());
}

// -------------------------------------------------------------------------------------------------
uint8_t FeatureSet::getFeatureID(FeatureCode fc) const
{
  if (!supportFeatureCode(fc)) return 0x00;

  const auto featurePair = m_featureTable.find(static_cast<uint16_t>(fc));
  return featurePair->second;
}

// -------------------------------------------------------------------------------------------------
QByteArray shortToLongMsg(const QByteArray& shortMsg)
{
  const bool isValidShortMsg = (shortMsg.at(0) == Bytes::SHORT_MSG && shortMsg.length() == 7);

  if (isValidShortMsg)
  {
    QByteArray longMsg;
    longMsg.reserve(20);
    longMsg.append(Bytes::LONG_MSG);
    longMsg.append(shortMsg.mid(1));
    longMsg.append(20 - longMsg.length(), 0);
    return longMsg;
  }

  return shortMsg;
}

// -------------------------------------------------------------------------------------------------
bool isValidMessage(const QByteArray& msg) {
  return (isValidShortMessage(msg) || isValidLongMessage(msg));
}

// -------------------------------------------------------------------------------------------------
bool isValidShortMessage(const QByteArray& msg) {
  return (msg.length() == 7 && static_cast<uint8_t>(msg.at(0)) == Bytes::SHORT_MSG);
}

// -------------------------------------------------------------------------------------------------
bool isValidLongMessage(const QByteArray& msg) {
  return (msg.length() == 20 && static_cast<uint8_t>(msg.at(0)) == Bytes::LONG_MSG);
}

// -------------------------------------------------------------------------------------------------
bool isMessageForUsb(const QByteArray& msg) {
  return (static_cast<uint8_t>(msg.at(1)) == Bytes::MSG_TO_USB_RECEIVER);
}

} // end namespace HIDPP