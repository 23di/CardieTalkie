#include "AppController.h"

#include <cstring>

#include "MacAddress.h"

namespace wt {

namespace {

constexpr uint8_t kHelpVisibleLines = 8;

const char* peerNameOrFallback(const PeerInfo* peer, const uint8_t* macAddress,
                               char* fallback, std::size_t fallbackLength) {
  if (peer != nullptr && peer->deviceName[0] != '\0') {
    return peer->deviceName;
  }
  mac::toString(macAddress, fallback, fallbackLength);
  return fallback;
}

}  // namespace

bool AppController::begin() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("[app] Cardie Talkie firmware booting");

  if (!board_.begin(profile_)) {
    Serial.println("[app] board init failed");
    return false;
  }
  if (!radio_.begin(profile_)) {
    Serial.println("[app] radio init failed");
    return false;
  }

  composeDeviceName();
  radio_.setDeviceName(deviceName_);
  peers_.begin(radio_.localMac());

  if (!audio_.begin(board_, radio_, codec_)) {
    Serial.println("[app] audio init failed");
    return false;
  }
  audio_.setQualityProfileIndex(config::kDefaultQualityProfileIndex);
  audio_.setTxGainIndex(config::kDefaultTxGainIndex);

  memset(conversations_, 0, sizeof(conversations_));
  memset(composeDraft_, 0, sizeof(composeDraft_));
  clearInlineNotice();

  ui_.begin();
  board_.beep(1400, 40);
  lastBeaconMs_ = millis() - config::kBeaconIntervalMs;
  lastRenderMs_ = 0;
  return true;
}

void AppController::composeDeviceName() {
  snprintf(deviceName_, sizeof(deviceName_), "%s-%04X",
           profile_.defaultNamePrefix, mac::shortId(radio_.localMac()));
}

void AppController::setInlineNotice(const char* message, uint32_t nowMs) {
  if (message == nullptr) {
    clearInlineNotice();
    return;
  }
  strncpy(inlineNotice_, message, sizeof(inlineNotice_) - 1);
  inlineNotice_[sizeof(inlineNotice_) - 1] = '\0';
  inlineNoticeUntilMs_ = nowMs + config::kUiNoticeMs;
}

void AppController::clearInlineNotice() {
  memset(inlineNotice_, 0, sizeof(inlineNotice_));
  inlineNoticeUntilMs_ = 0;
}

const PeerInfo* AppController::selectedPeer(uint32_t nowMs) const {
  return peers_.selectedPeer(nowMs);
}

bool AppController::selectedPeerSupportsText(uint32_t nowMs) const {
  const auto* peer = selectedPeer(nowMs);
  return peer != nullptr &&
         (peer->capabilityFlags & protocol::kCapabilityTextMessages) != 0;
}

AppController::Conversation* AppController::findConversation(
    const uint8_t* macAddress) {
  if (macAddress == nullptr) {
    return nullptr;
  }
  for (auto& conversation : conversations_) {
    if (conversation.occupied && mac::equals(conversation.mac, macAddress)) {
      return &conversation;
    }
  }
  return nullptr;
}

const AppController::Conversation* AppController::findConversation(
    const uint8_t* macAddress) const {
  if (macAddress == nullptr) {
    return nullptr;
  }
  for (const auto& conversation : conversations_) {
    if (conversation.occupied && mac::equals(conversation.mac, macAddress)) {
      return &conversation;
    }
  }
  return nullptr;
}

