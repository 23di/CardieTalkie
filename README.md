<img width="520" height="520" alt="IMG_6112 1@1x" src="https://github.com/user-attachments/assets/7a0ceb2c-3589-44f6-9fb6-64c90822e003" />

# CardieTalkie

CardieTalkie is an ESP-NOW walkie-talkie firmware for Cardputer and Cardputer ADV. It provides P2P mono voice, RAM-only text chat, and fixed quick messages in a split-screen handheld UI.

## Compatibility Warning

Current firmware uses protocol v2 and is not wire-compatible with the older v0.1.0 firmware. Legacy v1 devices still appear in discovery, but they are marked incompatible and cannot exchange voice, chat, or control traffic with v2 devices.

## Features

- one universal binary for Cardputer and Cardputer ADV with runtime board detection
- peer picker plus split-screen voice/chat UI
- push-to-talk voice with `ROBUST`, `BAL`, and `CLEAR` 10 ms presets
- larger RX jitter buffer with packet-loss concealment for short voice gaps
- quieter radio-style TX start/end cues heard on both devices during PTT
- fixed quick messages on `1..9`
- fullscreen text composer
- app-level ACK/retry for text and quick messages
- mixed-version discovery that shows legacy v1 peers as incompatible
- scrollable help screen

## Quick Messages

| Key | Message |
| --- | ------- |
| `1` | `OK` |
| `2` | `CALL ME` |
| `3` | `BUSY` |
| `4` | `WHERE ARE YOU?` |
| `5` | `COME HERE` |
| `6` | `TEST` |
| `7` | `NEED HELP` |
| `8` | `ON MY WAY` |
| `9` | `REPEAT` |

Quick messages are sent from the communication screen, added to chat history, and rate-limited to one send per second. Text and quick messages stay pending until the peer ACKs them. If FX is enabled, a spoken quick-message clip is sent only after the quick text packet is ACKed.

## Encryption

CardieTalkie uses ESP-NOW encrypted peer traffic with a fixed PMK and LMK configured in firmware. Devices must run matching keys to communicate. Chat history is not persisted and remains in RAM only.

## Protocol

Current firmware speaks protocol v2 for chat, control, and voice traffic. Legacy v1 devices are still visible in discovery, but they are marked incompatible and cannot be selected for communication. See `docs/PROTOCOL.md` for packet details.

## Universal Build

The default build now produces one binary for both supported boards. At boot, the firmware detects whether it is running on Cardputer or Cardputer ADV and applies the correct board profile automatically.

## Build

```bash
pio run -e onebin
pio run -e cardputer
pio run -e cardputer_adv
```

## Upload

```bash
pio run -e onebin -t upload --upload-port /dev/cu.usbmodem1101
pio run -e cardputer -t upload --upload-port /dev/cu.usbmodem1101
pio run -e cardputer_adv -t upload --upload-port /dev/cu.usbmodem1101
```

## Release Binary

The GitHub release includes one universal binary named `CardieTlakie Cardputer and Cardputer ADV.bin`.

Redistribution should keep `LICENSE` and `NOTICE`.
