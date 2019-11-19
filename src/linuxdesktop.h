// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include <cstdint>

class LinuxDesktop {
public:
  enum class Type : uint8_t { KDE, Gnome, Other };

  LinuxDesktop();

  bool isWayland() const { return m_wayland; };
  Type type() const { return m_type; };

private:
  bool m_wayland = false;
  Type m_type = Type::Other;
};