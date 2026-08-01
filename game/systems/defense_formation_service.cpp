#include "defense_formation_service.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <unordered_map>

#include "../core/component.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../units/spawn_type.h"
#include "command_service.h"
#include "nation_registry.h"
#include "order_service.h"

namespace Game::Systems {

namespace {

constexpr float k_slot_arrival_distance = 0.55F;
constexpr float k_min_formation_units = 2.0F;

std::uint64_t g_next_formation_id = 1;

[[nodiscard]] auto degrees_from_direction(float dx, float dz) -> float {
  return std::atan2(dx, dz) * 180.0F / std::numbers::pi_v<float>;
}

[[nodiscard]] auto signed_angle_delta(float from_degrees, float to_degrees) -> float {
  return std::fmod((to_degrees - from_degrees + 540.0F), 360.0F) - 180.0F;
}

[[nodiscard]] auto troop_type_of(const Engine::Core::Entity& entity)
    -> std::optional<Game::Units::TroopType> {
  const auto* unit = entity.get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return std::nullopt;
  }
  return Game::Units::spawn_typeToTroopType(unit->spawn_type);
}

[[nodiscard]] auto is_alive(const Engine::Core::Entity& entity) -> bool {
  const auto* unit = entity.get_component<Engine::Core::UnitComponent>();
  return unit != nullptr && unit->health > 0 &&
         !entity.has_component<Engine::Core::PendingRemovalComponent>();
}

[[nodiscard]] auto eligible_for(const Engine::Core::Entity& entity,
                                const DefenseFormationProfile& profile) -> bool {
  auto const troop = troop_type_of(entity);
  return troop.has_value() && profile.is_eligible_troop(*troop) && is_alive(entity);
}

} // namespace

auto DefenseFormationService::profile_for(const Engine::Core::Entity& entity)
    -> const DefenseFormationProfile* {
  const auto* unit = entity.get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return nullptr;
  }
  const auto* nation = NationRegistry::instance().get_nation(unit->nation_id);
  if (nation == nullptr || !nation->defense_formation.has_value()) {
    return nullptr;
  }
  return &nation->defense_formation.value();
}

auto DefenseFormationService::can_form(Engine::Core::World& world,
                                       const std::vector<Engine::Core::EntityID>& units)
    -> bool {
  const DefenseFormationProfile* profile = nullptr;
  int eligible = 0;

  for (auto id : units) {
    auto* entity = world.get_entity(id);
    if (entity == nullptr) {
      continue;
    }
    const auto* candidate = profile_for(*entity);
    if (candidate == nullptr) {
      continue;
    }
    if (profile == nullptr) {
      profile = candidate;
    } else if (profile->id != candidate->id) {
      continue;
    }
    if (eligible_for(*entity, *profile)) {
      ++eligible;
    }
  }

  return profile != nullptr && eligible >= profile->min_units;
}

