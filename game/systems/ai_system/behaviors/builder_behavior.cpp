#include "builder_behavior.h"

#include <QDebug>
#include <QVector2D>

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../../../map/terrain_service.h"
#include "../../building_collision_registry.h"
#include "../../construction_cost_catalog.h"
#include "../../nation_registry.h"
#include "../ai_base_manager.h"
#include "../ai_doctrine_catalog.h"
#include "../ai_utils.h"
#include "systems/ai_system/ai_types.h"
#include "units/spawn_type.h"

namespace Game::Systems::AI {

namespace {

constexpr const char* BUILDING_TYPE_HOME = "home";
constexpr const char* BUILDING_TYPE_DEFENSE_TOWER = "defense_tower";
constexpr const char* BUILDING_TYPE_WALL_SEGMENT = "wall_segment";
constexpr const char* BUILDING_TYPE_BARRACKS = "barracks";
constexpr const char* BUILDING_TYPE_MARKETPLACE = "marketplace";

constexpr int k_treasury_worth_a_market = 120;
constexpr const char* BUILDING_TYPE_CATAPULT = "catapult";
constexpr const char* BUILDING_TYPE_BALLISTA = "ballista";
constexpr const char* BUILDING_TYPE_FARM = "farm";
constexpr const char* HARVEST_TREE = "cut_tree";
constexpr const char* HARVEST_STONE = "collect_stone";
constexpr const char* HARVEST_IRON = "collect_iron_ore";
constexpr const char* HARVEST_GRAIN = "harvest_grain";

constexpr int MAX_HOMES = 20;
constexpr int MAX_DEFENSE_TOWERS = 10;
constexpr int MAX_WALL_SEGMENTS = 40;
constexpr int MAX_BARRACKS = 6;
constexpr int MAX_FARMS = 8;
constexpr int MAX_MARKETPLACES = 2;
constexpr int MAX_CATAPULTS = 5;

constexpr float MAP_EDGE_PADDING = 5.0F;

constexpr int k_recruit_wood_reserve = 80;
constexpr int k_recruit_iron_reserve = 50;

void clamp_to_map_bounds(const AISnapshot& snapshot, float& x, float& z) {
  if (!snapshot.has_map_bounds) {
    return;
  }

  const float min_x = snapshot.map_min_x + MAP_EDGE_PADDING;
  const float max_x = snapshot.map_max_x - MAP_EDGE_PADDING;
  const float min_z = snapshot.map_min_z + MAP_EDGE_PADDING;
  const float max_z = snapshot.map_max_z - MAP_EDGE_PADDING;

  x = std::clamp(x, min_x, max_x);
  z = std::clamp(z, min_z, max_z);
}

struct BuildCandidate {
  const char* type = nullptr;
  int current = 0;
  int target = 0;
};

auto unmet_candidates(std::initializer_list<BuildCandidate> candidates)
    -> std::vector<const char*> {
  std::vector<std::pair<float, const char*>> wanted;
  for (const auto& candidate : candidates) {
    if (candidate.type == nullptr || candidate.target <= candidate.current) {
      continue;
    }

    const float completion = static_cast<float>(candidate.current) /
                             static_cast<float>(std::max(1, candidate.target));
    wanted.emplace_back(completion, candidate.type);
  }
  std::stable_sort(wanted.begin(), wanted.end(), [](const auto& a, const auto& b) {
    return a.first < b.first;
  });

  std::vector<const char*> order;
  order.reserve(wanted.size());
  for (const auto& entry : wanted) {
    order.push_back(entry.second);
  }
  return order;
}

auto needs_outpost_construction(const AIContext& context) -> bool {
  if (!context.has_expansion_site) {
    return false;
  }

  if (context.outpost_barracks_count <
      context.strategy_config.desired_outpost_barracks_count) {
    return true;
  }

  return context.outpost_barracks_count >=
             context.strategy_config.desired_outpost_barracks_count &&
         context.outpost_home_count < context.strategy_config.outpost_home_target;
}

auto recent_outpost_order(const AISnapshot& snapshot,
                          const AIContext& context) -> bool {
  return (snapshot.game_time - context.last_expansion_order_time) < 4.0F;
}

auto exposed_secondary_base(const AIContext& context) -> const AIBase* {
  for (const auto& base : context.bases) {
    if (base.role == BaseRole::Main || base.defense_tower_count > 0) {
      continue;
    }
    if (base.under_threat) {
      return &base;
    }
  }
  return nullptr;
}

auto select_best_builder(const AISnapshot& snapshot,
                         const std::vector<Engine::Core::EntityID>& available_builders,
                         float target_x,
                         float target_z) -> Engine::Core::EntityID {
  Engine::Core::EntityID best_id = available_builders.front();
  float best_distance_sq = std::numeric_limits<float>::infinity();

  for (auto builder_id : available_builders) {
    auto it = std::find_if(
        snapshot.friendly_units.begin(),
        snapshot.friendly_units.end(),
        [builder_id](const EntitySnapshot& entity) { return entity.id == builder_id; });
    if (it == snapshot.friendly_units.end()) {
      continue;
    }

    const float distance_sq = (it->pos_x - target_x) * (it->pos_x - target_x) +
                              (it->pos_z - target_z) * (it->pos_z - target_z);
    if (distance_sq < best_distance_sq) {
      best_distance_sq = distance_sq;
      best_id = builder_id;
    }
  }

  return best_id;
}

auto select_strongest_builder(
    const AISnapshot& snapshot,
    const std::vector<Engine::Core::EntityID>& available_builders,
    float target_x,
    float target_z) -> Engine::Core::EntityID {
  Engine::Core::EntityID best_id = available_builders.front();
  int best_strength = -1;
  float best_distance_sq = std::numeric_limits<float>::infinity();

  for (const auto builder_id : available_builders) {
    const auto it = std::find_if(
        snapshot.friendly_units.begin(),
        snapshot.friendly_units.end(),
        [builder_id](const EntitySnapshot& entity) { return entity.id == builder_id; });
    if (it == snapshot.friendly_units.end()) {
      continue;
    }
    const float distance_sq = (it->pos_x - target_x) * (it->pos_x - target_x) +
                              (it->pos_z - target_z) * (it->pos_z - target_z);
    if (it->squad_strength > best_strength ||
        (it->squad_strength == best_strength && distance_sq < best_distance_sq)) {
      best_strength = it->squad_strength;
      best_distance_sq = distance_sq;
      best_id = builder_id;
    }
  }

  return best_id;
}

auto harvest_type_for_resource(ResourceType resource) -> const char* {
  switch (resource) {
  case ResourceType::Wood:
    return HARVEST_TREE;
  case ResourceType::Stone:
    return HARVEST_STONE;
  case ResourceType::Iron:
    return HARVEST_IRON;
  default:
    return nullptr;
  }
}

constexpr int k_ai_food_reserve = 60;

constexpr int k_ai_granary_target = 900;

constexpr int k_ai_larder_target = 320;

constexpr int k_repair_health_fraction_numerator = 9;
constexpr int k_repair_health_fraction_denominator = 10;
constexpr int k_max_repair_crews = 2;

constexpr int k_smallest_useful_work_party = 4;

auto stockpile_target(ResourceType type, int building_count) -> int {
  const int town = std::clamp(building_count, 1, 24);
  switch (type) {
  case ResourceType::Wood:
    return 240 + town * 20;
  case ResourceType::Stone:
    return 160 + town * 14;
  case ResourceType::Iron:
    return 120 + town * 10;
  default:
    return 0;
  }
}

auto starved_of_food(const AISnapshot& snapshot) -> bool {
  return snapshot.has_resource_snapshot &&
         snapshot.resources.get(ResourceType::Food) < k_ai_food_reserve;
}

auto wants_repair(const EntitySnapshot& entity) -> bool {
  return entity.is_building && entity.health > 0 && entity.max_health > 0 &&
         entity.health * k_repair_health_fraction_denominator <
             entity.max_health * k_repair_health_fraction_numerator;
}

auto field_is_worked(const AISnapshot& snapshot,
                     Engine::Core::EntityID field_id) -> bool {
  for (const auto& entity : snapshot.friendly_units) {
    if (entity.builder_production.has_component &&
        entity.builder_production.task_target_id == field_id) {
      return true;
    }
  }
  return false;
}

auto granary_has_room(const AISnapshot& snapshot) -> bool {
  return snapshot.has_resource_snapshot &&
         snapshot.resources.get(ResourceType::Food) < k_ai_granary_target;
}

auto recruit_reserve_shortfall(const AISnapshot& snapshot) -> ResourceType {
  if (!snapshot.has_resource_snapshot) {
    return ResourceType::Count;
  }
  const int wood_deficit =
      k_recruit_wood_reserve - snapshot.resources.get(ResourceType::Wood);
  const int iron_deficit =
      k_recruit_iron_reserve - snapshot.resources.get(ResourceType::Iron);
  if (wood_deficit <= 0 && iron_deficit <= 0) {
    return ResourceType::Count;
  }
  return iron_deficit > wood_deficit ? ResourceType::Iron : ResourceType::Wood;
}

auto node_matches_resource(const ResourceNodeSnapshot& node,
                           ResourceType resource) -> bool {
  switch (resource) {
  case ResourceType::Wood:
    return Game::Map::is_tree_world_prop_type(node.type);
  case ResourceType::Stone:
    return Game::Map::is_boulder_world_prop_type(node.type);
  case ResourceType::Iron:
    return Game::Map::is_iron_ore_world_prop_type(node.type);
  default:
    return false;
  }
}

auto building_type_name(const std::string& name) -> const char* {
  if (name == BUILDING_TYPE_HOME) {
    return BUILDING_TYPE_HOME;
  }
  if (name == BUILDING_TYPE_DEFENSE_TOWER) {
    return BUILDING_TYPE_DEFENSE_TOWER;
  }
  if (name == BUILDING_TYPE_WALL_SEGMENT) {
    return BUILDING_TYPE_WALL_SEGMENT;
  }
  if (name == BUILDING_TYPE_BARRACKS) {
    return BUILDING_TYPE_BARRACKS;
  }
  if (name == BUILDING_TYPE_MARKETPLACE) {
    return BUILDING_TYPE_MARKETPLACE;
  }
  if (name == BUILDING_TYPE_CATAPULT) {
    return BUILDING_TYPE_CATAPULT;
  }
  if (name == BUILDING_TYPE_BALLISTA) {
    return BUILDING_TYPE_BALLISTA;
  }
  if (name == BUILDING_TYPE_FARM) {
    return BUILDING_TYPE_FARM;
  }
  return nullptr;
}

auto preferred_siege_engine(const AIContext& context) -> const char* {
  const auto* doctrine = context.strategy_config.doctrine;
  if (doctrine == nullptr) {
    return BUILDING_TYPE_CATAPULT;
  }
  for (const auto& name : doctrine->recruitment.preferred) {
    if (name == BUILDING_TYPE_BALLISTA) {
      return BUILDING_TYPE_BALLISTA;
    }
    if (name == BUILDING_TYPE_CATAPULT) {
      return BUILDING_TYPE_CATAPULT;
    }
  }
  return BUILDING_TYPE_CATAPULT;
}

auto settlement_facing(const AIContext& context,
                       const AISnapshot& snapshot) -> QVector2D {
  float sum_x = 0.0F;
  float sum_z = 0.0F;
  int count = 0;
  for (const auto& objective : snapshot.strategic_objectives) {
    if (objective.health <= 0 || !objective.is_building) {
      continue;
    }
    sum_x += objective.pos_x;
    sum_z += objective.pos_z;
    count++;
  }
  if (count == 0) {
    for (const auto& contact : snapshot.visible_enemies) {
      if (contact.health <= 0) {
        continue;
      }
      const float weight = contact.is_building ? 4.0F : 1.0F;
      sum_x += contact.pos_x * weight;
      sum_z += contact.pos_z * weight;
      count += static_cast<int>(weight);
    }
  }
  if (count == 0) {
    return {0.0F, -1.0F};
  }

  const float dx = (sum_x / static_cast<float>(count)) - context.base_pos_x;
  const float dz = (sum_z / static_cast<float>(count)) - context.base_pos_z;
  const float length = std::sqrt(std::max(0.0F, dx * dx + dz * dz));
  if (length < 1.0F) {
    return {0.0F, -1.0F};
  }

  if (std::abs(dx) >= std::abs(dz)) {
    return {dx > 0.0F ? -1.0F : 1.0F, 0.0F};
  }
  return {0.0F, dz > 0.0F ? -1.0F : 1.0F};
}

auto plan_offset_to_world(const QVector2D& facing,
                          float local_x,
                          float local_z) -> QVector3D {
  const QVector2D forward = facing;
  const QVector2D right(forward.y(), -forward.x());
  return QVector3D(local_z * forward.x() + local_x * right.x(),
                   0.0F,
                   local_z * forward.y() + local_x * right.y());
}

auto plan_rotation_to_world(const QVector2D& facing, float local_rotation) -> float {
  constexpr float k_rad_to_deg = 180.0F / 3.14159265358979323846F;
  const float frame_yaw = std::atan2(facing.x(), facing.y()) * k_rad_to_deg;
  float yaw = std::fmod(frame_yaw + local_rotation, 360.0F);
  if (yaw < 0.0F) {
    yaw += 360.0F;
  }
  return yaw;
}

auto locked_settlement_facing(const AIContext& context,
                              const AISnapshot& snapshot) -> QVector2D {
  if (context.settlement_facing_locked) {
    return {context.settlement_facing_x, context.settlement_facing_z};
  }
  return settlement_facing(context, snapshot);
}

auto slot_clearance(const char* building_type,
                    Game::Units::SpawnType standing) -> float {
  if (building_type == BUILDING_TYPE_WALL_SEGMENT &&
      Game::Units::is_wall_network_spawn(standing)) {

    return 0.5F * k_wall_link_spacing;
  }
  const auto half_extent = [](const std::string& type) {
    const auto size = BuildingCollisionRegistry::get_building_size(type);
    return 0.5F * std::max(size.width, size.depth);
  };
  constexpr float k_slot_gap = 0.2F;
  return half_extent(std::string(building_type)) +
         half_extent(Game::Units::spawn_typeToString(standing)) + k_slot_gap;
}

struct SettlementCensus {
  int homes = 0;
  int barracks = 0;
  int towers = 0;
  int walls = 0;
  int markets = 0;
  int farms = 0;
};

using SettlementTargets = SettlementCensus;

[[nodiscard]] auto plan_step_is_already_met(const SettlementCensus& standing,
                                            const SettlementTargets& targets,
                                            const char* building) -> bool {
  if (building == BUILDING_TYPE_HOME) {
    return standing.homes >= targets.homes;
  }
  if (building == BUILDING_TYPE_BARRACKS) {
    return standing.barracks >= targets.barracks;
  }
  if (building == BUILDING_TYPE_DEFENSE_TOWER) {
    return standing.towers >= targets.towers;
  }
  if (building == BUILDING_TYPE_WALL_SEGMENT) {
    return standing.walls >= targets.walls;
  }
  if (building == BUILDING_TYPE_MARKETPLACE) {
    return standing.markets >= targets.markets;
  }
  if (building == BUILDING_TYPE_FARM) {
    return standing.farms >= targets.farms;
  }
  return false;
}

struct PlanStepChoice {
  const char* building = nullptr;
  QVector3D offset;
  float rotation_y = 0.0F;
  int slot = -1;
};

auto authored_plan_step(const AIContext& context,
                        const AISnapshot& snapshot,
                        const SettlementCensus& standing,
                        const SettlementTargets& targets,
                        const char* preferred,
                        const std::vector<int>& blocked_slots,
                        PlanStepChoice& out_choice) -> bool {
  const auto* doctrine = context.strategy_config.doctrine;
  if (doctrine == nullptr || doctrine->town_plan == nullptr) {
    return false;
  }

  constexpr float k_anchor_clearance = 9.0F;
  constexpr float k_anchor_clearance_sq = k_anchor_clearance * k_anchor_clearance;

  const QVector2D facing = locked_settlement_facing(context, snapshot);

  const bool fields_come_before_walls = standing.farms < 1 || standing.homes < 2;

  PlanStepChoice fallback;

  int engines_fielded = 0;
  for (const auto& entity : snapshot.friendly_units) {
    if (entity.spawn_type == Game::Units::SpawnType::Catapult ||
        entity.spawn_type == Game::Units::SpawnType::Ballista) {
      ++engines_fielded;
    }
  }
  int engine_steps_seen = 0;

  int slot = -1;
  for (const auto& step : doctrine->town_plan->steps) {
    ++slot;
    const char* resolved = building_type_name(step.building);
    if (resolved == nullptr) {
      continue;
    }
    if (std::find(blocked_slots.begin(), blocked_slots.end(), slot) !=
        blocked_slots.end()) {

      continue;
    }

    if (resolved == BUILDING_TYPE_CATAPULT || resolved == BUILDING_TYPE_BALLISTA) {

      ++engine_steps_seen;
      if (engines_fielded >= engine_steps_seen) {
        continue;
      }
    } else if (plan_step_is_already_met(standing, targets, resolved)) {

      continue;
    }
    if (resolved == BUILDING_TYPE_WALL_SEGMENT && fields_come_before_walls) {

      continue;
    }

    const QVector3D offset = plan_offset_to_world(facing, step.x, step.z);
    const float world_x = context.base_pos_x + offset.x();
    const float world_z = context.base_pos_z + offset.z();

    if ((offset.x() * offset.x() + offset.z() * offset.z()) < k_anchor_clearance_sq) {
      continue;
    }

    bool occupied = false;
    for (const auto& entity : snapshot.friendly_units) {
      if (entity.is_building) {
        const float clearance = slot_clearance(resolved, entity.spawn_type);
        if (distance_squared(
                entity.pos_x, 0.0F, entity.pos_z, world_x, 0.0F, world_z) <=
            clearance * clearance) {
          occupied = true;
          break;
        }
        continue;
      }
      const auto& raising = entity.builder_production;
      if (!raising.raising_a_building || !raising.has_construction_site) {
        continue;
      }

      const float clearance = slot_clearance(resolved, raising.building_under_way);
      if (distance_squared(raising.construction_site_x,
                           0.0F,
                           raising.construction_site_z,
                           world_x,
                           0.0F,
                           world_z) <= clearance * clearance) {
        occupied = true;
        break;
      }
    }
    if (occupied) {
      continue;
    }

    const float rotation_y = plan_rotation_to_world(facing, step.rotation);
    if (preferred != nullptr && resolved != preferred) {

      if (fallback.building == nullptr) {
        fallback = {resolved, offset, rotation_y, slot};
      }
      continue;
    }

    out_choice = {resolved, offset, rotation_y, slot};
    return true;
  }

  if (fallback.building == nullptr) {
    return false;
  }

  out_choice = fallback;
  return true;
}

auto site_is_free(const AISnapshot& snapshot,
                  const char* building_type,
                  float world_x,
                  float world_z) -> bool {
  for (const auto& entity : snapshot.friendly_units) {
    if (!entity.is_building) {
      continue;
    }
    const float clearance = slot_clearance(building_type, entity.spawn_type);
    if (distance_squared(entity.pos_x, 0.0F, entity.pos_z, world_x, 0.0F, world_z) <=
        clearance * clearance) {
      return false;
    }
  }
  return true;
}

auto expanding_ring_offset(int index,
                           int per_ring,
                           float first_radius,
                           float radius_step) -> QVector3D {
  const int ring = index / std::max(1, per_ring);
  const int step = index % std::max(1, per_ring);
  const float radius = first_radius + static_cast<float>(ring) * radius_step;
  const float angle = (6.2831853F * static_cast<float>(step) /
                       static_cast<float>(std::max(1, per_ring))) +
                      (static_cast<float>(ring) * 0.4F);
  return {radius * std::cos(angle), 0.0F, radius * std::sin(angle)};
}

auto planned_settlement_offset(const AIContext& context,
                               const char* building_type,
                               int construction_index) -> QVector3D {
  const bool carthaginian =
      context.nation != nullptr && context.nation->id == NationID::Carthage;
  if (building_type == BUILDING_TYPE_DEFENSE_TOWER) {
    static const std::array<QVector3D, 4> roman = {QVector3D{-8.0F, 0.0F, -8.0F},
                                                   QVector3D{8.0F, 0.0F, -8.0F},
                                                   QVector3D{8.0F, 0.0F, 8.0F},
                                                   QVector3D{-8.0F, 0.0F, 8.0F}};
    static const std::array<QVector3D, 4> punic = {QVector3D{-8.0F, 0.0F, -8.0F},
                                                   QVector3D{8.0F, 0.0F, -8.0F},
                                                   QVector3D{-8.0F, 0.0F, 8.0F},
                                                   QVector3D{8.0F, 0.0F, 8.0F}};
    const auto& offsets = carthaginian ? punic : roman;
    return offsets[static_cast<std::size_t>(construction_index) % offsets.size()];
  }
  if (building_type == BUILDING_TYPE_HOME) {
    static const std::array<QVector3D, 6> roman = {QVector3D{-8.0F, 0.0F, 5.0F},
                                                   QVector3D{-4.0F, 0.0F, 5.0F},
                                                   QVector3D{4.0F, 0.0F, 5.0F},
                                                   QVector3D{8.0F, 0.0F, 5.0F},
                                                   QVector3D{-8.0F, 0.0F, -5.0F},
                                                   QVector3D{8.0F, 0.0F, -5.0F}};
    static const std::array<QVector3D, 6> punic = {QVector3D{-6.0F, 0.0F, 4.0F},
                                                   QVector3D{-2.0F, 0.0F, 6.0F},
                                                   QVector3D{2.0F, 0.0F, 6.0F},
                                                   QVector3D{6.0F, 0.0F, 4.0F},
                                                   QVector3D{-5.0F, 0.0F, -5.0F},
                                                   QVector3D{5.0F, 0.0F, -5.0F}};
    const auto& offsets = carthaginian ? punic : roman;
    if (construction_index < static_cast<int>(offsets.size())) {
      return offsets[static_cast<std::size_t>(construction_index)];
    }
    return expanding_ring_offset(
        construction_index - static_cast<int>(offsets.size()), 7, 12.0F, 4.0F);
  }
  if (building_type == BUILDING_TYPE_FARM) {

    static const std::array<QVector3D, 6> fields = {QVector3D{-18.0F, 0.0F, 14.0F},
                                                    QVector3D{18.0F, 0.0F, 14.0F},
                                                    QVector3D{-24.0F, 0.0F, 2.0F},
                                                    QVector3D{24.0F, 0.0F, 2.0F},
                                                    QVector3D{-14.0F, 0.0F, 20.0F},
                                                    QVector3D{14.0F, 0.0F, 20.0F}};
    if (construction_index < static_cast<int>(fields.size())) {
      return fields[static_cast<std::size_t>(construction_index)];
    }
    return expanding_ring_offset(
        construction_index - static_cast<int>(fields.size()), 6, 28.0F, 8.0F);
  }
  if (building_type == BUILDING_TYPE_MARKETPLACE) {
    static const std::array<QVector3D, 4> roman = {QVector3D{3.0F, 0.0F, 9.0F},
                                                   QVector3D{-3.0F, 0.0F, 9.0F},
                                                   QVector3D{11.0F, 0.0F, 10.0F},
                                                   QVector3D{-11.0F, 0.0F, 10.0F}};
    static const std::array<QVector3D, 4> punic = {QVector3D{-3.0F, 0.0F, 9.0F},
                                                   QVector3D{3.0F, 0.0F, 9.0F},
                                                   QVector3D{-11.0F, 0.0F, 10.0F},
                                                   QVector3D{11.0F, 0.0F, 10.0F}};
    const auto& offsets = carthaginian ? punic : roman;
    if (construction_index < static_cast<int>(offsets.size())) {
      return offsets[static_cast<std::size_t>(construction_index)];
    }
    return expanding_ring_offset(
        construction_index - static_cast<int>(offsets.size()), 6, 16.0F, 5.0F);
  }
  if (building_type == BUILDING_TYPE_BARRACKS) {
    static const std::array<QVector3D, 5> roman = {QVector3D{0.0F, 0.0F, -8.0F},
                                                   QVector3D{-10.0F, 0.0F, -6.0F},
                                                   QVector3D{10.0F, 0.0F, -6.0F},
                                                   QVector3D{-16.0F, 0.0F, -12.0F},
                                                   QVector3D{16.0F, 0.0F, -12.0F}};
    static const std::array<QVector3D, 5> punic = {QVector3D{0.0F, 0.0F, -7.0F},
                                                   QVector3D{-12.0F, 0.0F, -4.0F},
                                                   QVector3D{12.0F, 0.0F, -4.0F},
                                                   QVector3D{-16.0F, 0.0F, -11.0F},
                                                   QVector3D{16.0F, 0.0F, -11.0F}};
    const auto& offsets = carthaginian ? punic : roman;
    if (construction_index < static_cast<int>(offsets.size())) {
      return offsets[static_cast<std::size_t>(construction_index)];
    }
    return expanding_ring_offset(
        construction_index - static_cast<int>(offsets.size()), 6, 20.0F, 6.0F);
  }
  if (building_type == BUILDING_TYPE_WALL_SEGMENT) {
    const int slot = construction_index % 11;
    return QVector3D{
        -10.0F + static_cast<float>(slot) * 2.0F, 0.0F, carthaginian ? -13.0F : -14.0F};
  }
  const float angle = static_cast<float>(construction_index) * 0.8F;
  return {18.0F * std::cos(angle), 0.0F, 18.0F * std::sin(angle)};
}

using SourNodes = std::unordered_map<std::uint64_t, float>;

auto node_is_sour(const SourNodes& sour, std::uint64_t node_id, float now) -> bool {
  const auto it = sour.find(node_id);
  return it != sour.end() && now < it->second;
}

auto entity_is_worked(const AISnapshot& snapshot,
                      Engine::Core::EntityID target_id) -> bool {
  for (const auto& entity : snapshot.friendly_units) {
    if (entity.builder_production.has_component &&
        entity.builder_production.task_target_id == target_id) {
      return true;
    }
  }
  return false;
}

template <typename TakeBuilder>
void order_harvest(const AISnapshot& snapshot,
                   const AIContext& context,
                   ResourceType resource,
                   const SourNodes& sour,
                   const TakeBuilder& take_builder,
                   std::vector<AICommand>& out_commands) {
  const char* harvest_type = harvest_type_for_resource(resource);
  if (harvest_type == nullptr) {
    return;
  }
  const ResourceNodeSnapshot* closest = nullptr;
  float closest_distance_sq = std::numeric_limits<float>::infinity();
  for (const auto& node : snapshot.resource_nodes) {
    if (node.reserved || !node_matches_resource(node, resource) ||
        node_is_sour(sour, node.id, snapshot.game_time)) {
      continue;
    }
    const float dx = node.pos_x - context.base_pos_x;
    const float dz = node.pos_z - context.base_pos_z;
    const float distance_sq = dx * dx + dz * dz;
    if (distance_sq < closest_distance_sq) {
      closest = &node;
      closest_distance_sq = distance_sq;
    }
  }
  if (closest == nullptr) {
    return;
  }
  const auto builder = take_builder(closest->pos_x, closest->pos_z);
  if (builder == 0) {
    return;
  }

  AICommand command;
  command.type = AICommandType::StartBuilderHarvest;
  command.units.push_back(builder);
  command.construction_type = harvest_type;
  command.construction_site_x = closest->pos_x;
  command.construction_site_z = closest->pos_z;
  command.resource_target_id = closest->id;
  out_commands.push_back(std::move(command));
}

template <typename TakeBuilder>
void order_repairs(const AISnapshot& snapshot,
                   const std::vector<Engine::Core::EntityID>& available,
                   int reserve,
                   const TakeBuilder& take_builder,
                   std::vector<AICommand>& out_commands) {
  int crews = 0;
  for (const auto& entity : snapshot.friendly_units) {
    if (crews >= k_max_repair_crews || static_cast<int>(available.size()) <= reserve) {
      return;
    }
    if (!wants_repair(entity) || entity_is_worked(snapshot, entity.id)) {
      continue;
    }
    const auto builder = take_builder(entity.pos_x, entity.pos_z);
    if (builder == 0) {
      return;
    }
    AICommand command;
    command.type = AICommandType::StartBuilderRepair;
    command.units.push_back(builder);
    command.target_id = entity.id;
    command.construction_site_x = entity.pos_x;
    command.construction_site_z = entity.pos_z;
    out_commands.push_back(std::move(command));
    ++crews;
  }
}

template <typename TakeBuilder>
void order_field_work(const AISnapshot& snapshot,
                      const AIContext& context,
                      const std::vector<Engine::Core::EntityID>& available,
                      int reserve,
                      const TakeBuilder& take_builder,
                      std::vector<AICommand>& out_commands) {
  if (!granary_has_room(snapshot)) {
    return;
  }
  std::vector<const EntitySnapshot*> ripe;
  for (const auto& entity : snapshot.friendly_units) {
    if (!entity.crop_is_ripe || field_is_worked(snapshot, entity.id)) {
      continue;
    }
    ripe.push_back(&entity);
  }
  std::sort(ripe.begin(),
            ripe.end(),
            [&context](const EntitySnapshot* lhs, const EntitySnapshot* rhs) {
              return distance_squared(lhs->pos_x,
                                      0.0F,
                                      lhs->pos_z,
                                      context.base_pos_x,
                                      0.0F,
                                      context.base_pos_z) <
                     distance_squared(rhs->pos_x,
                                      0.0F,
                                      rhs->pos_z,
                                      context.base_pos_x,
                                      0.0F,
                                      context.base_pos_z);
            });
  for (const auto* field : ripe) {
    if (static_cast<int>(available.size()) <= reserve) {
      return;
    }
    const auto builder = take_builder(field->pos_x, field->pos_z);
    if (builder == 0) {
      return;
    }
    AICommand command;
    command.type = AICommandType::StartBuilderHarvest;
    command.units.push_back(builder);
    command.construction_type = HARVEST_GRAIN;
    command.construction_site_x = field->pos_x;
    command.construction_site_z = field->pos_z;
    command.resource_target_id = field->id;
    out_commands.push_back(std::move(command));
  }
}

auto any_node_left(const AISnapshot& snapshot, ResourceType resource) -> bool {
  return std::any_of(snapshot.resource_nodes.begin(),
                     snapshot.resource_nodes.end(),
                     [resource](const ResourceNodeSnapshot& node) {
                       return !node.reserved && node_matches_resource(node, resource);
                     });
}

struct AffordabilityVerdict {

