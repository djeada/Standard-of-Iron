#include "elephant_special_processor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <numbers>
#include <span>

#include "../../core/component.h"
#include "../../core/entity.h"
#include "../../core/world.h"
#include "../../units/spawn_type.h"
#include "combat_hit_resolver.h"
#include "combat_random.h"
#include "combat_utils.h"
#include "damage_application.h"

namespace Game::Systems::Combat {

void expire_knockback_cooldowns(
    Engine::Core::ElephantKnockbackCooldownComponent& cooldowns, float delta_time) {
  auto& entries = cooldowns.cooldowns;
  for (auto it = entries.begin(); it != entries.end();) {
    it->remaining -= delta_time;
    if (it->remaining <= 0.0F) {
      it = entries.erase(it);
    } else {
      ++it;
    }
  }
}

namespace {

struct FootOffset {
  float x;
  float z;
};

constexpr float k_pi = std::numbers::pi_v<float>;

[[nodiscard]] auto get_entity_from_query_context(
    const CombatQueryContext& query_context,
    Engine::Core::EntityID entity_id) -> Engine::Core::Entity* {
  return query_context.find_entity(entity_id);
}

[[nodiscard]] auto
is_motion_active(const Engine::Core::Entity& entity) noexcept -> bool {
  auto const* motion =
      entity.get_component<Engine::Core::MotionPresentationComponent>();
  return motion != nullptr && motion->has_locomotion();
}

void add_all_foot_stomps(Engine::Core::TransformComponent* transform,
                         Engine::Core::ElephantComponent* elephant_comp,
                         Engine::Core::ElephantStompImpactComponent* stomp_impact) {
  float const scale = std::max(1.0F, (transform->scale.x + transform->scale.z) * 0.5F);
  float const lat = elephant_comp->foot_lateral * scale;
  float const fwd = elephant_comp->foot_forward * scale;

  std::array<FootOffset, 4> const foot_locals = {{
      {lat, fwd},
      {-lat, fwd},
      {lat, -fwd},
      {-lat, -fwd},
  }};

  float const yaw = transform->rotation.y * (k_pi / 180.0F);
  float const cos_y = std::cos(yaw);
  float const sin_y = std::sin(yaw);

  for (const auto& local : foot_locals) {
    Engine::Core::ElephantStompImpactComponent::ImpactRecord impact{};
    impact.x = transform->position.x + local.x * cos_y + local.z * sin_y;
    impact.z = transform->position.z - local.x * sin_y + local.z * cos_y;
    impact.time = 0.0F;
    stomp_impact->impacts.push_back(impact);
  }
}

auto apply_stomp_damage(Engine::Core::Entity& elephant,
                        Engine::Core::World& world,
                        std::span<Engine::Core::Entity* const> units,
                        int damage,
                        float minimum_launch_speed) -> bool {
  auto* elephant_comp = elephant.get_component<Engine::Core::ElephantComponent>();
  auto* unit = elephant.get_component<Engine::Core::UnitComponent>();
  auto* transform = elephant.get_component<Engine::Core::TransformComponent>();
  if (elephant_comp == nullptr || unit == nullptr || transform == nullptr ||
      unit->health <= 0 || damage <= 0) {
    return false;
  }

  bool hit_any = false;
  for (auto* other_entity : units) {
    if (other_entity == nullptr || other_entity == &elephant ||
        !is_valid_enemy_unit(unit, other_entity, false)) {
      continue;
    }

    auto* other_unit = other_entity->get_component<Engine::Core::UnitComponent>();
    auto* other_transform =
        other_entity->get_component<Engine::Core::TransformComponent>();
    if (other_unit == nullptr || other_transform == nullptr) {
      continue;
    }

    float const dx = other_transform->position.x - transform->position.x;
    float const dz = other_transform->position.z - transform->position.z;
    if (std::hypot(dx, dz) > elephant_comp->trample_radius) {
      continue;
    }

    int const old_health = other_unit->health;
    auto const application =
        apply_unit_damage(&world, other_entity, damage, elephant.get_id());
    bool const infantry_target =
        !Game::Units::is_cavalry(other_unit->spawn_type) &&
        other_unit->spawn_type != Game::Units::SpawnType::Elephant &&
        other_unit->spawn_type != Game::Units::SpawnType::Catapult &&
        other_unit->spawn_type != Game::Units::SpawnType::Ballista;
    if (infantry_target) {
      float impact_speed = minimum_launch_speed;
      if (auto const* motion =
              elephant.get_component<Engine::Core::MotionPresentationComponent>();
          motion != nullptr) {
        impact_speed = std::max(impact_speed, motion->speed);
      }
      launch_new_casualties(
          *other_entity, elephant, application.queued_soldier_casualties, impact_speed);
    }
    hit_any = hit_any || (old_health > 0 && other_unit->health < old_health);
  }
  return hit_any;
}

void publish_foot_stomps(Engine::Core::Entity& elephant) {
  auto* elephant_comp = elephant.get_component<Engine::Core::ElephantComponent>();
  auto* transform = elephant.get_component<Engine::Core::TransformComponent>();
  if (elephant_comp == nullptr || transform == nullptr) {
    return;
  }
  auto* impacts =
      Engine::Core::get_or_add_component<Engine::Core::ElephantStompImpactComponent>(
          &elephant);
  if (impacts != nullptr) {
    add_all_foot_stomps(transform, elephant_comp, impacts);
  }
}

[[nodiscard]] auto is_elephant_panicked(Engine::Core::Entity* elephant) -> bool {
  auto* panic = elephant->get_component<Engine::Core::ElephantPanicComponent>();
  return panic != nullptr && panic->duration > 0.0F;
}

void begin_panic(Engine::Core::Entity* elephant, float duration) {
  auto* panic =
      Engine::Core::get_or_add_component<Engine::Core::ElephantPanicComponent>(
          elephant);
  panic->duration = duration;
}

void process_panic_mechanic(Engine::Core::Entity* elephant, float delta_time) {
  auto* panic = elephant->get_component<Engine::Core::ElephantPanicComponent>();
  if (panic == nullptr) {
    return;
  }

  panic->duration -= delta_time;

  if (panic->duration <= 0.0F) {
    elephant->remove_component<Engine::Core::ElephantPanicComponent>();
  }
}

void process_charge_attack(Engine::Core::Entity* elephant,
                           const CombatQueryContext& query_context,
                           float delta_time) {
  auto* elephant_comp = elephant->get_component<Engine::Core::ElephantComponent>();
  auto* unit = elephant->get_component<Engine::Core::UnitComponent>();
  auto* transform = elephant->get_component<Engine::Core::TransformComponent>();
  auto* movement = elephant->get_component<Engine::Core::MovementComponent>();
  auto* attack_target = elephant->get_component<Engine::Core::AttackTargetComponent>();

  if (elephant_comp == nullptr || unit == nullptr || transform == nullptr ||
      movement == nullptr) {
    return;
  }

  switch (elephant_comp->charge_state) {
  case Engine::Core::ElephantComponent::ChargeState::Idle: {
    if (attack_target == nullptr || attack_target->target_id == 0 ||
        elephant_comp->charge_cooldown > 0.0F || is_elephant_panicked(elephant)) {
      break;
    }

    auto* target =
        get_entity_from_query_context(query_context, attack_target->target_id);
    if (!is_valid_enemy_unit(unit, target, false)) {
      break;
    }

    auto* target_transform = target->get_component<Engine::Core::TransformComponent>();
    if (target_transform == nullptr) {
      break;
    }

    float const dx = target_transform->position.x - transform->position.x;
    float const dz = target_transform->position.z - transform->position.z;
    float const dist = std::sqrt(dx * dx + dz * dz);

    if (dist >= 5.0F && dist <= 15.0F) {
      elephant_comp->charge_state =
          Engine::Core::ElephantComponent::ChargeState::Charging;
      elephant_comp->charge_duration = 3.0F;
    }
    break;
  }

  case Engine::Core::ElephantComponent::ChargeState::Charging:
    elephant_comp->charge_duration -= delta_time;
    if (elephant_comp->charge_duration <= 0.0F) {
      elephant_comp->charge_state =
          Engine::Core::ElephantComponent::ChargeState::Recovering;
      elephant_comp->charge_cooldown = 8.0F;
    }
    break;

  case Engine::Core::ElephantComponent::ChargeState::Recovering:
    elephant_comp->charge_state = Engine::Core::ElephantComponent::ChargeState::Idle;
    break;

  default:
    break;
  }
}

void process_trample_damage(Engine::Core::Entity* elephant,
                            Engine::Core::World* world,
                            const CombatQueryContext& query_context,
                            float delta_time) {
  auto* elephant_comp = elephant->get_component<Engine::Core::ElephantComponent>();
  auto* unit = elephant->get_component<Engine::Core::UnitComponent>();
  auto* transform = elephant->get_component<Engine::Core::TransformComponent>();

  if (elephant_comp == nullptr || unit == nullptr || transform == nullptr) {
    return;
  }

  bool const is_moving = is_motion_active(*elephant);
  if (!is_moving) {
    elephant_comp->trample_damage_accumulator = 0.0F;
    return;
  }

  elephant_comp->trample_damage_accumulator +=
      static_cast<float>(elephant_comp->trample_damage) * delta_time;
  constexpr float k_moving_footfall_interval = 0.28F;
  int const footfall_damage = std::max(
      1,
      static_cast<int>(std::lround(static_cast<float>(elephant_comp->trample_damage) *
                                   k_moving_footfall_interval)));
  int const footfalls =
      static_cast<int>(elephant_comp->trample_damage_accumulator) / footfall_damage;
  if (footfalls <= 0) {
    return;
  }
  int const damage = footfalls * footfall_damage;
  elephant_comp->trample_damage_accumulator -= static_cast<float>(damage);

  if (apply_stomp_damage(*elephant, *world, query_context.units, damage, 5.5F)) {
    publish_foot_stomps(*elephant);
  } else {
    elephant_comp->trample_damage_accumulator = 0.0F;
  }
}

void process_elephant(Engine::Core::Entity* entity,
                      Engine::Core::World* world,
                      const CombatQueryContext& query_context,
                      float delta_time) {
  auto* unit = entity->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr || unit->health <= 0 ||
      unit->spawn_type != Game::Units::SpawnType::Elephant) {
    return;
  }