AppController::Conversation* AppController::ensureConversation(
    const uint8_t* macAddress, const char* peerName, uint32_t nowMs) {
  if (macAddress == nullptr) {
    return nullptr;
  }

  if (auto* existing = findConversation(macAddress)) {
    if (peerName != nullptr && peerName[0] != '\0') {
      strncpy(existing->peerName, peerName, sizeof(existing->peerName) - 1);
      existing->peerName[sizeof(existing->peerName) - 1] = '\0';
    }
    existing->updatedAtMs = nowMs;
    return existing;
  }

  Conversation* candidate = nullptr;
  for (auto& conversation : conversations_) {
    if (!conversation.occupied) {
      candidate = &conversation;
      break;
    }
  }
  if (candidate == nullptr) {
    candidate = &conversations_[0];
    for (auto& conversation : conversations_) {
      if (conversation.updatedAtMs < candidate->updatedAtMs) {
        candidate = &conversation;
      }
    }
  }

  memset(candidate, 0, sizeof(*candidate));
  candidate->occupied = true;
  mac::copy(candidate->mac, macAddress);
  if (peerName != nullptr && peerName[0] != '\0') {
    strncpy(candidate->peerName, peerName, sizeof(candidate->peerName) - 1);
    candidate->peerName[sizeof(candidate->peerName) - 1] = '\0';
  }
  candidate->updatedAtMs = nowMs;
  return candidate;
}

void AppController::appendConversationEntry(const uint8_t* macAddress,
                                            const char* peerName,
                                            const char* text, bool fromLocal,
                                            bool quickMessage,
                                            uint8_t quickIndex,
                                            uint32_t nowMs) {
  auto* conversation = ensureConversation(macAddress, peerName, nowMs);
  if (conversation == nullptr || text == nullptr) {
    return;
  }

  auto& entry = conversation->entries[conversation->nextWriteIndex];
  memset(&entry, 0, sizeof(entry));
  entry.occupied = true;
  entry.fromLocal = fromLocal;
  entry.quickMessage = quickMessage;
  entry.quickIndex = quickIndex;
  strncpy(entry.text, text, sizeof(entry.text) - 1);
  entry.text[sizeof(entry.text) - 1] = '\0';

  conversation->nextWriteIndex =
      (conversation->nextWriteIndex + 1) % config::kChatHistoryLength;
  if (conversation->entryCount < config::kChatHistoryLength) {
    ++conversation->entryCount;
  }
  conversation->updatedAtMs = nowMs;
  conversation->unread = !fromLocal;
}

void AppController::processIncoming(uint32_t nowMs) {
  ReceivedFrame frame;
  while (radio_.pollReceived(frame)) {
    if (!protocol::isValidPacketBuffer(frame.bytes, frame.length)) {
      ++stats_.malformedPackets;
      continue;
    }

    const auto& header = *protocol::header(frame.bytes);
    if (!protocol::isTargetForLocal(header, radio_.localMac())) {
      continue;
    }

    peers_.touchPeer(frame.sourceMac, frame.rssi, frame.receivedAtMs);
    const auto packetType = static_cast<protocol::PacketType>(header.type);
    switch (packetType) {
      case protocol::PacketType::kPresence:
        handlePresencePacket(frame, header);
        break;
      case protocol::PacketType::kQuickMessage:
        handleQuickMessagePacket(frame, header);
        break;
      case protocol::PacketType::kTextMessage:
        handleTextMessagePacket(frame, header);
        break;
      case protocol::PacketType::kControl:
        handleControlPacket(frame, header);
        break;
      case protocol::PacketType::kVoice:
        handleVoicePacket(frame, header);
        break;
      case protocol::PacketType::kPresenceAck:
        break;
    }
  }

  if (inlineNoticeUntilMs_ != 0 && nowMs >= inlineNoticeUntilMs_) {
    clearInlineNotice();
  }
}

void AppController::handlePresencePacket(const ReceivedFrame& frame,
                                         const protocol::PacketHeader& header) {
  const auto* payload = protocol::presencePayload(frame.bytes);
  const auto boardVariant = static_cast<board::BoardVariant>(payload->boardVariant);
  const bool wasKnown = peers_.findPeer(header.sender, frame.receivedAtMs) != nullptr;

  peers_.updatePeer(header.sender, payload->deviceName, boardVariant,
                    payload->capabilityFlags, payload->batteryPercent,
                    frame.rssi, true, frame.receivedAtMs);
  ensureConversation(header.sender, payload->deviceName, frame.receivedAtMs);

  if (!wasKnown) {
    char macBuffer[18];
    Serial.printf("[peer] discovered %s (%s) RSSI %d\n", payload->deviceName,
                  mac::toString(header.sender, macBuffer, sizeof(macBuffer)),
                  frame.rssi);
  }
}

