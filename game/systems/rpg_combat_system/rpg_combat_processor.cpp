#include "rpg_combat_processor.h"

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

#include "../../core/ambient_session.h"
#include "../../core/component_commander.h"
#include "../../core/world.h"
#include "../combat_system/combat_random.h"
#include "../combat_system/combat_utils.h"
#include "../command_service.h"
#include "../formation_combat_geometry.h"
#include "../owner_registry.h"

namespace Game::Systems::RpgCombat {

namespace {

constexpr float k_pi = std::numbers::pi_v<float>;
constexpr float k_radians_to_degrees = 180.0F / k_pi;
constexpr float k_degrees_to_radians = k_pi / 180.0F;

constexpr float k_engaged_turn_rate_degrees = 260.0F;

constexpr float k_obstruction_cone_dot = 0.93F;
constexpr float k_obstruction_distance = 2.4F;

constexpr float k_press_sector_degrees = 46.0F;

constexpr float k_offscreen_press_arc_degrees = 100.0F;
constexpr int k_max_offscreen_pressers = 1;

constexpr float k_press_tenure_seconds = 3.0F;
constexpr float k_press_tenure_penalty = 2.4F;

constexpr float k_pressure_epoch_seconds = 1.35F;

constexpr float k_waiting_band_near = 2.3F;
constexpr float k_waiting_band_far = 4.1F;

constexpr float k_goal_hysteresis = 0.65F;

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

void issue_local_move(Engine::Core::World& world,
                      Engine::Core::EntityID entity_id,
                      const Engine::Core::TransformComponent& transform,
                      const QVector3D& target) {
  float const dx = target.x() - transform.position.x;
  float const dz = target.z() - transform.position.z;
  if ((dx * dx) + (dz * dz) <= 0.04F) {
    return;
  }

  auto* entity = world.get_entity(entity_id);
  auto const* movement = entity != nullptr
                             ? entity->get_component<Engine::Core::MovementComponent>()
                             : nullptr;
  if (movement != nullptr && movement->get_has_target()) {
    float const goal_dx = movement->get_goal_x() - target.x();
    float const goal_dz = movement->get_goal_y() - target.z();
    if ((goal_dx * goal_dx) + (goal_dz * goal_dz) <
        k_goal_hysteresis * k_goal_hysteresis) {
      return;
    }
  }

  CommandService::MoveOptions options;
  options.kind = MoveOrderKind::ScriptedMove;
  CommandService::move_unit(world, entity_id, target, options);
}

void turn_body_toward(Engine::Core::TransformComponent& transform,
                      float target_yaw_degrees,
                      float delta_time) {
  float const current =
      transform.has_desired_yaw ? transform.desired_yaw : transform.rotation.y;
  float gap = std::fmod((target_yaw_degrees - current + 540.0F), 360.0F) - 180.0F;
  float const step = std::clamp(gap,
                                -k_engaged_turn_rate_degrees * delta_time,
                                k_engaged_turn_rate_degrees * delta_time);
  transform.desired_yaw = current + step;
  transform.has_desired_yaw = true;
}

struct EngagedFighter {
  float x{0.0F};
  float z{0.0F};
};

} // namespace

auto ideal_engage_distance(const Engine::Core::Entity& attacker,
                           Engine::Core::Entity& commander) -> float {
  auto const* attack = attacker.get_component<Engine::Core::AttackComponent>();
  float const reach = attack != nullptr ? attack->melee_range
                                        : Engine::Core::Defaults::k_attack_melee_range;

  return std::max(reach + Game::Systems::Combat::combat_radius(&commander) - 0.15F,
                  1.2F);
}

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

  std::vector<Engine::Core::EntityID> previously_pressing;
  previously_pressing.reserve(engagement->engagement_slots.size());
  for (auto const& slot : engagement->engagement_slots) {
    if (slot.pressing) {
      previously_pressing.push_back(slot.entity_id);
    }
  }

  engagement->engagement_slots.clear();
  engagement->fight_context = Engine::Core::FightContext::None;

  float const yaw_radians = commander_transform->rotation.y * k_degrees_to_radians;
  float const forward_x = std::sin(yaw_radians);
  float const forward_z = std::cos(yaw_radians);
  float const radius_sq = engagement->ring_radius * engagement->ring_radius;
  auto& owners = *Game::Session::services_for(*world).owners;

