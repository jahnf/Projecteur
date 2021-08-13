// This file is part of Projecteur - https://github.com/jahnf/projecteur
// - See LICENSE.md and README.md

#pragma once

#include <cstdint>

// Bus on which device is connected
enum class BusType : uint8_t { Unknown, Usb, Bluetooth };

enum class ConnectionType : uint8_t { Event, Hidraw };

enum class ConnectionMode : uint8_t { ReadOnly, WriteOnly, ReadWrite };