void AppController::handleQuickMessagePacket(const ReceivedFrame& frame,
                                             const protocol::PacketHeader& header) {
  const auto* payload = protocol::quickMessagePayload(frame.bytes);
  const auto* peer = peers_.findPeer(header.sender, frame.receivedAtMs);
  char fallback[18];
  const char* senderName =
      peerNameOrFallback(peer, header.sender, fallback, sizeof(fallback));

  appendConversationEntry(header.sender, senderName, payload->text, false, true,
                          payload->messageId, frame.receivedAtMs);
  if ((uiMode_ == UiScreenMode::kComm || uiMode_ == UiScreenMode::kComposeMessage) &&
      peers_.hasSelection() && mac::equals(peers_.selectedMac(), header.sender)) {
    if (auto* conversation = findConversation(header.sender)) {
      conversation->unread = false;
    }
  }

  Serial.printf("[msg] %s -> %s\n", senderName, payload->text);
  if (config::kEnableQuickMessageBeep) {
    board_.beep(1800, 50);
  }
}

void AppController::handleTextMessagePacket(const ReceivedFrame& frame,
                                            const protocol::PacketHeader& header) {
  const auto* payload = protocol::textMessagePayload(frame.bytes);
  char text[config::kTextMessageLength + 1] = {};
  const std::size_t copyLength =
      (payload->textLength <= config::kTextMessageLength)
          ? payload->textLength
          : config::kTextMessageLength;
  memcpy(text, payload->text, copyLength);

  const auto* peer = peers_.findPeer(header.sender, frame.receivedAtMs);
  char fallback[18];
  const char* senderName =
      peerNameOrFallback(peer, header.sender, fallback, sizeof(fallback));
  appendConversationEntry(header.sender, senderName, text, false, false, 0,
                          frame.receivedAtMs);
  if ((uiMode_ == UiScreenMode::kComm || uiMode_ == UiScreenMode::kComposeMessage) &&
      peers_.hasSelection() && mac::equals(peers_.selectedMac(), header.sender)) {
    if (auto* conversation = findConversation(header.sender)) {
      conversation->unread = false;
    }
  }

  Serial.printf("[text] %s -> %s\n", senderName, text);
  board_.beep(1500, 16);
}

void AppController::handleControlPacket(const ReceivedFrame& frame,
                                        const protocol::PacketHeader& header) {
  const auto* payload = protocol::controlPayload(frame.bytes);
  audio_.handleControlPacket(header, *payload, frame.receivedAtMs);
}

void AppController::handleVoicePacket(const ReceivedFrame& frame,
                                      const protocol::PacketHeader& header) {
  audio_.handleVoicePacket(header, frame.bytes, frame.receivedAtMs);
}

void AppController::syncSelectedPeer(uint32_t nowMs) {
  const auto* peer = selectedPeer(nowMs);
  radio_.setSelectedPeer(peer != nullptr ? peer->mac : nullptr, peer != nullptr);
  audio_.setSelectedPeer(peer != nullptr ? peer->mac : nullptr, peer != nullptr);

  if (peer != nullptr &&
      (uiMode_ == UiScreenMode::kComm || uiMode_ == UiScreenMode::kComposeMessage)) {
    if (auto* conversation = findConversation(peer->mac)) {
      conversation->unread = false;
    }
  }
}

bool AppController::sendQuickMessage(std::size_t quickIndex, uint32_t nowMs) {
  if (quickIndex >= config::kQuickMessages.size()) {
    return false;
  }

  const auto* peer = selectedPeer(nowMs);
  if (peer == nullptr || peers_.selectedPeerLost(nowMs)) {
    setInlineNotice("NO LINK", nowMs);
    return false;
  }

  const bool sent = radio_.sendQuickMessage(peer->mac,
                                            static_cast<uint8_t>(quickIndex),
                                            config::kQuickMessages[quickIndex],
                                            false);
  if (!sent) {
    setInlineNotice("SEND FAILED", nowMs);
    return false;
  }

  appendConversationEntry(peer->mac, peer->deviceName,
                          config::kQuickMessages[quickIndex], true, true,
                          static_cast<uint8_t>(quickIndex), nowMs);
  lastQuickMessageMs_ = nowMs;
  clearInlineNotice();
  Serial.printf("[msg] sent '%s'\n", config::kQuickMessages[quickIndex]);
  board_.beep(1200, 25);
  if (board_.systemSoundsEnabled()) {
    if (!audio_.requestQuickMessageClip(static_cast<uint8_t>(quickIndex))) {
      Serial.printf("[audio] quick voice skipped for Q%u\n",
                    static_cast<unsigned>(quickIndex + 1));
    }
  }
  return true;
}