  std::vector<EngagedFighter> fighters;
  bool has_formation_opponent = false;
  for (auto [candidate_ref, unit_ref, transform_ref] :
       world->entity_view<Engine::Core::UnitComponent,
                          Engine::Core::TransformComponent>()) {
    Engine::Core::Entity* candidate = &candidate_ref;
    auto* unit = &unit_ref;
    auto* transform = &transform_ref;
    if (candidate->get_id() == commander_id) {
      continue;
    }

    if (unit->health <= 0 ||
        !owners.are_enemies(commander_unit->owner_id, unit->owner_id)) {
      continue;
    }

    float const dx = transform->position.x - commander_transform->position.x;
    float const dz = transform->position.z - commander_transform->position.z;
    float const dist_sq = (dx * dx) + (dz * dz);
    if (dist_sq <= 0.0001F || dist_sq > radius_sq) {
      continue;
    }

    Engine::Core::RpgEngagementComponent::Slot slot;
    slot.entity_id = candidate->get_id();
    slot.distance = std::sqrt(dist_sq);
    slot.signed_angle_degrees = signed_angle_degrees(forward_x, forward_z, dx, dz);
    engagement->engagement_slots.push_back(slot);
    fighters.push_back({.x = transform->position.x, .z = transform->position.z});

    if (!has_formation_opponent && FormationCombat::has_formation_slots(*candidate)) {
      has_formation_opponent = true;
    }
  }

  if (engagement->engagement_slots.empty()) {
    return;
  }

  engagement->fight_context = has_formation_opponent
                                  ? Engine::Core::FightContext::Skirmish
                                  : Engine::Core::FightContext::Duel;

  float const commander_x = commander_transform->position.x;
  float const commander_z = commander_transform->position.z;
  for (std::size_t i = 0; i < engagement->engagement_slots.size(); ++i) {
    auto& slot = engagement->engagement_slots[i];
    float const to_commander_x = commander_x - fighters[i].x;
    float const to_commander_z = commander_z - fighters[i].z;
    float const to_commander_len =
        std::max(0.0001F, std::hypot(to_commander_x, to_commander_z));
    float const dir_x = to_commander_x / to_commander_len;
    float const dir_z = to_commander_z / to_commander_len;

    for (std::size_t other = 0; other < fighters.size(); ++other) {
      if (other == i) {
        continue;
      }
      float const other_x = fighters[other].x - fighters[i].x;
      float const other_z = fighters[other].z - fighters[i].z;
      float const other_len = std::hypot(other_x, other_z);
      if (other_len <= 0.0001F || other_len > k_obstruction_distance ||
          other_len > to_commander_len) {
        continue;
      }
      float const alignment =
          ((dir_x * other_x) + (dir_z * other_z)) / std::max(other_len, 0.0001F);
      if (alignment >= k_obstruction_cone_dot) {
        slot.obstructed = true;
        break;
      }
    }
  }

  auto const epoch =
      static_cast<std::uint32_t>(engagement->pressure_clock / k_pressure_epoch_seconds);
  std::vector<std::size_t> order;
  order.reserve(engagement->engagement_slots.size());
  std::vector<float> scores(engagement->engagement_slots.size(), 0.0F);
  for (std::size_t i = 0; i < engagement->engagement_slots.size(); ++i) {
    auto const& slot = engagement->engagement_slots[i];
    if (slot.obstructed) {
      continue;
    }
    float score = slot.distance + (std::abs(slot.signed_angle_degrees) * 0.008F);
    score += Game::Systems::Combat::deterministic_unit_roll(
                 static_cast<std::uint32_t>(slot.entity_id), epoch) *
             1.1F;
    if (std::find(previously_pressing.begin(),
                  previously_pressing.end(),
                  slot.entity_id) != previously_pressing.end()) {

      auto const tenure = std::find_if(
          engagement->press_tenure.begin(),
          engagement->press_tenure.end(),
          [&slot](auto const& entry) { return entry.entity_id == slot.entity_id; });
      float const held_for = tenure != engagement->press_tenure.end()
                                 ? engagement->pressure_clock - tenure->started_at
                                 : 0.0F;
      score += held_for >= k_press_tenure_seconds ? k_press_tenure_penalty : -0.55F;
    }
    scores[i] = score;
    order.push_back(i);
  }
  std::sort(order.begin(), order.end(), [&scores](std::size_t lhs, std::size_t rhs) {
    return scores[lhs] < scores[rhs];
  });

