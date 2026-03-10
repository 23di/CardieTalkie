#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

#include "AppConfig.h"
#include "Protocol.h"

namespace wt::audio {

constexpr int kImaAdpcmIndexTable[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8,
};

constexpr int kImaAdpcmStepTable[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37,
    41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173,
    190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658,
    724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484,
    7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500,
    20350, 22385, 24623, 27086, 29794, 32767,
};

class AudioCodec {
 public:
  virtual ~AudioCodec() = default;
  virtual protocol::AudioCodec codecType() const = 0;
  virtual std::size_t maxEncodedFrameBytes(std::size_t pcmFrameBytes) const = 0;
  virtual std::size_t encode(const uint8_t* input, std::size_t pcmFrameBytes,
                             uint8_t* output, std::size_t outputCapacity,
                             int16_t& predictor, uint8_t& stepIndex) const = 0;
  virtual bool decode(const uint8_t* input, std::size_t encodedBytes,
                      uint8_t* output, std::size_t pcmFrameBytes,
                      int16_t predictor, uint8_t stepIndex) const = 0;
  virtual void makeSilence(uint8_t* output, std::size_t pcmFrameBytes) const = 0;
};

class PcmU8Codec final : public AudioCodec {
 public:
  protocol::AudioCodec codecType() const override {
    return protocol::AudioCodec::kPcmU8;
  }

  std::size_t maxEncodedFrameBytes(std::size_t pcmFrameBytes) const override {
    return pcmFrameBytes;
  }

  std::size_t encode(const uint8_t* input, std::size_t pcmFrameBytes,
                     uint8_t* output, std::size_t outputCapacity,
                     int16_t& predictor, uint8_t& stepIndex) const override {
    if (outputCapacity < pcmFrameBytes) {
      return 0;
    }
    predictor = 0;
    stepIndex = 0;
    memcpy(output, input, pcmFrameBytes);
    return pcmFrameBytes;
  }

  bool decode(const uint8_t* input, std::size_t encodedBytes, uint8_t* output,
              std::size_t pcmFrameBytes, int16_t predictor,
              uint8_t stepIndex) const override {
    (void)predictor;
    (void)stepIndex;
    if (encodedBytes != pcmFrameBytes) {
      return false;
    }
    memcpy(output, input, pcmFrameBytes);
    return true;
  }

  void makeSilence(uint8_t* output, std::size_t pcmFrameBytes) const override {
    memset(output, 0x80, pcmFrameBytes);
  }
};

class ImaAdpcmCodec final : public AudioCodec {
 public:
  protocol::AudioCodec codecType() const override {
    return protocol::AudioCodec::kAdpcmIma;
  }

  std::size_t maxEncodedFrameBytes(std::size_t pcmFrameBytes) const override {
    return (pcmFrameBytes + 1U) / 2U;
  }

  std::size_t encode(const uint8_t* input, std::size_t pcmFrameBytes,
                     uint8_t* output, std::size_t outputCapacity,
                     int16_t& predictor, uint8_t& stepIndex) const override {
    if (input == nullptr || output == nullptr) {
      return 0;
    }
    const std::size_t encodedBytes = maxEncodedFrameBytes(pcmFrameBytes);
    if (outputCapacity < encodedBytes || pcmFrameBytes == 0) {
      return 0;
    }

    predictor = static_cast<int16_t>(static_cast<int>(input[0]) - 128);
    stepIndex = 0;

    std::size_t outIndex = 0;
    uint8_t packedByte = 0;
    bool lowNibble = true;
    for (std::size_t i = 0; i < pcmFrameBytes; ++i) {
      const uint8_t nibble = encodeNibble(static_cast<int16_t>(
          static_cast<int>(input[i]) - 128), predictor, stepIndex);
      if (lowNibble) {
        packedByte = nibble & 0x0F;
        lowNibble = false;
      } else {
        packedByte |= static_cast<uint8_t>(nibble << 4);
        output[outIndex++] = packedByte;
        packedByte = 0;
        lowNibble = true;
      }
    }
    if (!lowNibble && outIndex < outputCapacity) {
      output[outIndex++] = packedByte;
    }
    return outIndex;
  }

  bool decode(const uint8_t* input, std::size_t encodedBytes, uint8_t* output,
              std::size_t pcmFrameBytes, int16_t predictor,
              uint8_t stepIndex) const override {
    if (input == nullptr || output == nullptr) {
      return false;
    }
    if (encodedBytes < maxEncodedFrameBytes(pcmFrameBytes)) {
      return false;
    }

    std::size_t outIndex = 0;
    for (std::size_t i = 0; i < encodedBytes && outIndex < pcmFrameBytes; ++i) {
      const uint8_t byte = input[i];
      output[outIndex++] =
          decodeNibble(static_cast<uint8_t>(byte & 0x0F), predictor, stepIndex);
      if (outIndex < pcmFrameBytes) {
        output[outIndex++] = decodeNibble(static_cast<uint8_t>((byte >> 4) & 0x0F),
                                          predictor, stepIndex);
      }
    }
    return true;
  }

 void makeSilence(uint8_t* output, std::size_t pcmFrameBytes) const override {
    memset(output, 0x80, pcmFrameBytes);
  }

 private:
  static uint8_t encodeNibble(int16_t sample, int16_t& predictor,
                              uint8_t& stepIndex) {
    int step = kImaAdpcmStepTable[stepIndex];
    int diff = static_cast<int>(sample) - predictor;
    uint8_t code = 0;
    if (diff < 0) {
      code = 8;
      diff = -diff;
    }

    int delta = step >> 3;
    if (diff >= step) {
      code |= 4;
      diff -= step;
      delta += step;
    }
    step >>= 1;
    if (diff >= step) {
      code |= 2;
      diff -= step;
      delta += step;
    }
    step >>= 1;
    if (diff >= step) {
      code |= 1;
      delta += step;
    }

    predictor = updatePredictor(code, predictor, delta);
    stepIndex = updateStepIndex(code, stepIndex);
    return code;
  }

  static uint8_t decodeNibble(uint8_t code, int16_t& predictor,
                              uint8_t& stepIndex) {
    const int step = kImaAdpcmStepTable[stepIndex];
    int delta = step >> 3;
    if ((code & 4U) != 0U) {
      delta += step;
    }
    if ((code & 2U) != 0U) {
      delta += step >> 1;
    }
    if ((code & 1U) != 0U) {
      delta += step >> 2;
    }

    predictor = updatePredictor(code, predictor, delta);
    stepIndex = updateStepIndex(code, stepIndex);
    int sample = predictor + 128;
    if (sample < 0) {
      sample = 0;
    } else if (sample > 255) {
      sample = 255;
    }
    return static_cast<uint8_t>(sample);
  }

  static int16_t updatePredictor(uint8_t code, int16_t predictor, int delta) {
    if ((code & 8U) != 0U) {
      predictor = static_cast<int16_t>(predictor - delta);
    } else {
      predictor = static_cast<int16_t>(predictor + delta);
    }
    if (predictor < -128) {
      predictor = -128;
    } else if (predictor > 127) {
      predictor = 127;
    }
    return predictor;
  }

  static uint8_t updateStepIndex(uint8_t code, uint8_t stepIndex) {
    int index = static_cast<int>(stepIndex) + kImaAdpcmIndexTable[code & 0x0F];
    if (index < 0) {
      index = 0;
    } else if (index > 88) {
      index = 88;
    }
    return static_cast<uint8_t>(index);
  }
};

}  // namespace wt::audio