bool AppController::sendTextMessage(uint32_t nowMs) {
  if (composeLength_ == 0) {
    uiMode_ = UiScreenMode::kComm;
    return false;
  }

  const auto* peer = selectedPeer(nowMs);
  if (peer == nullptr || peers_.selectedPeerLost(nowMs)) {
    setInlineNotice("NO LINK", nowMs);
    return false;
  }
  if (!selectedPeerSupportsText(nowMs)) {
    setInlineNotice("TEXT UNSUPPORTED", nowMs);
    return false;
  }

  const bool sent = radio_.sendTextMessage(peer->mac, composeDraft_);
  if (!sent) {
    setInlineNotice("SEND FAILED", nowMs);
    return false;
  }

  appendConversationEntry(peer->mac, peer->deviceName, composeDraft_, true, false,
                          0, nowMs);
  memset(composeDraft_, 0, sizeof(composeDraft_));
  composeLength_ = 0;
  clearInlineNotice();
  board_.beep(1100, 20);
  uiMode_ = UiScreenMode::kComm;
  return true;
}

void AppController::handleAudioControls(const InputSnapshot& input) {
  if (input.qualityDownPressed) {
    const uint8_t current = audio_.qualityProfileIndex();
    if (current > 0) {
      audio_.setQualityProfileIndex(current - 1);
      board_.beep(900, 20);
    }
  }
  if (input.qualityUpPressed) {
    const uint8_t current = audio_.qualityProfileIndex();
    if (current + 1 < config::kAudioQualityProfiles.size()) {
      audio_.setQualityProfileIndex(current + 1);
      board_.beep(1200, 20);
    }
  }
  if (input.soundTogglePressed) {
    const bool enabled = !board_.systemSoundsEnabled();
    board_.setSystemSoundsEnabled(enabled);
    Serial.printf("[sound] system sounds %s\n", enabled ? "on" : "off");
    if (enabled) {
      board_.beep(1500, 20);
    }
  }
  if (input.effectsDownPressed) {
    const uint8_t current = board_.effectsVolumeStep();
    if (current > 0) {
      board_.setEffectsVolumeStep(current - 1);
      board_.beep(1000, 18);
    }
  }
  if (input.effectsUpPressed) {
    const uint8_t current = board_.effectsVolumeStep();
    if (current < config::kMaxVolumeStep) {
      board_.setEffectsVolumeStep(current + 1);
      board_.beep(1300, 18);
    }
  }
  if (input.voiceDownPressed) {
    const uint8_t current = board_.voiceVolumeStep();
    if (current > 0) {
      board_.setVoiceVolumeStep(current - 1);
    }
  }
  if (input.voiceUpPressed) {
    const uint8_t current = board_.voiceVolumeStep();
    if (current < config::kMaxVolumeStep) {
      board_.setVoiceVolumeStep(current + 1);
    }
  }
  if (input.txGainDownPressed) {
    const uint8_t current = audio_.txGainIndex();
    if (current > 0) {
      audio_.setTxGainIndex(current - 1);
    }
  }
  if (input.txGainUpPressed) {
    const uint8_t current = audio_.txGainIndex();
    if (current + 1 < config::kTxGainPercents.size()) {
      audio_.setTxGainIndex(current + 1);
    }
  }
}

