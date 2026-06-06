#include "rpg_combat_processor.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

#include "../../core/component.h"
#include "../../core/world.h"
#include "../command_service.h"
#include "../owner_registry.h"

namespace Game::Systems::RpgCombat {

namespace {

constexpr float k_pi = std::numbers::pi_v<float>;
constexpr float k_radians_to_degrees = 180.0F / k_pi;

auto signed_angle_degrees(float forward_x,
                          float forward_z,
                          float to_x,
                          float to_z) -> float {
  float const len = std::sqrt(to_x * to_x + to_z * to_z);
  if (len <= 0.0001F) {
    return 0.0F;
  }
  to_x /= len;
  to_z /= len;
  float const dot = std::clamp(forward_x * to_x + forward_z * to_z, -1.0F, 1.0F);
  float const cross = forward_z * to_x - forward_x * to_z;
  return std::atan2(cross, dot) * k_radians_to_degrees;
}

auto slot_is_active(const Engine::Core::RpgEngagementComponent::Slot& slot) -> bool {
  return slot.role == Engine::Core::RpgEngagementRole::FrontAttacker ||
         slot.role == Engine::Core::RpgEngagementRole::LeftThreat ||
         slot.role == Engine::Core::RpgEngagementRole::RightThreat;
}

void issue_scripted_position_goal(Engine::Core::World& world,
                                  Engine::Core::EntityID entity_id,
                                  const QVector3D& target) {
  auto* entity = world.get_entity(entity_id);
  auto* transform = entity != nullptr
                        ? entity->get_component<Engine::Core::TransformComponent>()
                        : nullptr;
  if (transform == nullptr) {
    return;
  }

  float const dx = target.x() - transform->position.x;
  float const dz = target.z() - transform->position.z;
  if (dx * dx + dz * dz <= 0.04F) {
    return;
  }

  CommandService::MoveOptions options;
  options.kind = MoveOrderKind::ScriptedMove;
  CommandService::move_unit(world, entity_id, target, options);
}

} // namespace

void refresh_commander_engagement(Engine::Core::World* world,
                                  Engine::Core::EntityID commander_id) {
  if (world == nullptr || commander_id == 0) {
    return;
  }

  auto* commander = world->get_entity(commander_id);
  if (commander == nullptr) {
    return;
  }

  auto* commander_transform =
      commander->get_component<Engine::Core::TransformComponent>();
  auto* commander_unit = commander->get_component<Engine::Core::UnitComponent>();
  if (commander_transform == nullptr || commander_unit == nullptr) {
    commander->remove_component<Engine::Core::RpgEngagementComponent>();
    return;
  }

  auto* engagement =
      Engine::Core::get_or_add_component<Engine::Core::RpgEngagementComponent>(
          commander);
  if (engagement == nullptr) {
    return;
  }

  engagement->engagement_slots.clear();
  engagement->front_attacker_id = 0;
  engagement->left_threat_id = 0;
  engagement->right_threat_id = 0;
  engagement->active_attackers = 0;

  float const yaw_radians = commander_transform->rotation.y * (k_pi / 180.0F);
  float const forward_x = std::sin(yaw_radians);
  float const forward_z = std::cos(yaw_radians);
  float const radius_sq = engagement->ring_radius * engagement->ring_radius;
  auto& owners = Game::Systems::OwnerRegistry::instance();

  for (auto* candidate : world->get_entities_with<Engine::Core::UnitComponent>()) {
    if (candidate == nullptr || candidate->get_id() == commander_id) {
      continue;
    }

    auto* unit = candidate->get_component<Engine::Core::UnitComponent>();
    auto* transform = candidate->get_component<Engine::Core::TransformComponent>();
    if (unit == nullptr || transform == nullptr || unit->health <= 0 ||
        !owners.are_enemies(commander_unit->owner_id, unit->owner_id)) {
      continue;
    }

    float const dx = transform->position.x - commander_transform->position.x;
    float const dz = transform->position.z - commander_transform->position.z;
    float const dist_sq = dx * dx + dz * dz;
    if (dist_sq <= 0.0001F || dist_sq > radius_sq) {
      continue;
    }

    Engine::Core::RpgEngagementComponent::Slot slot;
    slot.entity_id = candidate->get_id();
    slot.distance = std::sqrt(dist_sq);
    slot.signed_angle_degrees = signed_angle_degrees(forward_x, forward_z, dx, dz);
    engagement->engagement_slots.push_back(slot);
  }

  std::sort(
      engagement->engagement_slots.begin(),
      engagement->engagement_slots.end(),
      [](const auto& lhs, const auto& rhs) { return lhs.distance < rhs.distance; });

  auto choose_best = [&](auto predicate) -> Engine::Core::EntityID {
    float best_score = std::numeric_limits<float>::infinity();
    Engine::Core::EntityID best_id = 0;
    for (auto const& slot : engagement->engagement_slots) {
      if (slot_is_active(slot) || !predicate(slot)) {
        continue;
      }
      float const score = slot.distance + std::abs(slot.signed_angle_degrees) * 0.025F;
      if (score < best_score) {
        best_score = score;
        best_id = slot.entity_id;
      }
    }
    return best_id;
  };

  auto assign_role = [&](Engine::Core::EntityID id,
                         Engine::Core::RpgEngagementRole role) {
    if (id == 0) {
      return;
    }
    for (auto& slot : engagement->engagement_slots) {
      if (slot.entity_id == id) {
        slot.role = role;
        ++engagement->active_attackers;
        return;
      }
    }
  };

  engagement->front_attacker_id = choose_best(
      [](const auto& slot) { return std::abs(slot.signed_angle_degrees) <= 65.0F; });
  assign_role(engagement->front_attacker_id,
              Engine::Core::RpgEngagementRole::FrontAttacker);

  engagement->left_threat_id = choose_best([](const auto& slot) {
    return slot.signed_angle_degrees < -30.0F && slot.signed_angle_degrees >= -135.0F;
  });
  assign_role(engagement->left_threat_id, Engine::Core::RpgEngagementRole::LeftThreat);

  engagement->right_threat_id = choose_best([](const auto& slot) {
    return slot.signed_angle_degrees > 30.0F && slot.signed_angle_degrees <= 135.0F;
  });
  assign_role(engagement->right_threat_id,
              Engine::Core::RpgEngagementRole::RightThreat);
}

void tick_rpg_combat(Engine::Core::World* world,
                     Engine::Core::EntityID commander_id,
                     float dt) {
  if (world == nullptr || commander_id == 0) {
    return;
  }

  refresh_commander_engagement(world, commander_id);

  for (auto* staggered : world->get_entities_with<Engine::Core::StaggerComponent>()) {
    auto* stagger = staggered->get_component<Engine::Core::StaggerComponent>();
    if (stagger == nullptr) {
      continue;
    }
    stagger->remaining -= dt;
    if (stagger->remaining <= 0.0F) {
      staggered->remove_component<Engine::Core::StaggerComponent>();
    } else {
      auto* fb = staggered->get_component<Engine::Core::HitFeedbackComponent>();
      if (fb == nullptr) {
        fb = staggered->add_component<Engine::Core::HitFeedbackComponent>();
      }
      if (fb != nullptr) {
        fb->is_reacting = true;
        fb->reaction_time = 0.0F;
        fb->stagger_tier = stagger->tier;
        // Scale intensity and knockback based on stagger tier
        switch (stagger->tier) {
        case Engine::Core::StaggerTier::LightFlinch:
          fb->reaction_intensity = 0.4F;
          break;
        case Engine::Core::StaggerTier::HeavyStagger:
          fb->reaction_intensity = 0.75F;
          break;
        case Engine::Core::StaggerTier::Knockback:
          fb->reaction_intensity = 1.0F;
          fb->knockback_x = fb->hit_direction_x * 0.12F;
          fb->knockback_z = fb->hit_direction_z * 0.12F;
          break;
        case Engine::Core::StaggerTier::Knockdown:
          fb->reaction_intensity = 1.0F;
          fb->knockback_x = fb->hit_direction_x * 0.08F;
          fb->knockback_z = fb->hit_direction_z * 0.08F;
          break;
        case Engine::Core::StaggerTier::GuardBreak:
          fb->reaction_intensity = 0.85F;
          break;
        }
      }
    }
  }

  auto* entity = world->get_entity(commander_id);
  if (entity == nullptr) {
    return;
  }

  auto* rpg = entity->get_component<Engine::Core::RpgHealthComponent>();
  auto* unit = entity->get_component<Engine::Core::UnitComponent>();
  if (rpg != nullptr && rpg->active && rpg->rpg_hp > 0 && unit != nullptr &&
      unit->health <= 0) {
    unit->health = 1;
  }

  // Tick enemy telegraph phases
  for (auto* telegraphed :
       world->get_entities_with<Engine::Core::EnemyTelegraphComponent>()) {
    auto* telegraph =
        telegraphed->get_component<Engine::Core::EnemyTelegraphComponent>();
    if (telegraph == nullptr ||
        telegraph->phase == Engine::Core::EnemyTelegraphPhase::None) {
      continue;
    }
    telegraph->phase_time += dt;
    switch (telegraph->phase) {
    case Engine::Core::EnemyTelegraphPhase::WindUp:
      telegraph->visual_tell_active = true;
      if (telegraph->phase_time >= telegraph->wind_up_duration) {
        telegraph->phase = Engine::Core::EnemyTelegraphPhase::Active;
        telegraph->phase_time = 0.0F;
      }
      break;
    case Engine::Core::EnemyTelegraphPhase::Active:
      telegraph->visual_tell_active = false;
      if (telegraph->phase_time >= telegraph->active_duration) {
        telegraph->phase = Engine::Core::EnemyTelegraphPhase::Recovery;
        telegraph->phase_time = 0.0F;
      }
      break;
    case Engine::Core::EnemyTelegraphPhase::Recovery:
      if (telegraph->phase_time >= telegraph->recovery_duration) {
        telegraph->phase = Engine::Core::EnemyTelegraphPhase::None;
        telegraph->phase_time = 0.0F;
      }
      break;
    case Engine::Core::EnemyTelegraphPhase::None:
      break;
    }
  }

  // ─── Enemy RPG combat AI: circling, spacing, turn-based engagement ───
  auto* engagement = entity->get_component<Engine::Core::RpgEngagementComponent>();
  auto* cmd_transform = entity->get_component<Engine::Core::TransformComponent>();
  if (engagement == nullptr || cmd_transform == nullptr) {
    return;
  }

  const float cmd_x = cmd_transform->position.x;
  const float cmd_z = cmd_transform->position.z;

  // Enemies circle the player and maintain spacing
  constexpr float k_ideal_engage_distance = 2.8F;
  constexpr float k_circle_speed = 1.4F;

  for (auto& slot : engagement->engagement_slots) {
    auto* enemy = world->get_entity(slot.entity_id);
    if (enemy == nullptr) {
      continue;
    }
    auto* enemy_tf = enemy->get_component<Engine::Core::TransformComponent>();
    auto* enemy_unit = enemy->get_component<Engine::Core::UnitComponent>();
    auto* enemy_stagger = enemy->get_component<Engine::Core::StaggerComponent>();
    if (enemy_tf == nullptr || enemy_unit == nullptr || enemy_unit->health <= 0) {
      continue;
    }
    // Don't move if staggered
    if (enemy_stagger != nullptr && enemy_stagger->remaining > 0.0F) {
      continue;
    }

    float dx = enemy_tf->position.x - cmd_x;
    float const dz = enemy_tf->position.z - cmd_z;
    float dist = std::sqrt(dx * dx + dz * dz);
    if (dist < 0.001F) {
      dist = 0.001F;
      dx = 0.001F;
    }

    float const nx = dx / dist;
    float const nz = dz / dist;
    bool const is_active_attacker = slot_is_active(slot);

    if (is_active_attacker) {
      if (dist > k_ideal_engage_distance + 0.5F) {
        issue_scripted_position_goal(*world,
                                     enemy->get_id(),
                                     QVector3D(cmd_x + nx * k_ideal_engage_distance,
                                               0.0F,
                                               cmd_z + nz * k_ideal_engage_distance));
      } else if (dist < k_ideal_engage_distance - 0.3F) {
        issue_scripted_position_goal(*world,
                                     enemy->get_id(),
                                     QVector3D(cmd_x + nx * k_ideal_engage_distance,
                                               0.0F,
                                               cmd_z + nz * k_ideal_engage_distance));
      }
      float const face_angle = std::atan2(-dx, -dz) * k_radians_to_degrees;
      enemy_tf->rotation.y = face_angle;
    } else {
      constexpr float k_support_ring = 4.5F;
      float const circle_dir = (slot.signed_angle_degrees >= 0.0F) ? 1.0F : -1.0F;
      float const angle = std::atan2(dz, dx) + circle_dir * k_circle_speed * dt;
      issue_scripted_position_goal(*world,
                                   enemy->get_id(),
                                   QVector3D(cmd_x + std::cos(angle) * k_support_ring,
                                             0.0F,
                                             cmd_z + std::sin(angle) * k_support_ring));
      float const face_angle = std::atan2(-dx, -dz) * k_radians_to_degrees;
      enemy_tf->rotation.y = face_angle;
    }
  }
}

} // namespace Game::Systems::RpgCombat