  int const crowd = static_cast<int>(engagement->engagement_slots.size());
  int const capacity = std::clamp(1 + (crowd / 3), 1, 4);
  std::vector<float> taken_bearings;
  taken_bearings.reserve(static_cast<std::size_t>(capacity));
  int offscreen_pressers = 0;
  for (std::size_t const index : order) {
    if (static_cast<int>(taken_bearings.size()) >= capacity) {
      break;
    }
    auto& slot = engagement->engagement_slots[index];

    bool const offscreen =
        std::abs(slot.signed_angle_degrees) > k_offscreen_press_arc_degrees;
    if (offscreen && offscreen_pressers >= k_max_offscreen_pressers) {
      continue;
    }
    bool sector_taken = false;
    for (float const bearing : taken_bearings) {
      float gap = std::fmod(slot.signed_angle_degrees - bearing + 540.0F, 360.0F);
      gap -= 180.0F;
      if (std::abs(gap) < k_press_sector_degrees) {
        sector_taken = true;
        break;
      }
    }
    if (sector_taken) {
      continue;
    }
    slot.pressing = true;
    offscreen_pressers += offscreen ? 1 : 0;
    taken_bearings.push_back(slot.signed_angle_degrees);
  }

  std::vector<Engine::Core::RpgEngagementComponent::PressTenure> refreshed_tenure;
  refreshed_tenure.reserve(engagement->press_tenure.size() + 1U);
  for (auto const& slot : engagement->engagement_slots) {
    if (!slot.pressing) {
      continue;
    }
    auto const existing = std::find_if(
        engagement->press_tenure.begin(),
        engagement->press_tenure.end(),
        [&slot](auto const& entry) { return entry.entity_id == slot.entity_id; });
    refreshed_tenure.push_back({.entity_id = slot.entity_id,
                                .started_at = existing != engagement->press_tenure.end()
                                                  ? existing->started_at
                                                  : engagement->pressure_clock});
  }
  engagement->press_tenure = std::move(refreshed_tenure);

  std::sort(
      engagement->engagement_slots.begin(),
      engagement->engagement_slots.end(),
      [](const auto& lhs, const auto& rhs) { return lhs.distance < rhs.distance; });
}

