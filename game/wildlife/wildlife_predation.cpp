#include "wildlife_predation.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

#include "../audio/cue_ids.h"
#include "../core/event_manager.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "../systems/combat_system/combat_utils.h"
#include "../systems/combat_system/damage_application.h"
#include "../systems/formation_combat_geometry.h"
#include "../util/planar_math.h"

namespace Game::Wildlife {

namespace {

constexpr float k_wolf_bite_recovery = 0.35F;
constexpr float k_prey_slot_hysteresis = 0.5F;
constexpr float k_wolf_bite_facing_degrees = 12.0F;
constexpr float k_wolf_contact_facing_degrees = 15.0F;
constexpr float k_wolf_contact_facing_max_degrees = 60.0F;
constexpr float k_wolf_bite_flinch_seconds = 0.30F;
constexpr float k_wolf_bite_blood_chance = 0.34F;
constexpr float k_wolf_bite_blood_spread = 0.55F;

auto hash_unit_interval(std::uint32_t value) -> float {
  value ^= value >> 16U;
  value *= 2246822519U;
  value ^= value >> 13U;
  value *= 3266489917U;
  value ^= value >> 16U;
  return static_cast<float>(value & 0xFFFFFFU) / 16777216.0F;
}

auto is_fighting_back(const Engine::Core::World& world,
                      Engine::Core::EntityID entity_id) -> bool {
  if (const auto* attack = world.try_get<Engine::Core::AttackComponent>(entity_id);
      attack != nullptr && attack->in_melee_lock) {
    return true;
  }
  const auto* target = world.try_get<Engine::Core::AttackTargetComponent>(entity_id);
  return target != nullptr && target->target_id != 0;
}

static auto prey_escape_allowance(Engine::Core::World& world,
                                  const PreyRef& prey) -> float {
  constexpr float k_impact_delay =
      Engine::Core::WildlifeComponent::k_bite_animation_seconds *
      Engine::Core::WildlifeComponent::k_bite_impact_phase;
  if (!prey.valid()) {
    return 0.0F;
  }
  const auto* unit = world.try_get<Engine::Core::UnitComponent>(prey.id);
  float const speed = unit != nullptr ? std::max(0.0F, unit->speed) : 0.0F;
  return speed * k_impact_delay;
}

} // namespace

auto resolve_prey(Engine::Core::World& world,
                  Engine::Core::EntityID entity_id,
                  float hunter_x,
                  float hunter_z,
                  const Engine::Core::Entity* hunter) -> PreyRef {
  auto* entity = world.get_entity(entity_id);
  if (entity == nullptr ||
      entity->has_component<Engine::Core::PendingRemovalComponent>()) {
    return {};
  }
  const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
  const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
  if (unit == nullptr || transform == nullptr || unit->health <= 0) {
    return {};
  }

  PreyRef prey;
  prey.entity = entity;
  prey.id = entity_id;
  prey.x = transform->position.x;
  prey.z = transform->position.z;
  prey.radius = std::max(0.0F, std::max(transform->scale.x, transform->scale.z) * 0.5F);
  prey.livestock = world.has<Engine::Core::WildlifeComponent>(entity_id);

  if (!prey.livestock && Game::Systems::FormationCombat::has_formation_slots(*entity)) {
    auto const layout = Game::Systems::FormationCombat::resolve_layout(*entity);
    thread_local std::vector<Game::Systems::FormationCombat::SoldierSpatialAnchor>
        anchors;
    Game::Systems::FormationCombat::soldier_spatial_anchors_into(
        *entity, layout, anchors);
    float nearest_sq = std::numeric_limits<float>::max();
    for (const auto& anchor : anchors) {
      float const dx = anchor.world_x - hunter_x;
      float const dz = anchor.world_z - hunter_z;
      nearest_sq = std::min(nearest_sq, (dx * dx) + (dz * dz));
    }
    auto const* hunter_transform =
        hunter != nullptr && hunter->registry() != nullptr
            ? hunter->registry()->try_get<Engine::Core::TransformComponent>(
                  hunter->get_id())
            : nullptr;
    float const tolerance = std::sqrt(nearest_sq) + k_prey_slot_hysteresis;
    float best_score = std::numeric_limits<float>::max();
    for (const auto& anchor : anchors) {
      float const dx = anchor.world_x - hunter_x;
      float const dz = anchor.world_z - hunter_z;
      float const distance_sq = (dx * dx) + (dz * dz);
      if (distance_sq > tolerance * tolerance) {
        continue;
      }
      float score = distance_sq;
      if (hunter_transform != nullptr) {
        float const bearing = std::atan2(dx, dz) * 180.0F / std::numbers::pi_v<float>;
        score = std::abs(
            Game::Systems::signed_yaw_delta(hunter_transform->rotation.y, bearing));
      }
      if (score < best_score) {
        best_score = score;
        prey.x = anchor.world_x;
        prey.z = anchor.world_z;
        prey.radius = layout.body_radius;
      }
    }
  }
  return prey;
}

WildlifePredation::WildlifePredation(WildlifeStats& stats)
    : m_stats(stats) {
}

auto WildlifePredation::begin_bite(Engine::Core::Entity& entity,
                                   Engine::Core::WildlifeComponent& wildlife,
                                   const PreyRef& prey,
                                   float hunter_x,
                                   float hunter_z) -> bool {
  if (wildlife.state_timer > 0.0F || wildlife.bite_timer > 0.0F) {
    return false;
  }
  const auto* attack = entity.get_component<Engine::Core::AttackComponent>();
  if (attack == nullptr) {
    return false;
  }

  if (Game::Systems::Combat::structure_separates_positions(
          QVector3D(hunter_x, 0.0F, hunter_z), QVector3D(prey.x, 0.0F, prey.z))) {
    return false;
  }

  auto* transform = entity.get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return false;
  }
  float const dx = prey.x - hunter_x;
  float const dz = prey.z - hunter_z;
  float const yaw = std::atan2(dx, dz) * 180.0F / std::numbers::pi_v<float>;
  transform->desired_yaw = yaw;
  transform->has_desired_yaw = true;
  float const reach = k_wolf_bite_windup_range + prey.radius;
  if (dx * dx + dz * dz > reach * reach ||
      std::abs(Game::Systems::signed_yaw_delta(transform->rotation.y, yaw)) >
          k_wolf_bite_facing_degrees) {
    return false;
  }

