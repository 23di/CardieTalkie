#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

#include "AppConfig.h"
#include "AppTypes.h"
#include "BoardProfiles.h"

namespace wt {

class PeerManager {
 public:
  void begin(const uint8_t* localMac);

  void updatePeer(const uint8_t* mac, const char* deviceName,
                  board::BoardVariant boardVariant, uint8_t capabilityFlags,
                  int8_t batteryPercent, int8_t rssi, bool compatible,
                  uint32_t nowMs);
  void touchPeer(const uint8_t* mac, int8_t rssi, uint32_t nowMs);
  void expirePeers(uint32_t nowMs);

  std::size_t activeCount(uint32_t nowMs) const;
  const PeerInfo* visiblePeerAt(std::size_t visibleIndex, uint32_t nowMs) const;
  const PeerInfo* findPeer(const uint8_t* macAddress, uint32_t nowMs) const;
  PeerInfo* mutableSelectedPeer(uint32_t nowMs);
  const PeerInfo* selectedPeer(uint32_t nowMs) const;

  void moveHighlight(int delta, uint32_t nowMs);
  void selectHighlighted(uint32_t nowMs);
  void clearSelection();
  bool selectedPeerLost(uint32_t nowMs) const;
  bool hasSelection() const;
  const uint8_t* selectedMac() const;
  std::size_t highlightedIndex() const;

 private:
  bool isAlive(const PeerInfo& peer, uint32_t nowMs) const;
  bool isSelectable(const PeerInfo& peer, uint32_t nowMs) const;
  bool hasCompatiblePeers(uint32_t nowMs) const;
  int findSlotByMac(const uint8_t* mac) const;
  int findOrAllocateSlot(uint32_t nowMs);
  int visibleToSlot(std::size_t visibleIndex, uint32_t nowMs) const;
  void ensureHighlightValid(uint32_t nowMs);

  PeerInfo peers_[config::kMaxPeers] = {};
  uint8_t localMac_[6] = {};
  uint8_t selectedMac_[6] = {};
  bool hasSelectedMac_ = false;
  std::size_t highlightedVisibleIndex_ = 0;
};

}  // namespace wt
