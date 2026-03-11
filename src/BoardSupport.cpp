#include "BoardSupport.h"

#include <M5Cardputer.h>

#include "AppConfig.h"

namespace wt {

namespace {

char normalizeLetter(char c) {
  if (c >= 'A' && c <= 'Z') {
    return static_cast<char>(c - 'A' + 'a');
  }
  return c;
}

}  // namespace

void BoardSupport::configureSpeaker() {
  auto speakerConfig = M5Cardputer.Speaker.config();
  speakerConfig.magnification = profile_.speakerMagnification;
  M5Cardputer.Speaker.config(speakerConfig);
  M5Cardputer.Speaker.begin();
  applyVoiceVolume();
  M5Cardputer.Speaker.setAllChannelVolume(255);
}

uint8_t BoardSupport::rawVolumeForStep(uint8_t step) const {
  const uint8_t clamped = min(step, config::kMaxVolumeStep);
  if (clamped == 0) {
    return 0;
  }
  return static_cast<uint8_t>((static_cast<uint16_t>(profile_.speakerVolume) *
                               clamped) /
                              config::kMaxVolumeStep);
}

void BoardSupport::applyVoiceVolume() {
  M5Cardputer.Speaker.setVolume(rawVolumeForStep(voiceVolumeStep_));
}

bool BoardSupport::begin(const board::BoardProfile& profile) {
  profile_ = profile;

  auto cfg = M5.config();
  cfg.clear_display = true;
  M5Cardputer.begin(cfg, true);

  const auto detectedBoard = M5.getBoard();
  if (detectedBoard == m5::board_t::board_M5CardputerADV) {
    profile_ = board::cardputerAdvProfile();
  } else if (detectedBoard == m5::board_t::board_M5Cardputer) {
    profile_ = board::cardputerProfile();
  }

  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.fillScreen(0x0000);

  // BoardProfile keeps the hardware-specific audio path isolated here.
  configureSpeaker();
  M5Cardputer.Speaker.stop();
  M5Cardputer.Mic.end();
  audioMode_ = AudioMode::kIdle;
  previousInput_ = {};
  Serial.printf("[board] %s spk_vol=%u spk_mag=%u\n", profile_.variantName,
                profile_.speakerVolume, profile_.speakerMagnification);
  return true;
}

const board::BoardProfile& BoardSupport::profile() const {
  return profile_;
}

void BoardSupport::setSystemSoundsEnabled(bool enabled) {
  systemSoundsEnabled_ = enabled;
}

bool BoardSupport::systemSoundsEnabled() const {
  return systemSoundsEnabled_;
}

void BoardSupport::setEffectsVolumeStep(uint8_t step) {
  effectsVolumeStep_ = min(step, config::kMaxVolumeStep);
}

uint8_t BoardSupport::effectsVolumeStep() const {
  return effectsVolumeStep_;
}

void BoardSupport::setVoiceVolumeStep(uint8_t step) {
  voiceVolumeStep_ = min(step, config::kMaxVolumeStep);
  if (audioMode_ != AudioMode::kTx) {
    applyVoiceVolume();
  }
}

uint8_t BoardSupport::voiceVolumeStep() const {
  return voiceVolumeStep_;
}

void BoardSupport::update() {
  M5Cardputer.update();
}

bool BoardSupport::containsKey(const std::vector<char>& keys, char needle) {
  const char normalizedNeedle = normalizeLetter(needle);
  for (char key : keys) {
    if (normalizeLetter(key) == normalizedNeedle) {
      return true;
    }
  }
  return false;
}

BoardSupport::RawInputState BoardSupport::currentRawInput() const {
  RawInputState raw;
  const auto keyState = M5Cardputer.Keyboard.keysState();
  const auto& pressedKeys = keyState.word;

  raw.up = containsKey(pressedKeys, ';') || containsKey(pressedKeys, '=');
  raw.down = containsKey(pressedKeys, '.') || containsKey(pressedKeys, '-');
  raw.enter = keyState.enter;
  raw.space = keyState.space;
  raw.melody = containsKey(pressedKeys, 'p');
  raw.qualityDown = containsKey(pressedKeys, 'a');
  raw.qualityUp = containsKey(pressedKeys, 'd');
  raw.soundToggle = containsKey(pressedKeys, 'x');
  raw.effectsDown = containsKey(pressedKeys, 'z');
  raw.effectsUp = containsKey(pressedKeys, 'c');
  raw.voiceDown = containsKey(pressedKeys, 'r');
  raw.voiceUp = containsKey(pressedKeys, 't');
  raw.txGainDown = containsKey(pressedKeys, 'f');
  raw.txGainUp = containsKey(pressedKeys, 'g');
  raw.fn = keyState.fn;
  raw.clearSelection = keyState.del;
  raw.escape = containsKey(pressedKeys, '`') || containsKey(pressedKeys, '~');
  raw.ptt = keyState.space || M5Cardputer.BtnA.isPressed();

  for (int index = 0; index < static_cast<int>(config::kMaxQuickMessages);
       ++index) {
    const char digit = static_cast<char>('1' + index);
    if (containsKey(pressedKeys, digit)) {
      raw.quickDirectIndex = index;
      break;
    }
  }

  return raw;
}

InputSnapshot BoardSupport::readInput() {
  const RawInputState raw = currentRawInput();
  InputSnapshot snapshot;
  snapshot.upPressed = raw.up && !previousInput_.up;
  snapshot.downPressed = raw.down && !previousInput_.down;
  snapshot.enterPressed = raw.enter && !previousInput_.enter;
  snapshot.spacePressed = raw.space && !previousInput_.space;
  snapshot.melodyHeld = raw.melody;
  snapshot.qualityDownPressed = raw.qualityDown && !previousInput_.qualityDown;
  snapshot.qualityUpPressed = raw.qualityUp && !previousInput_.qualityUp;
  snapshot.soundTogglePressed = raw.soundToggle && !previousInput_.soundToggle;
  snapshot.effectsDownPressed =
      raw.effectsDown && !previousInput_.effectsDown;
  snapshot.effectsUpPressed = raw.effectsUp && !previousInput_.effectsUp;
  snapshot.voiceDownPressed = raw.voiceDown && !previousInput_.voiceDown;
  snapshot.voiceUpPressed = raw.voiceUp && !previousInput_.voiceUp;
  snapshot.txGainDownPressed =
      raw.txGainDown && !previousInput_.txGainDown;
  snapshot.txGainUpPressed = raw.txGainUp && !previousInput_.txGainUp;
  snapshot.fnPressed = raw.fn && !previousInput_.fn;
  snapshot.clearSelectionPressed =
      raw.clearSelection && !previousInput_.clearSelection;
  snapshot.escapePressed = raw.escape && !previousInput_.escape;
  snapshot.pttHeld = raw.ptt;
  snapshot.quickDirectIndex =
      (raw.quickDirectIndex >= 0 &&
       raw.quickDirectIndex != previousInput_.quickDirectIndex)
          ? raw.quickDirectIndex
          : -1;

  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    const auto& keyState = M5Cardputer.Keyboard.keysState();
    for (char key : keyState.word) {
      if (key == '`' || key == '~') {
        continue;
      }
      if (key < 32 || key > 126) {
        continue;
      }
      if (snapshot.textCharCount >= config::kInputBatchChars) {
        break;
      }
      snapshot.textChars[snapshot.textCharCount++] = key;
    }
  }

