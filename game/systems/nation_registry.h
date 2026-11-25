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
#include "../units/spawn_type.h"
#include "../units/troop_type.h"
#include "nation_id.h"
#include "recruitment_rules.h"
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
  std::optional<std::string> lore_history;
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

  // false: the nation still fields this troop from authored spawns (missions,
  // map retinues) but no building of its recruits it (NATION_ASYMMETRY.md
  // decision 5 roster cuts; Carthage's hired troops come as mercenary bands).
  bool recruitable = true;
  // The building that recruits it for this nation, when that is not the
  // default rule (recruiting_building_for): Carthage's elephants come from
  // the Elephant Stables.
  std::optional<Game::Units::SpawnType> recruited_at;
  // Living squads of this type an owner may field at once; 0 is no limit
  // (Roman equites 4, Carthaginian elephants 8).
  int max_squads = 0;
  // A building the owner must have standing to recruit it (Roman triarii
  // need a Fabrica).
  std::optional<Game::Units::SpawnType> requires_building;
  // A standing Fabrica cuts this troop's iron to the nation's
  // fabrica_iron_multiplier (Roman infantry).
  bool fabrica_discount = false;
};

// A band hired for gold at a Mercenary Camp. The squad keeps its owner but
// carries the band's nation, so it renders and fights with that nation's
// profile (NATION_ASYMMETRY.md section 4.2).
struct MercenaryBand {
  std::string id;
  std::string display_name;
  Game::Units::TroopType unit_type = Game::Units::TroopType::Swordsman;
  NationID nation = NationID::Carthage;
  int gold = 100;
  float contract_seconds = 8.0F;
  int upkeep_per_minute = 5;
};

// How a nation's economy differs; every number defaults to "no such feature".
struct NationEconomyTraits {
  // Men one delivered civilian adds to a barracks reserve.
  int civilian_reserve_grant = 18;

  // Dilectus: every interval, each barracks gains men per standing home near it.
  float levy_interval_seconds = 0.0F;
  int levy_men_per_home = 0;
  float levy_radius = 40.0F;

  // Share of a squad's men that returns to the nearest barracks when it is
  // destroyed in the field.
  float loss_refund_share = 0.0F;

  // Emporion: what each standing marketplace earns a minute, and per farm near it.
  int trade_income_per_minute = 0;
  int trade_income_per_farm = 0;
  float trade_income_radius = 40.0F;
  int trade_income_max_buildings = 3;
  float trade_sell_multiplier = 1.0F;

  // Fabrica: the iron price of discounted troops while one stands.
  float fabrica_iron_multiplier = 1.0F;

  // Castra: troops resting inside heal this share of their health a second.
  float camp_heal_per_second = 0.0F;
  float camp_heal_radius = 10.0F;

  // Veterancy: squads of this troop that come through battles alive rise in
  // rank (Roman hastati become principes, then veterans).
  std::optional<Game::Units::TroopType> veteran_troop;
  int battles_for_first_rank = 2;
  int battles_for_second_rank = 4;
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
  std::vector<MercenaryBand> mercenary_bands;
  NationEconomyTraits economy;
  // The nation-only buildings it may raise ("mercenary_camp", "fabrica", ...).
  std::vector<std::string> unique_buildings;

  [[nodiscard]] auto find_band(Game::Units::TroopType unit_type,
                               NationID band_nation) const -> const MercenaryBand*;
  [[nodiscard]] auto
  find_band(const std::string& band_id) const -> const MercenaryBand*;
  // The building that recruits `unit_type` for this nation.
  [[nodiscard]] auto
  recruiting_building(Game::Units::TroopType unit_type) const -> Game::Units::SpawnType;

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

  void register_default_nations();
  [[nodiscard]] auto initialized() const -> bool { return m_initialized; }

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
