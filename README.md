# Cardputer Walkietalkie

ESP-NOW walkie-talkie firmware for M5Stack Cardputer and Cardputer ADV.

Current build includes:

- peer picker screen
- split-screen communication UI
- fullscreen text message composer
- scrollable one-page help
- push-to-talk voice
- fixed quick messages on `1..9`
- per-peer text chat history in RAM
- optional spoken quick-message clips
- prebuilt binaries in `releases/`

## UI Flow

The firmware uses four UI modes:

1. `PEERS`
   Select a device from the active peer list.
2. `COMMUNICATION`
   Main working screen with voice on the left and chat on the right.
3. `COMPOSE MESSAGE`
   Fullscreen text entry, opened only while sending a typed message.
4. `HELP`
   One scrollable page with controls and quick-message mapping.

The communication footer shows:

`FN HELP  <rate>kHz  VL <percent>%  MC <level>  FX ON|OFF`

## Controls

### Peer Picker

- Up / Down arrows: move highlight
- `Enter`: select peer
- `Fn`: open help

### Communication

- `Space` or `BtnA`: push to talk
- `1..9`: send quick message
- `Enter`: open fullscreen text composer
- `Fn`: open help
- `` ` `` or `~`: return to peer picker

### Compose Message

- type on keyboard: add text
- `Space`: insert space
- `Del`: erase one character
- `Enter`: send message
- `` ` `` or `~`: exit without sending

### Help

- Up / Down arrows: scroll
- `Fn`: close help
- `` ` `` or `~`: close help

### Audio Controls

- `A / D`: change quality preset
- `R / T`: change voice volume (`VL`)
- `F / G`: change mic gain (`MC`)
- `Z / C`: change FX volume
- `X`: toggle FX on or off

## Quick Messages

Quick messages are fixed and sent from the `COMMUNICATION` screen only.

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

Behavior:

- quick messages are inserted into chat history as quick entries
- if `FX` is enabled, a spoken clip is sent with the quick message
- quick sends are rate-limited to `1` message per second
- if a send is blocked by the limiter, the UI shows `WAIT 1S`

## Text Chat

- per-peer chat history is stored in RAM only
- history depth: `10` entries per peer
- typed message length: up to `120` characters
- if the selected peer does not advertise text support, the UI shows `TEXT UNSUPPORTED`

## Audio

Current voice transport is the stable PCM path.

- codec: `PCM U8`
- channels: mono
- frame duration: `10 ms`
- quality presets:
  - `ROBUST = 8 kHz`
  - `BAL = 10 kHz`
  - `CLEAR = 12 kHz`
- mic gain levels:
  - `1 = 100%`
  - `2 = 125%`
  - `3 = 150%`
  - `4 = 180%`
  - `5 = 220%`
- radio/audio queue depth:
  - RX: `24`
  - TX: `24`

Defaults:

- FX volume starts at minimum
- voice volume starts at `4 / 5`
- mic gain starts at `3 / 5`
- system FX are enabled by default

## Protocol Notes

- transport: `ESP-NOW`
- protocol version: `1`
- packet types:
  - presence
  - presence ack
  - voice
  - quick message
  - control
  - text message
- capability flags:
  - quick messages
  - voice
  - text messages

## Build

```bash
pio run -e cardputer
pio run -e cardputer_adv
```

## Upload

Regular Cardputer:

```bash
pio run -e cardputer -t upload --upload-port /dev/cu.usbmodem1101
```

Cardputer ADV:

```bash
pio run -e cardputer_adv -t upload --upload-port /dev/cu.usbmodem1101
```

## Release Files

Prebuilt binaries are stored in:

- `releases/cardputer/`
- `releases/cardputer_adv/`

Each board directory contains:

- `bootloader.bin`
- `partitions.bin`
- `boot_app0.bin`
- `firmware.bin`
- `merged.bin`

`merged.bin` is the simplest file for a full-device flash.

## Notes

- navigation is arrow-based now; `W / S` is no longer used
- the main communication screen shows `ENTER TO CHAT` instead of the old `1..9` legend
- signal quality is shown per peer instead of in the top bar
- chat preview wraps text; fullscreen compose is used only while typing

## License

This project is licensed under Apache-2.0.

That means people can use, modify, redistribute, and ship it commercially, but they must keep the license and `NOTICE` file with attribution to the original author.
