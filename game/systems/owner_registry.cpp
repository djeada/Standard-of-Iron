#include "owner_registry.h"

namespace Game::Systems {

OwnerRegistry &OwnerRegistry::instance() {
  static OwnerRegistry inst;
  return inst;
}

void OwnerRegistry::clear() {
  m_owners.clear();
  m_ownerIdToIndex.clear();
  m_nextOwnerId = 1;
  m_localPlayerId = 1;
}

int OwnerRegistry::registerOwner(OwnerType type, const std::string &name) {
  int ownerId = m_nextOwnerId++;
  OwnerInfo info;
  info.ownerId = ownerId;
  info.type = type;
  info.name = name.empty() ? ("Owner" + std::to_string(ownerId)) : name;

  size_t index = m_owners.size();
  m_owners.push_back(info);
  m_ownerIdToIndex[ownerId] = index;

  return ownerId;
}

void OwnerRegistry::setLocalPlayerId(int playerId) {
  m_localPlayerId = playerId;
}

int OwnerRegistry::getLocalPlayerId() const { return m_localPlayerId; }

bool OwnerRegistry::isPlayer(int ownerId) const {
  auto it = m_ownerIdToIndex.find(ownerId);
  if (it == m_ownerIdToIndex.end())
    return false;
  return m_owners[it->second].type == OwnerType::Player;
}

bool OwnerRegistry::isAI(int ownerId) const {
  auto it = m_ownerIdToIndex.find(ownerId);
  if (it == m_ownerIdToIndex.end())
    return false;
  return m_owners[it->second].type == OwnerType::AI;
}

OwnerType OwnerRegistry::getOwnerType(int ownerId) const {
  auto it = m_ownerIdToIndex.find(ownerId);
  if (it == m_ownerIdToIndex.end())
    return OwnerType::Neutral;
  return m_owners[it->second].type;
}

std::string OwnerRegistry::getOwnerName(int ownerId) const {
  auto it = m_ownerIdToIndex.find(ownerId);
  if (it == m_ownerIdToIndex.end())
    return "Unknown";
  return m_owners[it->second].name;
}

const std::vector<OwnerInfo> &OwnerRegistry::getAllOwners() const {
  return m_owners;
}

std::vector<int> OwnerRegistry::getPlayerOwnerIds() const {
  std::vector<int> result;
  for (const auto &owner : m_owners) {
    if (owner.type == OwnerType::Player) {
      result.push_back(owner.ownerId);
    }
  }
  return result;
}

std::vector<int> OwnerRegistry::getAIOwnerIds() const {
  std::vector<int> result;
  for (const auto &owner : m_owners) {
    if (owner.type == OwnerType::AI) {
      result.push_back(owner.ownerId);
    }
  }
  return result;
}

} // namespace Game::Systems
