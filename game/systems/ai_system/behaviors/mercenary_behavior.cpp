#include "mercenary_behavior.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "../../nation_registry.h"
#include "../ai_doctrine_catalog.h"
#include "../ai_doctrine_rules.h"
#include "systems/ai_system/ai_types.h"
#include "units/spawn_type.h"
#include "units/troop_type.h"

namespace Game::Systems::AI {

namespace {

constexpr float k_decision_interval = 2.0F;
// Gold the treasury never spends on hiring: next minute's upkeep on top.
constexpr int k_gold_floor = 60;
// The treasury model's horizon: no band whose upkeep would empty it sooner.
constexpr float k_solvency_minutes = 2.0F;
constexpr int k_troop_cap_margin = 12;

struct Treasury {
  int gold = 0;
  int food = 0;
  int income_per_minute = 0;
  int upkeep_per_minute = 0;
};

auto planar(float ax, float az, float bx, float bz) -> float {
  return std::hypot(ax - bx, az - bz);
}

// What the owner's trade houses earn a minute, read from the snapshot with the
// nation's own numbers (the same rule NationEconomySystem pays out).
auto trade_income(const AISnapshot& snapshot, const Nation& nation) -> int {
  const auto& traits = nation.economy;
  if (traits.trade_income_per_minute <= 0) {
    return 0;
  }
  std::vector<const EntitySnapshot*> markets;
  std::vector<const EntitySnapshot*> farms;
  for (const auto& entity : snapshot.friendly_units) {
    if (!entity.is_building || entity.health <= 0) {
      continue;
    }
    if (entity.spawn_type == Game::Units::SpawnType::Marketplace) {
      markets.push_back(&entity);
    } else if (entity.spawn_type == Game::Units::SpawnType::Farm) {
      farms.push_back(&entity);
    }
  }
  std::sort(markets.begin(), markets.end(), [](const auto* a, const auto* b) {
    return a->id < b->id;
  });
  int income = 0;
  const int counted =
      std::min(static_cast<int>(markets.size()), traits.trade_income_max_buildings);
  for (int index = 0; index < counted; ++index) {
    const auto* market = markets[static_cast<std::size_t>(index)];
    income += traits.trade_income_per_minute;
    for (const auto* farm : farms) {
      if (planar(market->pos_x, market->pos_z, farm->pos_x, farm->pos_z) <=
          traits.trade_income_radius) {
        income += traits.trade_income_per_farm;
      }
    }
  }
  return income;
}

auto read_treasury(const AISnapshot& snapshot, const Nation& nation) -> Treasury {
  Treasury treasury;
  treasury.gold = snapshot.resources.get(ResourceType::Gold);
  treasury.food = snapshot.resources.get(ResourceType::Food);
  treasury.income_per_minute = trade_income(snapshot, nation);
  for (const auto& entity : snapshot.friendly_units) {
    if (!entity.is_building && entity.health > 0) {
      treasury.upkeep_per_minute += entity.upkeep_per_minute;
    }
  }
  return treasury;
}

// Would hiring a band at `cost` with `upkeep` a minute leave enough gold to pay
// every band for the next two minutes?
auto stays_solvent(const Treasury& treasury, int cost, int upkeep) -> bool {
  const float after =
      static_cast<float>(treasury.gold - cost) +
      k_solvency_minutes * static_cast<float>(treasury.income_per_minute -
                                              treasury.upkeep_per_minute - upkeep);
  return treasury.gold - cost >= k_gold_floor && after >= 0.0F;
}

auto idle_building(const AISnapshot& snapshot,
                   Game::Units::SpawnType type) -> const EntitySnapshot* {
  for (const auto& entity : snapshot.friendly_units) {
    if (entity.is_building && entity.health > 0 && entity.spawn_type == type &&
        entity.production.has_component && !entity.production.in_progress &&
        entity.production.queue_size == 0) {
      return &entity;
    }
  }
  return nullptr;
}

auto fighting_squads(const AISnapshot& snapshot, int& hired) -> int {
  int squads = 0;
  hired = 0;
  for (const auto& entity : snapshot.friendly_units) {
    if (entity.is_building || entity.health <= 0 || entity.is_commander ||
        !Game::Units::is_combatant_spawn(entity.spawn_type)) {
      continue;
    }
    ++squads;
    if (entity.upkeep_per_minute > 0) {
      ++hired;
    }
  }
  return squads;
}

auto squads_of(const AISnapshot& snapshot, Game::Units::SpawnType type) -> int {
  int count = 0;
  for (const auto& entity : snapshot.friendly_units) {
    if (entity.health > 0 && !entity.is_building && entity.spawn_type == type) {
      ++count;
    }
    if (entity.is_building && entity.production.has_component &&
        entity.production.in_progress &&
        Game::Units::spawn_typeFromTroopType(entity.production.product_type) == type) {
      ++count;
    }
  }
  return count;
}

// The band to hire next: the doctrine's preferred troops first, in turn, then
// every band in turn.
auto choose_band(const Nation& nation,
                 const DoctrineRecruitment* recruitment,
                 int turn) -> const MercenaryBand* {
  std::vector<const MercenaryBand*> order;
  if (recruitment != nullptr) {
    for (const auto& name : recruitment->preferred) {
      const auto troop = Game::Units::try_parse_troop_type(name);
      if (!troop.has_value()) {
        continue;
      }
      for (const auto& band : nation.mercenary_bands) {
        if (band.unit_type == *troop &&
            std::find(order.begin(), order.end(), &band) == order.end()) {
          order.push_back(&band);
        }
      }
    }
  }
  // Every other band takes a turn too, so a preferred list never makes a
  // one-troop army.
  for (const auto& band : nation.mercenary_bands) {
    if (std::find(order.begin(), order.end(), &band) == order.end()) {
      order.push_back(&band);
    }
  }
  if (order.empty()) {
    return nullptr;
  }
  return order[static_cast<std::size_t>(turn) % order.size()];
}

auto set_rally(const EntitySnapshot& building,
               const AIContext& context,
               std::vector<AICommand>& out) -> void {
  const AIBase* main = nullptr;
  for (const auto& base : context.bases) {
    if (base.id == context.main_base_id) {
      main = &base;
    }
  }
  if (main == nullptr) {
    return;
  }
  const float x = context.station.source != StationSource::None ? context.station.x
                                                                : main->rally_x;
  const float z = context.station.source != StationSource::None ? context.station.z
                                                                : main->rally_z;
  if (building.production.rally_set &&
      planar(building.production.rally_x, building.production.rally_z, x, z) < 2.0F) {
    return;
  }
  AICommand rally;
  rally.type = AICommandType::SetRallyPoint;
  rally.building_id = building.id;
  rally.rally_x = x;
  rally.rally_z = z;
  out.push_back(std::move(rally));
}

} // namespace

void MercenaryBehavior::execute(const AISnapshot& snapshot,
                                AIContext& context,
                                float delta_time,
                                std::vector<AICommand>& out_commands) {
  m_timer += delta_time;
  if (m_timer < k_decision_interval) {
    return;
  }
  m_timer = 0.0F;
  const auto* nation = context.nation;
  if (nation == nullptr || !snapshot.has_resource_snapshot) {
    return;
  }
  if (snapshot.troop_count >= 0 && snapshot.max_troops_per_player > 0 &&
      snapshot.troop_count + k_troop_cap_margin > snapshot.max_troops_per_player) {
    return;
  }

  Treasury treasury = read_treasury(snapshot, *nation);
  const auto* doctrine = context.strategy_config.doctrine;
  const DoctrineRecruitment* recruitment =
      doctrine != nullptr ? &doctrine->recruitment : nullptr;

  // Elephants from the stables, for a commander who wants them or a treasury
  // that cannot spend its gold otherwise.
  if (const auto* stables =
          idle_building(snapshot, Game::Units::SpawnType::ElephantStables)) {
    set_rally(*stables, context, out_commands);
    const auto* elephant = nation->get_troop(Game::Units::TroopType::Elephant);
    const bool wanted =
        recruitment != nullptr &&
        std::find(recruitment->preferred.begin(),
                  recruitment->preferred.end(),
                  "elephant") != recruitment->preferred.end();
    if (elephant != nullptr &&
        (elephant->max_squads <= 0 ||
         squads_of(snapshot, Game::Units::SpawnType::Elephant) < elephant->max_squads)) {
      const int gold_cost = elephant->resource_costs.get(ResourceType::Gold);
      const int food_cost = elephant->resource_costs.get(ResourceType::Food);
      constexpr int k_rich = 600;
      if ((wanted || treasury.gold >= k_rich) && treasury.food >= food_cost + 40 &&
          stays_solvent(treasury, gold_cost, 0)) {
        AICommand hire;
        hire.type = AICommandType::StartProduction;
        hire.building_id = stables->id;
        hire.product_type = Game::Units::TroopType::Elephant;
        out_commands.push_back(std::move(hire));
        treasury.gold -= gold_cost;
      }
    }
  }

  const auto* camp = idle_building(snapshot, Game::Units::SpawnType::MercenaryCamp);
  if (camp == nullptr || nation->mercenary_bands.empty()) {
    return;
  }
  set_rally(*camp, context, out_commands);

  int hired = 0;
  const int squads = fighting_squads(snapshot, hired);
  const float share = recruitment != nullptr ? recruitment->mercenary_share : 0.3F;
  constexpr int k_too_small_to_shape = 4;
  const bool under_share =
      squads < k_too_small_to_shape ||
      static_cast<float>(hired) < share * static_cast<float>(squads + 1);
  // Below the doctrine's share the camp hires whenever the treasury allows.
  // Above it, gold is banked and spent in a burst: while a wave gathers, when
  // the town is threatened, or when the treasury holds three bands' worth.
  const MercenaryBand* band = choose_band(*nation, recruitment, m_hires);
  if (band == nullptr) {
    return;
  }
  const bool burst = context.wave.assembling || context.barracks_under_threat ||
                     context.state == AIState::Defending ||
                     treasury.gold >= 3 * band->gold + k_gold_floor;
  if (!under_share && !burst) {
    return;
  }
  if (!stays_solvent(treasury, band->gold, band->upkeep_per_minute)) {
    return;
  }
  AICommand hire;
  hire.type = AICommandType::StartProduction;
  hire.building_id = camp->id;
  hire.product_type = band->unit_type;
  hire.band_nation = band->nation;
  out_commands.push_back(std::move(hire));
  ++m_hires;
}

auto MercenaryBehavior::should_execute(const AISnapshot& snapshot,
                                       const AIContext& context) const -> bool {
  (void)snapshot;
  if (is_no_economy_nation(context) || context.nation == nullptr) {
    return false;
  }
  return !context.nation->mercenary_bands.empty() ||
         std::find(context.nation->unique_buildings.begin(),
                   context.nation->unique_buildings.end(),
                   "elephant_stables") != context.nation->unique_buildings.end();
}

} // namespace Game::Systems::AI
