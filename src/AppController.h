#pragma once

#include <Arduino.h>

#include "AppTypes.h"
#include "AudioCodec.h"
#include "AudioService.h"
#include "BoardSupport.h"
#include "PeerManager.h"
#include "RadioManager.h"
#include "UiRenderer.h"

namespace wt {

class AppController {
 public:
  bool begin();
  void update();

 private:
  struct ChatEntry {
    bool occupied = false;
    bool fromLocal = false;
    bool quickMessage = false;
    uint8_t quickIndex = 0;
    DeliveryState deliveryState = DeliveryState::kNone;
    uint16_t sequence = 0;
    char text[config::kTextMessageLength + 1] = {};
  };

  struct Conversation {
    bool occupied = false;
    uint8_t mac[6] = {};
    char peerName[config::kDeviceNameLength] = {};
    ChatEntry entries[config::kChatHistoryLength] = {};
    uint8_t entryCount = 0;
    uint8_t nextWriteIndex = 0;
    bool unread = false;
    uint32_t updatedAtMs = 0;
  };

  struct PendingReliableMessage {
    bool active = false;
    protocol::PacketType packetType = protocol::PacketType::kTextMessage;
    uint8_t targetMac[6] = {};
    protocol::PacketBuffer packet = {};
    uint16_t sequence = 0;
    uint8_t attempts = 0;
    uint32_t retryAtMs = 0;
    uint8_t conversationIndex = 0xFF;
    uint8_t entryIndex = 0xFF;
    uint8_t quickIndex = 0;
    bool playQuickClipOnAck = false;
  };

  struct RecentReliableMessage {
    bool valid = false;
    protocol::PacketType packetType = protocol::PacketType::kTextMessage;
    uint8_t senderMac[6] = {};
    uint16_t sequence = 0;
    uint32_t expiresAtMs = 0;
  };

  void composeDeviceName();
  void handleInput(uint32_t nowMs);
  void processIncoming(uint32_t nowMs);
  void processPendingReliable(uint32_t nowMs);
  void handlePresencePacket(const ReceivedFrame& frame,
                            const protocol::PacketHeader& header);
  void handleLegacyPresencePacket(const ReceivedFrame& frame,
                                  const protocol::PacketHeader& header);
  void handleQuickMessagePacket(const ReceivedFrame& frame,
                                const protocol::PacketHeader& header);
  void handleTextMessagePacket(const ReceivedFrame& frame,
                               const protocol::PacketHeader& header);
  void handleAckPacket(const ReceivedFrame& frame,
                       const protocol::PacketHeader& header);
  void handleControlPacket(const ReceivedFrame& frame,
                           const protocol::PacketHeader& header);
  void handleVoicePacket(const ReceivedFrame& frame,
                         const protocol::PacketHeader& header);
  void syncSelectedPeer(uint32_t nowMs);
  void handleAudioControls(const InputSnapshot& input);
  void handlePeerPickerInput(const InputSnapshot& input, uint32_t nowMs);
  void handleCommInput(const InputSnapshot& input, uint32_t nowMs);
  void handleComposeInput(const InputSnapshot& input, uint32_t nowMs);
  void handleHelpInput(const InputSnapshot& input);
  void updateMelodyHold(bool melodyHeld, uint32_t nowMs);
  void openHelp();
  void closeHelp();
  void setInlineNotice(const char* message, uint32_t nowMs);
  void clearInlineNotice();
  const PeerInfo* selectedPeer(uint32_t nowMs) const;
  bool selectedPeerSupportsText(uint32_t nowMs) const;
  Conversation* findConversation(const uint8_t* macAddress);
  const Conversation* findConversation(const uint8_t* macAddress) const;
  int conversationIndex(const uint8_t* macAddress) const;
  Conversation* ensureConversation(const uint8_t* macAddress, const char* peerName,
                                   uint32_t nowMs);
  void appendConversationEntry(const uint8_t* macAddress, const char* peerName,
                               const char* text, bool fromLocal,
                               bool quickMessage, uint8_t quickIndex,
                               uint32_t nowMs,
                               DeliveryState deliveryState = DeliveryState::kNone,
                               uint16_t sequence = 0);
  void updateConversationEntryDelivery(uint8_t conversationIndex, uint8_t entryIndex,
                                       DeliveryState deliveryState);
  void clearPendingReliable();
  void failPendingReliable(uint32_t nowMs);
  bool isReliableDuplicate(const uint8_t* senderMac,
                           protocol::PacketType packetType, uint16_t sequence,
                           uint32_t nowMs) const;
  void rememberReliableMessage(const uint8_t* senderMac,
                               protocol::PacketType packetType, uint16_t sequence,
                               uint32_t nowMs);
  bool sendQuickMessage(std::size_t quickIndex, uint32_t nowMs);
  bool sendTextMessage(uint32_t nowMs);
  void buildUiModel(UiRenderModel& model, uint32_t nowMs) const;
  VoiceVisualState voiceVisualState(uint32_t nowMs) const;

  board::BoardProfile profile_ = board::activeProfile();
  BoardSupport board_{};
  PeerManager peers_{};
  RadioManager radio_{};
  audio::PcmU8Codec codec_{};
  AudioService audio_{};
  UiRenderer ui_{};
  mutable UiRenderModel renderModel_{};

  char deviceName_[config::kDeviceNameLength] = {};
  UiScreenMode uiMode_ = UiScreenMode::kPeerPicker;
  UiScreenMode helpReturnMode_ = UiScreenMode::kPeerPicker;
  uint8_t helpPageIndex_ = 0;
  uint8_t nextMelodyIndex_ = 0;
  bool melodyHoldActive_ = false;
  bool melodyHoldTriggered_ = false;
  uint32_t melodyHoldStartedMs_ = 0;
  char composeDraft_[config::kTextMessageLength + 1] = {};
  uint8_t composeLength_ = 0;
  char inlineNotice_[32] = {};
  uint32_t inlineNoticeUntilMs_ = 0;
  Conversation conversations_[config::kMaxPeers] = {};
  RuntimeStats stats_{};
  uint32_t lastQuickMessageMs_ = 0;
  uint32_t lastBeaconMs_ = 0;
  uint32_t lastRenderMs_ = 0;
  PendingReliableMessage pendingReliable_{};
  RecentReliableMessage recentReliable_[config::kReliableMessageDedupEntries] = {};
};

}  // namespace wt
