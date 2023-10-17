// This file is part of Projecteur - https://github.com/jahnf/projecteur
// - See LICENSE.md and README.md

#include "device-command-helper.h"

#include "device-hidpp.h"
#include "spotlight.h"

// -------------------------------------------------------------------------------------------------
DeviceCommandHelper::DeviceCommandHelper(QObject* parent, Spotlight* spotlight)
  : QObject(parent), m_spotlight(spotlight)
{

}

// -------------------------------------------------------------------------------------------------
DeviceCommandHelper::~DeviceCommandHelper() = default;


// -------------------------------------------------------------------------------------------------
bool DeviceCommandHelper::sendVibrateCommand(uint8_t intensity, uint8_t length)
{
  if (m_spotlight.isNull()) {
    return false;
  }

  for ( auto const& dev : m_spotlight->connectedDevices()) {
    if (auto connection = m_spotlight->deviceConnection(dev.id)) {
      if (!connection->hasHidppSupport()) {
        continue;
      }

      for (auto const& subInfo : connection->subDevices()) {
        auto const& subConn = subInfo.second;
        if (!subConn || !subConn->hasFlags(DeviceFlag::Vibrate)) {
          continue;
        }

        if (auto hidppConn = std::dynamic_pointer_cast<SubHidppConnection>(subConn))
        {
          hidppConn->sendVibrateCommand(intensity, length,
          [](HidppConnectionInterface::MsgResult, HIDPP::Message&&) {
              // logDebug(hid) << tr("Vibrate command returned: %1 (%2)")
              //        .arg(toString(result)).arg(msg.hex());
          });
        }
      }
    }
  }

  return true;
}
