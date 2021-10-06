// This file is part of Projecteur - https://github.com/jahnf/projecteur
// - See LICENSE.md and README.md
#pragma once

#include "device-defs.h"
#include "deviceinput.h"

#include <QString>

namespace KeyName
{
  const QString& lookup(const DeviceId& dId, const DeviceInputEvent& die);
} // end namespace KeyName
