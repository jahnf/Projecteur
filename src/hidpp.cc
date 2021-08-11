// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "hidpp.h"
#include "logging.h"

#include <unistd.h>

#include <QCoreApplication>
#include <QTime>

DECLARE_LOGGING_CATEGORY(hid)

// -------------------------------------------------------------------------------------------------
namespace {
  using HidppMsg = uint8_t[];

  template <typename C, size_t N>
  QByteArray make_QByteArray(const C(&a)[N]) {
    return {reinterpret_cast<const char*>(a),N};
  }
}

namespace HIDPP {
// -------------------------------------------------------------------------------------------------
QByteArray FeatureSet::getResponseFromDevice(const QByteArray& expectedBytes)
{
  if (m_fdHIDDevice == -1) return QByteArray();

  QByteArray readVal(20, 0);
  int timeOut = 4; // time out just in case device did not reply;
                   // 4 seconds time out is used by other programs like Solaar.
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
uint8_t FeatureSet::getFeatureIndexFromDevice(FeatureCode fc)
{
  if (m_fdHIDDevice == -1) return 0x00;

  const uint8_t fSetLSB = static_cast<uint8_t>(static_cast<uint16_t>(fc) >> 8);
  const uint8_t fSetMSB = static_cast<uint8_t>(static_cast<uint16_t>(fc));

  const auto featureReqMessage = make_QByteArray(HidppMsg{
    HIDPP::Bytes::LONG_MSG, HIDPP::Bytes::MSG_TO_SPOTLIGHT, 0x00, getRandomFunctionCode(0x00),
    fSetLSB, fSetMSB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  });

  const auto res = ::write(m_fdHIDDevice, featureReqMessage.data(), featureReqMessage.size());
  if (res != featureReqMessage.size())
  {
    logDebug(hid) << tr("Failed to write feature request message to device.");
    return 0x00;
  }

  const auto response = getResponseFromDevice(featureReqMessage.mid(1, 3));
  if (!response.length() || static_cast<uint8_t>(response.at(2)) == 0x8f) return 0x00;
  uint8_t featureIndex = static_cast<uint8_t>(response.at(4));

  return featureIndex;
}

// -------------------------------------------------------------------------------------------------
uint8_t FeatureSet::getFeatureCountFromDevice(uint8_t featureSetIndex)
{
  if (m_fdHIDDevice == -1) return 0x00;

  // Get Number of features (except Root Feature) supported
  const auto featureCountReqMessage = make_QByteArray(HidppMsg{
    HIDPP::Bytes::LONG_MSG, HIDPP::Bytes::MSG_TO_SPOTLIGHT, featureSetIndex, getRandomFunctionCode(0x00),
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  });

  const auto res = ::write(m_fdHIDDevice, featureCountReqMessage.data(), featureCountReqMessage.size());
  if (res != featureCountReqMessage.size())
  {
    logDebug(hid) << tr("Failed to write feature count request message to device.");
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

  // To get firmware details: first get Feature Index corresponding to Firmware feature code
  uint8_t fwIndex = getFeatureIndexFromDevice(FeatureCode::FirmwareVersion);
  if (!fwIndex) return QByteArray();

  // Get the number of firmwares (Main HID++ application, BootLoader, or Hardware) now
  const auto fwCountReqMessage = make_QByteArray(HidppMsg{
    HIDPP::Bytes::LONG_MSG, HIDPP::Bytes::MSG_TO_SPOTLIGHT, fwIndex, getRandomFunctionCode(0x00),
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  });

  const auto res = ::write(m_fdHIDDevice, fwCountReqMessage.data(), fwCountReqMessage.size());
  if (res != fwCountReqMessage.size())
  {
    logDebug(hid) << tr("Failed to write firmware count request message to device.");
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
      HIDPP::Bytes::LONG_MSG, HIDPP::Bytes::MSG_TO_SPOTLIGHT, fwIndex, getRandomFunctionCode(0x10),
      i, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    });

    const auto res = ::write(m_fdHIDDevice, fwVerReqMessage.data(), fwVerReqMessage.length());
    if (res != fwCountReqMessage.size())
    {
      logDebug(hid) << tr("Failed to write firmware request message to device (%1).")
                             .arg(int(i));
      return 0x00;
    }
    const auto fwResponse = getResponseFromDevice(fwVerReqMessage.mid(1, 3));
    if (!fwResponse.length() || static_cast<uint8_t>(fwResponse.at(2)) == 0x8f) return QByteArray();
    const auto fwType = (fwResponse.at(4) & 0x0f);  // 0 for main HID++ application, 1 for BootLoader, 2 for Hardware, 3-15 others
    const auto fwVersion = fwResponse.mid(5, 7);

    if (fwType == 0)
    {
      logDebug(hid) << tr("Main application firmware Version:") << fwVersion.toHex();
      return fwVersion;
    }
  }
  return QByteArray();
}

// -------------------------------------------------------------------------------------------------
QString FeatureSet::getFirmwareVersion()
{
  if (!m_firmwareVersion.length()) return "";

  QString firmwareName = static_cast<QString>(m_firmwareVersion.mid(0, 3).data());
  int majorVesion = static_cast<uint8_t>(m_firmwareVersion.at(3));
  int minorVersion = static_cast<uint8_t>(m_firmwareVersion.at(4));
  uint16_t build = ((static_cast<uint16_t>(m_firmwareVersion.at(5)) << 8) | static_cast<uint16_t>(m_firmwareVersion.at(6)));

  return tr("%1 %2.%3 %4").arg(firmwareName).arg(majorVesion).arg(minorVersion).arg((build)?tr("(Build %1)").arg(build):"");
}

// -------------------------------------------------------------------------------------------------
void FeatureSet::populateFeatureTable()
{
  if (m_fdHIDDevice == -1) return;

  // Get the firmware version
  m_firmwareVersion = getFirmwareVersionFromDevice();
  if (!m_firmwareVersion.length()) return;

  logDebug(hid) << tr("Device Firmware Version (ByteArray):") << m_firmwareVersion.toHex();

  // if the firmware details match with cached file; then load the FeatureTable from file
  // else read the entire feature table from the device
  auto featureSetSetting = new QSettings(QCoreApplication::applicationName(), "Device_FeatureSet", this);
  QByteArray cacheFirmwareVersion = featureSetSetting->value("FirmwareVersion").toByteArray();
  if (cacheFirmwareVersion.length())
  {
    logDebug(hid) << tr("Cached Firmware Version (ByteArray):") << cacheFirmwareVersion.toHex();
  }

  if (m_firmwareVersion == cacheFirmwareVersion)
  {
    logDebug(hid) << tr("Loading HID++ Feature Set from cache.");
    featureSetSetting->beginGroup("FeatureSet");
    QStringList keys = featureSetSetting->childKeys();
      foreach (QString key, keys) {
           m_featureTable[static_cast<uint16_t>(key.toUInt())] = static_cast<uint8_t>(featureSetSetting->value(key).toUInt());
      }
    featureSetSetting->endGroup();
  }
  else
  {
    logDebug(hid) << tr("Loading HID++ Feature Set from Device.");

    // For reading feature table from device
    // first get featureIndex for FeatureCode::FeatureSet
    // then we can get the number of features supported by the device (except Root Feature)
    const uint8_t featureSetIndex = getFeatureIndexFromDevice(FeatureCode::FeatureSet);
    if (!featureSetIndex) return;
    const uint8_t featureCount = getFeatureCountFromDevice(featureSetIndex);
    if (!featureCount) return;

    // Root feature is supported by all HID++ 2.0 device and has a featureIndex of 0 always.
    m_featureTable.insert({static_cast<uint16_t>(FeatureCode::Root), 0x00});

    // Read Feature Code for other featureIndices from device.
    for (uint8_t featureIndex = 0x01; featureIndex <= featureCount; ++featureIndex)
    {
      const auto featureCodeReqMsg = make_QByteArray(HidppMsg{
        HIDPP::Bytes::LONG_MSG, HIDPP::Bytes::MSG_TO_SPOTLIGHT, featureSetIndex, getRandomFunctionCode(0x10), featureIndex,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
      });
      const auto res = ::write(m_fdHIDDevice, featureCodeReqMsg.data(), featureCodeReqMsg.size());
      if (res != featureCodeReqMsg.size()) {
        logDebug(hid) << tr("Failed to write feature code request message to device.");
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
      if (!(softwareHidden) && !(obsoleteFeature)) m_featureTable.insert({featureCode, featureIndex});
    }

    // Save the firmware version and feture set to cache file.
    featureSetSetting->setValue("FirmwareVersion", m_firmwareVersion);
    featureSetSetting->beginGroup("FeatureSet");
    for (auto f : m_featureTable) {
         featureSetSetting->setValue(tr("%1").arg(f.first), f.second);
     }
    featureSetSetting->endGroup();
  }
}

// -------------------------------------------------------------------------------------------------
bool FeatureSet::supportFeatureCode(FeatureCode fc) const
{
  const auto featurePair = m_featureTable.find(static_cast<uint16_t>(fc));
  return (featurePair != m_featureTable.end());
}

// -------------------------------------------------------------------------------------------------
uint8_t FeatureSet::getFeatureIndex(FeatureCode fc) const
{
  if (!supportFeatureCode(fc)) return 0x00;

  const auto featureInfo = m_featureTable.find(static_cast<uint16_t>(fc));
  return featureInfo->second;
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
