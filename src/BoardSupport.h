#pragma once

#include <Arduino.h>
#include <vector>

#include "AppTypes.h"
#include "BoardProfiles.h"

namespace wt {

class BoardSupport {
 public:
  bool begin(const board::BoardProfile& profile);
  void update();
  InputSnapshot readInput();
  int8_t batteryPercent() const;
  void beep(uint16_t frequencyHz, uint16_t durationMs);
  void setSystemSoundsEnabled(bool enabled);
  bool systemSoundsEnabled() const;
  void setEffectsVolumeStep(uint8_t step);
  uint8_t effectsVolumeStep() const;
  void setVoiceVolumeStep(uint8_t step);
  uint8_t voiceVolumeStep() const;

  bool enterTxMode();
  bool enterRxMode();
  void stopPlayback();
  bool captureAudioFrame(uint8_t* buffer, std::size_t frameBytes,
                         uint32_t sampleRateHz);
  bool playAudioFrame(const uint8_t* buffer, std::size_t frameBytes,
                      uint32_t sampleRateHz);

 private:
  void configureSpeaker();
  void applyVoiceVolume();
  uint8_t rawVolumeForStep(uint8_t step) const;

  struct RawInputState {
    bool up = false;
    bool down = false;
    bool enter = false;
    bool space = false;
    bool melody = false;
    bool qualityDown = false;
    bool qualityUp = false;
    bool soundToggle = false;
    bool effectsDown = false;
    bool effectsUp = false;
    bool voiceDown = false;
    bool voiceUp = false;
    bool txGainDown = false;
    bool txGainUp = false;
    bool fn = false;
    bool clearSelection = false;
    bool escape = false;
    bool ptt = false;
    int quickDirectIndex = -1;
  };

  RawInputState currentRawInput() const;
  static bool containsKey(const std::vector<char>& keys, char needle);

  board::BoardProfile profile_{};
  RawInputState previousInput_{};
  enum class AudioMode : uint8_t {
    kIdle = 0,
    kTx,
    kRx,
  } audioMode_ = AudioMode::kIdle;
  bool systemSoundsEnabled_ = config::kDefaultSystemSoundsEnabled;
  uint8_t effectsVolumeStep_ = config::kDefaultEffectsVolumeStep;
  uint8_t voiceVolumeStep_ = config::kDefaultVoiceVolumeStep;
};

}  // namespace wt
