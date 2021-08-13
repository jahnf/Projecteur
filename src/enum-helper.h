// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#pragma once

#include <type_traits>

/// @brief Cast enum type to underlying integral type.
template <typename T>
constexpr auto to_integral(T e) {
  return static_cast<std::underlying_type_t<T>>(e);
}

/// @brief Cast integral type to a given enum type.
template <typename E, typename T>
constexpr auto to_enum(T v) {
  return static_cast<E>(v);
}

// -------------------------------------------------------------------------------------------------

#define EXPAND_( x ) x // MSVC workaround
#define GET_ENUM_MACRO(_1,_2,NAME,...) NAME
#define ENUM(...) EXPAND_(GET_ENUM_MACRO(__VA_ARGS__, ENUM2, ENUM1)(__VA_ARGS__))
// enum flags macro (cannot be used inside class declaration)
#define ENUM1(ENUMCLASS) \
  inline ENUMCLASS operator|(ENUMCLASS lhs, ENUMCLASS rhs) { \
    return to_enum<ENUMCLASS>(to_integral(lhs) | to_integral(rhs)); } \
  inline ENUMCLASS operator&(ENUMCLASS lhs, ENUMCLASS rhs) { \
    return to_enum<ENUMCLASS>(to_integral(lhs) & to_integral(rhs)); } \
  inline ENUMCLASS operator~(ENUMCLASS lhs) { \
    return to_enum<ENUMCLASS>(~to_integral(lhs)); } \
  inline ENUMCLASS& operator |= (ENUMCLASS& lhs, ENUMCLASS rhs) {lhs = lhs | rhs; return lhs; } \
  inline ENUMCLASS& operator &= (ENUMCLASS& lhs, ENUMCLASS rhs) {lhs = lhs & rhs; return lhs; } \
  inline bool operator!(ENUMCLASS e) { return e == to_enum<ENUMCLASS>(0); }

// enum flags macro (cannot be used inside class declaration)
#define ENUM2(ENUMCLASS, PLURALNAME) \
  ENUM1(ENUMCLASS); \
  using PLURALNAME = ENUMCLASS;