auto DefenseFormationService::begin(Engine::Core::World& world,
                                    const std::vector<Engine::Core::EntityID>& units,
                                    const QVector3D& anchor,
                                    bool has_anchor) -> bool {
  const DefenseFormationProfile* profile = nullptr;
  std::vector<Engine::Core::Entity*> members;
  members.reserve(units.size());

  for (auto id : units) {
    auto* entity = world.get_entity(id);
    if (entity == nullptr) {
      continue;
    }
    const auto* candidate = profile_for(*entity);
    if (candidate == nullptr) {
      continue;
    }
    if (profile == nullptr) {
      profile = candidate;
    } else if (profile->id != candidate->id) {
      continue;
    }
    if (eligible_for(*entity, *profile)) {
      members.push_back(entity);
    }
  }

  if (profile == nullptr || static_cast<int>(members.size()) < profile->min_units) {
    return false;
  }

  QVector3D centre(0.0F, 0.0F, 0.0F);
  for (const auto* member : members) {
    const auto* transform = member->get_component<Engine::Core::TransformComponent>();
    if (transform != nullptr) {
      centre += QVector3D(transform->position.x, 0.0F, transform->position.z);
    }
  }
  centre /= std::max(k_min_formation_units, static_cast<float>(members.size()));

  QVector3D facing_target = has_anchor ? anchor : centre + QVector3D(0.0F, 0.0F, 1.0F);
  QVector3D facing = facing_target - centre;
  facing.setY(0.0F);
  if (facing.lengthSquared() <= 1.0e-4F) {
    facing = QVector3D(0.0F, 0.0F, 1.0F);
  }
  facing.normalize();
  QVector3D const right(facing.z(), 0.0F, -facing.x());
  float const facing_degrees = degrees_from_direction(facing.x(), facing.z());

  std::sort(members.begin(),
            members.end(),
            [&right, &facing](const Engine::Core::Entity* lhs,
                              const Engine::Core::Entity* rhs) {
              const auto* lhs_t =
                  lhs->get_component<Engine::Core::TransformComponent>();
              const auto* rhs_t =
                  rhs->get_component<Engine::Core::TransformComponent>();
              if (lhs_t == nullptr || rhs_t == nullptr) {
                return lhs < rhs;
              }
              QVector3D const lhs_pos(lhs_t->position.x, 0.0F, lhs_t->position.z);
              QVector3D const rhs_pos(rhs_t->position.x, 0.0F, rhs_t->position.z);
              float const lhs_depth = QVector3D::dotProduct(lhs_pos, facing);
              float const rhs_depth = QVector3D::dotProduct(rhs_pos, facing);
              if (std::fabs(lhs_depth - rhs_depth) > 0.5F) {
                return lhs_depth > rhs_depth;
              }
              return QVector3D::dotProduct(lhs_pos, right) <
                     QVector3D::dotProduct(rhs_pos, right);
            });

  int const per_rank = std::max(1, profile->max_units_per_rank);
  int const ranks =
      (static_cast<int>(members.size()) + per_rank - 1) / std::max(1, per_rank);
  std::uint64_t const formation_id = g_next_formation_id++;

  for (std::size_t index = 0; index < members.size(); ++index) {
    auto* entity = members[index];
    int const rank = static_cast<int>(index) / per_rank;
    int const file = static_cast<int>(index) % per_rank;
    int const rank_size =
        std::min(per_rank, static_cast<int>(members.size()) - rank * per_rank);

    float const file_centre = (static_cast<float>(rank_size) - 1.0F) * 0.5F;
    float const rank_centre = (static_cast<float>(ranks) - 1.0F) * 0.5F;

    QVector3D const slot =
        centre +
        right * ((static_cast<float>(file) - file_centre) * profile->file_spacing) +
        facing * ((rank_centre - static_cast<float>(rank)) * profile->rank_spacing);

    auto* formation =
        Engine::Core::get_or_add_component<Engine::Core::DefenseFormationComponent>(
            entity);
    if (formation == nullptr) {
      continue;
    }

    formation->state = Engine::Core::DefenseFormationState::Forming;
    formation->formation_id = formation_id;
    formation->slot_index = static_cast<int>(index);
    formation->rank = rank;
    formation->file = file;
    formation->slot_x = slot.x();
    formation->slot_z = slot.z();
    formation->facing_degrees = facing_degrees;
    formation->state_time = 0.0F;
    formation->cohesion = 1.0F;

    if (auto* guard = entity->get_component<Engine::Core::GuardModeComponent>()) {
      guard->guard_position_x = slot.x();
      guard->guard_position_z = slot.z();
      guard->has_guard_target = true;
      guard->returning_to_guard_position = false;
    }

    CommandService::MoveOptions options;
    options.kind = MoveOrderKind::GuardReturn;
    options.preserve_formation_mode = true;
    CommandService::move_unit(world, entity->get_id(), slot, options);
  }

  return true;
}

void DefenseFormationService::begin_break(Engine::Core::Entity* entity) {
  if (entity == nullptr) {
    return;
  }
  auto* formation = entity->get_component<Engine::Core::DefenseFormationComponent>();
  if (formation == nullptr || !formation->is_engaged()) {
    return;
  }
  formation->state = Engine::Core::DefenseFormationState::Breaking;
  formation->state_time = 0.0F;
}

void DefenseFormationService::clear(Engine::Core::Entity* entity) {
  if (entity == nullptr) {
    return;
  }
  entity->remove_component<Engine::Core::DefenseFormationComponent>();
}

auto DefenseFormationService::damage_multiplier(
    const Engine::Core::Entity& target,
    const DefenseFormationDamageContext& context) -> float {
  const auto* formation =
      target.get_component<Engine::Core::DefenseFormationComponent>();
  if (formation == nullptr || !formation->is_formed()) {
    return 1.0F;
  }
  const auto* profile = profile_for(target);
  const auto* transform = target.get_component<Engine::Core::TransformComponent>();
  if (profile == nullptr || transform == nullptr) {
    return 1.0F;
  }

  float const dx = context.attack_origin.x() - transform->position.x;
  float const dz = context.attack_origin.z() - transform->position.z;
  if ((dx * dx) + (dz * dz) <= 1.0e-6F) {
    return 1.0F;
  }

  float const incoming_degrees = degrees_from_direction(dx, dz);
  float const delta =
      std::fabs(signed_angle_delta(formation->facing_degrees, incoming_degrees));

  float multiplier = 1.0F;
  if (delta <= profile->frontal_arc_degrees * 0.5F) {
    multiplier = context.is_missile ? profile->frontal_missile_multiplier
                                    : profile->frontal_melee_multiplier;
  } else if (delta >= 180.0F - (profile->frontal_arc_degrees * 0.5F)) {
    multiplier = profile->rear_multiplier;
  } else {
    multiplier = profile->flank_multiplier;
  }

  if (context.is_cavalry_impact) {
    multiplier *= profile->cavalry_impact_multiplier;
  }

  return std::max(0.0F, multiplier);
}

