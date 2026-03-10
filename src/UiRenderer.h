#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

#include "AppConfig.h"
#include "AppTypes.h"

namespace wt {

struct UiPeerLine {
  bool valid = false;
  bool highlighted = false;
  bool selected = false;
  char deviceName[config::kDeviceNameLength] = {};
  char macText[18] = {};
  int8_t rssi = -127;
  uint32_t ageMs = 0;
};

struct UiRenderModel {
  UiScreenMode screenMode = UiScreenMode::kPeerPicker;
  VoiceVisualState voiceState = VoiceVisualState::kIdle;
  char headerTitle[24] = {};
  char peerName[config::kDeviceNameLength] = {};
  int8_t batteryPercent = -1;
  int8_t linkRssi = -127;
  bool showLink = false;
  bool systemSoundsEnabled = false;
  const char* qualityLabel = nullptr;
  uint16_t qualitySampleRateHz = 0;
  uint8_t effectsVolumeStep = 0;
  uint8_t voiceVolumeStep = 0;
  uint8_t voiceVolumePercent = 0;
  uint8_t txGainPercent = 100;
  uint8_t txGainLevel = 0;
  bool inlineNoticeActive = false;
  char inlineNotice[32] = {};
  char composeDraft[config::kTextMessageLength + 1] = {};
  uint8_t helpPageIndex = 0;
  uint8_t helpPageCount = 0;
  std::size_t activePeerCount = 0;
  std::size_t chatLineCount = 0;
  UiChatLine chatLines[config::kChatHistoryLength] = {};
  UiPeerLine peers[config::kMaxPeers] = {};
};

class UiRenderer {
 public:
  void begin();
  void render(const UiRenderModel& model, uint32_t nowMs);
};

}  // namespace wt
