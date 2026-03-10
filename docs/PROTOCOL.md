# Cardie Talkie Packet Protocol

Protocol version: `1`

Transport: ESP-NOW only

Recommended Wi-Fi channel: compile-time constant `kEspNowChannel` in `include/AppConfig.h`

Current link protection:
- `PRESENCE` remains unencrypted broadcast traffic
- unicast peers may use built-in ESP-NOW link encryption with a shared compile-time PMK/LMK
- this is a minimal privacy layer only; there is no pairing or authenticated key exchange

## Packet header

All packets start with the same packed header (`include/Protocol.h`):

```c
struct PacketHeader {
  uint8_t magic;        // 0x57
  uint8_t version;      // protocol version
  uint8_t type;         // PRESENCE / VOICE / QUICK_MESSAGE / CONTROL
  uint8_t flags;        // broadcast/ack bits
  uint8_t sender[6];    // sender MAC/device id
  uint8_t target[6];    // target MAC or FF:FF:FF:FF:FF:FF
  uint16_t sequence;    // packet sequence number
  uint16_t sessionId;   // voice push-to-talk session id
  uint8_t payloadLength;
};
```

The firmware rejects packets when:
- `magic` is wrong
- `version` is unsupported
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
  uint8_t capabilityFlags; // bit0 quick messages, bit1 voice
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
  uint8_t codec;           // currently PCM_U8
  uint8_t sampleRateKhz;   // 8
  uint8_t frameDurationMs; // 20
  uint8_t frameBytes;      // 160
};
uint8_t audio[160];
```

Current audio format:
- mono
- 8 kHz
- 8-bit unsigned PCM
- 20 ms per frame
- 160-byte audio payload

Notes:
- `sequence` is used by the receive jitter buffer
- `sessionId` changes on each push-to-talk press/release cycle
- missing frames are replaced with silence
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
- the `QUICK_MESSAGE` packet remains text-only
- when the sender has system sounds enabled, it may immediately follow the text packet with a normal `CONTROL + VOICE` burst that carries the matching bundled spoken clip
- no new packet type is used for spoken quick messages
- as a unicast packet, it can be protected by the shared ESP-NOW link key

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

## Size budget

This project intentionally stays well under classic ESP-NOW payload limits:
- header: 21 bytes
- voice payload header: 4 bytes
- voice audio: 160 bytes
- total voice packet: 185 bytes

## Compatibility rules

A device is considered compatible when:
- `magic == 0x57`
- `version == 1`
- packet sizes match the compiled protocol

Unknown versions are ignored instead of partially parsed.
