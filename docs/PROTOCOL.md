# Cardie Talkie Packet Protocol

Protocol version: `2`

Transport: ESP-NOW only

Recommended Wi-Fi channel: compile-time constant `kEspNowChannel` in `include/AppConfig.h`

Current link protection:
- `PRESENCE` remains unencrypted broadcast traffic
- unicast peers may use built-in ESP-NOW link encryption with a shared compile-time PMK/LMK
- this is a minimal privacy layer only; there is no pairing or authenticated key exchange
- protocol v1 `PRESENCE` packets are still parsed so older peers remain visible, but v1 devices are incompatible and cannot be selected

## Packet header

All packets start with the same packed header (`include/Protocol.h`):

```c
struct PacketHeader {
  uint8_t magic;        // 0x57
  uint8_t version;      // protocol version
  uint8_t type;         // PRESENCE / VOICE / QUICK_MESSAGE / CONTROL / TEXT / ACK
  uint8_t flags;        // broadcast / ack-request bits
  uint8_t sender[6];    // sender MAC/device id
  uint8_t target[6];    // target MAC or FF:FF:FF:FF:FF:FF
  uint16_t sequence;    // packet sequence number
  uint16_t sessionId;   // voice push-to-talk session id
  uint8_t payloadLength;
};
```

The firmware rejects packets when:
- `magic` is wrong
- `version` is unsupported for that packet type
- `type` is unknown
- `payloadLength` does not match the actual payload size
- `target` is neither local MAC nor broadcast

## Packet types

### `PRESENCE`

Broadcast every `kBeaconIntervalMs`.

Payload:

```c
struct PresencePayload {
  char deviceName[16];
  uint8_t boardVariant;
  uint8_t capabilityFlags; // bit0 quick messages, bit1 voice, bit2 text
  int8_t batteryPercent;
  uint8_t reserved;
};
```

Purpose:
- nearby peer discovery
- protocol compatibility filtering
- name / board / battery advertisement
- RSSI refresh from received packets

Security note:
- this packet stays visible on the air even when unicast encryption is enabled

### `PRESENCE_ACK`

Reserved, currently unused.

### `VOICE`

Directed unicast to the currently selected peer.

Payload:

```c
struct VoicePayloadHeader {
  uint8_t codec;           // PCM_U8 or ADPCM_IMA
  uint8_t sampleRateKhz;   // 8 / 10 / 12
  uint8_t frameDurationMs; // 10
  uint8_t frameBytes;      // encoded frame size
  uint8_t pcmFrameBytes;   // decoded PCM size
  int16_t predictor;       // ADPCM state
  uint8_t stepIndex;       // ADPCM state
};
uint8_t audio[frameBytes];
```

Current audio format:
- mono
- `ROBUST`: 8 kHz, 10 ms, 80-byte PCM frames
- `BAL`: 10 kHz, 10 ms, 100-byte PCM frames
- `CLEAR`: 12 kHz, 10 ms, 120-byte PCM frames
- 8-bit unsigned PCM on the air

Notes:
- `sequence` is used by the receive jitter buffer
- `sessionId` changes on each push-to-talk press/release cycle
- missing frames are concealed from the last good frame before falling back to silence
- when both devices use the same shared ESP-NOW link key, this unicast payload is encrypted at the link layer

### `QUICK_MESSAGE`

Directed unicast in the current firmware.

Payload:

```c
struct QuickMessagePayload {
  uint8_t messageId;
  char text[24];
};
```

The text is included so different builds can still display known canned messages.

Current behavior:
- `QUICK_MESSAGE` uses `kFlagAckRequested` and is retried by the app layer until an `ACK` arrives or retries are exhausted
- when the sender has system sounds enabled, it follows the ACKed text packet with a normal `CONTROL + VOICE` burst that carries the matching bundled spoken clip
- duplicates are ACKed but not delivered to chat history twice
- as a unicast packet, it can be protected by the shared ESP-NOW link key

### `TEXT_MESSAGE`

Payload:

```c
struct TextMessagePayload {
  uint8_t textLength;
  char text[120];
};
```

Current behavior:
- `TEXT_MESSAGE` uses `kFlagAckRequested` and is retried by the app layer until an `ACK` arrives or retries are exhausted
- duplicates are ACKed but not delivered to chat history twice

### `CONTROL`

Used for push-to-talk edge transitions.

Payload:

```c
struct ControlPayload {
  uint8_t controlCode; // 1 = PTT_START, 2 = PTT_STOP
  uint8_t reserved0;
  uint16_t reserved1;
};
```

Purpose:
- mark TX start before voice frames arrive
- mark TX stop so the receiver can exit RX state cleanly
- reset the jitter buffer when a new talk burst starts
- carry pre-generated spoken quick-message clips over the existing voice session flow
- as a unicast packet, it can be protected by the shared ESP-NOW link key

### `ACK`

Payload:

```c
struct AckPayload {
  uint8_t ackedType;      // QUICK_MESSAGE or TEXT_MESSAGE
  uint16_t ackedSequence; // original packet sequence
  uint8_t status;         // 1 = delivered
};
```

Purpose:
- confirm reliable text / quick-message delivery
- keep retransmits idempotent using `(sender MAC, type, sequence)`

## Size budget

This project intentionally stays well under classic ESP-NOW payload limits:
- header: 21 bytes
- voice payload header: 7 bytes
- voice audio: up to 120 bytes
- total voice packet: up to 148 bytes

## Compatibility rules

A device is considered fully compatible when:
- `magic == 0x57`
- `version == 2`
- packet sizes match the compiled protocol

Legacy rule:
- v1 `PRESENCE` packets are accepted for discovery only
- v1 quick/text/control/voice packets are ignored