  previousInput_ = raw;
  return snapshot;
}

int8_t BoardSupport::batteryPercent() const {
  const auto level = M5Cardputer.Power.getBatteryLevel();
  if (level < 0) {
    return -1;
  }
  if (level > 100) {
    return 100;
  }
  return static_cast<int8_t>(level);
}

void BoardSupport::beep(uint16_t frequencyHz, uint16_t durationMs,
                        uint8_t gainPercent) {
  if (!systemSoundsEnabled_ || effectsVolumeStep_ == 0 ||
      (audioMode_ != AudioMode::kRx && audioMode_ != AudioMode::kIdle)) {
    return;
  }
  const uint8_t baseVolume = rawVolumeForStep(effectsVolumeStep_);
  const uint8_t scaledVolume = static_cast<uint8_t>(
      (static_cast<uint16_t>(baseVolume) * gainPercent) / 100);
  M5Cardputer.Speaker.setVolume(scaledVolume);
  M5Cardputer.Speaker.tone(frequencyHz, durationMs);
}

bool BoardSupport::enterTxMode() {
  if (audioMode_ == AudioMode::kTx) {
    return true;
  }

  M5Cardputer.Speaker.stop();
  M5Cardputer.Speaker.end();
  M5Cardputer.Mic.begin();
  audioMode_ = AudioMode::kTx;
  return true;
}

bool BoardSupport::enterRxMode() {
  if (audioMode_ == AudioMode::kRx) {
    return true;
  }

  M5Cardputer.Mic.end();
  configureSpeaker();
  audioMode_ = AudioMode::kRx;
  return true;
}

void BoardSupport::stopPlayback() {
  if (audioMode_ != AudioMode::kTx) {
    M5Cardputer.Speaker.stop();
  }
}

bool BoardSupport::captureAudioFrame(uint8_t* buffer, std::size_t frameBytes,
                                     uint32_t sampleRateHz) {
  if (!enterTxMode()) {
    return false;
  }

  return M5Cardputer.Mic.record(buffer, frameBytes, sampleRateHz, false);
}

bool BoardSupport::playAudioFrame(const uint8_t* buffer,
                                  std::size_t frameBytes,
                                  uint32_t sampleRateHz) {
  if (!enterRxMode()) {
    return false;
  }

  applyVoiceVolume();
  M5Cardputer.Speaker.playRaw(buffer, frameBytes, sampleRateHz, false,
                              1, 0);
  return true;
}

}  // namespace wt
