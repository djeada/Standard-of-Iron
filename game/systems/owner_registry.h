#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace Game::Systems {

enum class OwnerType { Player, AI, Neutral };

struct OwnerInfo {
  int ownerId;
  OwnerType type;
  std::string name;
};

class OwnerRegistry {
public:
  static OwnerRegistry &instance();

  void clear();

  int registerOwner(OwnerType type, const std::string &name = "");

  void setLocalPlayerId(int playerId);

  int getLocalPlayerId() const;

  bool isPlayer(int ownerId) const;

  bool isAI(int ownerId) const;

  OwnerType getOwnerType(int ownerId) const;

  std::string getOwnerName(int ownerId) const;

  const std::vector<OwnerInfo> &getAllOwners() const;

  std::vector<int> getPlayerOwnerIds() const;

  std::vector<int> getAIOwnerIds() const;

private:
  OwnerRegistry() = default;
  ~OwnerRegistry() = default;
  OwnerRegistry(const OwnerRegistry &) = delete;
  OwnerRegistry &operator=(const OwnerRegistry &) = delete;

  int m_nextOwnerId = 1;
  int m_localPlayerId = 1;
  std::vector<OwnerInfo> m_owners;
  std::unordered_map<int, size_t> m_ownerIdToIndex;
};

} // namespace Game::Systems
