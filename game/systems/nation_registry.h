#pragma once

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../formation/army_formation_types.h"
#include "../units/building_type.h"
#include "../units/troop_type.h"
#include "nation_id.h"
#include "resource_types.h"

namespace Game::Systems {

struct NationTroopVariant {
  Game::Units::TroopType unit_type;
  std::optional<int> health;
  std::optional<int> max_health;
  std::optional<float> speed;
  std::optional<float> vision_range;
  std::optional<int> attack_damage;
  std::optional<float> attack_range;
  std::optional<int> melee_damage;
  std::optional<float> melee_range;
  std::optional<float> attack_cooldown;
  std::optional<float> melee_cooldown;
  std::optional<int> individuals_per_unit;
  std::optional<int> max_units_per_row;
  std::optional<float> selection_ring_size;
  std::optional<float> selection_ring_ground_offset;
  std::optional<float> formation_spacing;
  std::optional<float> render_scale;
  std::optional<Game::Formation::FormationDoctrineId> doctrine;
  std::optional<std::string> renderer_id;
  std::optional<bool> can_ranged;
  std::optional<bool> can_melee;
  std::optional<float> max_stamina;
  std::optional<float> stamina_regen_rate;
  std::optional<float> stamina_depletion_rate;
  std::vector<std::string> abilities;
};

struct DefensiveUnitLayoutProfile {
  std::string id;
  std::string display_name;
  std::vector<Game::Units::TroopType> eligible_troops;

  float form_seconds = 2.5F;
  float break_seconds = 1.2F;

  float move_speed_multiplier = 0.35F;
  float turn_speed_multiplier = 0.45F;
  bool allows_charge = false;

  float frontal_missile_multiplier = 0.2F;
  float frontal_melee_multiplier = 0.7F;
  float flank_multiplier = 1.2F;
  float rear_multiplier = 1.5F;
  float cavalry_impact_multiplier = 1.0F;
  float attack_output_multiplier = 0.55F;

  float frontal_arc_degrees = 100.0F;
  [[nodiscard]] auto is_eligible_troop(Game::Units::TroopType unit_type) const -> bool {
    return std::find(eligible_troops.begin(), eligible_troops.end(), unit_type) !=
           eligible_troops.end();
  }
};

struct TroopType {
  Game::Units::TroopType unit_type;
  std::string display_name;
  bool is_melee = false;
  int cost = 100;
  ResourceAmounts resource_costs{};
  float build_time = 5.0F;
  int priority = 0;
};

struct Nation {
  NationID id;
  std::string display_name;
  std::vector<TroopType> available_troops;
  std::optional<Game::Units::BuildingType> primary_building =
      Game::Units::BuildingType::Barracks;
  Game::Formation::FormationDoctrineId doctrine{"rome"};
  std::optional<DefensiveUnitLayoutProfile> defensive_unit_layout;
  bool playable = true;
  bool has_economy = true;
  std::string ai_profile = "standard";
  bool selectable_in_skirmish = true;
  std::unordered_map<Game::Units::TroopType, NationTroopVariant> troop_variants;

  [[nodiscard]] auto get_melee_troops() const -> std::vector<const TroopType*>;

  [[nodiscard]] auto get_ranged_troops() const -> std::vector<const TroopType*>;

  [[nodiscard]] auto
  get_troop(Game::Units::TroopType unit_type) const -> const TroopType*;

  [[nodiscard]] auto get_best_melee_troop() const -> const TroopType*;
  [[nodiscard]] auto get_best_ranged_troop() const -> const TroopType*;

  [[nodiscard]] auto is_melee_unit(Game::Units::TroopType unit_type) const -> bool;
  [[nodiscard]] auto is_ranged_unit(Game::Units::TroopType unit_type) const -> bool;
};

class NationRegistry {
public:
  NationRegistry() = default;
  ~NationRegistry() = default;
  NationRegistry(const NationRegistry&) = delete;
  NationRegistry(NationRegistry&&) = delete;
  auto operator=(const NationRegistry&) -> NationRegistry& = delete;
  auto operator=(NationRegistry&&) -> NationRegistry& = delete;

  static auto instance() -> NationRegistry&;

  void register_nation(Nation nation);

  auto get_nation(NationID nation_id) const -> const Nation*;

  auto get_nation_for_player(int player_id) const -> const Nation*;

  void set_player_nation(int player_id, NationID nation_id);

  [[nodiscard]] auto
  player_nation_assignments() const -> std::vector<std::pair<int, NationID>>;
  void restore_player_nations(const std::vector<std::pair<int, NationID>>& assignments);

  auto get_all_nations() const -> const std::vector<Nation>& { return m_nations; }

  void initialize_defaults();

  void clear();

  void clear_player_assignments();

  auto default_nation_id() const -> NationID { return m_default_nation; }

private:
  std::vector<Nation> m_nations;
  std::unordered_map<NationID, size_t> m_nation_index;
  std::unordered_map<int, NationID> m_player_nations;
  NationID m_default_nation = NationID::RomanRepublic;
  bool m_initialized = false;
};

} // namespace Game::Systems
