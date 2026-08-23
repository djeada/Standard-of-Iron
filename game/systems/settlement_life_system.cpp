#include "settlement_life_system.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

#include "../core/component.h"
#include "../core/world.h"
#include "../map/map_definition.h"
#include "../map/terrain_service.h"
#include "building_collision_registry.h"
#include "command_service.h"
#include "nav_grid.h"
#include "owner_registry.h"
#include "units/spawn_type.h"

namespace Game::Systems {
namespace {

using Engine::Core::SettlementErrand;
using Engine::Core::SettlementErrandRole;
using Engine::Core::SettlementResidentComponent;

constexpr float k_building_standoff = 1.15F;
constexpr float k_prop_standoff = 1.25F;
constexpr float k_travel_allowance_per_metre = 1.6F;
constexpr float k_min_travel_allowance = 6.0F;
constexpr int k_pick_attempts = 6;

constexpr float k_labour_chance = 0.62F;
constexpr float k_labour_dwell_bonus = 6.0F;

auto next_random(std::uint32_t& state) -> float {
  state = (state * 1664525U) + 1013904223U;
  return static_cast<float>((state >> 8U) & 0xFFFFFFU) / 16777216.0F;
}

auto random_range(std::uint32_t& state, float low, float high) -> float {
  return low + ((high - low) * next_random(state));
}

auto is_life_prop(Game::Map::WorldProp::Type type) -> bool {
  switch (type) {
  case Game::Map::WorldProp::Type::FireCamp:
  case Game::Map::WorldProp::Type::Tent:
  case Game::Map::WorldProp::Type::SupplyCart:
  case Game::Map::WorldProp::Type::WeaponRack:
  case Game::Map::WorldProp::Type::Plant:
  case Game::Map::WorldProp::Type::OliveTree:
  case Game::Map::WorldProp::Type::CypressTree:
  case Game::Map::WorldProp::Type::PalmTree:
    return true;
  default:
    return false;
  }
}

struct Errand {
  float x{0.0F};
  float z{0.0F};
  float dwell{3.0F};
  Engine::Core::EntityID focus{0};
  SettlementErrandRole role{SettlementErrandRole::Loiter};

  float focus_x{0.0F};
  float focus_z{0.0F};
};

auto roll_errand_role(std::uint32_t& rng) -> SettlementErrandRole {
  return next_random(rng) < k_labour_chance ? SettlementErrandRole::Labour
                                            : SettlementErrandRole::Loiter;
}

auto errand_at_building(const Engine::Core::TransformComponent& building_transform,
                        Engine::Core::EntityID building_id,
                        std::uint32_t& rng) -> Errand {
  float half_width = 1.5F;
  float half_depth = 1.5F;
  if (const auto* footprint =
          BuildingCollisionRegistry::instance().find_building(building_id);
      footprint != nullptr) {
    half_width = footprint->width * 0.5F;
    half_depth = footprint->depth * 0.5F;
  }

  float const angle = random_range(rng, 0.0F, 6.2831853F);
  float const reach_x =
      half_width + k_building_standoff + random_range(rng, 0.0F, 1.1F);
  float const reach_z =
      half_depth + k_building_standoff + random_range(rng, 0.0F, 1.1F);

  Errand errand;
  errand.x = building_transform.position.x + (std::cos(angle) * reach_x);
  errand.z = building_transform.position.z + (std::sin(angle) * reach_z);
  errand.dwell = random_range(rng, 3.5F, 8.0F);
  errand.focus = building_id;
  errand.focus_x = building_transform.position.x;
  errand.focus_z = building_transform.position.z;
  errand.role = roll_errand_role(rng);
  if (errand.role == SettlementErrandRole::Labour) {
    errand.dwell += k_labour_dwell_bonus;
  }
  return errand;
}

auto errand_at_prop(const QVector3D& prop_position, std::uint32_t& rng) -> Errand {
  float const angle = random_range(rng, 0.0F, 6.2831853F);
  float const reach = k_prop_standoff + random_range(rng, 0.0F, 0.9F);

  Errand errand;
  errand.x = prop_position.x() + (std::cos(angle) * reach);
  errand.z = prop_position.z() + (std::sin(angle) * reach);
  errand.dwell = random_range(rng, 4.0F, 9.5F);
  errand.focus_x = prop_position.x();
  errand.focus_z = prop_position.z();
  errand.role = roll_errand_role(rng);
  if (errand.role == SettlementErrandRole::Labour) {
    errand.dwell += k_labour_dwell_bonus;
  }
  return errand;
}

auto errand_on_the_street(const SettlementResidentComponent& resident,
                          std::uint32_t& rng) -> Errand {
  float const angle = random_range(rng, 0.0F, 6.2831853F);
  float const reach =
      random_range(rng, resident.roam_radius * 0.25F, resident.roam_radius * 0.85F);

  Errand errand;
  errand.x = resident.hearth_x + (std::cos(angle) * reach);
  errand.z = resident.hearth_z + (std::sin(angle) * reach);
  errand.dwell = random_range(rng, 1.8F, 4.5F);

  errand.focus_x = resident.hearth_x;
  errand.focus_z = resident.hearth_z;
  return errand;
}

struct Candidate {
  enum class Kind : std::uint8_t {
    Building,
    Prop,
    Street
  };

