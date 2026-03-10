#pragma once

#include <cstdint>

#include "AppConfig.h"

namespace wt {

struct QuickMessageClip {
  constexpr QuickMessageClip(const uint8_t* dataValue = nullptr,
                             uint32_t lengthValue = 0,
                             uint16_t sampleRateHzValue = 0,
                             uint8_t frameDurationMsValue = 0)
      : data(dataValue),
        length(lengthValue),
        sampleRateHz(sampleRateHzValue),
        frameDurationMs(frameDurationMsValue) {}

  const uint8_t* data = nullptr;
  uint32_t length = 0;
  uint16_t sampleRateHz = 0;
  uint8_t frameDurationMs = 0;
};

const QuickMessageClip* quickMessageClip(uint8_t index);
uint8_t quickMessageClipCount();

}  // namespace wt
