#pragma once

#include <Arduino.h>
#include <esp_now.h>

#include "AppConfig.h"
#include "BoardProfiles.h"
#include "Protocol.h"

namespace wt {

struct ReceivedFrame {
  uint8_t sourceMac[6] = {};
  int8_t rssi = -127;
  uint32_t receivedAtMs = 0;
  uint8_t length = 0;
  uint8_t bytes[protocol::kMaxPacketBytes] = {};
};

class RadioManager {
 public:
  bool begin(const board::BoardProfile& profile);
  void setDeviceName(const char* deviceName);
  void setSelectedPeer(const uint8_t* macAddress, bool hasSelection);
  const char* deviceName() const;
  const uint8_t* localMac() const;

  bool pollReceived(ReceivedFrame& frame);
  uint16_t reserveSequence();
  bool queuePacket(const uint8_t* targetMac, const protocol::PacketBuffer& packet);
  bool sendPresence(int8_t batteryPercent);
  bool sendQuickMessage(const uint8_t* targetMac, uint8_t messageId,
                        const char* text, bool broadcast = false);
  bool sendTextMessage(const uint8_t* targetMac, const char* text);
  bool sendAck(const uint8_t* targetMac, protocol::PacketType ackedType,
               uint16_t ackedSequence, uint8_t status = 1);
  bool sendControl(const uint8_t* targetMac, protocol::ControlCode code,
                   uint16_t sessionId);
  bool sendVoiceFrame(const uint8_t* targetMac, uint16_t sessionId,
                      protocol::AudioCodec codec, const uint8_t* audioFrame,
                      std::size_t encodedBytes, uint32_t sampleRateHz,
                      uint8_t frameDurationMs, uint8_t pcmFrameBytes,
                      int16_t predictor, uint8_t stepIndex);
  uint32_t txPackets() const;
  uint32_t rxPackets() const;
  uint32_t sendFailures() const;
  uint32_t sendTimeouts() const;

 private:
  struct TxRequest {
    uint8_t targetMac[6] = {};
    uint8_t length = 0;
    uint8_t bytes[protocol::kMaxPacketBytes] = {};
  };

  static void txTaskEntry(void* context);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  static void onReceive(const esp_now_recv_info_t* recvInfo, const uint8_t* data,
                        int length);
#else
  static void onReceive(const uint8_t* sourceMac, const uint8_t* data,
                        int length);
#endif
  static void onSend(const uint8_t* macAddr, esp_now_send_status_t status);

  bool ensurePeerRegistered(const uint8_t* targetMac);
  bool installEncryptedPeer(const uint8_t* macAddress);
  void clearEncryptedPeer();
  bool sendRaw(const TxRequest& request);
  bool waitForSendCompletion();

  board::BoardProfile profile_{};
  char deviceName_[config::kDeviceNameLength] = {};
  uint8_t localMac_[6] = {};
  uint8_t encryptedPeerMac_[6] = {};
  bool hasEncryptedPeer_ = false;
  uint16_t nextSequence_ = 1;
  volatile uint32_t txPackets_ = 0;
  volatile uint32_t rxPackets_ = 0;
  volatile uint32_t sendFailures_ = 0;
  volatile uint32_t sendTimeouts_ = 0;
  portMUX_TYPE sequenceMux_ = portMUX_INITIALIZER_UNLOCKED;
  portMUX_TYPE sendMux_ = portMUX_INITIALIZER_UNLOCKED;
  esp_now_send_status_t pendingSendStatus_ = ESP_NOW_SEND_FAIL;
  bool pendingSendReady_ = false;

  TaskHandle_t txTaskHandle_ = nullptr;
  QueueHandle_t rxQueue_ = nullptr;
  QueueHandle_t txQueue_ = nullptr;
  StaticQueue_t rxQueueStorageStruct_{};
  StaticQueue_t txQueueStorageStruct_{};
  uint8_t rxQueueStorage_[config::kRxQueueDepth * sizeof(ReceivedFrame)] = {};
  uint8_t txQueueStorage_[config::kTxQueueDepth * sizeof(TxRequest)] = {};

  static RadioManager* instance_;
};

}  // namespace wt