  Kind kind{Kind::Street};
  Engine::Core::EntityID entity_id{0};
  QVector3D position;
};

void collect_settlement_candidates(Engine::Core::World& world,
                                   const SettlementResidentComponent& resident,
                                   int owner_id,
                                   std::vector<Candidate>& out) {
  out.clear();

  float const radius_sq = resident.roam_radius * resident.roam_radius;
  for (auto [entity_id, building, unit, transform] :
       world.view<Engine::Core::BuildingComponent,
                  Engine::Core::UnitComponent,
                  Engine::Core::TransformComponent>()) {
    (void)building;
    if (unit.health <= 0 || unit.owner_id != owner_id) {
      continue;
    }
    if (Game::Units::is_wall_network_spawn(unit.spawn_type)) {
      continue;
    }
    float const dx = transform.position.x - resident.hearth_x;
    float const dz = transform.position.z - resident.hearth_z;
    if ((dx * dx) + (dz * dz) > radius_sq) {
      continue;
    }
    out.push_back({Candidate::Kind::Building, entity_id, {}});
  }

  auto const& terrain = Game::Map::TerrainService::instance();
  for (auto const& prop : terrain.world_props()) {
    if (!is_life_prop(prop.type)) {
      continue;
    }
    QVector3D const position = terrain.world_prop_world_position(prop);
    float const dx = position.x() - resident.hearth_x;
    float const dz = position.z() - resident.hearth_z;
    if ((dx * dx) + (dz * dz) > radius_sq) {
      continue;
    }
    out.push_back({Candidate::Kind::Prop, 0, position});
  }
}

void face_focus(Engine::Core::TransformComponent& transform,
                const SettlementResidentComponent& resident) {
  float const dx = resident.focus_x - transform.position.x;
  float const dz = resident.focus_z - transform.position.z;
  if (((dx * dx) + (dz * dz)) < 0.04F) {
    return;
  }

  transform.desired_yaw = std::atan2(dx, dz) * 180.0F / 3.14159265F;
  transform.has_desired_yaw = true;
}

auto find_settlement_anchor(Engine::Core::World& world,
                            int owner_id,
                            float from_x,
                            float from_z,
                            float search_radius) -> std::optional<QVector3D> {
  std::optional<QVector3D> nearest;
  float nearest_distance_sq = search_radius * search_radius;

  for (auto [entity_id, building, unit, transform] :
       world.view<Engine::Core::BuildingComponent,
                  Engine::Core::UnitComponent,
                  Engine::Core::TransformComponent>()) {
    (void)building;
    (void)entity_id;
    if (unit.health <= 0 || unit.owner_id != owner_id ||
        Game::Units::is_wall_network_spawn(unit.spawn_type)) {
      continue;
    }
    float const dx = transform.position.x - from_x;
    float const dz = transform.position.z - from_z;
    float const distance_sq = (dx * dx) + (dz * dz);
    if (distance_sq > nearest_distance_sq) {
      continue;
    }
    nearest_distance_sq = distance_sq;
    nearest = QVector3D(transform.position.x, 0.0F, transform.position.z);
  }

  return nearest;
}

auto is_unclaimed_by_the_player(const Engine::Core::Entity& entity) -> bool {
  if (entity.has_component<Engine::Core::CivilianDeliveryComponent>() ||
      entity.has_component<Engine::Core::PlayerOrderIntentComponent>() ||
      entity.has_component<Engine::Core::AttackTargetComponent>()) {
    return false;
  }
  const auto* movement = entity.get_component<Engine::Core::MovementComponent>();
  return movement != nullptr && !movement->get_has_target();
}

auto endangers_residents(Game::Units::SpawnType type) -> bool {
  if (type == Game::Units::SpawnType::Wolf) {
    return true;
  }
  return Game::Units::can_use_attack_mode(type) &&
         type != Game::Units::SpawnType::Civilian;
}

void collect_armed_units(Engine::Core::World& world,
                         std::vector<SettlementLifeSystem::ArmedUnit>& out) {
  out.clear();
  for (auto [entity_id, unit, transform] :
       world.view<Engine::Core::UnitComponent, Engine::Core::TransformComponent>()) {
    (void)entity_id;
    if (unit.health <= 0 || !endangers_residents(unit.spawn_type)) {
      continue;
    }
    out.push_back({transform.position.x, transform.position.z, unit.owner_id});
  }
}

auto nearest_danger(const std::vector<SettlementLifeSystem::ArmedUnit>& armed_units,
                    int resident_owner_id,
                    float x,
                    float z,
                    float radius) -> std::optional<QVector3D> {
  std::optional<QVector3D> nearest;
  float nearest_distance_sq = radius * radius;

  for (auto const& armed : armed_units) {
    float const dx = armed.x - x;
    float const dz = armed.z - z;
    float const distance_sq = (dx * dx) + (dz * dz);
    if (distance_sq > nearest_distance_sq) {
      continue;
    }
    if (!OwnerRegistry::instance().are_enemies(resident_owner_id, armed.owner_id)) {
      continue;
    }
    nearest_distance_sq = distance_sq;
    nearest = QVector3D(armed.x, 0.0F, armed.z);
  }

  return nearest;
}

void run_from(Engine::Core::World& world,
              Engine::Core::Entity& entity,
              const Engine::Core::TransformComponent& transform,
              SettlementResidentComponent& resident,
              const QVector3D& danger) {
  float away_x = transform.position.x - danger.x();
  float away_z = transform.position.z - danger.z();
  float const length_sq = (away_x * away_x) + (away_z * away_z);
  if (length_sq < 0.01F) {
    away_x = 1.0F;
    away_z = 0.0F;
  } else {
    float const length = std::sqrt(length_sq);
    away_x /= length;
    away_z /= length;
  }

  float const hearth_dx = resident.hearth_x - transform.position.x;
  float const hearth_dz = resident.hearth_z - transform.position.z;
  float const hearth_length =
      std::sqrt((hearth_dx * hearth_dx) + (hearth_dz * hearth_dz));

  if (hearth_length > 1.0F && (((hearth_dx / hearth_length) * away_x) +
                               ((hearth_dz / hearth_length) * away_z)) > 0.0F) {

    away_x = ((away_x + (hearth_dx / hearth_length)) * 0.5F);
    away_z = ((away_z + (hearth_dz / hearth_length)) * 0.5F);
    float const blended = std::sqrt((away_x * away_x) + (away_z * away_z));
    if (blended > 0.01F) {
      away_x /= blended;
      away_z /= blended;
    }
  }

  QVector3D const destination = NavGrid::snap_to_walkable_ground(QVector3D(
      transform.position.x + (away_x * SettlementLifeSystem::k_flee_distance),
      0.0F,
      transform.position.z + (away_z * SettlementLifeSystem::k_flee_distance)));

  resident.errand = SettlementErrand::Fleeing;
  resident.errand_x = destination.x();
  resident.errand_z = destination.z();
  resident.errand_remaining = SettlementLifeSystem::k_flee_leg_seconds;

  CommandService::MoveOptions options;
  options.kind = MoveOrderKind::ScriptedMove;
  CommandService::move_unit(world, entity.get_id(), destination, options);
}

void adopt_idle_civilians(Engine::Core::World& world) {
  for (auto [entity, unit_ref, transform_ref] :
       world.entity_view<Engine::Core::UnitComponent,
                         Engine::Core::TransformComponent>()) {
    auto const* unit = &unit_ref;
    auto const* transform = &transform_ref;
    if (unit->spawn_type != Game::Units::SpawnType::Civilian || unit->health <= 0) {
      continue;
    }
    if (entity.has_component<SettlementResidentComponent>()) {
      continue;
    }
    if (!is_unclaimed_by_the_player(entity)) {
      continue;
    }

    auto const anchor = find_settlement_anchor(world,
                                               unit->owner_id,
                                               transform->position.x,
                                               transform->position.z,
                                               SettlementLifeSystem::k_adoption_radius);
    if (!anchor.has_value()) {
      continue;
    }

    auto* resident = entity.add_component<SettlementResidentComponent>();
    if (resident == nullptr) {
      continue;
    }
    resident->hearth_x = anchor->x();
    resident->hearth_z = anchor->z();
    resident->hearth_assigned = true;
    resident->roam_radius = SettlementLifeSystem::k_adopted_roam_radius;
  }
}

} // namespace

void SettlementLifeSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  m_adoption_cooldown -= delta_time;
  if (m_adoption_cooldown <= 0.0F) {
    m_adoption_cooldown = k_adoption_interval;
    adopt_idle_civilians(*world);
  }