  wildlife.state_timer = std::max(k_wolf_bite_recovery, attack->melee_cooldown);
  wildlife.bite_timer = Engine::Core::WildlifeComponent::k_bite_animation_seconds;
  wildlife.bite_target_id = prey.id;
  wildlife.bite_impact_pending = true;
  m_stats.bites += 1U;

  if (auto* movement = entity.get_component<Engine::Core::MovementComponent>();
      movement != nullptr) {
    movement->stop();
  }

  return true;
}

void WildlifePredation::try_contact_bite(Engine::Core::World& world,
                                         const AnimalRef& animal,
                                         Engine::Core::WildlifeComponent& wildlife) {
  if (wildlife.focus_id == 0 || wildlife.state_timer > 0.0F ||
      wildlife.bite_timer > 0.0F) {
    return;
  }
  PreyRef const prey =
      resolve_prey(world, wildlife.focus_id, animal.x, animal.z, animal.entity);
  if (!prey.valid()) {
    wildlife.focus_id = 0;
    return;
  }

  float const dx = prey.x - animal.x;
  float const dz = prey.z - animal.z;
  float const reach = k_wolf_bite_windup_range + prey.radius;
  if ((dx * dx) + (dz * dz) > reach * reach) {
    return;
  }

  begin_bite(*animal.entity, wildlife, prey, animal.x, animal.z);
}

