#include "RadioManager.h"

#include <esp_arduino_version.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include "MacAddress.h"

namespace wt {

RadioManager* RadioManager::instance_ = nullptr;

bool RadioManager::begin(const board::BoardProfile& profile) {
  profile_ = profile;
  instance_ = this;

  rxQueue_ = xQueueCreateStatic(config::kRxQueueDepth, sizeof(ReceivedFrame),
                                rxQueueStorage_, &rxQueueStorageStruct_);
  txQueue_ = xQueueCreateStatic(config::kTxQueueDepth, sizeof(TxRequest),
                                txQueueStorage_, &txQueueStorageStruct_);
  if (rxQueue_ == nullptr || txQueue_ == nullptr) {
    Serial.println("[radio] queue allocation failed");
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.setSleep(false);
  esp_wifi_set_promiscuous(false);
  esp_wifi_set_channel(config::kEspNowChannel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_get_mac(WIFI_IF_STA, localMac_);

  if (esp_now_init() != ESP_OK) {
    Serial.println("[radio] esp_now_init failed");
    return false;
  }

  if (esp_now_set_pmk(config::kEspNowPmk.data()) != ESP_OK) {
    Serial.println("[radio] esp_now_set_pmk failed");
    return false;
  }

  esp_now_register_send_cb(onSend);
  esp_now_register_recv_cb(onReceive);

  esp_now_peer_info_t broadcastPeer = {};
  mac::setBroadcast(broadcastPeer.peer_addr);
  broadcastPeer.ifidx = WIFI_IF_STA;
  broadcastPeer.channel = config::kEspNowChannel;
  broadcastPeer.encrypt = false;
  if (esp_now_add_peer(&broadcastPeer) != ESP_OK &&
      !esp_now_is_peer_exist(broadcastPeer.peer_addr)) {
    Serial.println("[radio] failed to register broadcast peer");
    return false;
  }

  BaseType_t created = xTaskCreate(txTaskEntry, "wt_radio_tx",
                                   config::kRadioTxTaskStack, this, 2,
                                   &txTaskHandle_);
  if (created != pdPASS) {
    Serial.println("[radio] failed to start tx task");
    return false;
  }

  char macBuffer[18];
  Serial.printf("[radio] started on channel %u as %s\n", config::kEspNowChannel,
                mac::toString(localMac_, macBuffer, sizeof(macBuffer)));
  return true;
}

void RadioManager::setDeviceName(const char* deviceName) {
  strncpy(deviceName_, deviceName, sizeof(deviceName_) - 1);
  deviceName_[sizeof(deviceName_) - 1] = '\0';
}

void RadioManager::setSelectedPeer(const uint8_t* macAddress, bool hasSelection) {
  if (!hasSelection || macAddress == nullptr) {
    clearEncryptedPeer();
    return;
  }

  if (hasEncryptedPeer_ && mac::equals(encryptedPeerMac_, macAddress) &&
      esp_now_is_peer_exist(macAddress)) {
    return;
  }

  clearEncryptedPeer();
  installEncryptedPeer(macAddress);
}

const char* RadioManager::deviceName() const {
  return deviceName_;
}

const uint8_t* RadioManager::localMac() const {
  return localMac_;
}

bool RadioManager::pollReceived(ReceivedFrame& frame) {
  if (xQueueReceive(rxQueue_, &frame, 0) != pdTRUE) {
    return false;
  }
  ++rxPackets_;
  return true;
}

uint16_t RadioManager::nextSequence() {
  portENTER_CRITICAL(&sequenceMux_);
  const uint16_t value = nextSequence_++;
  portEXIT_CRITICAL(&sequenceMux_);
  return value;
}

bool RadioManager::enqueuePacket(const uint8_t* targetMac,
                                 const protocol::PacketBuffer& packet) {
  TxRequest request;
  mac::copy(request.targetMac, targetMac);
  request.length = packet.length;
  memcpy(request.bytes, packet.bytes, packet.length);
  if (xQueueSend(txQueue_, &request, 0) != pdTRUE) {
    Serial.println("[radio] tx queue full");
    return false;
  }
  ++txPackets_;
  return true;
}

bool RadioManager::sendPresence(int8_t batteryPercent) {
  protocol::PresencePayload payload;
  protocol::makePresencePayload(
      payload, deviceName_, profile_.variant,
      protocol::kCapabilityQuickMessages | protocol::kCapabilityVoice |
          protocol::kCapabilityTextMessages,
      batteryPercent);

  uint8_t broadcastMac[6];
  mac::setBroadcast(broadcastMac);

  protocol::PacketBuffer packet;
  if (!protocol::buildPacket(packet, protocol::PacketType::kPresence, localMac_,
                             broadcastMac, nextSequence(), 0,
                             protocol::kFlagBroadcast, &payload,
                             sizeof(payload))) {
    return false;
  }
  return enqueuePacket(broadcastMac, packet);
}

bool RadioManager::sendQuickMessage(const uint8_t* targetMac, uint8_t messageId,
                                    const char* text, bool broadcast) {
  protocol::QuickMessagePayload payload;
  protocol::makeQuickMessagePayload(payload, messageId, text);

  uint8_t resolvedTarget[6];
  if (broadcast) {
    mac::setBroadcast(resolvedTarget);
  } else {
    mac::copy(resolvedTarget, targetMac);
  }

  protocol::PacketBuffer packet;
  if (!protocol::buildPacket(packet, protocol::PacketType::kQuickMessage,
                             localMac_, resolvedTarget, nextSequence(), 0,
                             broadcast ? protocol::kFlagBroadcast : 0, &payload,
                             sizeof(payload))) {
    return false;
  }
  return enqueuePacket(resolvedTarget, packet);
}

bool RadioManager::sendTextMessage(const uint8_t* targetMac, const char* text) {
  if (targetMac == nullptr || text == nullptr) {
    return false;
  }

  protocol::TextMessagePayload payload;
  protocol::makeTextMessagePayload(payload, text);

  protocol::PacketBuffer packet;
  if (!protocol::buildPacket(packet, protocol::PacketType::kTextMessage,
                             localMac_, targetMac, nextSequence(), 0, 0,
                             &payload, sizeof(payload))) {
    return false;
  }
  return enqueuePacket(targetMac, packet);
}

bool RadioManager::sendControl(const uint8_t* targetMac,
                               protocol::ControlCode code,
                               uint16_t sessionId) {
  protocol::ControlPayload payload;
  protocol::makeControlPayload(payload, code);

  protocol::PacketBuffer packet;
  if (!protocol::buildPacket(packet, protocol::PacketType::kControl, localMac_,
                             targetMac, nextSequence(), sessionId, 0, &payload,
                             sizeof(payload))) {
    return false;
  }
  return enqueuePacket(targetMac, packet);
}

bool RadioManager::sendVoiceFrame(const uint8_t* targetMac, uint16_t sessionId,
                                  protocol::AudioCodec codec,
                                  const uint8_t* audioFrame,
                                  std::size_t encodedBytes,
                                  uint32_t sampleRateHz,
                                  uint8_t frameDurationMs,
                                  uint8_t pcmFrameBytes,
                                  int16_t predictor,
                                  uint8_t stepIndex) {
  if (encodedBytes == 0 || encodedBytes > config::kMaxEncodedAudioFrameBytes ||
      pcmFrameBytes == 0 || pcmFrameBytes > config::kMaxAudioFrameBytes) {
    return false;
  }

  uint8_t voicePayload[protocol::kMaxVoicePayloadSize] = {};
  const std::size_t voicePayloadLength = protocol::makeVoicePayload(
      voicePayload, codec, sampleRateHz, frameDurationMs,
      static_cast<uint8_t>(encodedBytes), pcmFrameBytes, predictor, stepIndex,
      audioFrame);

  protocol::PacketBuffer packet;
  if (!protocol::buildPacket(packet, protocol::PacketType::kVoice, localMac_,
                             targetMac, nextSequence(), sessionId, 0,
                             voicePayload, voicePayloadLength)) {
    return false;
  }
  return enqueuePacket(targetMac, packet);
}

bool RadioManager::ensurePeerRegistered(const uint8_t* targetMac) {
  if (mac::isBroadcast(targetMac)) {
    return true;
  }

  if (hasEncryptedPeer_ && mac::equals(encryptedPeerMac_, targetMac) &&
      esp_now_is_peer_exist(targetMac)) {
    return true;
  }

  if (hasEncryptedPeer_ && mac::equals(encryptedPeerMac_, targetMac)) {
    return installEncryptedPeer(targetMac);
  }

  char macBuffer[18];
  Serial.printf("[radio] encrypted peer is not selected for %s\n",
                mac::toString(targetMac, macBuffer, sizeof(macBuffer)));
  return false;
}

bool RadioManager::installEncryptedPeer(const uint8_t* macAddress) {
  if (macAddress == nullptr || mac::isBroadcast(macAddress)) {
    return false;
  }

  if (esp_now_is_peer_exist(macAddress)) {
    esp_now_del_peer(macAddress);
  }

  esp_now_peer_info_t peerInfo = {};
  mac::copy(peerInfo.peer_addr, macAddress);
  memcpy(peerInfo.lmk, config::kEspNowLmk.data(), config::kEspNowLmk.size());
  peerInfo.ifidx = WIFI_IF_STA;
  peerInfo.channel = config::kEspNowChannel;
  peerInfo.encrypt = true;
  const esp_err_t err = esp_now_add_peer(&peerInfo);
  if (err != ESP_OK) {
    char macBuffer[18];
    Serial.printf("[radio] encrypted peer add failed for %s (%d)\n",
                  mac::toString(macAddress, macBuffer, sizeof(macBuffer)),
                  static_cast<int>(err));
    hasEncryptedPeer_ = false;
    memset(encryptedPeerMac_, 0, sizeof(encryptedPeerMac_));
    return false;
  }

  hasEncryptedPeer_ = true;
  mac::copy(encryptedPeerMac_, macAddress);
  return true;
}

void RadioManager::clearEncryptedPeer() {
  if (hasEncryptedPeer_ && esp_now_is_peer_exist(encryptedPeerMac_)) {
    esp_now_del_peer(encryptedPeerMac_);
  }
  hasEncryptedPeer_ = false;
  memset(encryptedPeerMac_, 0, sizeof(encryptedPeerMac_));
}

bool RadioManager::sendRaw(const TxRequest& request) {
  if (!ensurePeerRegistered(request.targetMac)) {
    return false;
  }
  const esp_err_t err = esp_now_send(request.targetMac, request.bytes,
                                     request.length);
  if (err != ESP_OK) {
    char macBuffer[18];
    Serial.printf("[radio] send failed to %s (%d)\n",
                  mac::toString(request.targetMac, macBuffer, sizeof(macBuffer)),
                  static_cast<int>(err));
    return false;
  }
  return true;
}

void RadioManager::txTaskEntry(void* context) {
  auto* self = static_cast<RadioManager*>(context);
  TxRequest request;
  for (;;) {
    if (xQueueReceive(self->txQueue_, &request, portMAX_DELAY) == pdTRUE) {
      self->sendRaw(request);
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }
}

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
void RadioManager::onReceive(const esp_now_recv_info_t* recvInfo,
                             const uint8_t* data, int length) {
  if (instance_ == nullptr || recvInfo == nullptr || data == nullptr ||
      length <= 0) {
    return;
  }

  ReceivedFrame frame;
  mac::copy(frame.sourceMac, recvInfo->src_addr);
  frame.rssi = (recvInfo->rx_ctrl != nullptr) ? recvInfo->rx_ctrl->rssi : -127;
  frame.receivedAtMs = millis();
  frame.length = static_cast<uint8_t>(min(length,
      static_cast<int>(protocol::kMaxPacketBytes)));
  memcpy(frame.bytes, data, frame.length);
  xQueueSend(instance_->rxQueue_, &frame, 0);
}
#else
void RadioManager::onReceive(const uint8_t* sourceMac, const uint8_t* data,
                             int length) {
  if (instance_ == nullptr || sourceMac == nullptr || data == nullptr ||
      length <= 0) {
    return;
  }

  ReceivedFrame frame;
  mac::copy(frame.sourceMac, sourceMac);
  frame.rssi = -127;
  frame.receivedAtMs = millis();
  frame.length = static_cast<uint8_t>(min(length,
      static_cast<int>(protocol::kMaxPacketBytes)));
  memcpy(frame.bytes, data, frame.length);
  xQueueSend(instance_->rxQueue_, &frame, 0);
}
#endif

void RadioManager::onSend(const uint8_t* macAddr, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS || macAddr == nullptr) {
    return;
  }
  char macBuffer[18];
  Serial.printf("[radio] send status %d for %s\n", static_cast<int>(status),
                mac::toString(macAddr, macBuffer, sizeof(macBuffer)));
}

uint32_t RadioManager::txPackets() const {
  return txPackets_;
}

uint32_t RadioManager::rxPackets() const {
  return rxPackets_;
}

}  // namespace wt
