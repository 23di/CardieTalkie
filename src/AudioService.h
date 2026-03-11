#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

#include "AppConfig.h"
#include "AudioCodec.h"
#include "BoardSupport.h"
#include "Protocol.h"
#include "RadioManager.h"

namespace wt {

class AudioService {
 public:
  bool begin(BoardSupport& board, RadioManager& radio,
             const audio::AudioCodec& codec);

  void setSelectedPeer(const uint8_t* macAddress, bool hasSelection);
  void setPttHeld(bool held);
  void setQualityProfileIndex(uint8_t index);
  uint8_t qualityProfileIndex() const;
  const config::AudioQualityProfile& qualityProfile() const;
  void setTxGainIndex(uint8_t index);
  uint8_t txGainIndex() const;
  uint8_t txGainPercent() const;
  bool requestMelody(uint8_t index);
  bool requestQuickMessageClip(uint8_t index);
  static uint8_t melodyCount();
  static const char* melodyName(uint8_t index);

  void handleVoicePacket(const protocol::PacketHeader& header,
                         const uint8_t* packetBytes, uint32_t nowMs);
  void handleControlPacket(const protocol::PacketHeader& header,
                           const protocol::ControlPayload& payload,
                           uint32_t nowMs);
  void clearReceiveState();

  bool isTransmitting() const;
  bool isReceiving(uint32_t nowMs) const;
  uint32_t droppedPackets() const;
  uint32_t underruns() const;
  uint32_t concealedFrames() const;

 private:
  struct VoiceSlot {
    bool valid = false;
    uint8_t codec = 0;
    uint16_t sessionId = 0;
    uint16_t sequence = 0;
    uint16_t sampleRateHz = 0;
    uint8_t frameDurationMs = 0;
    uint8_t frameBytes = 0;
    uint8_t pcmFrameBytes = 0;
    int16_t predictor = 0;
    uint8_t stepIndex = 0;
    uint32_t arrivalMs = 0;
    uint8_t data[config::kMaxEncodedAudioFrameBytes] = {};
  };

  struct PlaybackFrame {
    uint8_t data[config::kMaxAudioFrameBytes] = {};
    uint16_t sampleRateHz = 0;
    uint8_t frameDurationMs = 0;
    uint8_t frameBytes = 0;
  };

  static void txTaskEntry(void* context);
  static void rxTaskEntry(void* context);

  bool popNextPlaybackFrame(PlaybackFrame& output, uint32_t nowMs);
  bool captureMicFrame(uint8_t* frame,
                       const config::AudioQualityProfile& profile);
  void insertVoiceFrame(uint16_t sessionId, uint16_t sequence,
                        protocol::AudioCodec codec, uint16_t sampleRateHz,
                        uint8_t frameDurationMs, uint8_t encodedFrameBytes,
                        uint8_t pcmFrameBytes, int16_t predictor,
                        uint8_t stepIndex, const uint8_t* data, uint32_t nowMs);
  void clearVoiceSlotsLocked();
  void resetConcealmentState();
  void rememberPlaybackFrame(const PlaybackFrame& frame);
  bool makeConcealedPlaybackFrame(PlaybackFrame& output, uint16_t sampleRateHz,
                                  uint8_t frameDurationMs, uint8_t frameBytes);
  void removeSlot(int slotIndex);
  static void applyTxGain(uint8_t* frame, std::size_t frameBytes,
                          uint8_t gainPercent);
  static void applyPcmU8Gain(uint8_t* frame, std::size_t frameBytes,
                             uint8_t gainPercent);

  BoardSupport* board_ = nullptr;
  RadioManager* radio_ = nullptr;
  const audio::AudioCodec* codec_ = nullptr;
  audio::PcmU8Codec pcmCodec_{};

  TaskHandle_t txTaskHandle_ = nullptr;
  TaskHandle_t rxTaskHandle_ = nullptr;

  mutable portMUX_TYPE stateMux_ = portMUX_INITIALIZER_UNLOCKED;
  mutable portMUX_TYPE rxMux_ = portMUX_INITIALIZER_UNLOCKED;

  uint8_t selectedPeerMac_[6] = {};
  bool hasSelectedPeer_ = false;
  bool pttHeld_ = false;
  bool melodyRequested_ = false;
  bool quickClipRequested_ = false;
  bool txActive_ = false;
  bool remotePttActive_ = false;
  uint16_t txSessionId_ = 0;
  uint16_t rxSessionId_ = 0;
  uint16_t expectedSequence_ = 0;
  uint8_t qualityProfileIndex_ = config::kDefaultQualityProfileIndex;
  uint8_t txGainIndex_ = config::kDefaultTxGainIndex;
  uint8_t requestedMelodyIndex_ = 0;
  uint8_t requestedQuickClipIndex_ = 0;
  uint32_t rxActiveUntilMs_ = 0;
  uint32_t droppedPackets_ = 0;
  uint32_t audioUnderruns_ = 0;
  uint32_t concealedFrames_ = 0;
  uint8_t concealmentRun_ = 0;
  bool lastPlaybackFrameValid_ = false;
  PlaybackFrame lastPlaybackFrame_{};
  VoiceSlot rxSlots_[config::kJitterBufferFrames] = {};
};

}  // namespace wt