auto DefenseFormationService::attack_output_multiplier(
    const Engine::Core::Entity& entity) -> float {
  const auto* formation =
      entity.get_component<Engine::Core::DefenseFormationComponent>();
  if (formation == nullptr || !formation->is_formed()) {
    return 1.0F;
  }
  const auto* profile = profile_for(entity);
  return profile != nullptr ? std::max(0.0F, profile->attack_output_multiplier) : 1.0F;
}

auto DefenseFormationService::move_speed_multiplier(const Engine::Core::Entity& entity)
    -> float {
  const auto* formation =
      entity.get_component<Engine::Core::DefenseFormationComponent>();
  if (formation == nullptr || !formation->is_formed()) {
    return 1.0F;
  }
  const auto* profile = profile_for(entity);
  return profile != nullptr ? std::clamp(profile->move_speed_multiplier, 0.05F, 1.0F)
                            : 1.0F;
}

auto DefenseFormationService::turn_speed_multiplier(const Engine::Core::Entity& entity)
    -> float {
  const auto* formation =
      entity.get_component<Engine::Core::DefenseFormationComponent>();
  if (formation == nullptr || !formation->is_engaged()) {
    return 1.0F;
  }
  const auto* profile = profile_for(entity);
  return profile != nullptr ? std::clamp(profile->turn_speed_multiplier, 0.05F, 1.0F)
                            : 1.0F;
}

auto DefenseFormationService::blocks_charge(const Engine::Core::Entity& entity)
    -> bool {
  const auto* formation =
      entity.get_component<Engine::Core::DefenseFormationComponent>();
  if (formation == nullptr || !formation->is_engaged()) {
    return false;
  }
  const auto* profile = profile_for(entity);
  return profile != nullptr && !profile->allows_charge;
}

void DefenseFormationSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  auto entities = world->get_entities_with<Engine::Core::DefenseFormationComponent>();
  if (entities.empty()) {
    return;
  }

  struct FormationTally {
    int members{0};
    int in_place{0};
  };
  std::unordered_map<std::uint64_t, FormationTally> tallies;

  for (auto* entity : entities) {
    auto* formation = entity->get_component<Engine::Core::DefenseFormationComponent>();
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (formation == nullptr || transform == nullptr || !formation->is_engaged()) {
      continue;
    }
    if (!is_alive(*entity)) {
      continue;
    }

    auto& tally = tallies[formation->formation_id];
    ++tally.members;

    float const dx = formation->slot_x - transform->position.x;
    float const dz = formation->slot_z - transform->position.z;
    float const distance_sq = (dx * dx) + (dz * dz);

    const auto* profile = DefenseFormationService::profile_for(*entity);
    float const cohesion_radius =
        profile != nullptr ? profile->cohesion_radius : k_slot_arrival_distance;
    if (distance_sq <= cohesion_radius * cohesion_radius) {
      ++tally.in_place;
    }
  }

  for (auto* entity : entities) {
    auto* formation = entity->get_component<Engine::Core::DefenseFormationComponent>();
    if (formation == nullptr) {
      continue;
    }

    if (!is_alive(*entity)) {
      DefenseFormationService::clear(entity);
      continue;
    }

    const auto* profile = DefenseFormationService::profile_for(*entity);
    if (profile == nullptr) {
      DefenseFormationService::clear(entity);
      continue;
    }

    formation->state_time += delta_time;

    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (transform != nullptr && formation->is_engaged()) {
      transform->desired_yaw = formation->facing_degrees;
      transform->has_desired_yaw = true;
    }

    auto const tally = tallies.find(formation->formation_id);
    int const members = tally != tallies.end() ? tally->second.members : 0;
    int const in_place = tally != tallies.end() ? tally->second.in_place : 0;
    formation->cohesion =
        members > 0 ? static_cast<float>(in_place) / static_cast<float>(members) : 0.0F;

    switch (formation->state) {
    case Engine::Core::DefenseFormationState::Forming: {
      bool arrived = false;
      if (transform != nullptr) {
        float const dx = formation->slot_x - transform->position.x;
        float const dz = formation->slot_z - transform->position.z;
        arrived =
            (dx * dx) + (dz * dz) <= k_slot_arrival_distance * k_slot_arrival_distance;
      }
      if (arrived || formation->state_time >= profile->form_seconds) {
        formation->state = Engine::Core::DefenseFormationState::Formed;
        formation->state_time = 0.0F;
        if (auto* movement = entity->get_component<Engine::Core::MovementComponent>()) {
          movement->stop();
        }
      }
      break;
    }
    case Engine::Core::DefenseFormationState::Formed: {
      bool const lost_cohesion = members < profile->min_units ||
                                 formation->cohesion < profile->min_cohesion_ratio;
      if (lost_cohesion) {
        formation->state = Engine::Core::DefenseFormationState::Breaking;
        formation->state_time = 0.0F;
      }
      break;
    }
    case Engine::Core::DefenseFormationState::Breaking: {
      if (formation->state_time >= profile->break_seconds) {
        DefenseFormationService::clear(entity);
      }
      break;
    }
    case Engine::Core::DefenseFormationState::Normal:
      DefenseFormationService::clear(entity);
      break;
    }
  }
}

} // namespace Game::Systems
