#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

#include "AppConfig.h"
#include "BoardProfiles.h"
#include "MacAddress.h"

namespace wt::protocol {

enum class PacketType : uint8_t {
  kPresence = 1,
  kPresenceAck = 2,
  kVoice = 3,
  kQuickMessage = 4,
  kControl = 5,
  kTextMessage = 6,
};

enum class ControlCode : uint8_t {
  kPttStart = 1,
  kPttStop = 2,
};

enum class AudioCodec : uint8_t {
  kPcmU8 = 1,
  kAdpcmIma = 2,
};

enum PacketFlags : uint8_t {
  kFlagBroadcast = 1 << 0,
  kFlagAckRequested = 1 << 1,
  kFlagReserved = 1 << 2,
};

enum CapabilityFlags : uint8_t {
  kCapabilityQuickMessages = 1 << 0,
  kCapabilityVoice = 1 << 1,
  kCapabilityTextMessages = 1 << 2,
};

#pragma pack(push, 1)
struct PacketHeader {
  uint8_t magic;
  uint8_t version;
  uint8_t type;
  uint8_t flags;
  uint8_t sender[mac::kMacLength];
  uint8_t target[mac::kMacLength];
  uint16_t sequence;
  uint16_t sessionId;
  uint8_t payloadLength;
};

struct PresencePayload {
  char deviceName[config::kDeviceNameLength];
  uint8_t boardVariant;
  uint8_t capabilityFlags;
  int8_t batteryPercent;
  uint8_t reserved;
};

struct VoicePayloadHeader {
  uint8_t codec;
  uint8_t sampleRateKhz;
  uint8_t frameDurationMs;
  uint8_t frameBytes;
  uint8_t pcmFrameBytes;
  int16_t predictor;
  uint8_t stepIndex;
};

struct QuickMessagePayload {
  uint8_t messageId;
  char text[config::kQuickMessageTextLength];
};

struct TextMessagePayload {
  uint8_t textLength;
  char text[config::kTextMessageLength];
};

struct ControlPayload {
  uint8_t controlCode;
  uint8_t reserved0;
  uint16_t reserved1;
};
#pragma pack(pop)

constexpr std::size_t kHeaderSize = sizeof(PacketHeader);
constexpr std::size_t kMaxPacketBytes = kHeaderSize + config::kPacketPayloadMax;
constexpr std::size_t kLegacyVoiceHeaderSize = 4;
constexpr std::size_t kMinVoicePayloadSize = sizeof(VoicePayloadHeader);
constexpr std::size_t kMaxVoicePayloadSize =
    sizeof(VoicePayloadHeader) + config::kMaxEncodedAudioFrameBytes;

struct PacketBuffer {
  uint8_t bytes[kMaxPacketBytes] = {};
  uint8_t length = 0;
};

inline const char* packetTypeName(PacketType type) {
  switch (type) {
    case PacketType::kPresence:
      return "PRESENCE";
    case PacketType::kPresenceAck:
      return "PRESENCE_ACK";
    case PacketType::kVoice:
      return "VOICE";
    case PacketType::kQuickMessage:
      return "QUICK_MESSAGE";
    case PacketType::kControl:
      return "CONTROL";
    case PacketType::kTextMessage:
      return "TEXT_MESSAGE";
  }
  return "UNKNOWN";
}

inline bool isKnownType(uint8_t rawType) {
  return rawType >= static_cast<uint8_t>(PacketType::kPresence) &&
         rawType <= static_cast<uint8_t>(PacketType::kTextMessage);
}

inline bool isTargetForLocal(const PacketHeader& header,
                             const uint8_t* localMac) {
  return mac::isBroadcast(header.target) || mac::equals(header.target, localMac);
}

inline bool buildPacket(PacketBuffer& out, PacketType type, const uint8_t* sender,
                        const uint8_t* target, uint16_t sequence,
                        uint16_t sessionId, uint8_t flags,
                        const void* payload, std::size_t payloadLength) {
  if (payloadLength > config::kPacketPayloadMax) {
    return false;
  }

  auto* header = reinterpret_cast<PacketHeader*>(out.bytes);
  header->magic = config::kProtocolMagic;
  header->version = config::kProtocolVersion;
  header->type = static_cast<uint8_t>(type);
  header->flags = flags;
  mac::copy(header->sender, sender);
  mac::copy(header->target, target);
  header->sequence = sequence;
  header->sessionId = sessionId;
  header->payloadLength = static_cast<uint8_t>(payloadLength);

  if ((payloadLength > 0) && (payload != nullptr)) {
    memcpy(out.bytes + kHeaderSize, payload, payloadLength);
  }
  out.length = static_cast<uint8_t>(kHeaderSize + payloadLength);
  return true;
}

inline bool isValidPacketBuffer(const uint8_t* data, std::size_t length) {
  if (length < kHeaderSize || length > kMaxPacketBytes) {
    return false;
  }

  const auto* header = reinterpret_cast<const PacketHeader*>(data);
  if (header->magic != config::kProtocolMagic ||
      header->version != config::kProtocolVersion ||
      !isKnownType(header->type)) {
    return false;
  }

  const std::size_t expectedSize = kHeaderSize + header->payloadLength;
  if (expectedSize != length) {
    return false;
  }

  switch (static_cast<PacketType>(header->type)) {
    case PacketType::kPresence:
      return header->payloadLength == sizeof(PresencePayload);
    case PacketType::kPresenceAck:
      return header->payloadLength == 0;
    case PacketType::kVoice:
      if (header->payloadLength < kLegacyVoiceHeaderSize ||
          header->payloadLength > kMaxVoicePayloadSize) {
        return false;
      }
      {
        const uint8_t* payload = data + kHeaderSize;
        const uint8_t codec = payload[0];
        const uint8_t frameBytes = payload[3];
        if (codec == static_cast<uint8_t>(AudioCodec::kPcmU8) &&
            header->payloadLength ==
                kLegacyVoiceHeaderSize + frameBytes &&
            frameBytes <= config::kMaxAudioFrameBytes &&
            payload[1] > 0 && payload[2] > 0) {
          return true;
        }
        if (header->payloadLength < sizeof(VoicePayloadHeader)) {
          return false;
        }
        const auto* voice = reinterpret_cast<const VoicePayloadHeader*>(payload);
        return header->payloadLength ==
                   sizeof(VoicePayloadHeader) + voice->frameBytes &&
               voice->frameBytes <= config::kMaxEncodedAudioFrameBytes &&
               voice->pcmFrameBytes > 0 &&
               voice->pcmFrameBytes <= config::kMaxAudioFrameBytes &&
               voice->sampleRateKhz > 0 && voice->frameDurationMs > 0;
      }
    case PacketType::kQuickMessage:
      return header->payloadLength == sizeof(QuickMessagePayload);
    case PacketType::kControl:
      return header->payloadLength == sizeof(ControlPayload);
    case PacketType::kTextMessage:
      if (header->payloadLength != sizeof(TextMessagePayload)) {
        return false;
      }
      {
        const auto* text = reinterpret_cast<const TextMessagePayload*>(
            data + kHeaderSize);
        return text->textLength <= config::kTextMessageLength;
      }
  }

  return false;
}

inline const PacketHeader* header(const uint8_t* data) {
  return reinterpret_cast<const PacketHeader*>(data);
}

inline const PresencePayload* presencePayload(const uint8_t* data) {
  return reinterpret_cast<const PresencePayload*>(data + kHeaderSize);
}

inline const VoicePayloadHeader* voiceHeader(const uint8_t* data) {
  return reinterpret_cast<const VoicePayloadHeader*>(data + kHeaderSize);
}

inline const uint8_t* voiceData(const uint8_t* data) {
  const uint8_t codec = data[kHeaderSize];
  const std::size_t headerSize =
      (codec == static_cast<uint8_t>(AudioCodec::kPcmU8) &&
       header(data)->payloadLength ==
           kLegacyVoiceHeaderSize + data[kHeaderSize + 3])
          ? kLegacyVoiceHeaderSize
          : sizeof(VoicePayloadHeader);
  return data + kHeaderSize + headerSize;
}

inline const QuickMessagePayload* quickMessagePayload(const uint8_t* data) {
  return reinterpret_cast<const QuickMessagePayload*>(data + kHeaderSize);
}

inline const TextMessagePayload* textMessagePayload(const uint8_t* data) {
  return reinterpret_cast<const TextMessagePayload*>(data + kHeaderSize);
}

inline const ControlPayload* controlPayload(const uint8_t* data) {
  return reinterpret_cast<const ControlPayload*>(data + kHeaderSize);
}

inline void makePresencePayload(PresencePayload& payload, const char* deviceName,
                                board::BoardVariant boardVariant,
                                uint8_t capabilityFlags,
                                int8_t batteryPercent) {
  memset(&payload, 0, sizeof(payload));
  strncpy(payload.deviceName, deviceName, sizeof(payload.deviceName) - 1);
  payload.boardVariant = static_cast<uint8_t>(boardVariant);
  payload.capabilityFlags = capabilityFlags;
  payload.batteryPercent = batteryPercent;
}

inline void makeQuickMessagePayload(QuickMessagePayload& payload,
                                    uint8_t messageId, const char* text) {
  memset(&payload, 0, sizeof(payload));
  payload.messageId = messageId;
  strncpy(payload.text, text, sizeof(payload.text) - 1);
}

inline void makeTextMessagePayload(TextMessagePayload& payload,
                                   const char* text) {
  memset(&payload, 0, sizeof(payload));
  if (text == nullptr) {
    return;
  }
  const std::size_t length = strnlen(text, config::kTextMessageLength);
  payload.textLength = static_cast<uint8_t>(length);
  memcpy(payload.text, text, length);
}

inline void makeControlPayload(ControlPayload& payload, ControlCode code) {
  memset(&payload, 0, sizeof(payload));
  payload.controlCode = static_cast<uint8_t>(code);
}

inline std::size_t makeVoicePayload(uint8_t* outPayload, AudioCodec codec,
                                    uint32_t sampleRateHz,
                                    uint8_t frameDurationMs,
                                    uint8_t frameBytes,
                                    uint8_t pcmFrameBytes,
                                    int16_t predictor,
                                    uint8_t stepIndex,
                                    const uint8_t* audioData) {
  auto* header = reinterpret_cast<VoicePayloadHeader*>(outPayload);
  header->codec = static_cast<uint8_t>(codec);
  header->sampleRateKhz = static_cast<uint8_t>(sampleRateHz / 1000);
  header->frameDurationMs = frameDurationMs;
  header->frameBytes = frameBytes;
  header->pcmFrameBytes = pcmFrameBytes;
  header->predictor = predictor;
  header->stepIndex = stepIndex;
  memcpy(outPayload + sizeof(VoicePayloadHeader), audioData, frameBytes);
  return sizeof(VoicePayloadHeader) + frameBytes;
}

}  // namespace wt::protocol
