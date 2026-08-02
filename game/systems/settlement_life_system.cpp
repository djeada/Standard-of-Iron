#include "settlement_life_system.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <vector>

#include "../core/component.h"
#include "../core/world.h"
#include "../map/map_definition.h"
#include "../map/terrain_service.h"
#include "building_collision_registry.h"
#include "command_service.h"

namespace Game::Systems {
namespace {

using Engine::Core::SettlementErrand;
using Engine::Core::SettlementResidentComponent;

constexpr float k_building_standoff = 1.15F;
constexpr float k_prop_standoff = 1.25F;
constexpr float k_travel_allowance_per_metre = 1.6F;
constexpr float k_min_travel_allowance = 6.0F;
constexpr int k_pick_attempts = 6;

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
};

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
  return errand;
}

auto errand_at_prop(const QVector3D& prop_position, std::uint32_t& rng) -> Errand {
  float const angle = random_range(rng, 0.0F, 6.2831853F);
  float const reach = k_prop_standoff + random_range(rng, 0.0F, 0.9F);

  Errand errand;
  errand.x = prop_position.x() + (std::cos(angle) * reach);
  errand.z = prop_position.z() + (std::sin(angle) * reach);
  errand.dwell = random_range(rng, 4.0F, 9.5F);
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
  for (auto* entity : world.get_entities_with<Engine::Core::BuildingComponent>()) {
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if ((unit == nullptr) || (transform == nullptr) || unit->health <= 0) {
      continue;
    }
    if (unit->owner_id != owner_id) {
      continue;
    }
    if (Game::Units::is_wall_network_spawn(unit->spawn_type)) {
      continue;
    }
    float const dx = transform->position.x - resident.hearth_x;
    float const dz = transform->position.z - resident.hearth_z;
    if ((dx * dx) + (dz * dz) > radius_sq) {
      continue;
    }
    out.push_back({Candidate::Kind::Building, entity->get_id(), {}});
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

} // namespace

void SettlementLifeSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  auto residents = world->get_entities_with<SettlementResidentComponent>();
  if (residents.empty()) {
    return;
  }

  std::vector<Candidate> candidates;

  for (auto* entity : residents) {
    auto* resident = entity->get_component<SettlementResidentComponent>();
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    auto* movement = entity->get_component<Engine::Core::MovementComponent>();

    if ((resident == nullptr) || (unit == nullptr) || (transform == nullptr) ||
        (movement == nullptr) || unit->health <= 0) {
      continue;
    }

    auto const* attack_target =
        entity->get_component<Engine::Core::AttackTargetComponent>();
    bool const busy_elsewhere =
        ((attack_target != nullptr) && attack_target->target_id != 0) ||
        entity->has_component<Engine::Core::CivilianDeliveryComponent>() ||
        entity->has_component<Engine::Core::PlayerOrderIntentComponent>();
    if (busy_elsewhere) {
      resident->errand = SettlementErrand::Settling;
      resident->focus_id = 0;
      continue;
    }

    if (resident->rng_state == 0U) {
      resident->rng_state =
          static_cast<std::uint32_t>((entity->get_id() * 2654435761ULL) | 1ULL);
      if (!resident->hearth_assigned) {
        resident->hearth_x = transform->position.x;
        resident->hearth_z = transform->position.z;
        resident->hearth_assigned = true;
      }

      resident->think_cooldown = random_range(resident->rng_state, 0.0F, 2.5F);
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
      resident->errand_remaining = random_range(resident->rng_state, 1.0F, 3.0F);
      continue;
    }

    QVector3D const destination =
        CommandService::snap_to_walkable_ground(QVector3D(chosen.x, 0.0F, chosen.z));

    resident->errand = SettlementErrand::WalkingTo;
    resident->focus_id = chosen.focus;
    resident->errand_x = destination.x();
    resident->errand_z = destination.z();
    resident->planned_dwell = chosen.dwell;

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
