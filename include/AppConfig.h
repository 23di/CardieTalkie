#pragma once

#include <Arduino.h>
#include <array>
#include <cstddef>
#include <cstdint>

namespace wt::config {

constexpr uint8_t kLegacyProtocolVersion = 1;
constexpr uint8_t kProtocolVersion = 2;
constexpr uint8_t kProtocolMagic = 0x57;
constexpr uint8_t kEspNowChannel = 1;
constexpr std::size_t kEspNowKeyLength = 16;
constexpr std::array<uint8_t, kEspNowKeyLength> kEspNowPmk = {{
    'C', 'a', 'r', 'd', 'i', 'e', 'T', 'a',
    'l', 'k', 'i', 'e', 'P', 'M', 'K', '1',
}};
constexpr std::array<uint8_t, kEspNowKeyLength> kEspNowLmk = {{
    'C', 'a', 'r', 'd', 'i', 'e', 'T', 'a',
    'l', 'k', 'i', 'e', 'L', 'M', 'K', '1',
}};

constexpr uint32_t kBeaconIntervalMs = 3000;
constexpr uint32_t kPeerTimeoutMs = 12000;
constexpr uint32_t kScreenRefreshMs = 125;
constexpr uint32_t kRxActivityHoldMs = 450;
constexpr uint32_t kMelodyHoldMs = 1000;
constexpr uint32_t kUiNoticeMs = 1800;
constexpr uint32_t kQuickMessageCooldownMs = 1000;

constexpr std::size_t kMaxPeers = 8;
constexpr std::size_t kMaxQuickMessages = 9;
constexpr std::size_t kDeviceNameLength = 16;
constexpr std::size_t kQuickMessageTextLength = 24;
constexpr std::size_t kTextMessageLength = 120;
constexpr std::size_t kChatHistoryLength = 10;
constexpr std::size_t kUiChatPreviewLines = 4;
constexpr std::size_t kUiComposeChatLines = 6;
constexpr std::size_t kInputBatchChars = 12;

constexpr uint32_t kSampleRateHz = 12000;
constexpr uint8_t kAudioBitsPerSample = 8;
constexpr uint8_t kAudioChannels = 1;
constexpr uint16_t kAudioFrameDurationMs = 10;
constexpr std::size_t kAudioFrameSamples =
    (kSampleRateHz * kAudioFrameDurationMs) / 1000;
constexpr std::size_t kMaxAudioFrameBytes =
    kAudioFrameSamples * kAudioChannels * (kAudioBitsPerSample / 8);
constexpr std::size_t kAudioFrameBytes = kMaxAudioFrameBytes;
constexpr std::size_t kMaxEncodedAudioFrameBytes = kMaxAudioFrameBytes;

struct AudioQualityProfile {
  const char* label;
  uint16_t sampleRateHz;
  uint8_t frameDurationMs;
  uint8_t frameBytes;
};

constexpr uint8_t kDefaultQualityProfileIndex = 2;
constexpr std::array<AudioQualityProfile, 3> kAudioQualityProfiles = {{
    {"ROBUST", 8000, 10,
     static_cast<uint8_t>((8000 * 10) / 1000)},
    {"BAL", 10000, 10,
     static_cast<uint8_t>((10000 * 10) / 1000)},
    {"CLEAR", 12000, 10,
     static_cast<uint8_t>((12000 * 10) / 1000)},
}};

constexpr std::size_t kJitterBufferFrames = 8;
constexpr std::size_t kJitterStartFrames = 3;
constexpr std::size_t kJitterGapWaitFrames = 2;
constexpr std::size_t kMaxConcealmentFrames = 3;
constexpr std::array<uint8_t, kMaxConcealmentFrames> kConcealmentDecayPercents = {{
    100,
    70,
    45,
}};
constexpr std::size_t kRxQueueDepth = 24;
constexpr std::size_t kTxQueueDepth = 24;
constexpr std::size_t kPacketPayloadMax = 176;
constexpr uint16_t kTxAudioTaskStack = 4096;
constexpr uint16_t kRxAudioTaskStack = 4096;
constexpr uint16_t kRadioTxTaskStack = 4096;
constexpr uint32_t kEspNowSendTimeoutMs = 20;
constexpr uint8_t kReliableMessageMaxAttempts = 3;
constexpr uint32_t kReliableAckTimeoutMs = 150;
constexpr uint32_t kReliableMessageDedupMs = 5000;
constexpr std::size_t kReliableMessageDedupEntries = 8;

constexpr uint8_t kMaxVolumeStep = 5;
constexpr uint8_t kDefaultEffectsVolumeStep = 1;
constexpr uint8_t kDefaultVoiceVolumeStep = 4;
constexpr bool kDefaultSystemSoundsEnabled = true;

constexpr uint8_t kSpeakerVolume = 180;
constexpr bool kEnableQuickMessageBeep = true;
constexpr bool kBroadcastQuickMessagesEnabled = false;
constexpr uint16_t kPttStartToneHz = 1600;
constexpr uint16_t kPttStartToneMs = 28;
constexpr uint16_t kPttStopToneHz = 1100;
constexpr uint16_t kPttStopToneMs = 28;
constexpr uint16_t kPttStopToneTailHz = 700;
constexpr uint16_t kPttStopToneTailMs = 36;
constexpr uint16_t kPttToneGapMs = 14;
constexpr uint8_t kLocalPttCueGainPercent = 25;
constexpr uint8_t kRemotePttCueGainPercent = 25;
constexpr uint8_t kMelodyTxGainPercent = 25;

constexpr uint8_t kDefaultTxGainIndex = 2;
constexpr std::array<uint8_t, 5> kTxGainPercents = {{
    100,
    125,
    150,
    180,
    220,
}};

constexpr char kDeviceNamePrefix[] = "WT";

static_assert(kAudioQualityProfiles[kDefaultQualityProfileIndex].frameBytes <=
                  kMaxAudioFrameBytes,
              "Default quality profile must fit raw audio frame limits.");
static_assert(kMaxEncodedAudioFrameBytes <= kPacketPayloadMax,
              "Encoded audio frame must fit inside the packet payload.");

constexpr std::array<const char*, kMaxQuickMessages> kQuickMessages = {
    "OK",
    "Call me",
    "Busy",
    "Where are you?",
    "Come here",
    "Test",
    "Need help",
    "On my way",
    "Repeat",
};

constexpr std::array<const char*, 21> kHelpLines = {{
    "CONTROLS",
    "SPACE      TALK",
    "ENTER      TYPE",
    "1         OK",
    "2         CALL ME",
    "3         BUSY",
    "4         WHERE ARE YOU?",
    "5         COME HERE",
    "6         TEST",
    "7         NEED HELP",
    "8         ON MY WAY",
    "9         REPEAT",
    "A / D      QUALITY",
    "R / T      VOICE VOLUME",
    "F / G      MIC GAIN",
    "Z / C      FX VOL",
    "X          TOGGLE FX",
    "^ / v      SCROLL",
    "",
    "Created by Yura",
    "https://github.com/23di/CardieTalkie",
}};

static_assert(kQuickMessages.size() == kMaxQuickMessages,
              "Quick message table must match kMaxQuickMessages.");
static_assert(kDefaultTxGainIndex < kTxGainPercents.size(),
              "Default TX gain index must be valid.");
static_assert(kEspNowPmk.size() == kEspNowKeyLength,
              "ESP-NOW PMK must be 16 bytes.");
static_assert(kEspNowLmk.size() == kEspNowKeyLength,
              "ESP-NOW LMK must be 16 bytes.");

}  // namespace wt::config
