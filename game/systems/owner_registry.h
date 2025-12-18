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

  auto register_owner(OwnerType type, const std::string &name = "") -> int;

  void register_owner_with_id(int owner_id, OwnerType type,
                              const std::string &name = "");

  void set_local_player_id(int player_id);

  auto get_local_player_id() const -> int;

  auto is_player(int owner_id) const -> bool;

  auto is_ai(int owner_id) const -> bool;

  auto get_owner_type(int owner_id) const -> OwnerType;

  auto get_owner_name(int owner_id) const -> std::string;

  auto get_all_owners() const -> const std::vector<OwnerInfo> &;

  auto get_player_owner_ids() const -> std::vector<int>;

  auto get_ai_owner_ids() const -> std::vector<int>;

  void set_owner_team(int owner_id, int team_id);
  auto get_owner_team(int owner_id) const -> int;
  auto are_allies(int owner_id1, int owner_id2) const -> bool;
  auto are_enemies(int owner_id1, int owner_id2) const -> bool;
  auto get_allies_of(int owner_id) const -> std::vector<int>;
  auto get_enemies_of(int owner_id) const -> std::vector<int>;

  void set_owner_color(int owner_id, float r, float g, float b);
  auto get_owner_color(int owner_id) const -> std::array<float, 3>;

  auto to_json() const -> QJsonObject;
  void from_json(const QJsonObject &json);

private:
  OwnerRegistry() = default;
  ~OwnerRegistry() = default;
  OwnerRegistry(const OwnerRegistry &) = delete;
  auto operator=(const OwnerRegistry &) -> OwnerRegistry & = delete;

  int m_next_owner_id = 1;
  int m_local_player_id = 1;
  std::vector<OwnerInfo> m_owners;
  std::unordered_map<int, size_t> m_owner_id_to_index;
};

} // namespace Game::Systems
