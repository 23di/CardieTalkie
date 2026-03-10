#pragma once

#include <Arduino.h>
#include <cstdint>

#define BOARD_VARIANT_CARDPUTER 1
#define BOARD_VARIANT_CARDPUTER_ADV 2

#ifndef WALKIETALKIE_BOARD_VARIANT
#define WALKIETALKIE_BOARD_VARIANT BOARD_VARIANT_CARDPUTER
#endif

namespace wt::board {

enum class AudioBackendType : uint8_t {
  kM5UnifiedCodec = 0,
  kRawI2S = 1,
};

enum class BoardVariant : uint8_t {
  kCardputer = BOARD_VARIANT_CARDPUTER,
  kCardputerAdv = BOARD_VARIANT_CARDPUTER_ADV,
};

struct AudioPins {
  constexpr AudioPins(int bclkValue = -1, int wsValue = -1, int doutValue = -1,
                      int dinValue = -1, int mclkValue = -1)
      : bclk(bclkValue),
        ws(wsValue),
        dout(doutValue),
        din(dinValue),
        mclk(mclkValue) {}

  int bclk;
  int ws;
  int dout;
  int din;
  int mclk;
};

struct BoardProfile {
  BoardVariant variant;
  const char* variantName;
  const char* defaultNamePrefix;
  AudioBackendType audioBackend;
  AudioPins audioPins;
  uint8_t speakerVolume;
  uint8_t speakerMagnification;
};

inline const BoardProfile& activeProfile() {
  static constexpr BoardProfile kCardputerProfile = {
      BoardVariant::kCardputer,
      "Cardputer",
      "CARD",
      AudioBackendType::kM5UnifiedCodec,
      AudioPins{-1, -1, -1, -1, -1},
      255,
      24,
  };

  static constexpr BoardProfile kCardputerAdvProfile = {
      BoardVariant::kCardputerAdv,
      "Cardputer Adv",
      "C-ADV",
      AudioBackendType::kM5UnifiedCodec,
      AudioPins{-1, -1, -1, -1, -1},
      180,
      16,
  };

#if WALKIETALKIE_BOARD_VARIANT == BOARD_VARIANT_CARDPUTER_ADV
  return kCardputerAdvProfile;
#else
  return kCardputerProfile;
#endif
}

// Audio pin data stays here so a raw I2S backend can be added per board profile
// without changing the radio or UI layers.

}  // namespace wt::board
