#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

#include "AppConfig.h"
#include "BoardProfiles.h"

namespace wt {

enum class RadioUiState : uint8_t {
  kScanning = 0,
  kIdle,
  kConnected,
  kTx,
  kRx,
  kPeerLost,
};

enum class UiScreenMode : uint8_t {
  kPeerPicker = 0,
  kComm,
  kComposeMessage,
  kHelp,
};

enum class VoiceVisualState : uint8_t {
  kIdle = 0,
  kTx,
  kRx,
  kNoLink,
};

enum class DeliveryState : uint8_t {
  kNone = 0,
  kPending,
  kDelivered,
  kFailed,
};

struct PeerInfo {
  bool occupied = false;
  bool compatible = false;
  char deviceName[config::kDeviceNameLength] = {};
  uint8_t mac[6] = {};
  int8_t rssi = -127;
  uint32_t lastSeenMs = 0;
  board::BoardVariant boardVariant = board::BoardVariant::kCardputer;
  uint8_t capabilityFlags = 0;
  int8_t batteryPercent = -1;
};

struct UiChatLine {
  bool visible = false;
  bool fromLocal = false;
  bool quickMessage = false;
  DeliveryState deliveryState = DeliveryState::kNone;
  char text[config::kTextMessageLength + 1] = {};
};

struct InputSnapshot {
  bool upPressed = false;
  bool downPressed = false;
  bool enterPressed = false;
  bool spacePressed = false;
  bool melodyHeld = false;
  bool qualityDownPressed = false;
  bool qualityUpPressed = false;
  bool soundTogglePressed = false;
  bool effectsDownPressed = false;
  bool effectsUpPressed = false;
  bool voiceDownPressed = false;
  bool voiceUpPressed = false;
  bool txGainDownPressed = false;
  bool txGainUpPressed = false;
  bool fnPressed = false;
  bool clearSelectionPressed = false;
  bool escapePressed = false;
  bool pttHeld = false;
  int quickDirectIndex = -1;
  uint8_t textCharCount = 0;
  char textChars[config::kInputBatchChars] = {};
};

struct RuntimeStats {
  uint32_t txPackets = 0;
  uint32_t rxPackets = 0;
  uint32_t droppedVoicePackets = 0;
  uint32_t malformedPackets = 0;
  uint32_t audioUnderruns = 0;
  uint32_t radioSendFailures = 0;
  uint32_t radioSendTimeouts = 0;
  uint32_t reliableRetries = 0;
  uint32_t reliableAckTimeouts = 0;
  uint32_t reliableDuplicates = 0;
  uint32_t concealedFrames = 0;
};

}  // namespace wt
