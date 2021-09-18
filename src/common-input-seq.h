// This file is part of Projecteur - https://github.com/jahnf/projecteur
// - See LICENSE.md and README.md
#pragma once

#include "deviceinput.h"
#include <linux/input.h>

#include <array>

namespace CommonKeyEventSeqInfo {
  static const std::array<SpecialKeys::SpecialKeyEventSeqInfo, 5> commonInputSeq = {{
    {"Double Click", {{{EV_KEY, BTN_LEFT, 1}}, {{EV_KEY, BTN_LEFT, 0}}, {{EV_KEY, BTN_LEFT, 1}}, {{EV_KEY, BTN_LEFT, 0}}}},
    {"Click", {{{EV_KEY, BTN_LEFT, 1}}, {{EV_KEY, BTN_LEFT, 0}}}},
    {"Right Click", {{{EV_KEY, BTN_RIGHT, 1}}, {{EV_KEY, BTN_RIGHT, 0}}}},
    {"Next", {{{EV_KEY, KEY_RIGHT, 1}}, {{EV_KEY, KEY_RIGHT, 0}}}},
    {"Back", {{{EV_KEY, KEY_LEFT, 1}}, {{EV_KEY, KEY_LEFT, 0}}}},
  }};
}
