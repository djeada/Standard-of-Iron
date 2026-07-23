#include "elephant_special_processor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <numbers>

#include "../../core/component.h"
#include "../../core/entity.h"
#include "../../core/world.h"
#include "../../units/spawn_type.h"
#include "combat_hit_resolver.h"
#include "combat_random.h"
#include "combat_utils.h"
#include "damage_application.h"

namespace Game::Systems::Combat {

namespace {

struct FootOffset {
  float x;
  float z;
};

constexpr float k_pi = std::numbers::pi_v<float>;

[[nodiscard]] auto get_entity_from_query_context(
    const CombatQueryContext& query_context,
    Engine::Core::EntityID entity_id) -> Engine::Core::Entity* {
  auto const it = query_context.entities_by_id.find(entity_id);
  return (it != query_context.entities_by_id.end()) ? it->second : nullptr;
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

[[nodiscard]] auto is_elephant_panicked(Engine::Core::Entity* elephant) -> bool {
  auto* panic = elephant->get_component<Engine::Core::ElephantPanicComponent>();
  return panic != nullptr && panic->duration > 0.0F;
}

void begin_panic(Engine::Core::Entity* elephant, float duration) {
  auto* panic = elephant->get_component<Engine::Core::ElephantPanicComponent>();
  if (panic == nullptr) {
    panic = elephant->add_component<Engine::Core::ElephantPanicComponent>();
  }
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
  auto* attack = elephant->get_component<Engine::Core::AttackComponent>();
  auto* attack_target = elephant->get_component<Engine::Core::AttackTargetComponent>();

  if (elephant_comp == nullptr || unit == nullptr || transform == nullptr) {
    return;
  }

  bool const is_moving = is_motion_active(*elephant);

  bool has_close_target = false;
  if (!is_moving && attack_target != nullptr && attack_target->target_id != 0) {
    auto* target =
        get_entity_from_query_context(query_context, attack_target->target_id);
    if (is_valid_enemy_unit(unit, target, false)) {
      auto* target_transform =
          target->get_component<Engine::Core::TransformComponent>();
      if (target_transform != nullptr) {
        float const dx = target_transform->position.x - transform->position.x;
        float const dz = target_transform->position.z - transform->position.z;
        float const dist = std::sqrt(dx * dx + dz * dz);
        float const engage_range =
            (attack != nullptr)
                ? std::max(elephant_comp->trample_radius, attack->melee_range)
                : elephant_comp->trample_radius;
        has_close_target = (dist <= engage_range);
      }
    }
  }

  if (!is_moving && !has_close_target) {
    elephant_comp->trample_damage_accumulator = 0.0F;
    return;
  }

  elephant_comp->trample_damage_accumulator +=
      static_cast<float>(elephant_comp->trample_damage) * delta_time;
  int const damage = static_cast<int>(elephant_comp->trample_damage_accumulator);
  if (damage <= 0) {
    return;
  }

  auto* stomp_impact =
      elephant->get_component<Engine::Core::ElephantStompImpactComponent>();
  if (stomp_impact == nullptr) {
    stomp_impact =
        elephant->add_component<Engine::Core::ElephantStompImpactComponent>();
  }

  bool hit_any = false;
  for (auto* other_entity : query_context.units) {
    if (other_entity == elephant) {
      continue;
    }
    if (!is_valid_enemy_unit(unit, other_entity, false)) {
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
    float const dist = std::sqrt(dx * dx + dz * dz);

    if (dist <= elephant_comp->trample_radius) {
      int const old_health = other_unit->health;
      auto const application =
          apply_unit_damage(world, other_entity, damage, elephant->get_id());

      bool const infantry_target =
          !Game::Units::is_cavalry(other_unit->spawn_type) &&
          other_unit->spawn_type != Game::Units::SpawnType::Elephant &&
          other_unit->spawn_type != Game::Units::SpawnType::Catapult &&
          other_unit->spawn_type != Game::Units::SpawnType::Ballista;
      if (infantry_target) {
        float impact_speed = 5.5F;
        if (auto const* motion =
                elephant->get_component<Engine::Core::MotionPresentationComponent>();
            motion != nullptr) {
          impact_speed = std::max(impact_speed, motion->speed);
        }
        launch_new_casualties(*other_entity,
                              *elephant,
                              application.queued_soldier_casualties,
                              impact_speed);
      }

      if (old_health > 0 && other_unit->health < old_health) {
        hit_any = true;
      }
    }
  }

  if (hit_any) {
    add_all_foot_stomps(transform, elephant_comp, stomp_impact);
    elephant_comp->trample_damage_accumulator -= damage;
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

  auto* elephant = entity->get_component<Engine::Core::ElephantComponent>();
  if (elephant == nullptr) {
    elephant = entity->add_component<Engine::Core::ElephantComponent>();
  }

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