void AppController::updateMelodyHold(bool melodyHeld, uint32_t nowMs) {
  if (melodyHeld) {
    if (!melodyHoldActive_) {
      melodyHoldActive_ = true;
      melodyHoldTriggered_ = false;
      melodyHoldStartedMs_ = nowMs;
    } else if (!melodyHoldTriggered_ &&
               (nowMs - melodyHoldStartedMs_) >= config::kMelodyHoldMs) {
      const bool canTriggerMelody =
          selectedPeer(nowMs) != nullptr && !peers_.selectedPeerLost(nowMs);
      if (canTriggerMelody && audio_.requestMelody(nextMelodyIndex_)) {
        melodyHoldTriggered_ = true;
        nextMelodyIndex_ = (nextMelodyIndex_ + 1) % AudioService::melodyCount();
      }
    }
  } else {
    melodyHoldActive_ = false;
    melodyHoldTriggered_ = false;
  }
}

void AppController::openHelp() {
  if (uiMode_ == UiScreenMode::kHelp) {
    return;
  }
  helpReturnMode_ = uiMode_;
  helpPageIndex_ = 0;
  uiMode_ = UiScreenMode::kHelp;
}

void AppController::closeHelp() {
  if (uiMode_ == UiScreenMode::kHelp) {
    uiMode_ = helpReturnMode_;
  }
}

void AppController::handlePeerPickerInput(const InputSnapshot& input,
                                          uint32_t nowMs) {
  updateMelodyHold(false, nowMs);
  if (input.fnPressed) {
    openHelp();
    return;
  }
  if (input.upPressed) {
    peers_.moveHighlight(-1, nowMs);
  }
  if (input.downPressed) {
    peers_.moveHighlight(1, nowMs);
  }
  if (input.enterPressed) {
    peers_.selectHighlighted(nowMs);
    const auto* peer = selectedPeer(nowMs);
    if (peer != nullptr) {
      ensureConversation(peer->mac, peer->deviceName, nowMs);
      uiMode_ = UiScreenMode::kComm;
      clearInlineNotice();
      board_.beep(1000, 30);
      Serial.printf("[peer] selected %s\n", peer->deviceName);
    }
  }
}

void AppController::handleCommInput(const InputSnapshot& input,
                                    uint32_t nowMs) {
  if (input.fnPressed) {
    updateMelodyHold(false, nowMs);
    openHelp();
    return;
  }
  if (input.escapePressed) {
    updateMelodyHold(false, nowMs);
    uiMode_ = UiScreenMode::kPeerPicker;
    clearInlineNotice();
    return;
  }

  if (input.enterPressed) {
    updateMelodyHold(false, nowMs);
    if (audio_.isTransmitting()) {
      setInlineNotice("WAIT", nowMs);
      return;
    }
    if (selectedPeer(nowMs) == nullptr || peers_.selectedPeerLost(nowMs)) {
      setInlineNotice("NO LINK", nowMs);
      return;
    }
    if (!selectedPeerSupportsText(nowMs)) {
      setInlineNotice("TEXT UNSUPPORTED", nowMs);
      return;
    }
    memset(composeDraft_, 0, sizeof(composeDraft_));
    composeLength_ = 0;
    clearInlineNotice();
    uiMode_ = UiScreenMode::kComposeMessage;
    return;
  }

  if (input.quickDirectIndex >= 0) {
    if (audio_.isTransmitting()) {
      setInlineNotice("WAIT", nowMs);
      return;
    }
    if (lastQuickMessageMs_ != 0 &&
        (nowMs - lastQuickMessageMs_) < config::kQuickMessageCooldownMs) {
      setInlineNotice("WAIT 1S", nowMs);
      return;
    }
    sendQuickMessage(static_cast<std::size_t>(input.quickDirectIndex), nowMs);
  }

  updateMelodyHold(input.melodyHeld, nowMs);
  const bool canTransmit =
      selectedPeer(nowMs) != nullptr && !peers_.selectedPeerLost(nowMs);
  audio_.setPttHeld(input.pttHeld && canTransmit);
}

