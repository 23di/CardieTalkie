<img width="520" height="520" alt="IMG_6112 1@1x" src="https://github.com/user-attachments/assets/7a0ceb2c-3589-44f6-9fb6-64c90822e003" />
# CardieTalkie

CardieTalkie is a walkie-talkie app for Cardputer and Cardputer ADV based on ESP-NOW. It supports P2P PCM U8 mono audio communication. Text messages are stored in RAM only.

## Features

- peer picker plus split-screen communication UI
- push-to-talk voice with `ROBUST`, `BAL`, and `CLEAR` presets
- fixed quick messages on `1..9`
- fullscreen text composer
- scrollable help screen
- prebuilt full-flash binaries:
  - `releases/cardputer.bin`
  - `releases/cardputer-adv.bin`

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

Quick messages are sent from the communication screen, added to chat history, and rate-limited to one send per second. If FX is enabled, they also send a spoken clip.

## Encryption

CardieTalkie uses ESP-NOW encrypted peer traffic with a fixed PMK and LMK configured in firmware. Devices must run matching keys to communicate. Chat history is not persisted and remains in RAM only.

## Build

```bash
pio run -e cardputer
pio run -e cardputer_adv
```

## Upload

```bash
pio run -e cardputer -t upload --upload-port /dev/cu.usbmodem1101
pio run -e cardputer_adv -t upload --upload-port /dev/cu.usbmodem1101
```

Redistribution should keep `LICENSE` and `NOTICE`.
