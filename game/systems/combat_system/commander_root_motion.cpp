#include "commander_root_motion.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>

#include "../../core/world.h"
#include "../../util/planar_math.h"
#include "../combat_rules.h"
#include "../duel_spacing.h"
#include "../navigation/pathfinding.h"
#include "../navigation/walkability.h"
#include "../rpg_combat_system/rpg_targeting.h"
#include "combat_utils.h"

namespace Game::Systems::Combat {

namespace {

auto authored_drive(float s) -> float {
  s = std::clamp(s, 0.0F, 1.0F);
  return s * (2.0F - s);
}

constexpr float k_rts_commander_root_motion_max_speed = 5.5F;
constexpr float k_rts_commander_gap_closer_max_speed = 12.0F;

constexpr float k_rts_commander_lunge_clearance_per_scale = 0.70F;

constexpr float k_rts_commander_assist_turn_degrees_per_second = 360.0F;

struct RootMotionBody {
  Engine::Core::TransformComponent* transform{nullptr};
  const Engine::Core::MovementComponent* movement{nullptr};
  float own_radius{0.45F};
  float forward_x{0.0F};
  float forward_z{1.0F};
};

[[nodiscard]] auto authored_step(
    const Engine::Core::RpgCommanderActionComponent& action,
    const Game::Systems::CombatActions::CombatActionDefinition& definition) -> float {
  auto const& profile = definition.movement;
  float const window = profile.end_normalized - profile.start_normalized;
  float const s_prev =
      (action.previous_normalized_action_time - profile.start_normalized) / window;
  float const s_now =
      (action.normalized_action_time - profile.start_normalized) / window;
  float const elapsed = std::max(
      0.0F,
      (action.normalized_action_time - action.previous_normalized_action_time) *
          std::max(0.001F, action.action_duration));
  float const max_speed =
      definition.role == Game::Systems::CombatActions::CommanderActionRole::GapCloser
          ? k_rts_commander_gap_closer_max_speed
          : k_rts_commander_root_motion_max_speed;
  return std::min(profile.distance * (authored_drive(s_now) - authored_drive(s_prev)),
                  max_speed * elapsed);
}

[[nodiscard]] auto nearest_soldier(Engine::Core::Entity& target,
                                   const Engine::Core::TransformComponent& from,
                                   float& nearest_distance)
    -> std::optional<Game::Systems::RpgCombat::SoldierTarget> {
  std::optional<Game::Systems::RpgCombat::SoldierTarget> nearest;
  nearest_distance = 0.0F;
  for (auto const& soldier : Game::Systems::RpgCombat::live_soldier_targets(target)) {
    float const distance = std::hypot(soldier.position.x() - from.position.x,
                                      soldier.position.z() - from.position.z);
    if (!nearest.has_value() || distance < nearest_distance) {
      nearest = soldier;
      nearest_distance = distance;
    }
  }
  return nearest;
}

void assist_turn_toward(RootMotionBody& body,
                        const Game::Systems::CombatActions::CombatActionDefinition& def,
                        float to_x,
                        float to_z,
                        float delta_time) {
  float const facing = body.forward_x * to_x + body.forward_z * to_z;
  float const cone = std::cos(def.target_assist.cone_degrees * 0.5F *
                              std::numbers::pi_v<float> / 180.0F);
  if (facing < cone) {
    return;
  }
  auto& transform = *body.transform;
  transform.rotation.y = Game::Systems::turn_yaw_toward(
      transform.rotation.y,
      Game::Systems::yaw_degrees_from_direction(to_x, to_z),
      k_rts_commander_assist_turn_degrees_per_second * std::max(0.0F, delta_time));
  transform.desired_yaw = transform.rotation.y;
  float const yaw_rad = transform.rotation.y * std::numbers::pi_v<float> / 180.0F;
  body.forward_x = std::sin(yaw_rad);
  body.forward_z = std::cos(yaw_rad);
}

[[nodiscard]] auto
lunge_stop_distance(const Engine::Core::Entity& entity,
                    const Engine::Core::Entity& target,
                    const Engine::Core::TransformComponent& transform,
                    const RootMotionBody& body,
                    float soldier_body_radius) -> float {
  auto const* target_transform =
      target.get_component<Engine::Core::TransformComponent>();
  float const drawn_scales =
      std::max(0.0F, transform.scale.x) +
      (target_transform != nullptr ? std::max(0.0F, target_transform->scale.x)
                                   : std::max(0.0F, transform.scale.x));
  float stop_distance =
      std::max(soldier_body_radius + body.own_radius + 0.16F,
               drawn_scales * k_rts_commander_lunge_clearance_per_scale);
  if (Game::Systems::DuelSpacing::is_duel_body(target)) {
    stop_distance = std::max(
        stop_distance,
        Game::Systems::DuelSpacing::standoff_between(entity, target).preferred);
  }
  return stop_distance;
}

[[nodiscard]] auto allowed_step_toward_target(
    Engine::Core::Entity& entity,
    Engine::Core::Entity& target,
    RootMotionBody& body,
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    float step,
    float delta_time) -> float {
  auto& transform = *body.transform;
  float nearest_distance = 0.0F;
  auto const nearest = nearest_soldier(target, transform, nearest_distance);
  if (!nearest.has_value() || nearest_distance <= 0.001F) {
    return step;
  }
  float const to_x = (nearest->position.x() - transform.position.x) / nearest_distance;
  float const to_z = (nearest->position.z() - transform.position.z) / nearest_distance;
  assist_turn_toward(body, definition, to_x, to_z, delta_time);
  float const stop_distance =
      lunge_stop_distance(entity, target, transform, body, nearest->body_radius);
  float const contact_gap = nearest_distance - stop_distance;
  return std::clamp(step, 0.0F, std::max(0.0F, contact_gap));
}

[[nodiscard]] auto can_stand_at(const RootMotionBody& body,
                                const QVector3D& destination) -> bool {
  Game::Systems::BodyProfile profile;
  profile.radius = body.own_radius;
  profile.passability =
      body.movement != nullptr && body.movement->get_can_enter_forest()
          ? Pathfinding::Passability::Light
          : Pathfinding::Passability::Heavy;
  return Game::Systems::Walkability::can_stand(destination, profile);
}

} // namespace

void apply_rts_commander_root_motion(
    Engine::Core::World& world,
    Engine::Core::Entity& entity,
    const Engine::Core::RpgCommanderActionComponent& action,
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    float delta_time) {
  auto const& profile = definition.movement;
  if (!action.action_running || profile.distance <= 0.0F ||
      profile.end_normalized <= profile.start_normalized) {
    return;
  }
  auto const* commander = entity.get_component<Engine::Core::CommanderComponent>();
  if (commander == nullptr || commander->fpv_controlled ||
      !commander->advanced_combat_enabled || !definition.commander_only ||
      entity.has_component<Engine::Core::StaggerComponent>()) {
    return;
  }
  auto* transform = entity.get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return;
  }
  auto const* movement = entity.get_component<Engine::Core::MovementComponent>();

  float const step = authored_step(action, definition);
  if (step <= 1.0e-4F) {
    return;
  }

  float const yaw = transform->rotation.y * std::numbers::pi_v<float> / 180.0F;
  RootMotionBody body{
      .transform = transform,
      .movement = movement,
      .own_radius = movement != nullptr ? movement->get_navigation_clearance() : 0.45F,
      .forward_x = std::sin(yaw),
      .forward_z = std::cos(yaw)};
  float allowed = step;
  auto* target = world.get_entity(action.active_target_id);
  if (target != nullptr && !is_building(target)) {
    allowed =
        allowed_step_toward_target(entity, *target, body, definition, step, delta_time);
  }
  if (allowed <= 1.0e-4F) {
    return;
  }

  QVector3D const destination(transform->position.x + body.forward_x * allowed,
                              transform->position.y,
                              transform->position.z + body.forward_z * allowed);
  if (!can_stand_at(body, destination)) {
    return;
  }
  transform->position.x = destination.x();
  transform->position.z = destination.z();
}

} // namespace Game::Systems::Combat