void AppController::handleComposeInput(const InputSnapshot& input,
                                       uint32_t nowMs) {
  updateMelodyHold(false, nowMs);
  if (input.escapePressed) {
    memset(composeDraft_, 0, sizeof(composeDraft_));
    composeLength_ = 0;
    uiMode_ = UiScreenMode::kComm;
    return;
  }

  if (input.clearSelectionPressed && composeLength_ > 0) {
    composeDraft_[--composeLength_] = '\0';
  }
  if (input.spacePressed && composeLength_ < config::kTextMessageLength) {
    composeDraft_[composeLength_++] = ' ';
    composeDraft_[composeLength_] = '\0';
  }
  for (uint8_t i = 0; i < input.textCharCount; ++i) {
    if (composeLength_ >= config::kTextMessageLength) {
      break;
    }
    composeDraft_[composeLength_++] = input.textChars[i];
    composeDraft_[composeLength_] = '\0';
  }

  if (input.enterPressed) {
    sendTextMessage(nowMs);
  }
}

void AppController::handleHelpInput(const InputSnapshot& input) {
  if (input.fnPressed || input.escapePressed) {
    closeHelp();
    return;
  }
  if (input.upPressed && helpPageIndex_ > 0) {
    --helpPageIndex_;
  }
  const uint8_t maxScroll =
      (config::kHelpLines.size() > kHelpVisibleLines)
          ? static_cast<uint8_t>(config::kHelpLines.size() - kHelpVisibleLines)
          : 0;
  if (input.downPressed && helpPageIndex_ < maxScroll) {
    ++helpPageIndex_;
  }
}

void AppController::handleInput(uint32_t nowMs) {
  const InputSnapshot input = board_.readInput();

  audio_.setPttHeld(false);
  if (uiMode_ == UiScreenMode::kPeerPicker || uiMode_ == UiScreenMode::kComm) {
    handleAudioControls(input);
  }

  switch (uiMode_) {
    case UiScreenMode::kPeerPicker:
      handlePeerPickerInput(input, nowMs);
      break;
    case UiScreenMode::kComm:
      handleCommInput(input, nowMs);
      break;
    case UiScreenMode::kComposeMessage:
      handleComposeInput(input, nowMs);
      break;
    case UiScreenMode::kHelp:
      handleHelpInput(input);
      break;
  }
}

VoiceVisualState AppController::voiceVisualState(uint32_t nowMs) const {
  if (uiMode_ == UiScreenMode::kPeerPicker || uiMode_ == UiScreenMode::kHelp) {
    return VoiceVisualState::kIdle;
  }
  if (peers_.selectedPeerLost(nowMs)) {
    return VoiceVisualState::kNoLink;
  }
  if (audio_.isTransmitting()) {
    return VoiceVisualState::kTx;
  }
  if (audio_.isReceiving(nowMs)) {
    return VoiceVisualState::kRx;
  }
  return VoiceVisualState::kIdle;
}

