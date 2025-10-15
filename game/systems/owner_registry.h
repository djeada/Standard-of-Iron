#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

namespace Game::Systems {

enum class OwnerType { Player, AI, Neutral };

struct OwnerInfo {
  int ownerId;
  OwnerType type;
  std::string name;
  int teamId = 0;
  std::array<float, 3> color = {0.8f, 0.9f, 1.0f};
};

class OwnerRegistry {
public:
  static OwnerRegistry &instance();

  void clear();

  int registerOwner(OwnerType type, const std::string &name = "");

  void registerOwnerWithId(int ownerId, OwnerType type,
                           const std::string &name = "");

  void setLocalPlayerId(int playerId);

  int getLocalPlayerId() const;

  bool isPlayer(int ownerId) const;

  bool isAI(int ownerId) const;

  OwnerType getOwnerType(int ownerId) const;

  std::string getOwnerName(int ownerId) const;

  const std::vector<OwnerInfo> &getAllOwners() const;

  std::vector<int> getPlayerOwnerIds() const;

  std::vector<int> getAIOwnerIds() const;

  void setOwnerTeam(int ownerId, int teamId);
  int getOwnerTeam(int ownerId) const;
  bool areAllies(int ownerId1, int ownerId2) const;
  bool areEnemies(int ownerId1, int ownerId2) const;
  std::vector<int> getAlliesOf(int ownerId) const;
  std::vector<int> getEnemiesOf(int ownerId) const;

  void setOwnerColor(int ownerId, float r, float g, float b);
  std::array<float, 3> getOwnerColor(int ownerId) const;

  QJsonObject toJson() const;
  void fromJson(const QJsonObject &json);

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