  auto* elephant =
      Engine::Core::get_or_add_component<Engine::Core::ElephantComponent>(entity);

  float const health_ratio =
      static_cast<float>(unit->health) / static_cast<float>(unit->max_health);
  if (health_ratio < 0.3F && !is_elephant_panicked(entity)) {
    std::uint32_t const panic_seed =
        static_cast<std::uint32_t>(entity->get_id() * 2246822519U) ^
        static_cast<std::uint32_t>(unit->health * 3266489917U);
    if (deterministic_unit_roll(panic_seed, 0xE1E471U) < 0.5F) {
      begin_panic(entity, 10.0F);
    }
  }

  if (is_elephant_panicked(entity)) {
    process_panic_mechanic(entity, delta_time);
  }

  if (elephant->charge_cooldown > 0.0F) {
    elephant->charge_cooldown -= delta_time;
  }

  process_charge_attack(entity, query_context, delta_time);
  process_trample_damage(entity, world, query_context, delta_time);
}

} // namespace

auto apply_elephant_stomp_impact(Engine::Core::World* world,
                                 Engine::Core::Entity* elephant) -> bool {
  if (world == nullptr || elephant == nullptr) {
    return false;
  }
  auto* elephant_comp = elephant->get_component<Engine::Core::ElephantComponent>();
  if (elephant_comp == nullptr) {
    return false;
  }
  auto units = world->collect_entities_with<Engine::Core::UnitComponent>();
  bool const hit = apply_stomp_damage(
      *elephant, *world, units, std::max(1, elephant_comp->trample_damage), 4.5F);
  if (hit) {
    publish_foot_stomps(*elephant);
  }
  return hit;
}

void process_elephant_specials(Engine::Core::World* world,
                               const CombatQueryContext& query_context,
                               float delta_time) {
  if (world == nullptr) {
    return;
  }

  for (auto* entity : query_context.units) {
    process_elephant(entity, world, query_context, delta_time);
  }
}

} // namespace Game::Systems::Combat
