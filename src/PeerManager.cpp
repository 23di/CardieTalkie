#include "PeerManager.h"

#include <cstring>

#include "MacAddress.h"

namespace wt {

void PeerManager::begin(const uint8_t* localMac) {
  memset(peers_, 0, sizeof(peers_));
  mac::copy(localMac_, localMac);
  memset(selectedMac_, 0, sizeof(selectedMac_));
  hasSelectedMac_ = false;
  highlightedVisibleIndex_ = 0;
}

bool PeerManager::isAlive(const PeerInfo& peer, uint32_t nowMs) const {
  return peer.occupied && (nowMs - peer.lastSeenMs <= config::kPeerTimeoutMs);
}

bool PeerManager::isSelectable(const PeerInfo& peer, uint32_t nowMs) const {
  return isAlive(peer, nowMs) && peer.compatible;
}

bool PeerManager::hasCompatiblePeers(uint32_t nowMs) const {
  for (const auto& peer : peers_) {
    if (isSelectable(peer, nowMs)) {
      return true;
    }
  }
  return false;
}

int PeerManager::findSlotByMac(const uint8_t* macAddress) const {
  for (std::size_t i = 0; i < config::kMaxPeers; ++i) {
    if (peers_[i].occupied && mac::equals(peers_[i].mac, macAddress)) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

int PeerManager::findOrAllocateSlot(uint32_t nowMs) {
  int freeIndex = -1;
  int stalestIndex = 0;
  uint32_t stalestSeenMs = UINT32_MAX;

  for (std::size_t i = 0; i < config::kMaxPeers; ++i) {
    if (!peers_[i].occupied) {
      freeIndex = static_cast<int>(i);
      break;
    }
    if (!isAlive(peers_[i], nowMs)) {
      return static_cast<int>(i);
    }
    if (peers_[i].lastSeenMs < stalestSeenMs) {
      stalestSeenMs = peers_[i].lastSeenMs;
      stalestIndex = static_cast<int>(i);
    }
  }

  return (freeIndex >= 0) ? freeIndex : stalestIndex;
}

void PeerManager::updatePeer(const uint8_t* macAddress, const char* deviceName,
                             board::BoardVariant boardVariant,
                             uint8_t capabilityFlags, int8_t batteryPercent,
                             int8_t rssi, bool compatible, uint32_t nowMs) {
  if (mac::equals(macAddress, localMac_)) {
    return;
  }

  int slot = findSlotByMac(macAddress);
  if (slot < 0) {
    slot = findOrAllocateSlot(nowMs);
  }

  auto& peer = peers_[slot];
  peer.occupied = true;
  peer.compatible = compatible;
  strncpy(peer.deviceName, deviceName, sizeof(peer.deviceName) - 1);
  peer.deviceName[sizeof(peer.deviceName) - 1] = '\0';
  mac::copy(peer.mac, macAddress);
  if (rssi > -127) {
    peer.rssi = rssi;
  }
  peer.lastSeenMs = nowMs;
  peer.boardVariant = boardVariant;
  peer.capabilityFlags = capabilityFlags;
  peer.batteryPercent = batteryPercent;
  ensureHighlightValid(nowMs);
}

void PeerManager::touchPeer(const uint8_t* macAddress, int8_t rssi,
                            uint32_t nowMs) {
  const int slot = findSlotByMac(macAddress);
  if (slot < 0) {
    return;
  }
  peers_[slot].lastSeenMs = nowMs;
  if (rssi > -127) {
    peers_[slot].rssi = rssi;
  }
}

void PeerManager::expirePeers(uint32_t nowMs) {
  for (auto& peer : peers_) {
    if (peer.occupied && !isAlive(peer, nowMs)) {
      peer.compatible = false;
    }
  }
  ensureHighlightValid(nowMs);
}

std::size_t PeerManager::activeCount(uint32_t nowMs) const {
  std::size_t count = 0;
  for (const auto& peer : peers_) {
    if (isAlive(peer, nowMs)) {
      ++count;
    }
  }
  return count;
}

int PeerManager::visibleToSlot(std::size_t visibleIndex, uint32_t nowMs) const {
  std::size_t currentVisible = 0;
  for (std::size_t i = 0; i < config::kMaxPeers; ++i) {
    if (!isAlive(peers_[i], nowMs)) {
      continue;
    }
    if (currentVisible == visibleIndex) {
      return static_cast<int>(i);
    }
    ++currentVisible;
  }
  return -1;
}

const PeerInfo* PeerManager::visiblePeerAt(std::size_t visibleIndex,
                                           uint32_t nowMs) const {
  const int slot = visibleToSlot(visibleIndex, nowMs);
  return (slot >= 0) ? &peers_[slot] : nullptr;
}

const PeerInfo* PeerManager::findPeer(const uint8_t* macAddress,
                                      uint32_t nowMs) const {
  const int slot = findSlotByMac(macAddress);
  if (slot < 0 || !isAlive(peers_[slot], nowMs)) {
    return nullptr;
  }
  return &peers_[slot];
}

PeerInfo* PeerManager::mutableSelectedPeer(uint32_t nowMs) {
  if (!hasSelectedMac_) {
    return nullptr;
  }
  const int slot = findSlotByMac(selectedMac_);
  if (slot < 0 || !isAlive(peers_[slot], nowMs) || !peers_[slot].compatible) {
    return nullptr;
  }
  return &peers_[slot];
}

const PeerInfo* PeerManager::selectedPeer(uint32_t nowMs) const {
  if (!hasSelectedMac_) {
    return nullptr;
  }
  const int slot = findSlotByMac(selectedMac_);
  if (slot < 0 || !isAlive(peers_[slot], nowMs) || !peers_[slot].compatible) {
    return nullptr;
  }
  return &peers_[slot];
}

void PeerManager::ensureHighlightValid(uint32_t nowMs) {
  const std::size_t count = activeCount(nowMs);
  if (count == 0) {
    highlightedVisibleIndex_ = 0;
    return;
  }
  if (highlightedVisibleIndex_ >= count) {
    highlightedVisibleIndex_ = count - 1;
  }

  const int currentSlot = visibleToSlot(highlightedVisibleIndex_, nowMs);
  if (currentSlot >= 0 && peers_[currentSlot].compatible) {
    return;
  }
  if (!hasCompatiblePeers(nowMs)) {
    highlightedVisibleIndex_ = 0;
    return;
  }
  for (std::size_t offset = 0; offset < count; ++offset) {
    const std::size_t candidate = (highlightedVisibleIndex_ + offset) % count;
    const int slot = visibleToSlot(candidate, nowMs);
    if (slot >= 0 && peers_[slot].compatible) {
      highlightedVisibleIndex_ = candidate;
      return;
    }
  }
}

void PeerManager::moveHighlight(int delta, uint32_t nowMs) {
  const std::size_t count = activeCount(nowMs);
  if (count == 0) {
    highlightedVisibleIndex_ = 0;
    return;
  }
  if (!hasCompatiblePeers(nowMs)) {
    highlightedVisibleIndex_ = 0;
    return;
  }

  int next = static_cast<int>(highlightedVisibleIndex_) + delta;
  for (std::size_t attempts = 0; attempts < count; ++attempts) {
    if (next < 0) {
      next = static_cast<int>(count) - 1;
    } else if (next >= static_cast<int>(count)) {
      next = 0;
    }
    const int slot = visibleToSlot(static_cast<std::size_t>(next), nowMs);
    if (slot >= 0 && peers_[slot].compatible) {
      highlightedVisibleIndex_ = static_cast<std::size_t>(next);
      return;
    }
    next += (delta < 0) ? -1 : 1;
  }
}

void PeerManager::selectHighlighted(uint32_t nowMs) {
  const auto* peer = visiblePeerAt(highlightedVisibleIndex_, nowMs);
  if (peer == nullptr || !peer->compatible) {
    hasSelectedMac_ = false;
    memset(selectedMac_, 0, sizeof(selectedMac_));
    return;
  }
  hasSelectedMac_ = true;
  mac::copy(selectedMac_, peer->mac);
}

void PeerManager::clearSelection() {
  hasSelectedMac_ = false;
  memset(selectedMac_, 0, sizeof(selectedMac_));
}

bool PeerManager::selectedPeerLost(uint32_t nowMs) const {
  if (!hasSelectedMac_) {
    return false;
  }
  const int slot = findSlotByMac(selectedMac_);
  if (slot < 0) {
    return true;
  }
  return !isAlive(peers_[slot], nowMs) || !peers_[slot].compatible;
}

bool PeerManager::hasSelection() const {
  return hasSelectedMac_;
}

const uint8_t* PeerManager::selectedMac() const {
  return hasSelectedMac_ ? selectedMac_ : nullptr;
}

std::size_t PeerManager::highlightedIndex() const {
  return highlightedVisibleIndex_;
}

}  // namespace wt