void WildlifePredation::hurt_prey(Engine::Core::World& world,
                                  const AnimalRef& animal,
                                  const PreyRef& prey,
                                  int melee_damage) {
  const auto damage = Game::Systems::Combat::apply_unit_damage(
      &world, prey.entity, melee_damage, animal.entity->get_id());
  auto bite = Engine::Core::AudioCueEvent::for_owner(
      Engine::Core::owner_id_of(prey.entity), Game::Audio::Cue::k_wildlife_wolf_bite);
  bite.at(prey.x, 0.0F, prey.z);
  Engine::Core::EventManager::instance().publish(bite);

  const auto* prey_unit = prey.entity->get_component<Engine::Core::UnitComponent>();
  if (prey_unit != nullptr && is_civilian_spawn(prey_unit->spawn_type) &&
      !is_fighting_back(world, prey.entity->get_id())) {
    Game::Systems::Combat::add_or_extend_stagger(
        prey.entity,
        k_wolf_bite_flinch_seconds,
        Engine::Core::StaggerTier::LightFlinch);
  }

  auto const bite_salt =
      static_cast<std::uint32_t>((animal.id * 2654435761ULL) + m_stats.bites);
  if (damage.applied_damage > 0 && !damage.killed &&
      hash_unit_interval(bite_salt) < k_wolf_bite_blood_chance) {
    Game::Systems::Combat::spawn_blood_stain(
        &world, prey.entity, k_wolf_bite_blood_spread, bite_salt);
  }
}

void WildlifePredation::land_bite(Engine::Core::World& world,
                                  const AnimalRef& animal,
                                  const Engine::Core::WildlifeComponent& wildlife) {
  PreyRef const prey =
      resolve_prey(world, wildlife.bite_target_id, animal.x, animal.z, animal.entity);
  auto const* wolf_transform =
      animal.entity->get_component<Engine::Core::TransformComponent>();
  auto const* attack = animal.entity->get_component<Engine::Core::AttackComponent>();
  if (!prey.valid() || wolf_transform == nullptr || attack == nullptr) {
    return;
  }

  float const dx = prey.x - wolf_transform->position.x;
  float const dz = prey.z - wolf_transform->position.z;
  float const escape = prey_escape_allowance(world, prey);
  float const contact_reach = k_wolf_bite_range + prey.radius + escape;
  float const distance = std::sqrt((dx * dx) + (dz * dz));
  float const yaw = std::atan2(dx, dz) * 180.0F / std::numbers::pi_v<float>;

  float const swing_degrees = std::atan2(escape, std::max(distance, 0.05F)) * 180.0F /
                              std::numbers::pi_v<float>;
  float const facing_allowance = std::min(k_wolf_contact_facing_degrees + swing_degrees,
                                          k_wolf_contact_facing_max_degrees);
  bool const facing = std::abs(std::remainder(yaw - wolf_transform->rotation.y,
                                              360.0F)) <= facing_allowance;

  bool const walled_off = Game::Systems::Combat::structure_separates_positions(
      QVector3D(wolf_transform->position.x, 0.0F, wolf_transform->position.z),
      QVector3D(prey.x, 0.0F, prey.z));
  if (!walled_off && facing && (dx * dx) + (dz * dz) <= contact_reach * contact_reach) {
    hurt_prey(world, animal, prey, attack->melee_damage);
  }
}

void WildlifePredation::advance_bite(Engine::Core::World& world,
                                     const AnimalRef& animal,
                                     Engine::Core::WildlifeComponent& wildlife,
                                     float delta_time) {
  float const bite_timer_before = wildlife.bite_timer;
  wildlife.bite_timer = std::max(0.0F, wildlife.bite_timer - delta_time);
  constexpr float k_bite_impact_remaining =
      Engine::Core::WildlifeComponent::k_bite_animation_seconds *
      (1.0F - Engine::Core::WildlifeComponent::k_bite_impact_phase);
  if (wildlife.bite_impact_pending && bite_timer_before > k_bite_impact_remaining &&
      wildlife.bite_timer <= k_bite_impact_remaining) {
    land_bite(world, animal, wildlife);
    wildlife.bite_impact_pending = false;
    wildlife.bite_target_id = 0;
  } else if (wildlife.bite_timer <= 0.0F) {
    wildlife.bite_impact_pending = false;
    wildlife.bite_target_id = 0;
  }
}

} // namespace Game::Wildlife
