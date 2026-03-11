## CardieTalkie v0.2.0

Reliability-first release for Cardputer and Cardputer ADV.

### Highlights

- one universal binary for Cardputer and Cardputer ADV
- protocol v2 with app-level ACK/retry for text and quick messages
- callback-paced ESP-NOW TX path
- mixed-version discovery with legacy peers shown as incompatible
- larger voice jitter buffer with short-gap concealment
- updated help and settings labels in English
- distinct PTT start and stop tones

### Included Binary

- `CardieTlakie Cardputer and Cardputer ADV.bin`

### Compatibility

- this release is not compatible with the older v0.1.0 firmware
- legacy v1 devices remain visible in discovery, but they cannot exchange voice, text, quick messages, or control packets with v2 devices

### Notes

- quick messages are limited to one send per second
- if FX is enabled, a spoken quick-message clip is sent only after the quick text packet is ACKed
- keep `LICENSE` and `NOTICE` when redistributing