void tick_rpg_combat(Engine::Core::World* world,
                     Engine::Core::EntityID commander_id,
                     float dt) {
  if (world == nullptr || commander_id == 0) {
    return;
  }

  if (auto* commander = world->get_entity(commander_id); commander != nullptr) {
    if (auto* engagement =
            commander->get_component<Engine::Core::RpgEngagementComponent>()) {
      engagement->pressure_clock += dt;
    }
  }

  refresh_commander_engagement(world, commander_id);

  std::vector<Engine::Core::EntityID> recovered;
  for (auto [staggered_ref, stagger_ref] :
       world->entity_view<Engine::Core::StaggerComponent>()) {
    Engine::Core::Entity* staggered = &staggered_ref;
    auto* stagger = &stagger_ref;
    stagger->remaining -= dt;
    if (stagger->remaining <= 0.0F) {
      recovered.push_back(staggered->get_id());
    } else {
      auto* fb = Engine::Core::get_or_add_component<Engine::Core::HitFeedbackComponent>(
          staggered);
      if (fb != nullptr) {
        fb->is_reacting = true;
        fb->reaction_time = 0.0F;
        fb->stagger_tier = stagger->tier;

        switch (stagger->tier) {
        case Engine::Core::StaggerTier::LightFlinch:
          fb->reaction_intensity = 0.4F;
          break;
        case Engine::Core::StaggerTier::HeavyStagger:
          fb->reaction_intensity = 0.75F;
          break;
        case Engine::Core::StaggerTier::Knockback:
          fb->reaction_intensity = 1.0F;
          fb->knockback_x = fb->hit_direction_x * 0.18F;
          fb->knockback_z = fb->hit_direction_z * 0.18F;
          break;
        case Engine::Core::StaggerTier::Knockdown:
          fb->reaction_intensity = 1.0F;
          fb->knockback_x = fb->hit_direction_x * 0.12F;
          fb->knockback_z = fb->hit_direction_z * 0.12F;
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

  for (const Engine::Core::EntityID entity_id : recovered) {
    world->remove<Engine::Core::StaggerComponent>(entity_id);
  }

  auto* engagement = entity->get_component<Engine::Core::RpgEngagementComponent>();
  auto* cmd_transform = entity->get_component<Engine::Core::TransformComponent>();
  if (engagement == nullptr || cmd_transform == nullptr) {
    return;
  }

  const float cmd_x = cmd_transform->position.x;
  const float cmd_z = cmd_transform->position.z;

  std::vector<QVector3D> fighter_positions;
  fighter_positions.reserve(engagement->engagement_slots.size());
  for (auto const& slot : engagement->engagement_slots) {
    auto const* fighter = world->get_entity(slot.entity_id);
    auto const* fighter_tf =
        fighter != nullptr ? fighter->get_component<Engine::Core::TransformComponent>()
                           : nullptr;
    fighter_positions.emplace_back(
        fighter_tf != nullptr ? fighter_tf->position.x : cmd_x,
        0.0F,
        fighter_tf != nullptr ? fighter_tf->position.z : cmd_z);
  }

  float const commander_forward_x =
      std::sin(cmd_transform->rotation.y * k_degrees_to_radians);
  float const commander_forward_z =
      std::cos(cmd_transform->rotation.y * k_degrees_to_radians);

  std::size_t slot_index = 0;
  for (auto& slot : engagement->engagement_slots) {
    std::size_t const own_index = slot_index++;
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

    if (enemy_stagger != nullptr && enemy_stagger->remaining > 0.0F) {
      continue;
    }

    if (FormationCombat::has_formation_slots(*enemy)) {
      continue;
    }

    float dx = enemy_tf->position.x - cmd_x;
    float const dz = enemy_tf->position.z - cmd_z;
    float dist = std::sqrt((dx * dx) + (dz * dz));
    if (dist < 0.001F) {
      dist = 0.001F;
      dx = 0.001F;
    }

    float const nx = dx / dist;
    float const nz = dz / dist;

    if (slot.pressing) {

      float const engage_distance = ideal_engage_distance(*enemy, *entity);
      if (dist > engage_distance + 0.35F) {
        issue_local_move(*world,
                         enemy->get_id(),
                         *enemy_tf,
                         QVector3D(cmd_x + (nx * engage_distance * 0.55F),
                                   0.0F,
                                   cmd_z + (nz * engage_distance * 0.55F)));
      }
    } else {

      constexpr std::array<float, 5> k_probe_degrees = {
          0.0F, 27.0F, -27.0F, 58.0F, -58.0F};
      float const own_bearing = std::atan2(dx, dz);
      float best_score = -1.0e9F;
      QVector3D best_goal(enemy_tf->position.x, 0.0F, enemy_tf->position.z);
      float const band_target =
          std::clamp(dist, k_waiting_band_near, k_waiting_band_far);
      for (float const probe : k_probe_degrees) {
        float const bearing = own_bearing + (probe * k_degrees_to_radians);
        float const goal_x = cmd_x + (std::sin(bearing) * band_target);
        float const goal_z = cmd_z + (std::cos(bearing) * band_target);

        float clearance = 0.0F;
        for (std::size_t other = 0; other < fighter_positions.size(); ++other) {
          if (other == own_index) {
            continue;
          }
          float const gap = std::hypot(fighter_positions[other].x() - goal_x,
                                       fighter_positions[other].z() - goal_z);
          clearance += std::min(gap, 3.0F);
        }

        float const commander_facing = std::abs(signed_angle_degrees(
            commander_forward_x, commander_forward_z, goal_x - cmd_x, goal_z - cmd_z));
        float const score =
            clearance + (commander_facing * 0.012F) - (std::abs(probe) * 0.006F);
        if (score > best_score) {
          best_score = score;
          best_goal = QVector3D(goal_x, 0.0F, goal_z);
        }
      }
      issue_local_move(*world, enemy->get_id(), *enemy_tf, best_goal);
    }

    turn_body_toward(*enemy_tf, std::atan2(-dx, -dz) * k_radians_to_degrees, dt);
  }
}

} // namespace Game::Systems::RpgCombat
