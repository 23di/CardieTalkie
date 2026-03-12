#include "UiRenderer.h"

#include <cstring>

#include <M5Cardputer.h>
#include <M5GFX.h>

namespace wt {

namespace {

constexpr uint16_t kColorWhite = 0xFFFF;
constexpr uint16_t kColorBlack = 0x0000;
constexpr uint16_t kColorRed = 0xF800;
constexpr uint16_t kColorGreen = 0x07E0;
constexpr uint16_t kColorYellow = 0xFFE0;
constexpr uint16_t kColorCyan = 0x07FF;
constexpr uint16_t kColorOrange = 0xFD20;
constexpr uint16_t kColorMagenta = 0xF81F;
constexpr uint16_t kColorLightGray = 0xC618;
constexpr uint16_t kColorDarkGray = 0x4A49;
constexpr uint16_t kColorBlue = 0x0416;
constexpr uint16_t kColorPanel = 0x1082;
constexpr uint16_t kColorPanelAlt = 0x18C3;
constexpr uint16_t kColorBorder = 0x5AEB;
constexpr uint16_t kColorQuick = 0xFF40;
constexpr uint16_t kColorLocal = 0xAFE5;

constexpr int16_t kScreenW = 240;
constexpr int16_t kScreenH = 135;
constexpr int16_t kHeaderY = 4;
constexpr int16_t kHeaderH = 18;
constexpr int16_t kContentY = 26;
constexpr int16_t kFooterY = 116;
constexpr int16_t kFooterH = 15;
constexpr int16_t kContentH = kFooterY - kContentY - 2;
constexpr int16_t kLeftPanelX = 4;
constexpr int16_t kPanelGap = 4;
constexpr int16_t kLeftPanelW = 112;
constexpr int16_t kRightPanelX = kLeftPanelX + kLeftPanelW + kPanelGap;
constexpr int16_t kRightPanelW = 120;
constexpr int16_t kTextLineH = 10;
constexpr std::size_t kWrapLineLength = 40;
constexpr std::size_t kWrapFragmentsPerEntry = 8;
constexpr uint8_t kHelpVisibleLines = 8;

M5Canvas canvas(&M5Cardputer.Display);
bool canvasReady = false;

void drawText(int16_t x, int16_t y, const char* text, uint16_t fg = kColorWhite,
              uint16_t bg = kColorPanel) {
  canvas.setTextColor(fg, bg);
  canvas.setCursor(x, y);
  canvas.print(text);
}

void drawPanel(int16_t x, int16_t y, int16_t w, int16_t h,
               uint16_t fill = kColorPanel,
               uint16_t border = kColorBorder) {
  canvas.fillRoundRect(x, y, w, h, 6, fill);
  canvas.drawRoundRect(x, y, w, h, 6, border);
}

const char* linkQualityText(bool active, int8_t rssi) {
  if (!active) {
    return "LOST";
  }
  if (rssi <= -120) {
    return "WAIT";
  }
  if (rssi >= -67) {
    return "GOOD";
  }
  if (rssi >= -78) {
    return "FAIR";
  }
  if (rssi >= -88) {
    return "BAD";
  }
  return "LOST";
}

uint16_t linkQualityColor(bool active, int8_t rssi) {
  if (!active) {
    return kColorDarkGray;
  }
  if (rssi <= -120) {
    return kColorLightGray;
  }
  if (rssi >= -67) {
    return kColorGreen;
  }
  if (rssi >= -78) {
    return kColorYellow;
  }
  if (rssi >= -88) {
    return kColorOrange;
  }
  return kColorRed;
}

void drawBatteryIcon(int16_t x, int16_t y, int8_t percent) {
  canvas.drawRoundRect(x, y, 18, 10, 2, kColorWhite);
  canvas.fillRect(x + 18, y + 3, 2, 4, kColorWhite);

  uint16_t fillColor = kColorGreen;
  if (percent >= 0 && percent < 30) {
    fillColor = kColorRed;
  } else if (percent >= 0 && percent < 60) {
    fillColor = kColorYellow;
  }

  const int clamped =
      (percent < 0) ? 0 : ((percent > 100) ? 100 : static_cast<int>(percent));
  const int fillWidth = (clamped * 14) / 100;
  canvas.fillRect(x + 2, y + 2, 14, 6, kColorBlack);
  if (fillWidth > 0) {
    canvas.fillRect(x + 2, y + 2, fillWidth, 6, fillColor);
  }
}

int pulseRadius(uint32_t nowMs) {
  const int phase = (nowMs / 90U) % 12U;
  return (phase <= 6) ? phase : (12 - phase);
}

struct WrappedChatRow {
  char text[kWrapLineLength] = {};
  uint16_t color = kColorWhite;
};

void drawVoiceCircle(int16_t x, int16_t y, int16_t w, int16_t h,
                     VoiceVisualState state, uint32_t nowMs,
                     const char* peerName, bool showLink, int8_t linkRssi) {
  drawPanel(x, y, w, h);
  char linkLine[24] = {};
  if (showLink && state != VoiceVisualState::kNoLink) {
    if (linkRssi <= -120) {
      strncpy(linkLine, "LINK WAIT", sizeof(linkLine) - 1);
    } else {
      snprintf(linkLine, sizeof(linkLine), "%s %ddBm",
               linkQualityText(true, linkRssi), static_cast<int>(linkRssi));
    }
  } else {
    strncpy(linkLine, "LINK LOST", sizeof(linkLine) - 1);
  }
  drawText(x + 8, y + 6, linkLine, linkQualityColor(showLink, linkRssi));

  const int16_t centerX = x + (w / 2);
  const int16_t centerY = y + 44;
  const int pulse = pulseRadius(nowMs);
  int radius = 12;
  uint16_t color = kColorCyan;
  const char* label = "SPACE TO TALK";
  char dynamicLabel[24] = {};

  switch (state) {
    case VoiceVisualState::kIdle:
      radius = 12 + pulse / 3;
      color = kColorCyan;
      label = "SPACE TO TALK";
      break;
    case VoiceVisualState::kRx:
      radius = 17 + pulse / 2;
      color = kColorGreen;
      snprintf(dynamicLabel, sizeof(dynamicLabel), "%s TALKS",
               (peerName != nullptr && peerName[0] != '\0') ? peerName : "PEER");
      label = dynamicLabel;
      break;
    case VoiceVisualState::kTx:
      radius = 21 + pulse / 2;
      color = kColorOrange;
      label = "YOU TALK";
      break;
    case VoiceVisualState::kNoLink:
      radius = 12;
      color = kColorDarkGray;
      label = "NO LINK";
      break;
  }

  canvas.drawCircle(centerX, centerY, radius, color);
  canvas.drawCircle(centerX, centerY, radius - 1, color);
  if (state == VoiceVisualState::kRx || state == VoiceVisualState::kTx) {
    canvas.drawCircle(centerX, centerY, radius + 5, color);
  }
  if (state == VoiceVisualState::kTx) {
    canvas.drawCircle(centerX, centerY, radius + 10, color);
  }

  drawText(x + 10, y + h - 18, label, color);
}

void formatChatLine(char* out, std::size_t outSize, const UiChatLine& line,
                    const char* peerName) {
  const char* sender = line.fromLocal ? "ME" : peerName;
  if (sender == nullptr || sender[0] == '\0') {
    sender = line.fromLocal ? "ME" : "PEER";
  }
  if (line.quickMessage) {
    const char* suffix = "";
    if (line.fromLocal && line.deliveryState == DeliveryState::kPending) {
      suffix = " ...";
    } else if (line.fromLocal && line.deliveryState == DeliveryState::kFailed) {
      suffix = " FAIL";
    }
    snprintf(out, outSize, "%s %s%s", line.fromLocal ? "Q>" : "Q<", line.text,
             suffix);
    return;
  }
  const char* suffix = "";
  if (line.fromLocal && line.deliveryState == DeliveryState::kPending) {
    suffix = " ...";
  } else if (line.fromLocal && line.deliveryState == DeliveryState::kFailed) {
    suffix = " FAIL";
  }
  snprintf(out, outSize, "%s: %s%s", sender, line.text, suffix);
}

std::size_t wrapTextRows(const char* text, std::size_t maxChars,
                         char rows[][kWrapLineLength], std::size_t maxRows) {
  if (text == nullptr || text[0] == '\0' || maxChars == 0 || maxRows == 0) {
    return 0;
  }

  const char* cursor = text;
  std::size_t rowCount = 0;
  while (*cursor != '\0' && rowCount < maxRows) {
    while (*cursor == ' ') {
      ++cursor;
    }
    if (*cursor == '\0') {
      break;
    }

    const std::size_t remaining = strlen(cursor);
    std::size_t take = remaining;
    if (take > maxChars) {
      take = maxChars;
      while (take > 0 && cursor[take] != '\0' && cursor[take] != ' ') {
        --take;
      }
      if (take == 0) {
        take = maxChars;
      }
    }

    while (take > 0 && cursor[take - 1] == ' ') {
      --take;
    }

    memcpy(rows[rowCount], cursor, take);
    rows[rowCount][take] = '\0';
    ++rowCount;
    cursor += take;
    while (*cursor == ' ') {
      ++cursor;
    }
  }
  return rowCount;
}

std::size_t collectWrappedChatRows(const UiRenderModel& model, std::size_t maxChars,
                                   std::size_t rowBudget,
                                   WrappedChatRow* rowsOut) {
  if (rowBudget == 0 || rowsOut == nullptr) {
    return 0;
  }

  std::size_t totalRows = 0;
  std::size_t visibleRows = 0;

  for (std::size_t i = 0; i < model.chatLineCount; ++i) {
    const auto& chatLine = model.chatLines[i];
    if (!chatLine.visible) {
      continue;
    }

    char formatted[config::kTextMessageLength + 16] = {};
    formatChatLine(formatted, sizeof(formatted), chatLine, model.peerName);

    char wrapped[kWrapFragmentsPerEntry][kWrapLineLength] = {};
    const std::size_t wrappedCount =
        wrapTextRows(formatted, maxChars, wrapped, kWrapFragmentsPerEntry);
    uint16_t fg = chatLine.quickMessage
                      ? kColorQuick
                      : (chatLine.fromLocal ? kColorLocal : kColorWhite);
    if (chatLine.fromLocal && chatLine.deliveryState == DeliveryState::kPending) {
      fg = kColorYellow;
    } else if (chatLine.fromLocal &&
               chatLine.deliveryState == DeliveryState::kFailed) {
      fg = kColorRed;
    }
    for (std::size_t row = 0; row < wrappedCount; ++row) {
      WrappedChatRow nextRow = {};
      strncpy(nextRow.text, wrapped[row], sizeof(nextRow.text) - 1);
      nextRow.color = fg;

      if (visibleRows < rowBudget) {
        rowsOut[visibleRows++] = nextRow;
      } else {
        for (std::size_t shift = 1; shift < rowBudget; ++shift) {
          rowsOut[shift - 1] = rowsOut[shift];
        }
        rowsOut[rowBudget - 1] = nextRow;
      }
      ++totalRows;
    }
  }

  if (totalRows == 0) {
    return 0;
  }
  return visibleRows;
}

void drawChatPreview(const UiRenderModel& model, int16_t x, int16_t y, int16_t w,
                     int16_t h) {
  drawPanel(x, y, w, h, kColorPanelAlt);
  drawText(x + 8, y + 6, "CHAT", kColorLightGray, kColorPanelAlt);

  const std::size_t maxChars = static_cast<std::size_t>((w - 16) / 6);
  WrappedChatRow visibleRows[4];
  const std::size_t rowCount =
      collectWrappedChatRows(model, maxChars, model.inlineNoticeActive ? 3 : 4,
                             visibleRows);

  if (rowCount == 0) {
    drawText(x + 8, y + 18, "No messages yet", kColorLightGray, kColorPanelAlt);
  } else {
    int16_t cursorY = y + 18;
    for (std::size_t i = 0; i < rowCount; ++i) {
      drawText(x + 8, cursorY, visibleRows[i].text, visibleRows[i].color,
               kColorPanelAlt);
      cursorY += kTextLineH;
    }
  }

  if (model.inlineNoticeActive) {
    drawText(x + 8, y + h - 40, model.inlineNotice, kColorYellow, kColorPanelAlt);
  }

  drawText(x + 8, y + h - 16, "ENTER TO CHAT", kColorLightGray, kColorPanelAlt);
}

void drawComposeScreen(const UiRenderModel& model) {
  drawPanel(4, kContentY, 232, kContentH, kColorPanelAlt);

  const std::size_t maxChars = static_cast<std::size_t>((220) / 6);
  WrappedChatRow visibleRows[5];
  const std::size_t rowCount =
      collectWrappedChatRows(model, maxChars, model.inlineNoticeActive ? 4 : 5,
                             visibleRows);

  if (rowCount == 0) {
    drawText(12, kContentY + 8, "No messages yet", kColorLightGray, kColorPanelAlt);
  } else {
    int16_t cursorY = kContentY + 8;
    for (std::size_t i = 0; i < rowCount; ++i) {
      drawText(12, cursorY, visibleRows[i].text, visibleRows[i].color,
               kColorPanelAlt);
      cursorY += kTextLineH;
    }
  }

  if (model.inlineNoticeActive) {
    drawText(12, kContentY + kContentH - 40, model.inlineNotice, kColorYellow,
             kColorPanelAlt);
  }

  canvas.fillRoundRect(10, kContentY + kContentH - 30, 220, 22, 4, kColorBlack);
  char draftLine[config::kTextMessageLength + 4] = {};
  snprintf(draftLine, sizeof(draftLine), "> %s_", model.composeDraft);
  char wrappedDraft[4][kWrapLineLength] = {};
  const std::size_t wrappedCount = wrapTextRows(draftLine, maxChars, wrappedDraft, 4);
  const std::size_t visibleDraftCount = (wrappedCount > 2) ? 2 : wrappedCount;
  const std::size_t startDraft = (wrappedCount > visibleDraftCount)
                                     ? (wrappedCount - visibleDraftCount)
                                     : 0;
  int16_t draftY = kContentY + kContentH - 27;
  for (std::size_t i = 0; i < visibleDraftCount; ++i) {
    drawText(14, draftY, wrappedDraft[startDraft + i], kColorWhite, kColorBlack);
    draftY += kTextLineH;
  }
}

void drawHelpScreen(const UiRenderModel& model) {
  drawPanel(4, kContentY, 232, kContentH, kColorPanelAlt);

  int16_t y = kContentY + 8;
  std::size_t start = model.helpPageIndex;
  if (start > config::kHelpLines.size()) {
    start = config::kHelpLines.size();
  }
  std::size_t end = start + kHelpVisibleLines;
  if (end > config::kHelpLines.size()) {
    end = config::kHelpLines.size();
  }
  for (std::size_t i = start; i < end; ++i) {
    const char* line = config::kHelpLines[i];
    if (line[0] == '\0') {
      y += kTextLineH;
      continue;
    }
    const bool heading = strcmp(line, "CONTROLS") == 0;
    drawText(12, y, line, heading ? kColorWhite : kColorLightGray, kColorPanelAlt);
    y += 9;
  }

  if (model.helpPageCount > kHelpVisibleLines) {
    const int16_t trackX = 225;
    const int16_t trackY = kContentY + 8;
    const int16_t trackH = 72;
    canvas.drawRoundRect(trackX, trackY, 6, trackH, 3, kColorBorder);
    int16_t thumbH = (trackH * kHelpVisibleLines) / model.helpPageCount;
    if (thumbH < 12) {
      thumbH = 12;
    }
    const int16_t maxOffset = trackH - thumbH;
    const int16_t thumbY =
        trackY + ((maxOffset * model.helpPageIndex) /
                  (model.helpPageCount - kHelpVisibleLines));
    canvas.fillRoundRect(trackX + 1, thumbY + 1, 4, thumbH - 2, 2, kColorCyan);
  }
}

void drawPeerPicker(const UiRenderModel& model) {
  drawPanel(4, kContentY, 232, kContentH, kColorPanelAlt);
  int16_t y = kContentY + 10;
  for (std::size_t i = 0; i < model.activePeerCount && i < config::kMaxPeers; ++i) {
    const auto& peer = model.peers[i];
    if (!peer.valid) {
      continue;
    }
    char label[20];
    snprintf(label, sizeof(label), "%c %.10s", peer.highlighted ? '>' : ' ',
             peer.deviceName);
    uint16_t nameColor = peer.selected ? kColorOrange
                                       : (peer.highlighted ? kColorCyan : kColorWhite);
    if (!peer.compatible) {
      nameColor = kColorDarkGray;
    }
    drawText(12, y, label, nameColor, kColorPanelAlt);

    if (peer.compatible) {
      drawText(104, y, linkQualityText(true, peer.rssi),
               linkQualityColor(true, peer.rssi), kColorPanelAlt);
    } else {
      drawText(104, y, "UPDATE", kColorYellow, kColorPanelAlt);
    }

    char rssiText[8];
    if (peer.rssi <= -120) {
      strncpy(rssiText, "--", sizeof(rssiText) - 1);
    } else {
      snprintf(rssiText, sizeof(rssiText), "%d", static_cast<int>(peer.rssi));
    }
    drawText(152, y, rssiText, peer.compatible ? kColorLightGray : kColorDarkGray,
             kColorPanelAlt);

    char ageText[8];
    snprintf(ageText, sizeof(ageText), "%lus",
             static_cast<unsigned long>(peer.ageMs / 1000));
    drawText(192, y, ageText, peer.compatible ? kColorLightGray : kColorDarkGray,
             kColorPanelAlt);
    y += 12;
  }
  if (model.activePeerCount == 0) {
    drawText(12, kContentY + 20, "Waiting for peers...", kColorLightGray,
             kColorPanelAlt);
  }
}

void drawFooter(const UiRenderModel& model) {
  canvas.fillRoundRect(4, kFooterY, 232, kFooterH, 4, kColorBlue);
  if (model.screenMode == UiScreenMode::kPeerPicker) {
    drawText(8, kFooterY + 4, "^ v MOVE", kColorWhite, kColorBlue);
    drawText(84, kFooterY + 4, "ENTER SELECT", kColorWhite, kColorBlue);
    drawText(182, kFooterY + 4, "FN HELP", kColorWhite, kColorBlue);
    return;
  }
  if (model.screenMode == UiScreenMode::kComposeMessage) {
    drawText(8, kFooterY + 4, "ENTER SEND", kColorWhite, kColorBlue);
    drawText(96, kFooterY + 4, "~/ESC EXIT", kColorWhite, kColorBlue);
    drawText(180, kFooterY + 4, "DEL ERASE", kColorWhite, kColorBlue);
    return;
  }
  if (model.screenMode == UiScreenMode::kHelp) {
    drawText(8, kFooterY + 4, "^ v SCROLL", kColorWhite, kColorBlue);
    drawText(94, kFooterY + 4, "FN CLOSE", kColorWhite, kColorBlue);
    drawText(166, kFooterY + 4, "~/ESC BACK", kColorWhite, kColorBlue);
    return;
  }

  char line[24];
  drawText(8, kFooterY + 4, "FN HELP", kColorWhite, kColorBlue);

  snprintf(line, sizeof(line), "%ukHz",
           static_cast<unsigned>(model.qualitySampleRateHz / 1000U));
  drawText(58, kFooterY + 4, line, kColorWhite, kColorBlue);

  snprintf(line, sizeof(line), "VOC %u%%",
           static_cast<unsigned>(model.voiceVolumePercent));
  drawText(92, kFooterY + 4, line, kColorWhite, kColorBlue);

  snprintf(line, sizeof(line), "MIC %u%%",
           static_cast<unsigned>(model.txGainPercent));
  drawText(144, kFooterY + 4, line, kColorWhite, kColorBlue);

  snprintf(line, sizeof(line), "FX %s",
           model.systemSoundsEnabled ? "ON" : "OFF");
  drawText(202, kFooterY + 4, line, kColorWhite, kColorBlue);
}

}  // namespace

void UiRenderer::begin() {
  M5Cardputer.Display.setTextWrap(false);
  if (!canvasReady) {
    canvas.setColorDepth(16);
    canvas.createSprite(M5Cardputer.Display.width(), M5Cardputer.Display.height());
    canvas.setTextSize(1);
    canvas.setTextWrap(false);
    canvasReady = true;
  }
}

void UiRenderer::render(const UiRenderModel& model, uint32_t nowMs) {
  if (!canvasReady) {
    begin();
  }

  canvas.fillScreen(kColorBlack);
  canvas.setTextSize(1);

  canvas.fillRoundRect(4, kHeaderY, 232, kHeaderH, 6, kColorBlue);
  drawText(10, kHeaderY + 5, model.headerTitle, kColorWhite, kColorBlue);
  drawBatteryIcon(214, kHeaderY + 4, model.batteryPercent);

  switch (model.screenMode) {
    case UiScreenMode::kPeerPicker:
      drawPeerPicker(model);
      break;
    case UiScreenMode::kComm:
      drawVoiceCircle(kLeftPanelX, kContentY, kLeftPanelW, kContentH,
                      model.voiceState, nowMs, model.peerName, model.showLink,
                      model.linkRssi);
      drawChatPreview(model, kRightPanelX, kContentY, kRightPanelW, kContentH);
      break;
    case UiScreenMode::kComposeMessage:
      drawComposeScreen(model);
      break;
    case UiScreenMode::kHelp:
      drawHelpScreen(model);
      break;
  }

  drawFooter(model);
  canvas.pushSprite(0, 0);
}

}  // namespace wt
