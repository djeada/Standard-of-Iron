#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

namespace Game::Systems {

enum class OwnerType { Player, AI, Neutral };

namespace Defaults {
inline constexpr std::array<float, 3> kDefaultOwnerColor{0.8F, 0.9F, 1.0F};
}

struct OwnerInfo {
  int owner_id{};
  OwnerType type;
  std::string name;
  int team_id = 0;
  std::array<float, 3> color = Defaults::kDefaultOwnerColor;
};

class OwnerRegistry {
public:
  static auto instance() -> OwnerRegistry &;

  void clear();

  auto registerOwner(OwnerType type, const std::string &name = "") -> int;

  void registerOwnerWithId(int owner_id, OwnerType type,
                           const std::string &name = "");

  void setLocalPlayerId(int player_id);

  auto getLocalPlayerId() const -> int;

  auto isPlayer(int owner_id) const -> bool;

  auto isAI(int owner_id) const -> bool;

  auto getOwnerType(int owner_id) const -> OwnerType;

  auto getOwnerName(int owner_id) const -> std::string;

  auto getAllOwners() const -> const std::vector<OwnerInfo> &;

  auto getPlayerOwnerIds() const -> std::vector<int>;

  auto getAIOwnerIds() const -> std::vector<int>;

  void setOwnerTeam(int owner_id, int team_id);
  auto getOwnerTeam(int owner_id) const -> int;
  auto areAllies(int owner_id1, int owner_id2) const -> bool;
  auto areEnemies(int owner_id1, int owner_id2) const -> bool;
  auto getAlliesOf(int owner_id) const -> std::vector<int>;
  auto getEnemiesOf(int owner_id) const -> std::vector<int>;

  void setOwnerColor(int owner_id, float r, float g, float b);
  auto getOwnerColor(int owner_id) const -> std::array<float, 3>;

  auto toJson() const -> QJsonObject;
  void fromJson(const QJsonObject &json);

private:
  OwnerRegistry() = default;
  ~OwnerRegistry() = default;
  OwnerRegistry(const OwnerRegistry &) = delete;
  auto operator=(const OwnerRegistry &) -> OwnerRegistry & = delete;

  int m_nextOwnerId = 1;
  int m_localPlayerId = 1;
  std::vector<OwnerInfo> m_owners;
  std::unordered_map<int, size_t> m_owner_idToIndex;
};

} // namespace Game::Systems
