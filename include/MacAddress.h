#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

namespace wt::mac {

constexpr std::size_t kMacLength = 6;

inline void copy(uint8_t* dst, const uint8_t* src) {
  memcpy(dst, src, kMacLength);
}

inline bool equals(const uint8_t* lhs, const uint8_t* rhs) {
  return memcmp(lhs, rhs, kMacLength) == 0;
}

inline bool isBroadcast(const uint8_t* address) {
  for (std::size_t i = 0; i < kMacLength; ++i) {
    if (address[i] != 0xFF) {
      return false;
    }
  }
  return true;
}

inline void setBroadcast(uint8_t* address) {
  memset(address, 0xFF, kMacLength);
}

inline const char* toString(const uint8_t* address, char* buffer, std::size_t length) {
  if (length < 18) {
    if (length > 0) {
      buffer[0] = '\0';
    }
    return buffer;
  }
  snprintf(buffer, length, "%02X:%02X:%02X:%02X:%02X:%02X", address[0],
           address[1], address[2], address[3], address[4], address[5]);
  return buffer;
}

inline uint16_t shortId(const uint8_t* address) {
  return static_cast<uint16_t>((address[4] << 8) | address[5]);
}

}  // namespace wt::mac