  bool blocked = false;

  ResourceType missing = ResourceType::Count;
};

auto affordability_of(const AISnapshot& snapshot,
                      const char* building_type) -> AffordabilityVerdict {
  AffordabilityVerdict verdict;
  if (building_type == nullptr) {
    verdict.blocked = true;
    return verdict;
  }
  if (!snapshot.has_resource_snapshot) {
    return verdict;
  }

  const auto costs = construction_cost_info(building_type).resource_costs;
  int largest_deficit = 0;
  for (const ResourceType type : k_all_resource_types) {
    const int deficit = costs.get(type) - snapshot.resources.get(type);
    if (deficit <= 0) {
      continue;
    }
    if (harvest_type_for_resource(type) == nullptr) {

      verdict.blocked = true;
      return verdict;
    }
    if (!any_node_left(snapshot, type)) {

      verdict.blocked = true;
      return verdict;
    }
    if (deficit > largest_deficit) {
      largest_deficit = deficit;
      verdict.missing = type;
    }
  }
  return verdict;
}

auto neediest_stockpile(const AISnapshot& snapshot, int building_count) -> const char* {
  if (!snapshot.has_resource_snapshot) {
    return nullptr;
  }

  const bool a_field_is_ripe =
      std::any_of(snapshot.friendly_units.begin(),
                  snapshot.friendly_units.end(),
                  [](const EntitySnapshot& entity) { return entity.crop_is_ripe; });
  if (a_field_is_ripe &&
      snapshot.resources.get(ResourceType::Food) < k_ai_larder_target) {
    return HARVEST_GRAIN;
  }

  const char* neediest = nullptr;
  float worst_ratio = 1.0F;
  for (const auto type :
       {ResourceType::Wood, ResourceType::Stone, ResourceType::Iron}) {
    const int target = stockpile_target(type, building_count);

    if (target <= 0 || !any_node_left(snapshot, type)) {
      continue;
    }
    const float ratio =
        static_cast<float>(snapshot.resources.get(type)) / static_cast<float>(target);
    if (ratio < worst_ratio) {
      worst_ratio = ratio;
      neediest = harvest_type_for_resource(type);
    }
  }
  if (neediest != nullptr) {
    return neediest;
  }

  return a_field_is_ripe ? HARVEST_GRAIN : nullptr;
}

struct ConstructionIntent {