  if (world->entities_with<SettlementResidentComponent>().empty()) {
    return;
  }

  m_alarm_cooldown -= delta_time;
  bool const alarm_due = m_alarm_cooldown <= 0.0F;
  if (alarm_due) {
    m_alarm_cooldown = k_alarm_interval;
    collect_armed_units(*world, m_armed_units);
  }

  std::vector<Candidate> candidates;

  for (auto [entity_ref, resident_ref, unit_ref, transform_ref, movement_ref] :
       world->entity_view<SettlementResidentComponent,
                          Engine::Core::UnitComponent,
                          Engine::Core::TransformComponent,
                          Engine::Core::MovementComponent>()) {
    Engine::Core::Entity* entity = &entity_ref;
    auto* resident = &resident_ref;
    auto* unit = &unit_ref;
    auto* transform = &transform_ref;
    auto* movement = &movement_ref;

    if (unit->health <= 0) {

      resident->errand = SettlementErrand::Settling;
      resident->role = SettlementErrandRole::Loiter;
      resident->work_elapsed = 0.0F;
      resident->focus_id = 0;
      continue;
    }

    if (resident->released) {
      continue;
    }

    bool const claimed_by_the_player =
        entity->has_component<Engine::Core::CivilianDeliveryComponent>() ||
        entity->has_component<Engine::Core::PlayerOrderIntentComponent>();
    if (claimed_by_the_player) {
      resident->errand = SettlementErrand::Settling;
      resident->role = SettlementErrandRole::Loiter;
      resident->focus_id = 0;
      resident->work_elapsed = 0.0F;

      resident->hearth_assigned = false;
      continue;
    }

    if (resident->rng_state == 0U) {
      resident->rng_state =
          static_cast<std::uint32_t>((entity->get_id() * 2654435761ULL) | 1ULL);
      resident->think_cooldown = random_range(resident->rng_state, 0.0F, 2.5F);
    }

    if (!resident->hearth_assigned) {
      auto const anchor = find_settlement_anchor(*world,
                                                 unit->owner_id,
                                                 transform->position.x,
                                                 transform->position.z,
                                                 k_adoption_radius);
      resident->hearth_x = anchor.has_value() ? anchor->x() : transform->position.x;
      resident->hearth_z = anchor.has_value() ? anchor->z() : transform->position.z;
      resident->hearth_assigned = true;
    }

    auto const* attack_target =
        entity->get_component<Engine::Core::AttackTargetComponent>();
    bool const fighting = (attack_target != nullptr) && attack_target->target_id != 0;

    if (alarm_due && !fighting) {
      bool const already_fleeing = resident->errand == SettlementErrand::Fleeing;
      auto const danger =
          nearest_danger(m_armed_units,
                         unit->owner_id,
                         transform->position.x,
                         transform->position.z,
                         already_fleeing ? k_all_clear_radius : k_alarm_radius);

      if (danger.has_value()) {
        resident->role = SettlementErrandRole::Loiter;
        resident->work_elapsed = 0.0F;
        resident->focus_id = 0;
        resident->errand_remaining -= k_alarm_interval;

        if (!already_fleeing || resident->errand_remaining <= 0.0F ||
            movement->get_state() == Engine::Core::MovementState::Idle) {
          run_from(*world, *entity, *transform, *resident, *danger);
        }
        continue;
      }

      if (already_fleeing) {
        resident->errand = SettlementErrand::Settling;
        resident->think_cooldown = 0.0F;
      }
    }

    if (fighting) {
      resident->errand = SettlementErrand::Settling;
      resident->role = SettlementErrandRole::Loiter;
      resident->focus_id = 0;
      resident->work_elapsed = 0.0F;
      continue;
    }

    if (resident->errand == SettlementErrand::Fleeing) {
      continue;
    }

    if (resident->is_labouring()) {
      resident->work_elapsed += delta_time;
    }

    resident->think_cooldown -= delta_time;
    if (resident->think_cooldown > 0.0F) {
      continue;
    }
    resident->think_cooldown = k_think_interval;

    float const dx = resident->errand_x - transform->position.x;
    float const dz = resident->errand_z - transform->position.z;
    float const distance_sq = (dx * dx) + (dz * dz);

    bool pick_new_errand = false;
    switch (resident->errand) {
    case SettlementErrand::Settling:
      pick_new_errand = true;
      break;
    case SettlementErrand::WalkingTo:
      resident->errand_remaining -= k_think_interval;
      if (distance_sq <= k_arrival_radius * k_arrival_radius) {
        movement->stop();
        resident->errand = SettlementErrand::Working;
        resident->errand_remaining = resident->planned_dwell;
        resident->work_elapsed = 0.0F;
        face_focus(*transform, *resident);
      } else if (resident->errand_remaining <= 0.0F) {
        pick_new_errand = true;
      } else if (movement->get_state() == Engine::Core::MovementState::Idle) {

        pick_new_errand = true;
      }
      break;
    case SettlementErrand::Working:
      resident->errand_remaining -= k_think_interval;
      if (resident->errand_remaining <= 0.0F) {
        pick_new_errand = true;
      }
      break;
    case SettlementErrand::Fleeing:
      break;
    }

    if (!pick_new_errand) {
      continue;
    }

    collect_settlement_candidates(*world, *resident, unit->owner_id, candidates);

    Errand chosen;
    bool has_choice = false;
    for (int attempt = 0; attempt < k_pick_attempts && !has_choice; ++attempt) {

      bool const use_candidate =
          !candidates.empty() && next_random(resident->rng_state) < 0.7F;
      Errand proposal;
      if (use_candidate) {
        auto const index = static_cast<std::size_t>(
            next_random(resident->rng_state) * static_cast<float>(candidates.size()));
        auto const& candidate = candidates[std::min(index, candidates.size() - 1)];
        if (candidate.kind == Candidate::Kind::Building) {
          if (candidate.entity_id == resident->focus_id) {
            continue;
          }
          auto* building = world->get_entity(candidate.entity_id);
          auto* building_transform =
              building != nullptr
                  ? building->get_component<Engine::Core::TransformComponent>()
                  : nullptr;
          if (building_transform == nullptr) {
            continue;
          }
          proposal = errand_at_building(
              *building_transform, candidate.entity_id, resident->rng_state);
        } else {
          proposal = errand_at_prop(candidate.position, resident->rng_state);
        }
      } else {
        proposal = errand_on_the_street(*resident, resident->rng_state);
      }

      float const step_x = proposal.x - transform->position.x;
      float const step_z = proposal.z - transform->position.z;
      if ((step_x * step_x) + (step_z * step_z) <
          k_min_errand_distance * k_min_errand_distance) {
        continue;
      }
      chosen = proposal;
      has_choice = true;
    }

    if (!has_choice) {
      resident->errand = SettlementErrand::Working;
      resident->role = SettlementErrandRole::Loiter;
      resident->work_elapsed = 0.0F;
      resident->errand_remaining = random_range(resident->rng_state, 1.0F, 3.0F);
      continue;
    }

    QVector3D const destination =
        NavGrid::snap_to_walkable_ground(QVector3D(chosen.x, 0.0F, chosen.z));

    resident->errand = SettlementErrand::WalkingTo;
    resident->role = chosen.role;
    resident->focus_id = chosen.focus;
    resident->errand_x = destination.x();
    resident->errand_z = destination.z();
    resident->focus_x = chosen.focus_x;
    resident->focus_z = chosen.focus_z;
    resident->planned_dwell = chosen.dwell;
    resident->work_elapsed = 0.0F;

    float const travel_x = destination.x() - transform->position.x;
    float const travel_z = destination.z() - transform->position.z;
    float const travel_distance =
        std::sqrt((travel_x * travel_x) + (travel_z * travel_z));
    resident->errand_remaining = std::max(
        k_min_travel_allowance, travel_distance * k_travel_allowance_per_metre);

    CommandService::MoveOptions options;
    options.kind = MoveOrderKind::ScriptedMove;
    CommandService::move_unit(*world, entity->get_id(), destination, options);
  }
}

} // namespace Game::Systems
