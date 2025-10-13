#include "owner_registry.h"
#include <QDebug>

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

  switch (ownerId) {
  case 1:
    info.color = {0.20f, 0.55f, 1.00f};
    break;
  case 2:
    info.color = {1.00f, 0.30f, 0.30f};
    break;
  case 3:
    info.color = {0.20f, 0.80f, 0.40f};
    break;
  case 4:
    info.color = {1.00f, 0.80f, 0.20f};
    break;
  default:
    info.color = {0.8f, 0.9f, 1.0f};
    break;
  }

  size_t index = m_owners.size();
  m_owners.push_back(info);
  m_ownerIdToIndex[ownerId] = index;

  return ownerId;
}

void OwnerRegistry::registerOwnerWithId(int ownerId, OwnerType type,
                                        const std::string &name) {
  if (m_ownerIdToIndex.find(ownerId) != m_ownerIdToIndex.end()) {
    return;
  }

  OwnerInfo info;
  info.ownerId = ownerId;
  info.type = type;
  info.name = name.empty() ? ("Owner" + std::to_string(ownerId)) : name;

  switch (ownerId) {
  case 1:
    info.color = {0.20f, 0.55f, 1.00f};
    break;
  case 2:
    info.color = {1.00f, 0.30f, 0.30f};
    break;
  case 3:
    info.color = {0.20f, 0.80f, 0.40f};
    break;
  case 4:
    info.color = {1.00f, 0.80f, 0.20f};
    break;
  default:
    info.color = {0.8f, 0.9f, 1.0f};
    break;
  }

  size_t index = m_owners.size();
  m_owners.push_back(info);
  m_ownerIdToIndex[ownerId] = index;

  if (ownerId >= m_nextOwnerId) {
    m_nextOwnerId = ownerId + 1;
  }
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

void OwnerRegistry::setOwnerTeam(int ownerId, int teamId) {
  auto it = m_ownerIdToIndex.find(ownerId);
  if (it != m_ownerIdToIndex.end()) {
    m_owners[it->second].teamId = teamId;
  }
}

int OwnerRegistry::getOwnerTeam(int ownerId) const {
  auto it = m_ownerIdToIndex.find(ownerId);
  if (it == m_ownerIdToIndex.end())
    return 0;
  return m_owners[it->second].teamId;
}

bool OwnerRegistry::areAllies(int ownerId1, int ownerId2) const {

  if (ownerId1 == ownerId2)
    return true;

  int team1 = getOwnerTeam(ownerId1);
  int team2 = getOwnerTeam(ownerId2);

  bool result = (team1 == team2);

  return result;
}

bool OwnerRegistry::areEnemies(int ownerId1, int ownerId2) const {

  if (ownerId1 == ownerId2)
    return false;

  if (areAllies(ownerId1, ownerId2))
    return false;

  return true;
}

std::vector<int> OwnerRegistry::getAlliesOf(int ownerId) const {
  std::vector<int> result;
  int myTeam = getOwnerTeam(ownerId);

  if (myTeam == 0)
    return result;

  for (const auto &owner : m_owners) {
    if (owner.ownerId != ownerId && owner.teamId == myTeam) {
      result.push_back(owner.ownerId);
    }
  }
  return result;
}

std::vector<int> OwnerRegistry::getEnemiesOf(int ownerId) const {
  std::vector<int> result;

  for (const auto &owner : m_owners) {
    if (areEnemies(ownerId, owner.ownerId)) {
      result.push_back(owner.ownerId);
    }
  }
  return result;
}

void OwnerRegistry::setOwnerColor(int ownerId, float r, float g, float b) {
  auto it = m_ownerIdToIndex.find(ownerId);
  if (it != m_ownerIdToIndex.end()) {
    m_owners[it->second].color = {r, g, b};
  }
}

std::array<float, 3> OwnerRegistry::getOwnerColor(int ownerId) const {
  auto it = m_ownerIdToIndex.find(ownerId);
  if (it != m_ownerIdToIndex.end()) {
    return m_owners[it->second].color;
  }

  return {0.8f, 0.9f, 1.0f};
}

} // namespace Game::Systems
