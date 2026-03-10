## Cardputer Walkietalkie v0.1.0

Initial public release for M5Stack Cardputer and Cardputer ADV.

### Highlights

- ESP-NOW peer discovery and direct peer selection
- split-screen communication UI
- fullscreen text composer
- fixed quick messages on `1..9`
- per-peer text chat history in RAM
- scrollable one-page help
- stable PCM voice transport
- quality presets: `8 / 10 / 12 kHz`
- prebuilt binaries for both board variants

### Included binaries

- `releases/cardputer/merged.bin`
- `releases/cardputer_adv/merged.bin`
- full flash bundles for both boards:
  - `bootloader.bin`
  - `partitions.bin`
  - `boot_app0.bin`
  - `firmware.bin`

### Notes

- quick messages are limited to one send per second
- if FX is enabled, quick messages also send a spoken clip
- keep `LICENSE` and `NOTICE` when redistributing