  const char* type = nullptr;
  float x = 0.0F;
  float z = 0.0F;
  float rotation_y = 0.0F;

  bool site_known = false;

  bool expansion = false;

  int plan_slot = -1;
};

auto desired_work_parties(const AIContext& context) -> int {
  (void)context;

  return 4;
}

} // namespace

auto BuilderBehavior::is_deferred(const char* building_type,
                                  float game_time) const -> bool {
  return building_type != nullptr && building_type == m_deferred_type &&
         game_time < m_deferred_until;
}

void BuilderBehavior::note_construction_order(const char* building_type,
                                              int building_total,
                                              float game_time,
                                              int plan_slot) {
  constexpr int k_orders_before_giving_up = 8;
  constexpr int k_slot_orders_before_giving_up = 4;
  constexpr float k_defer_seconds = 90.0F;

  if (plan_slot >= 0 && plan_slot == m_last_plan_slot &&
      building_total == m_last_building_total) {
    ++m_last_plan_slot_repeats;
    if (m_last_plan_slot_repeats >= k_slot_orders_before_giving_up) {

      m_blocked_plan_slots.push_back(plan_slot);
      m_last_plan_slot_repeats = 0;
    }
  } else {
    m_last_plan_slot = plan_slot;
    m_last_plan_slot_repeats = 1;
  }

  if (building_type == m_last_order_type && building_total == m_last_building_total) {
    ++m_last_order_repeats;
  } else {
    m_last_order_type = building_type;
    m_last_building_total = building_total;
    m_last_order_repeats = 1;
  }

  if (m_last_order_repeats >= k_orders_before_giving_up) {

    m_deferred_type = building_type;
    m_deferred_until = game_time + k_defer_seconds;
    m_last_order_repeats = 0;
  }
}

void BuilderBehavior::review_stalled_workers(const AISnapshot& snapshot, float now) {

  constexpr float k_stall_seconds = 40.0F;

  constexpr float k_sour_seconds = 240.0F;

  m_stalled_builders.clear();
  std::erase_if(m_sour_nodes, [now](const auto& entry) { return now >= entry.second; });

  std::unordered_map<Engine::Core::EntityID, WorkerWatch> surviving;
  surviving.reserve(m_worker_watch.size());

  for (const auto& entity : snapshot.friendly_units) {
    if (entity.spawn_type != Game::Units::SpawnType::Builder ||
        !entity.builder_production.has_component) {
      continue;
    }
    const auto& work = entity.builder_production;

    const bool holds_a_task = work.has_construction_site || work.has_task_target;
    const bool making_progress =
        work.in_progress || work.at_construction_site || work.carrying_load;
    if (!holds_a_task || making_progress) {
      continue;
    }

    WorkerWatch watch{.task_target_id = work.task_target_id,
                      .site_x = work.construction_site_x,
                      .site_z = work.construction_site_z,
                      .since = now};
    if (const auto previous = m_worker_watch.find(entity.id);
        previous != m_worker_watch.end() &&
        previous->second.task_target_id == watch.task_target_id &&
        std::abs(previous->second.site_x - watch.site_x) < 0.5F &&
        std::abs(previous->second.site_z - watch.site_z) < 0.5F) {
      watch.since = previous->second.since;
    }
    surviving.emplace(entity.id, watch);

    if (now - watch.since < k_stall_seconds) {
      continue;
    }

    m_stalled_builders.insert(entity.id);
    if (watch.task_target_id != 0) {

      m_sour_nodes[watch.task_target_id] = now + k_sour_seconds;
    }
  }

  m_worker_watch = std::move(surviving);
}

void BuilderBehavior::divide_work_parties(const AISnapshot& snapshot,
                                          const AIContext& context,
                                          std::vector<AICommand>& out_commands) const {
  const int wanted = desired_work_parties(context);
  if (context.builder_count >= wanted) {
    return;
  }

  const EntitySnapshot* biggest = nullptr;
  for (const auto& entity : snapshot.friendly_units) {
    if (entity.spawn_type != Game::Units::SpawnType::Builder) {
      continue;
    }

    if (entity.squad_strength < k_smallest_useful_work_party * 2) {
      continue;
    }
    if (biggest == nullptr || entity.squad_strength > biggest->squad_strength) {
      biggest = &entity;
    }
  }
  if (biggest == nullptr) {
    return;
  }

  AICommand command;
  command.type = AICommandType::DivideSquads;
  command.units.push_back(biggest->id);
  out_commands.push_back(std::move(command));
}

void BuilderBehavior::manage_gather_crew(
    const AISnapshot& snapshot,
    const AIContext& context,
    bool reclaim_one,
    std::vector<Engine::Core::EntityID>& available_builders,
    std::vector<AICommand>& out_commands) {
  std::vector<Engine::Core::EntityID> gatherers;
  int builder_total = 0;
  for (const auto& entity : snapshot.friendly_units) {
    if (entity.spawn_type != Game::Units::SpawnType::Builder) {
      continue;
    }
    ++builder_total;
    if (entity.builder_production.auto_gather) {
      gatherers.push_back(entity.id);
    }
  }
  if (builder_total == 0) {
    return;
  }

  const int construction_crew = std::clamp(2 + (builder_total / 6), 2, 5);
  const int desired_gatherers =
      std::max(0, builder_total - construction_crew - (reclaim_one ? 1 : 0));
  const char* priority =
      neediest_stockpile(snapshot, static_cast<int>(context.buildings.size()));

  if (static_cast<int>(gatherers.size()) > desired_gatherers) {
    AICommand release;
    release.type = AICommandType::SetAutoGather;
    release.auto_gather_active = false;
    for (int i = desired_gatherers; i < static_cast<int>(gatherers.size()); ++i) {
      release.units.push_back(gatherers[static_cast<std::size_t>(i)]);
    }
    out_commands.push_back(std::move(release));
    return;
  }

  constexpr float k_priority_hold_seconds = 45.0F;
  const bool may_switch =
      snapshot.game_time - m_gather_priority_time >= k_priority_hold_seconds;
  const bool priority_changed = priority != m_gather_priority && may_switch;
  if (priority_changed || m_gather_priority == nullptr) {
    m_gather_priority = priority;
    m_gather_priority_time = snapshot.game_time;
  }

  AICommand order;
  order.type = AICommandType::SetAutoGather;
  order.auto_gather_active = true;
  order.construction_type = m_gather_priority;
  if (priority_changed) {

    order.units = gatherers;
  }
  while (static_cast<int>(gatherers.size()) + static_cast<int>(order.units.size()) <
             desired_gatherers &&
         !available_builders.empty()) {
    order.units.push_back(available_builders.back());
    available_builders.pop_back();
  }
  if (!order.units.empty()) {
    out_commands.push_back(std::move(order));
  }
}

void BuilderBehavior::execute(const AISnapshot& snapshot,
                              AIContext& context,
                              float delta_time,
                              std::vector<AICommand>& out_commands) {
  if (context.nation != nullptr && !context.nation->has_economy) {
    return;
  }

  m_construction_timer += delta_time;
  if (m_construction_timer < 3.0F) {
    return;
  }
  m_construction_timer = 0.0F;

  review_stalled_workers(snapshot, snapshot.game_time);

  std::vector<Engine::Core::EntityID> available_builders;
  int busy_site = 0;
  int busy_task = 0;
  int busy_load = 0;
  int busy_moving = 0;
  int busy_other = 0;
  for (const auto& entity : snapshot.friendly_units) {
    if (entity.spawn_type != Game::Units::SpawnType::Builder) {
      continue;
    }

    const bool stalled = m_stalled_builders.contains(entity.id);
    if (!stalled && entity.builder_production.has_component &&
        (entity.builder_production.has_construction_site ||
         entity.builder_production.has_task_target ||
         entity.builder_production.carrying_load)) {
      busy_site += entity.builder_production.has_construction_site ? 1 : 0;
      busy_task += entity.builder_production.has_task_target ? 1 : 0;
      busy_load += entity.builder_production.carrying_load ? 1 : 0;
      continue;
    }

    if (stalled) {

      available_builders.push_back(entity.id);
      continue;
    }

    if (entity.movement.has_component && !entity.movement.has_target) {
      available_builders.push_back(entity.id);
    } else if (entity.movement.has_component) {
      ++busy_moving;
    } else {
      ++busy_other;
    }
  }

  if (available_builders.empty()) {
    if (qEnvironmentVariableIsSet("SOI_BUILD_TRACE")) {
      qWarning() << "BUILDTRACE p" << context.player_id << "no available builders of"
                 << context.builder_count << "site" << busy_site << "task" << busy_task
                 << "load" << busy_load << "moving" << busy_moving << "other"
                 << busy_other;
      for (const auto& entity : snapshot.friendly_units) {
        if (entity.spawn_type != Game::Units::SpawnType::Builder) {
          continue;
        }
        qWarning() << "BUILDTRACE p" << context.player_id << "  builder" << entity.id
                   << "site" << entity.builder_production.has_construction_site
                   << "progress" << entity.builder_production.in_progress << "task"
                   << entity.builder_production.has_task_target << "target"
                   << entity.builder_production.task_target_id << "load"
                   << entity.builder_production.carrying_load << "gather"
                   << entity.builder_production.auto_gather << "moving"
                   << entity.movement.has_target;
      }
    }
  }

  const auto take_builder = [&available_builders,
                             &snapshot](float x, float z) -> Engine::Core::EntityID {
    if (available_builders.empty()) {
      return 0;
    }
    const auto id = select_best_builder(snapshot, available_builders, x, z);
    std::erase(available_builders, id);
    return id;
  };

  const auto take_strongest_builder =
      [&available_builders, &snapshot](float x, float z) -> Engine::Core::EntityID {
    if (available_builders.empty()) {
      return 0;
    }
    const auto id = select_strongest_builder(snapshot, available_builders, x, z);
    std::erase(available_builders, id);
    return id;
  };

  int siege_count = 0;
  for (const auto& entity : snapshot.friendly_units) {
    if (entity.spawn_type == Game::Units::SpawnType::Catapult ||
        entity.spawn_type == Game::Units::SpawnType::Ballista) {
      siege_count++;
    }
  }

  const char* siege_engine = preferred_siege_engine(context);

  const auto raising = [&snapshot](Game::Units::SpawnType type) {
    int count = 0;
    for (const auto& entity : snapshot.friendly_units) {
      if (entity.builder_production.raising_a_building &&
          entity.builder_production.building_under_way == type) {
        ++count;
      }
    }
    return count;
  };

  const SettlementCensus standing{
      .homes = context.home_count + raising(Game::Units::SpawnType::Home),
      .barracks = context.barracks_count + raising(Game::Units::SpawnType::Barracks),
      .towers =
          context.defense_tower_count + raising(Game::Units::SpawnType::DefenseTower),
      .walls =
          context.wall_segment_count + raising(Game::Units::SpawnType::WallSegment),
      .markets =
          context.marketplace_count + raising(Game::Units::SpawnType::Marketplace),
      .farms = context.farm_count + raising(Game::Units::SpawnType::Farm)};

  const auto& targets = context.macro_targets;
  const int target_homes = std::clamp(targets.home_count, 2, MAX_HOMES);
  const int target_barracks = std::clamp(targets.barracks_count, 1, MAX_BARRACKS);
  const int target_towers =
      std::clamp(targets.defense_tower_count, 0, MAX_DEFENSE_TOWERS);
  const auto* town_plan = context.strategy_config.doctrine != nullptr
                              ? context.strategy_config.doctrine->town_plan
                              : nullptr;
  const int planned_walls = town_plan != nullptr ? town_plan->wall_step_count() : 0;
  const int target_walls = std::clamp(
      std::max(targets.wall_segment_count, planned_walls), 0, MAX_WALL_SEGMENTS);
  const int target_catapults = std::clamp(targets.catapult_count, 0, MAX_CATAPULTS);
  const int target_farms = std::clamp(targets.farm_count, 0, MAX_FARMS);
  const int target_markets = std::clamp(targets.marketplace_count, 0, MAX_MARKETPLACES);

  std::vector<ConstructionIntent> intents;

  if (context.state == AIState::Expanding && needs_outpost_construction(context) &&
      !context.expansion_construction_pending &&
      !recent_outpost_order(snapshot, context)) {
    ConstructionIntent outpost;
    outpost.expansion = true;
    outpost.site_known = true;
    if (context.outpost_barracks_count <
        context.strategy_config.desired_outpost_barracks_count) {
      outpost.type = BUILDING_TYPE_BARRACKS;
      outpost.x = context.expansion_site_x;
      outpost.z = context.expansion_site_z;
    } else {
      outpost.type = BUILDING_TYPE_HOME;
      const float dx = context.expansion_site_x - context.base_pos_x;
      const float dz = context.expansion_site_z - context.base_pos_z;
      const float dist = std::sqrt(std::max(0.0F, dx * dx + dz * dz));
      const float offset_scale = (dist > 0.1F) ? (8.0F / dist) : 0.0F;
      outpost.x = context.expansion_site_x - dz * offset_scale;
      outpost.z = context.expansion_site_z + dx * offset_scale;
    }
    intents.push_back(outpost);
  }

  if (const AIBase* exposed = exposed_secondary_base(context); exposed != nullptr) {
    constexpr int k_outpost_site_attempts = 12;
    for (int attempt = 0; attempt < k_outpost_site_attempts; ++attempt) {
      const QVector3D offset =
          expanding_ring_offset(m_construction_counter + attempt, 6, 9.0F, 4.0F);
      const float candidate_x = exposed->center_x + offset.x();
      const float candidate_z = exposed->center_z + offset.z();
      if (!site_is_free(
              snapshot, BUILDING_TYPE_DEFENSE_TOWER, candidate_x, candidate_z)) {
        continue;
      }
      ConstructionIntent relief;
      relief.type = BUILDING_TYPE_DEFENSE_TOWER;
      relief.x = candidate_x;
      relief.z = candidate_z;
      relief.site_known = true;
      intents.push_back(relief);
      break;
    }
  }

  PlanStepChoice planned;

  const bool needs_a_field = standing.farms < target_farms;
  const bool starving = starved_of_food(snapshot);
  const char* preferred = nullptr;
  if (needs_a_field && starving) {
    preferred = BUILDING_TYPE_FARM;
  } else if (targets.raise_homes_first) {
    preferred = BUILDING_TYPE_HOME;
  }
  const SettlementTargets settlement{.homes = target_homes,
                                     .barracks = target_barracks,
                                     .towers = target_towers,
                                     .walls = target_walls,
                                     .markets = target_markets,
                                     .farms = target_farms};
  const bool has_plan_step =
      context.primary_barracks != 0 && authored_plan_step(context,
                                                          snapshot,
                                                          standing,
                                                          settlement,
                                                          preferred,
                                                          m_blocked_plan_slots,
                                                          planned);

  const auto wish = [&intents](const char* type) {
    if (type != nullptr) {
      ConstructionIntent intent;
      intent.type = type;
      intents.push_back(intent);
    }
  };

  constexpr int k_roofs_before_the_frame = 2;

  constexpr int k_fields_before_the_frame = 2;

  if (standing.barracks == 0) {
    wish(BUILDING_TYPE_BARRACKS);
  }
  if (standing.homes < k_roofs_before_the_frame) {
    wish(BUILDING_TYPE_HOME);
  }
  const bool field_before_plan =
      needs_a_field && (starving || standing.farms < k_fields_before_the_frame);
  if (field_before_plan) {
    wish(BUILDING_TYPE_FARM);
  }
  if (standing.markets < 1 && target_markets > 0 && snapshot.has_resource_snapshot &&
      snapshot.resources.get(ResourceType::Gold) >= k_treasury_worth_a_market) {
    wish(BUILDING_TYPE_MARKETPLACE);
  }
  if (targets.raise_homes_first && standing.homes < MAX_HOMES) {

    wish(BUILDING_TYPE_HOME);
  }

  if (has_plan_step) {
    ConstructionIntent step;
    step.type = planned.building;
    step.x = context.base_pos_x + planned.offset.x();
    step.z = context.base_pos_z + planned.offset.z();
    step.rotation_y = planned.rotation_y;
    step.site_known = context.has_base_anchor;
    step.plan_slot = planned.slot;
    intents.push_back(step);
  }

  if (context.barracks_under_threat && standing.towers < target_towers) {
    wish(BUILDING_TYPE_DEFENSE_TOWER);
  }

  for (const char* candidate : unmet_candidates({
           {BUILDING_TYPE_FARM, standing.farms, target_farms},
           {BUILDING_TYPE_BARRACKS, standing.barracks, target_barracks},
           {BUILDING_TYPE_DEFENSE_TOWER, standing.towers, target_towers},
           {BUILDING_TYPE_MARKETPLACE, standing.markets, target_markets},
           {BUILDING_TYPE_WALL_SEGMENT, standing.walls, target_walls},
           {BUILDING_TYPE_HOME, standing.homes, target_homes},
           {siege_engine, siege_count, target_catapults},
       })) {
    wish(candidate);
  }

  const ConstructionIntent* chosen = nullptr;
  ResourceType missing_resource = ResourceType::Count;
  for (const auto& intent : intents) {
    if (is_deferred(intent.type, snapshot.game_time)) {
      continue;
    }
    const auto verdict = affordability_of(snapshot, intent.type);
    if (verdict.blocked) {
      continue;
    }
    if (verdict.missing != ResourceType::Count) {

      if (missing_resource == ResourceType::Count) {
        missing_resource = verdict.missing;
      }
      continue;
    }
    chosen = &intent;
    break;
  }

  if (missing_resource != ResourceType::Count) {
    order_harvest(
        snapshot, context, missing_resource, m_sour_nodes, take_builder, out_commands);
  }

  const char* building_to_construct = chosen != nullptr ? chosen->type : nullptr;
  float construction_x = context.base_pos_x;
  float construction_z = context.base_pos_z;
  const float construction_rotation_y = chosen != nullptr ? chosen->rotation_y : 0.0F;
  const int plan_slot = chosen != nullptr ? chosen->plan_slot : -1;
  bool expansion_order = chosen != nullptr && chosen->expansion;
  bool site_resolved = false;
  bool wanted_a_builder = false;

  if (chosen != nullptr) {
    if (chosen->site_known) {
      construction_x = chosen->x;
      construction_z = chosen->z;
      site_resolved = true;
    } else if (!context.has_base_anchor) {
      site_resolved = true;
    } else {
      constexpr int k_site_search_attempts = 24;
      for (int attempt = 0; attempt < k_site_search_attempts; ++attempt) {
        const QVector3D offset = planned_settlement_offset(
            context, building_to_construct, m_construction_counter + attempt);
        const float candidate_x = context.base_pos_x + offset.x();
        const float candidate_z = context.base_pos_z + offset.z();
        if (!site_is_free(snapshot, building_to_construct, candidate_x, candidate_z)) {
          continue;
        }
        construction_x = candidate_x;
        construction_z = candidate_z;
        m_construction_counter += attempt;
        site_resolved = true;
        break;
      }
      if (!site_resolved) {
        m_construction_counter += k_site_search_attempts;
        building_to_construct = nullptr;
      }
    }
  }

  order_field_work(
      snapshot, context, available_builders, 1, take_builder, out_commands);
  order_repairs(snapshot, available_builders, 1, take_builder, out_commands);

  if (building_to_construct != nullptr && site_resolved) {
    clamp_to_map_bounds(snapshot, construction_x, construction_z);

    if (qEnvironmentVariableIsSet("SOI_BUILD_TRACE")) {
      qWarning() << "BUILDTRACE p" << context.player_id << "wants"
                 << building_to_construct << "at" << construction_x << construction_z
                 << "base" << context.base_pos_x << context.base_pos_z << "homes"
                 << context.home_count << "barracks" << context.barracks_count;
    }

    const auto builder = take_strongest_builder(construction_x, construction_z);
    if (builder == 0) {
      wanted_a_builder = true;
    } else {
      AICommand command;
      command.type = AICommandType::StartBuilderConstruction;
      command.units.push_back(builder);
      command.construction_type = building_to_construct;
      command.construction_site_x = construction_x;
      command.construction_site_z = construction_z;
      command.construction_rotation_y = construction_rotation_y;
      out_commands.push_back(std::move(command));

      if (expansion_order) {
        AIBaseManager::note_expansion_order(
            context, snapshot.game_time, construction_x, construction_z);
      }
      if (plan_slot >= 0 && !context.settlement_facing_locked) {

        const QVector2D facing = settlement_facing(context, snapshot);
        context.settlement_facing_x = facing.x();
        context.settlement_facing_z = facing.y();
        context.settlement_facing_locked = true;
      }
      note_construction_order(building_to_construct,
                              static_cast<int>(context.buildings.size()),
                              snapshot.game_time,
                              plan_slot);
      m_construction_counter++;
    }
  }

  divide_work_parties(snapshot, context, out_commands);
  manage_gather_crew(
      snapshot, context, wanted_a_builder, available_builders, out_commands);
}

auto BuilderBehavior::should_execute(const AISnapshot& snapshot,
                                     const AIContext& context) const -> bool {
  (void)snapshot;
  if (context.nation != nullptr && !context.nation->has_economy) {
    return false;
  }

  return context.builder_count > 0;
}

} // namespace Game::Systems::AI