UiRenderModel AppController::buildUiModel(uint32_t nowMs) const {
  UiRenderModel model;
  model.screenMode = uiMode_;
  model.voiceState = voiceVisualState(nowMs);
  model.batteryPercent = board_.batteryPercent();
  model.systemSoundsEnabled = board_.systemSoundsEnabled();
  model.qualityLabel = audio_.qualityProfile().label;
  model.qualitySampleRateHz = audio_.qualityProfile().sampleRateHz;
  model.effectsVolumeStep = board_.effectsVolumeStep();
  model.voiceVolumeStep = board_.voiceVolumeStep();
  model.voiceVolumePercent =
      static_cast<uint8_t>((static_cast<uint16_t>(board_.voiceVolumeStep()) * 100U) /
                           config::kMaxVolumeStep);
  model.txGainPercent = audio_.txGainPercent();
  model.txGainLevel = static_cast<uint8_t>(audio_.txGainIndex() + 1);
  model.helpPageIndex = helpPageIndex_;
  model.helpPageCount = static_cast<uint8_t>(config::kHelpLines.size());
  if (inlineNoticeUntilMs_ != 0 && nowMs < inlineNoticeUntilMs_) {
    model.inlineNoticeActive = true;
    strncpy(model.inlineNotice, inlineNotice_, sizeof(model.inlineNotice) - 1);
  }
  strncpy(model.composeDraft, composeDraft_, sizeof(model.composeDraft) - 1);

  char peerFallback[18] = {};
  const auto* peer = selectedPeer(nowMs);
  if (peer != nullptr) {
    strncpy(model.peerName, peer->deviceName, sizeof(model.peerName) - 1);
    strncpy(model.headerTitle, peer->deviceName, sizeof(model.headerTitle) - 1);
    model.linkRssi = peer->rssi;
    model.showLink = true;
  } else if (peers_.hasSelection()) {
    const auto* conversation = findConversation(peers_.selectedMac());
    if (conversation != nullptr && conversation->peerName[0] != '\0') {
      strncpy(model.peerName, conversation->peerName, sizeof(model.peerName) - 1);
      strncpy(model.headerTitle, conversation->peerName,
              sizeof(model.headerTitle) - 1);
    } else {
      mac::toString(peers_.selectedMac(), peerFallback, sizeof(peerFallback));
      strncpy(model.peerName, peerFallback, sizeof(model.peerName) - 1);
      strncpy(model.headerTitle, peerFallback, sizeof(model.headerTitle) - 1);
    }
  }

  if (uiMode_ == UiScreenMode::kPeerPicker) {
    strncpy(model.headerTitle, "PEERS", sizeof(model.headerTitle) - 1);
  } else if (uiMode_ == UiScreenMode::kHelp) {
    strncpy(model.headerTitle, "HELP", sizeof(model.headerTitle) - 1);
  }

  model.activePeerCount = peers_.activeCount(nowMs);
  for (std::size_t i = 0; i < model.activePeerCount && i < config::kMaxPeers; ++i) {
    const auto* visiblePeer = peers_.visiblePeerAt(i, nowMs);
    if (visiblePeer == nullptr) {
      continue;
    }
    auto& line = model.peers[i];
    line.valid = true;
    line.highlighted = (i == peers_.highlightedIndex());
    line.selected =
        (peers_.selectedMac() != nullptr) &&
        mac::equals(visiblePeer->mac, peers_.selectedMac());
    strncpy(line.deviceName, visiblePeer->deviceName,
            sizeof(line.deviceName) - 1);
    line.rssi = visiblePeer->rssi;
    line.ageMs = nowMs - visiblePeer->lastSeenMs;
  }

  const Conversation* conversation = nullptr;
  if (peer != nullptr) {
    conversation = findConversation(peer->mac);
  } else if (peers_.hasSelection()) {
    conversation = findConversation(peers_.selectedMac());
  }

  if (conversation != nullptr && conversation->entryCount > 0) {
    const std::size_t logicalCount = conversation->entryCount;
    const std::size_t oldestIndex =
        (conversation->nextWriteIndex + config::kChatHistoryLength -
         conversation->entryCount) %
        config::kChatHistoryLength;

    for (std::size_t logical = 0;
         logical < logicalCount &&
         model.chatLineCount < config::kChatHistoryLength;
         ++logical) {
      const std::size_t actualIndex =
          (oldestIndex + logical) % config::kChatHistoryLength;
      const auto& entry = conversation->entries[actualIndex];
      if (!entry.occupied) {
        continue;
      }
      auto& line = model.chatLines[model.chatLineCount++];
      line.visible = true;
      line.fromLocal = entry.fromLocal;
      line.quickMessage = entry.quickMessage;
      strncpy(line.text, entry.text, sizeof(line.text) - 1);
    }
  }

  return model;
}

void AppController::update() {
  board_.update();
  const uint32_t nowMs = millis();

  processIncoming(nowMs);
  peers_.expirePeers(nowMs);
  syncSelectedPeer(nowMs);
  handleInput(nowMs);
  syncSelectedPeer(nowMs);

  if (nowMs - lastBeaconMs_ >= config::kBeaconIntervalMs) {
    radio_.sendPresence(board_.batteryPercent());
    lastBeaconMs_ = nowMs;
  }

  if (nowMs - lastRenderMs_ >= config::kScreenRefreshMs) {
    const UiRenderModel model = buildUiModel(nowMs);
    ui_.render(model, nowMs);
    lastRenderMs_ = nowMs;
  }
}

}  // namespace wt
