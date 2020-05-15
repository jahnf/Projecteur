// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

// -------------------------------------------------------------------------------------------------
#define EXPAND_( x ) x // MSVC workaround
#define GET_ENUM_MACRO(_1,_2,NAME,...) NAME
#define ENUM(...) EXPAND_(GET_ENUM_MACRO(__VA_ARGS__, ENUM2, ENUM1)(__VA_ARGS__))
// enum flags macro (cannot be used inside class declaration)
#define ENUM1(ENUMCLASS) \
  inline ENUMCLASS operator|(ENUMCLASS lhs, ENUMCLASS rhs) { \
    using T = std::underlying_type_t<ENUMCLASS>; \
    return static_cast<ENUMCLASS>(static_cast<T>(lhs) | static_cast<T>(rhs)); } \
  inline ENUMCLASS operator&(ENUMCLASS lhs, ENUMCLASS rhs) { \
    using T = std::underlying_type_t<ENUMCLASS>; \
    return static_cast<ENUMCLASS>(static_cast<T>(lhs) & static_cast<T>(rhs)); } \
  inline ENUMCLASS operator~(ENUMCLASS lhs) { \
    using T = std::underlying_type_t<ENUMCLASS>; \
    return static_cast<ENUMCLASS>(~static_cast<T>(lhs)); } \
  inline ENUMCLASS& operator |= (ENUMCLASS& lhs, ENUMCLASS rhs) {lhs = lhs | rhs; return lhs; } \
  inline ENUMCLASS& operator &= (ENUMCLASS& lhs, ENUMCLASS rhs) {lhs = lhs & rhs; return lhs; }

// enum flags macro (cannot be used inside class declaration)
#define ENUM2(ENUMCLASS, PLURALNAME) \
  ENUM1(ENUMCLASS); \
  using PLURALNAME = ENUMCLASS;
