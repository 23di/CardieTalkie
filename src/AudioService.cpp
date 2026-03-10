#include "AudioService.h"

#include <cstring>

#include "MacAddress.h"
#include "QuickMessageClips.h"

namespace wt {

namespace {

struct MelodyNote {
  uint16_t frequencyHz;
  uint16_t durationMs;
};

struct MelodyDefinition {
  const char* name;
  const MelodyNote* notes;
  std::size_t noteCount;
};

constexpr MelodyNote kMelodyNyan[] = {
    {622, 100}, {659, 100}, {740, 100}, {494, 100}, {659, 100}, {622, 100},
    {659, 100}, {740, 100}, {494, 100}, {466, 100}, {740, 100}, {1109, 100},
    {1047, 100}, {1109, 100}, {1245, 100}, {1047, 100}, {622, 100}, {659, 100},
    {740, 100}, {494, 100}, {466, 100}, {698, 100}, {784, 100}, {587, 100},
    {622, 100}, {554, 100}, {523, 100}, {587, 100}, {587, 100}, {523, 100},
    {587, 100}, {698, 100}, {784, 100}, {587, 100}, {698, 100}, {523, 100},
    {587, 100}, {698, 100}, {784, 100}, {587, 100}, {698, 100}, {523, 100},
    {587, 100}, {523, 100}, {587, 100}, {698, 100}, {523, 100}, {587, 100},
    {523, 100}, {523, 100}, {698, 100}, {784, 100}, {587, 100}, {622, 100},
    {523, 100}, {587, 100}, {523, 100}, {523, 100}, {587, 100}, {587, 100},
    {523, 100},
};

constexpr MelodyNote kMelodyRickroll[] = {
    {587, 180}, {659, 180}, {784, 180}, {659, 180}, {988, 360}, {988, 180},
    {880, 180}, {784, 180}, {659, 180}, {587, 360}, {587, 180}, {659, 180},
    {784, 180}, {659, 180}, {880, 360}, {880, 180}, {784, 180}, {659, 180},
    {587, 180}, {523, 360},
};

constexpr MelodyNote kMelodyTetris[] = {
    {659, 240}, {494, 120}, {523, 120}, {587, 240}, {523, 120}, {494, 120},
    {440, 240}, {440, 120}, {523, 120}, {659, 240}, {587, 120}, {523, 120},
    {494, 360},
};

constexpr MelodyNote kMelodyOde[] = {
    {330, 180}, {330, 180}, {349, 180}, {392, 180}, {392, 180}, {349, 180},
    {330, 180}, {294, 180}, {262, 180}, {262, 180}, {294, 180}, {330, 180},
    {330, 240}, {294, 120}, {294, 300},
};

constexpr MelodyNote kMelodyElise[] = {
    {659, 120}, {622, 120}, {659, 120}, {622, 120}, {659, 120}, {494, 120},
    {587, 120}, {523, 120}, {440, 240}, {262, 120}, {330, 120}, {440, 120},
    {494, 240},
};

constexpr MelodyDefinition kMelodies[] = {
    {"NYAN CAT", kMelodyNyan, sizeof(kMelodyNyan) / sizeof(kMelodyNyan[0])},
    {"NEVER GONNA", kMelodyRickroll,
     sizeof(kMelodyRickroll) / sizeof(kMelodyRickroll[0])},
    {"TETRIS", kMelodyTetris,
     sizeof(kMelodyTetris) / sizeof(kMelodyTetris[0])},
    {"ODE", kMelodyOde, sizeof(kMelodyOde) / sizeof(kMelodyOde[0])},
    {"ELISE", kMelodyElise, sizeof(kMelodyElise) / sizeof(kMelodyElise[0])},
};

void synthesizeMelodyFrame(uint8_t* output, std::size_t frameBytes,
                           uint32_t sampleRateHz,
                           const MelodyDefinition& melody,
                           std::size_t& noteIndex,
                           uint32_t& samplesRemainingInNote,
                           uint16_t& currentFrequencyHz,
                           uint32_t& phaseAccumulator) {
  const uint8_t kLowSample = 88;
  const uint8_t kHighSample = 168;

  for (std::size_t i = 0; i < frameBytes; ++i) {
    while (samplesRemainingInNote == 0) {
      if (noteIndex >= melody.noteCount) {
        currentFrequencyHz = 0;
        output[i] = 128;
        for (std::size_t j = i + 1; j < frameBytes; ++j) {
          output[j] = 128;
        }
        return;
      }
      currentFrequencyHz = melody.notes[noteIndex].frequencyHz;
      samplesRemainingInNote = static_cast<uint32_t>(
          (static_cast<uint64_t>(melody.notes[noteIndex].durationMs) *
           sampleRateHz) /
          1000ULL);
      if (samplesRemainingInNote == 0) {
        samplesRemainingInNote = 1;
      }
      ++noteIndex;
      if (currentFrequencyHz == 0) {
        phaseAccumulator = 0;
      }
    }

    if (currentFrequencyHz == 0) {
      output[i] = 128;
    } else {
      const uint32_t phaseStep = static_cast<uint32_t>(
          (static_cast<uint64_t>(currentFrequencyHz) << 32) / sampleRateHz);
      phaseAccumulator += phaseStep;
      output[i] =
          (phaseAccumulator & 0x80000000UL) != 0 ? kHighSample : kLowSample;
    }
    --samplesRemainingInNote;
  }
}

}  // namespace

bool AudioService::begin(BoardSupport& board, RadioManager& radio,
                         const audio::AudioCodec& codec) {
  board_ = &board;
  radio_ = &radio;
  codec_ = &codec;

  BaseType_t txCreated = xTaskCreate(txTaskEntry, "wt_audio_tx",
                                     config::kTxAudioTaskStack, this, 3,
                                     &txTaskHandle_);
  BaseType_t rxCreated = xTaskCreate(rxTaskEntry, "wt_audio_rx",
                                     config::kRxAudioTaskStack, this, 3,
                                     &rxTaskHandle_);
  if (txCreated != pdPASS || rxCreated != pdPASS) {
    Serial.println("[audio] task creation failed");
    return false;
  }
  return true;
}

void AudioService::setQualityProfileIndex(uint8_t index) {
  if (index >= config::kAudioQualityProfiles.size()) {
    return;
  }
  portENTER_CRITICAL(&stateMux_);
  qualityProfileIndex_ = index;
  portEXIT_CRITICAL(&stateMux_);
}

uint8_t AudioService::qualityProfileIndex() const {
  uint8_t index = 0;
  portENTER_CRITICAL(&stateMux_);
  index = qualityProfileIndex_;
  portEXIT_CRITICAL(&stateMux_);
  return index;
}

const config::AudioQualityProfile& AudioService::qualityProfile() const {
  return config::kAudioQualityProfiles[qualityProfileIndex()];
}

void AudioService::setTxGainIndex(uint8_t index) {
  if (index >= config::kTxGainPercents.size()) {
    return;
  }
  portENTER_CRITICAL(&stateMux_);
  txGainIndex_ = index;
  portEXIT_CRITICAL(&stateMux_);
}

uint8_t AudioService::txGainIndex() const {
  uint8_t index = 0;
  portENTER_CRITICAL(&stateMux_);
  index = txGainIndex_;
  portEXIT_CRITICAL(&stateMux_);
  return index;
}

uint8_t AudioService::txGainPercent() const {
  return config::kTxGainPercents[txGainIndex()];
}

bool AudioService::requestMelody(uint8_t index) {
  bool accepted = false;
  portENTER_CRITICAL(&stateMux_);
  if (!txActive_ && !quickClipRequested_ && !melodyRequested_) {
    requestedMelodyIndex_ = index % melodyCount();
    melodyRequested_ = true;
    accepted = true;
  }
  portEXIT_CRITICAL(&stateMux_);
  return accepted;
}

bool AudioService::requestQuickMessageClip(uint8_t index) {
  const QuickMessageClip* clip = quickMessageClip(index);
  if (clip == nullptr || clip->data == nullptr || clip->length == 0 ||
      clip->sampleRateHz == 0 || clip->frameDurationMs == 0) {
    return false;
  }

  if (isReceiving(millis())) {
    return false;
  }

  bool accepted = false;
  portENTER_CRITICAL(&stateMux_);
  if (!txActive_ && !pttHeld_ && !melodyRequested_ && !quickClipRequested_) {
    requestedQuickClipIndex_ = index;
    quickClipRequested_ = true;
    accepted = true;
  }
  portEXIT_CRITICAL(&stateMux_);
  return accepted;
}

uint8_t AudioService::melodyCount() {
  return static_cast<uint8_t>(sizeof(kMelodies) / sizeof(kMelodies[0]));
}

const char* AudioService::melodyName(uint8_t index) {
  return kMelodies[index % melodyCount()].name;
}

void AudioService::setSelectedPeer(const uint8_t* macAddress, bool hasSelection) {
  bool changed = false;
  portENTER_CRITICAL(&stateMux_);
  if (hasSelectedPeer_ != hasSelection ||
      (hasSelection && !mac::equals(selectedPeerMac_, macAddress))) {
    changed = true;
  }
  hasSelectedPeer_ = hasSelection;
  if (hasSelection && macAddress != nullptr) {
    mac::copy(selectedPeerMac_, macAddress);
  } else {
    memset(selectedPeerMac_, 0, sizeof(selectedPeerMac_));
  }
  portEXIT_CRITICAL(&stateMux_);

  if (changed) {
    clearReceiveState();
  }
}

void AudioService::setPttHeld(bool held) {
  portENTER_CRITICAL(&stateMux_);
  pttHeld_ = held;
  portEXIT_CRITICAL(&stateMux_);

  if (held) {
    clearReceiveState();
    if (board_ != nullptr) {
      board_->stopPlayback();
    }
  }
}

void AudioService::clearVoiceSlotsLocked() {
  memset(rxSlots_, 0, sizeof(rxSlots_));
  rxSessionId_ = 0;
  expectedSequence_ = 0;
}

void AudioService::clearReceiveState() {
  portENTER_CRITICAL(&rxMux_);
  clearVoiceSlotsLocked();
  portEXIT_CRITICAL(&rxMux_);

  portENTER_CRITICAL(&stateMux_);
  remotePttActive_ = false;
  rxActiveUntilMs_ = 0;
  portEXIT_CRITICAL(&stateMux_);
}

void AudioService::insertVoiceFrame(uint16_t sessionId, uint16_t sequence,
                                    protocol::AudioCodec codec,
                                    uint16_t sampleRateHz,
                                    uint8_t frameDurationMs,
                                    uint8_t encodedFrameBytes,
                                    uint8_t pcmFrameBytes,
                                    int16_t predictor,
                                    uint8_t stepIndex,
                                    const uint8_t* data,
                                    uint32_t nowMs) {
  portENTER_CRITICAL(&rxMux_);

  if (rxSessionId_ != sessionId) {
    clearVoiceSlotsLocked();
    rxSessionId_ = sessionId;
    expectedSequence_ = sequence;
  }

  if (expectedSequence_ != 0 && sequence < expectedSequence_) {
    portEXIT_CRITICAL(&rxMux_);
    return;
  }

  for (auto& slot : rxSlots_) {
    if (slot.valid && slot.sessionId == sessionId && slot.sequence == sequence) {
      portEXIT_CRITICAL(&rxMux_);
      return;
    }
  }

  VoiceSlot* freeSlot = nullptr;
  for (auto& slot : rxSlots_) {
    if (!slot.valid) {
      freeSlot = &slot;
      break;
    }
  }

  if (freeSlot == nullptr) {
    ++droppedPackets_;
    portEXIT_CRITICAL(&rxMux_);
    return;
  }

  freeSlot->valid = true;
  freeSlot->codec = static_cast<uint8_t>(codec);
  freeSlot->sessionId = sessionId;
  freeSlot->sequence = sequence;
  freeSlot->sampleRateHz = sampleRateHz;
  freeSlot->frameDurationMs = frameDurationMs;
  freeSlot->frameBytes = encodedFrameBytes;
  freeSlot->pcmFrameBytes = pcmFrameBytes;
  freeSlot->predictor = predictor;
  freeSlot->stepIndex = stepIndex;
  freeSlot->arrivalMs = nowMs;
  memcpy(freeSlot->data, data, encodedFrameBytes);
  portEXIT_CRITICAL(&rxMux_);

  portENTER_CRITICAL(&stateMux_);
  remotePttActive_ = true;
  rxActiveUntilMs_ = nowMs + config::kRxActivityHoldMs;
  portEXIT_CRITICAL(&stateMux_);
}

void AudioService::handleVoicePacket(const protocol::PacketHeader& header,
                                     const uint8_t* packetBytes,
                                     uint32_t nowMs) {
  bool accept = false;
  portENTER_CRITICAL(&stateMux_);
  accept = hasSelectedPeer_ && !txActive_ &&
           mac::equals(selectedPeerMac_, header.sender);
  portEXIT_CRITICAL(&stateMux_);
  if (!accept) {
    return;
  }

  const uint8_t* payload = packetBytes + protocol::kHeaderSize;
  const protocol::AudioCodec codec =
      static_cast<protocol::AudioCodec>(payload[0]);
  const uint8_t sampleRateKhz = payload[1];
  const uint8_t frameDurationMs = payload[2];
  const uint8_t encodedBytes = payload[3];

  uint8_t pcmFrameBytes = encodedBytes;
  int16_t predictor = 0;
  uint8_t stepIndex = 0;
  if (codec == protocol::AudioCodec::kAdpcmIma) {
    const auto* voiceHeader =
        reinterpret_cast<const protocol::VoicePayloadHeader*>(payload);
    pcmFrameBytes = voiceHeader->pcmFrameBytes;
    predictor = voiceHeader->predictor;
    stepIndex = voiceHeader->stepIndex;
  }

  if (encodedBytes == 0 || encodedBytes > config::kMaxEncodedAudioFrameBytes ||
      pcmFrameBytes == 0 || pcmFrameBytes > config::kMaxAudioFrameBytes ||
      sampleRateKhz == 0 || frameDurationMs == 0) {
    return;
  }

  insertVoiceFrame(header.sessionId, header.sequence, codec,
                   static_cast<uint16_t>(sampleRateKhz) * 1000, frameDurationMs,
                   encodedBytes, pcmFrameBytes, predictor, stepIndex,
                   protocol::voiceData(packetBytes), nowMs);
}

void AudioService::handleControlPacket(const protocol::PacketHeader& header,
                                       const protocol::ControlPayload& payload,
                                       uint32_t nowMs) {
  bool accept = false;
  portENTER_CRITICAL(&stateMux_);
  accept = hasSelectedPeer_ && mac::equals(selectedPeerMac_, header.sender);
  portEXIT_CRITICAL(&stateMux_);
  if (!accept) {
    return;
  }

  const auto code = static_cast<protocol::ControlCode>(payload.controlCode);
  if (code == protocol::ControlCode::kPttStart) {
    portENTER_CRITICAL(&rxMux_);
    clearVoiceSlotsLocked();
    rxSessionId_ = header.sessionId;
    expectedSequence_ = 0;
    portEXIT_CRITICAL(&rxMux_);

    portENTER_CRITICAL(&stateMux_);
    remotePttActive_ = true;
    rxActiveUntilMs_ = nowMs + config::kRxActivityHoldMs;
    portEXIT_CRITICAL(&stateMux_);
  } else if (code == protocol::ControlCode::kPttStop) {
    portENTER_CRITICAL(&stateMux_);
    remotePttActive_ = false;
    rxActiveUntilMs_ = nowMs + config::kAudioFrameDurationMs;
    portEXIT_CRITICAL(&stateMux_);
  }
}

void AudioService::removeSlot(int slotIndex) {
  if (slotIndex < 0 || slotIndex >= static_cast<int>(config::kJitterBufferFrames)) {
    return;
  }
  rxSlots_[slotIndex].valid = false;
}

void AudioService::applyTxGain(uint8_t* frame, std::size_t frameBytes,
                               uint8_t gainPercent) {
  if (frame == nullptr || gainPercent == 100) {
    return;
  }

  for (std::size_t i = 0; i < frameBytes; ++i) {
    const int centered = static_cast<int>(frame[i]) - 128;
    int scaled = (centered * gainPercent) / 100;
    if (scaled < -128) {
      scaled = -128;
    } else if (scaled > 127) {
      scaled = 127;
    }
    frame[i] = static_cast<uint8_t>(scaled + 128);
  }
}

bool AudioService::popNextPlaybackFrame(PlaybackFrame& output, uint32_t nowMs) {
  for (;;) {
    portENTER_CRITICAL(&rxMux_);
    int bestIndex = -1;
    uint16_t bestSequence = UINT16_MAX;
    std::size_t validCount = 0;
    uint32_t earliestArrival = UINT32_MAX;

    for (int i = 0; i < static_cast<int>(config::kJitterBufferFrames); ++i) {
      if (!rxSlots_[i].valid) {
        continue;
      }
      ++validCount;
      if (rxSlots_[i].arrivalMs < earliestArrival) {
        earliestArrival = rxSlots_[i].arrivalMs;
      }
      if (rxSlots_[i].sequence < bestSequence) {
        bestSequence = rxSlots_[i].sequence;
        bestIndex = i;
      }
    }

    if (bestIndex < 0) {
      portEXIT_CRITICAL(&rxMux_);
      return false;
    }

    if (expectedSequence_ == 0) {
      expectedSequence_ = rxSlots_[bestIndex].sequence;
    }

    if (bestSequence < expectedSequence_) {
      removeSlot(bestIndex);
      portEXIT_CRITICAL(&rxMux_);
      continue;
    }

    const bool bufferedEnough = validCount >= config::kJitterStartFrames;
    const bool waitedLongEnough =
        earliestArrival != UINT32_MAX &&
        (nowMs - earliestArrival) >= config::kAudioFrameDurationMs;

    if (bestSequence == expectedSequence_) {
      output.sampleRateHz = rxSlots_[bestIndex].sampleRateHz;
      output.frameDurationMs = rxSlots_[bestIndex].frameDurationMs;
      output.frameBytes = rxSlots_[bestIndex].pcmFrameBytes;
      const protocol::AudioCodec codec =
          static_cast<protocol::AudioCodec>(rxSlots_[bestIndex].codec);
      bool decoded = false;
      if (codec == protocol::AudioCodec::kAdpcmIma) {
        decoded = codec_->decode(rxSlots_[bestIndex].data, rxSlots_[bestIndex].frameBytes,
                                 output.data, output.frameBytes,
                                 rxSlots_[bestIndex].predictor,
                                 rxSlots_[bestIndex].stepIndex);
      } else {
        decoded = pcmCodec_.decode(rxSlots_[bestIndex].data, rxSlots_[bestIndex].frameBytes,
                                   output.data, output.frameBytes, 0, 0);
      }
      if (!decoded) {
        codec_->makeSilence(output.data, output.frameBytes);
      }
      removeSlot(bestIndex);
      ++expectedSequence_;
      portEXIT_CRITICAL(&rxMux_);
      return true;
    }

    if (bufferedEnough || waitedLongEnough) {
      output.sampleRateHz =
          rxSlots_[bestIndex].sampleRateHz
              ? rxSlots_[bestIndex].sampleRateHz
              : qualityProfile().sampleRateHz;
      output.frameDurationMs =
          rxSlots_[bestIndex].frameDurationMs
              ? rxSlots_[bestIndex].frameDurationMs
              : qualityProfile().frameDurationMs;
      output.frameBytes =
          rxSlots_[bestIndex].pcmFrameBytes
              ? rxSlots_[bestIndex].pcmFrameBytes
              : qualityProfile().frameBytes;
      codec_->makeSilence(output.data, output.frameBytes);
      ++expectedSequence_;
      ++droppedPackets_;
      portEXIT_CRITICAL(&rxMux_);
      return true;
    }

    portEXIT_CRITICAL(&rxMux_);
    return false;
  }
}

bool AudioService::isTransmitting() const {
  bool value = false;
  portENTER_CRITICAL(&stateMux_);
  value = txActive_;
  portEXIT_CRITICAL(&stateMux_);
  return value;
}

bool AudioService::isReceiving(uint32_t nowMs) const {
  bool txActive = false;
  bool remotePtt = false;
  uint32_t activeUntil = 0;
  portENTER_CRITICAL(&stateMux_);
  txActive = txActive_;
  remotePtt = remotePttActive_;
  activeUntil = rxActiveUntilMs_;
  portEXIT_CRITICAL(&stateMux_);
  if (txActive) {
    return false;
  }

  bool anyFrames = false;
  portENTER_CRITICAL(&rxMux_);
  for (const auto& slot : rxSlots_) {
    if (slot.valid) {
      anyFrames = true;
      break;
    }
  }
  portEXIT_CRITICAL(&rxMux_);
  return remotePtt || anyFrames || (nowMs < activeUntil);
}

uint32_t AudioService::droppedPackets() const {
  return droppedPackets_;
}

uint32_t AudioService::underruns() const {
  return audioUnderruns_;
}

void AudioService::txTaskEntry(void* context) {
  auto* self = static_cast<AudioService*>(context);
  uint8_t frame[config::kMaxAudioFrameBytes] = {};
  uint8_t encoded[config::kMaxEncodedAudioFrameBytes] = {};
  uint8_t activeTarget[6] = {};
  uint16_t activeSessionId = 0;
  const QuickMessageClip* activeQuickClip = nullptr;
  std::size_t activeQuickClipOffset = 0;
  std::size_t melodyNoteIndex = 0;
  uint32_t melodySamplesRemaining = 0;
  uint16_t melodyFrequencyHz = 0;
  uint32_t melodyPhaseAccumulator = 0;
  uint8_t activeMelodyIndex = 0;
  enum class TxMode : uint8_t {
    kIdle = 0,
    kMic,
    kMelody,
    kQuickClip,
  };
  TxMode activeMode = TxMode::kIdle;
  bool wasActive = false;

  auto startSession = [&](const uint8_t* target) {
    self->board_->beep(config::kPttStartToneHz, config::kPttStartToneMs);
    activeSessionId = static_cast<uint16_t>(millis() & 0xFFFF);
    if (activeSessionId == 0) {
      activeSessionId = 1;
    }
    mac::copy(activeTarget, target);
    self->clearReceiveState();
    self->board_->stopPlayback();
    self->radio_->sendControl(activeTarget, protocol::ControlCode::kPttStart,
                              activeSessionId);
    portENTER_CRITICAL(&self->stateMux_);
    self->txActive_ = true;
    self->txSessionId_ = activeSessionId;
    portEXIT_CRITICAL(&self->stateMux_);
    wasActive = true;
  };

  auto finishSession = [&]() {
    self->radio_->sendControl(activeTarget, protocol::ControlCode::kPttStop,
                              activeSessionId);
    portENTER_CRITICAL(&self->stateMux_);
    self->txActive_ = false;
    portEXIT_CRITICAL(&self->stateMux_);
    self->board_->enterRxMode();
    self->board_->beep(config::kPttStopToneHz, config::kPttStopToneMs);
    wasActive = false;
    activeMode = TxMode::kIdle;
    activeQuickClip = nullptr;
    activeQuickClipOffset = 0;
  };

  for (;;) {
    bool pttHeld = false;
    bool hasPeer = false;
    uint8_t target[6] = {};
    uint8_t qualityIndex = config::kDefaultQualityProfileIndex;
    uint8_t txGainIndex = config::kDefaultTxGainIndex;
    bool melodyRequested = false;
    uint8_t melodyIndex = 0;
    bool quickClipRequested = false;
    uint8_t quickClipIndex = 0;

    portENTER_CRITICAL(&self->stateMux_);
    pttHeld = self->pttHeld_;
    hasPeer = self->hasSelectedPeer_;
    qualityIndex = self->qualityProfileIndex_;
    txGainIndex = self->txGainIndex_;
    melodyRequested = self->melodyRequested_;
    melodyIndex = self->requestedMelodyIndex_;
    quickClipRequested = self->quickClipRequested_;
    quickClipIndex = self->requestedQuickClipIndex_;
    if (hasPeer) {
      mac::copy(target, self->selectedPeerMac_);
    }
    portEXIT_CRITICAL(&self->stateMux_);

    if (activeMode == TxMode::kIdle && pttHeld && hasPeer) {
      startSession(target);
      activeMode = TxMode::kMic;
    }

    if (activeMode == TxMode::kIdle && !pttHeld && hasPeer && quickClipRequested) {
      const QuickMessageClip* clip = quickMessageClip(quickClipIndex);
      portENTER_CRITICAL(&self->stateMux_);
      self->quickClipRequested_ = false;
      portEXIT_CRITICAL(&self->stateMux_);
      if (clip != nullptr && clip->data != nullptr && clip->length > 0) {
        startSession(target);
        activeQuickClip = clip;
        activeQuickClipOffset = 0;
        activeMode = TxMode::kQuickClip;
        Serial.printf("[audio] sending quick voice %u\n",
                      static_cast<unsigned>(quickClipIndex + 1));
      }
    }

    if (activeMode == TxMode::kIdle && !pttHeld && hasPeer && melodyRequested) {
      startSession(target);
      portENTER_CRITICAL(&self->stateMux_);
      self->melodyRequested_ = false;
      portEXIT_CRITICAL(&self->stateMux_);
      activeMelodyIndex = melodyIndex % melodyCount();
      melodyNoteIndex = 0;
      melodySamplesRemaining = 0;
      melodyFrequencyHz = 0;
      melodyPhaseAccumulator = 0;
      activeMode = TxMode::kMelody;
      Serial.printf("[audio] sending melody %s\n", melodyName(activeMelodyIndex));
    }

    if (activeMode == TxMode::kIdle) {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    if (activeMode == TxMode::kMic && !(pttHeld && hasPeer)) {
      if (wasActive) {
        finishSession();
      }
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    uint8_t frameBytes = 0;
    uint32_t sampleRateHz = 0;
    uint8_t frameDurationMs = 0;

    if (activeMode == TxMode::kMic) {
      const auto& profile = config::kAudioQualityProfiles[qualityIndex];
      if (!self->board_->captureAudioFrame(frame, profile.frameBytes,
                                           profile.sampleRateHz)) {
        vTaskDelay(pdMS_TO_TICKS(2));
        continue;
      }
      frameBytes = profile.frameBytes;
      sampleRateHz = profile.sampleRateHz;
      frameDurationMs = profile.frameDurationMs;
    } else if (activeMode == TxMode::kMelody) {
      const auto& profile = config::kAudioQualityProfiles[qualityIndex];
      synthesizeMelodyFrame(frame, profile.frameBytes, profile.sampleRateHz,
                            kMelodies[activeMelodyIndex], melodyNoteIndex,
                            melodySamplesRemaining, melodyFrequencyHz,
                            melodyPhaseAccumulator);
      frameBytes = profile.frameBytes;
      sampleRateHz = profile.sampleRateHz;
      frameDurationMs = profile.frameDurationMs;
    } else {
      if (activeQuickClip == nullptr || activeQuickClipOffset >= activeQuickClip->length) {
        finishSession();
        vTaskDelay(pdMS_TO_TICKS(10));
        continue;
      }
      memset(frame, 128, sizeof(frame));
      const std::size_t remainingBytes =
          activeQuickClip->length - activeQuickClipOffset;
      frameBytes = static_cast<uint8_t>(
          remainingBytes > config::kMaxAudioFrameBytes
              ? config::kMaxAudioFrameBytes
              : remainingBytes);
      memcpy(frame, activeQuickClip->data + activeQuickClipOffset, frameBytes);
      activeQuickClipOffset += frameBytes;
      sampleRateHz = activeQuickClip->sampleRateHz;
      frameDurationMs = activeQuickClip->frameDurationMs;
    }

    applyTxGain(frame, frameBytes, config::kTxGainPercents[txGainIndex]);
    int16_t predictor = 0;
    uint8_t stepIndex = 0;
    const std::size_t encodedBytes =
        self->codec_->encode(frame, frameBytes, encoded, sizeof(encoded),
                             predictor, stepIndex);
    if (encodedBytes == 0) {
      vTaskDelay(pdMS_TO_TICKS(2));
      continue;
    }
    self->radio_->sendVoiceFrame(activeTarget, activeSessionId,
                                 self->codec_->codecType(), encoded,
                                 encodedBytes, sampleRateHz, frameDurationMs,
                                 frameBytes, predictor, stepIndex);

    if (activeMode == TxMode::kMelody) {
      if (melodyNoteIndex >= kMelodies[activeMelodyIndex].noteCount &&
          melodySamplesRemaining == 0) {
        finishSession();
        vTaskDelay(pdMS_TO_TICKS(10));
        continue;
      }
      vTaskDelay(pdMS_TO_TICKS(frameDurationMs));
      continue;
    }

    if (activeMode == TxMode::kQuickClip) {
      if (activeQuickClip == nullptr || activeQuickClipOffset >= activeQuickClip->length) {
        finishSession();
        vTaskDelay(pdMS_TO_TICKS(10));
        continue;
      }
      vTaskDelay(pdMS_TO_TICKS(frameDurationMs));
    }
  }
}

void AudioService::rxTaskEntry(void* context) {
  auto* self = static_cast<AudioService*>(context);
  PlaybackFrame frame;
  uint32_t lastUnderrunMs = 0;

  for (;;) {
    const uint32_t nowMs = millis();
    if (self->isTransmitting()) {
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }

    if (self->popNextPlaybackFrame(frame, nowMs)) {
      self->board_->playAudioFrame(frame.data, frame.frameBytes,
                                   frame.sampleRateHz);
      vTaskDelay(pdMS_TO_TICKS(frame.frameDurationMs));
      continue;
    }

    if (self->isReceiving(nowMs) &&
        (nowMs - lastUnderrunMs) >= config::kAudioFrameDurationMs) {
      ++self->audioUnderruns_;
      lastUnderrunMs = nowMs;
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

}  // namespace wt
