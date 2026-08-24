#include "rpg_bow_aim.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "../../core/ambient_session.h"
#include "../../core/world.h"
#include "../../map/terrain_service.h"
#include "../building_line_of_sight.h"
#include "../combat_system/combat_utils.h"

namespace Game::Systems::RpgCombat {

namespace {

constexpr float k_degrees_to_radians = std::numbers::pi_v<float> / 180.0F;

constexpr float k_terrain_probe_step = 0.75F;
constexpr int k_terrain_refine_steps = 6;

[[nodiscard]] auto
view_direction(const Engine::Core::RpgCommanderAimComponent& aim) -> QVector3D {
  if (aim.camera_forward_valid) {

    QVector3D const forward(
        aim.camera_forward_x, aim.camera_forward_y, aim.camera_forward_z);
    if (forward.lengthSquared() > 1.0e-6F) {
      return forward.normalized();
    }
  }
  float const yaw = aim.view_yaw_degrees * k_degrees_to_radians;
  float const pitch = aim.view_pitch_degrees * k_degrees_to_radians;
  float const pitch_cos = std::cos(pitch);
  return QVector3D(
             std::sin(yaw) * pitch_cos, std::sin(pitch), std::cos(yaw) * pitch_cos)
      .normalized();
}

[[nodiscard]] auto hash_to_unit(std::uint32_t seed) -> float {
  std::uint32_t value = seed * 0x9e3779b9U;
  value ^= value >> 16U;
  value *= 0x7feb352dU;
  value ^= value >> 15U;
  value *= 0x846ca68bU;
  value ^= value >> 16U;
  return static_cast<float>(value & 0xffffffU) / 16777215.0F;
}

[[nodiscard]] auto ray_cylinder_distance(const AimRay& ray,
                                         const QVector3D& base,
                                         float radius,
                                         float height,
                                         float max_distance) -> std::optional<float> {
  float const dx = ray.direction.x();
  float const dz = ray.direction.z();
  float const ox = ray.origin.x() - base.x();
  float const oz = ray.origin.z() - base.z();

  float const a = (dx * dx) + (dz * dz);
  if (a <= 1.0e-6F) {
    return std::nullopt;
  }
  float const b = 2.0F * ((ox * dx) + (oz * dz));
  float const c = (ox * ox) + (oz * oz) - (radius * radius);
  float const discriminant = (b * b) - (4.0F * a * c);
  if (discriminant < 0.0F) {
    return std::nullopt;
  }

  float const root = std::sqrt(discriminant);
  float const inv_denominator = 1.0F / (2.0F * a);
  std::array<float, 2> const candidates{(-b - root) * inv_denominator,
                                        (-b + root) * inv_denominator};
  for (float const distance : candidates) {
    if (distance < 0.0F || distance > max_distance) {
      continue;
    }
    float const y = ray.origin.y() + (ray.direction.y() * distance);
    if (y >= base.y() && y <= base.y() + height) {
      return distance;
    }
  }
  return std::nullopt;
}

[[nodiscard]] auto terrain_distance_along(const Engine::Core::World& world,
                                          const AimRay& ray,
                                          float max_distance) -> std::optional<float> {
  auto const& terrain = *Game::Session::services_for(world).terrain;
  if (!terrain.is_initialized()) {
    return std::nullopt;
  }

  auto height_above_ground = [&](float distance) {
    QVector3D const point = ray.origin + (ray.direction * distance);
    return point.y() - terrain.get_terrain_height(point.x(), point.z());
  };

  if (height_above_ground(0.0F) <= 0.0F) {
    return 0.0F;
  }

  float previous = 0.0F;
  for (float distance = k_terrain_probe_step; distance <= max_distance;
       distance += k_terrain_probe_step) {
    if (height_above_ground(distance) > 0.0F) {
      previous = distance;
      continue;
    }
    float low = previous;
    float high = distance;
    for (int step = 0; step < k_terrain_refine_steps; ++step) {
      float const mid = 0.5F * (low + high);
      if (height_above_ground(mid) > 0.0F) {
        low = mid;
      } else {
        high = mid;
      }
    }
    return high;
  }
  return std::nullopt;
}

} // namespace

auto default_weapon_stance(const Engine::Core::Entity& commander)
    -> Engine::Core::FpvWeaponStance {
  auto const* attack = commander.get_component<Engine::Core::AttackComponent>();
  auto const* unit = commander.get_component<Engine::Core::UnitComponent>();
  if (attack == nullptr || unit == nullptr || !attack->can_ranged) {
    return Engine::Core::FpvWeaponStance::Melee;
  }
  if (Engine::Core::resolve_combat_attack_family(
          unit->spawn_type, Engine::Core::AttackComponent::CombatMode::Ranged) !=
      Engine::Core::CombatAttackFamily::Bow) {
    return Engine::Core::FpvWeaponStance::Melee;
  }
  if (!attack->can_melee || attack->damage >= attack->melee_damage) {
    return Engine::Core::FpvWeaponStance::Bow;
  }
  return Engine::Core::FpvWeaponStance::Melee;
}

auto sync_commander_aim(Engine::Core::Entity& commander, const FpvAimInput& input)
    -> Engine::Core::RpgCommanderAimComponent* {
  auto* aim =
      Engine::Core::get_or_add_component<Engine::Core::RpgCommanderAimComponent>(
          &commander);
  if (aim == nullptr) {
    return nullptr;
  }
  if (!aim->stance_resolved) {
    aim->stance = default_weapon_stance(commander);
    aim->stance_resolved = true;
  }
  aim->view_yaw_degrees = input.view_yaw_degrees;
  aim->view_pitch_degrees = input.view_pitch_degrees;
  aim->move_speed = input.move_speed;
  aim->running = input.running;
  aim->draw_held = input.primary_held;
  aim->camera_origin_x = input.camera_origin.x();
  aim->camera_origin_y = input.camera_origin.y();
  aim->camera_origin_z = input.camera_origin.z();
  aim->camera_origin_valid = input.camera_origin_valid;
  aim->camera_forward_x = input.camera_forward.x();
  aim->camera_forward_y = input.camera_forward.y();
  aim->camera_forward_z = input.camera_forward.z();
  aim->camera_forward_valid = input.camera_forward_valid;
  aim->camera_fov_degrees = input.camera_fov_degrees;
  if (!input.primary_held) {
    aim->relaxed_from_overhold = false;
  }
  return aim;
}

auto toggle_weapon_stance(Engine::Core::Entity& commander) -> bool {
  auto const* attack = commander.get_component<Engine::Core::AttackComponent>();
  auto* aim = commander.get_component<Engine::Core::RpgCommanderAimComponent>();
  if (aim == nullptr || attack == nullptr || !attack->can_ranged ||
      !attack->can_melee) {
    return false;
  }
  if (default_weapon_stance(commander) == Engine::Core::FpvWeaponStance::Melee &&
      aim->stance == Engine::Core::FpvWeaponStance::Melee) {

    auto const* unit = commander.get_component<Engine::Core::UnitComponent>();
    if (unit != nullptr &&
        Engine::Core::resolve_combat_attack_family(
            unit->spawn_type, Engine::Core::AttackComponent::CombatMode::Ranged) !=
            Engine::Core::CombatAttackFamily::Bow) {
      return false;
    }
  }
  aim->stance = aim->stance == Engine::Core::FpvWeaponStance::Bow
                    ? Engine::Core::FpvWeaponStance::Melee
                    : Engine::Core::FpvWeaponStance::Bow;
  aim->stance_resolved = true;
  aim->draw_stage = Engine::Core::BowDrawStage::None;
  aim->draw_progress = 0.0F;
  aim->full_draw_hold = 0.0F;
  aim->shot_power = 0.0F;
  return true;
}

auto crosshair_ray(const Engine::Core::Entity& commander,
                   const Engine::Core::RpgCommanderAimComponent& aim) -> AimRay {
  AimRay ray;
  ray.direction = view_direction(aim);

  if (aim.camera_origin_valid) {
    ray.origin =
        QVector3D(aim.camera_origin_x, aim.camera_origin_y, aim.camera_origin_z);
    return ray;
  }
  auto const* transform = commander.get_component<Engine::Core::TransformComponent>();
  if (transform != nullptr) {
    ray.origin = QVector3D(transform->position.x,
                           transform->position.y + aim.eye_height,
                           transform->position.z);
  }
  return ray;
}

auto bow_muzzle(const Engine::Core::Entity& commander,
                const Engine::Core::RpgCommanderAimComponent& aim) -> QVector3D {
  auto const* transform = commander.get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return {};
  }
  float const yaw = aim.view_yaw_degrees * k_degrees_to_radians;
  QVector3D const forward(std::sin(yaw), 0.0F, std::cos(yaw));
  QVector3D const right(-forward.z(), 0.0F, forward.x());

  return QVector3D(transform->position.x,
                   transform->position.y + k_bow_hand_height,
                   transform->position.z) +
         (forward * k_bow_hand_forward) - (right * k_bow_hand_lateral);
}

auto commander_aim_ray(const Engine::Core::Entity& commander,
                       const Engine::Core::RpgCommanderAimComponent& aim) -> AimRay {

  AimRay ray = crosshair_ray(commander, aim);
  auto const* transform = commander.get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return ray;
  }

  QVector3D const eye(transform->position.x,
                      transform->position.y + aim.eye_height,
                      transform->position.z);
  float const advance = QVector3D::dotProduct(eye - ray.origin, ray.direction);
  if (advance > 0.0F) {
    ray.origin += ray.direction * advance;
  }
  return ray;
}

auto raycast_enemy_bodies(Engine::Core::World& world,
                          Engine::Core::Entity& commander,
                          const AimRay& ray,
                          float max_range,
                          float forgiveness) -> std::optional<AimHit> {
  auto const* commander_unit = commander.get_component<Engine::Core::UnitComponent>();
  if (commander_unit == nullptr || max_range <= 0.0F) {
    return std::nullopt;
  }

  const auto& buildings = *Game::Session::services_for(world).building_collision;
  std::optional<AimHit> best;
  for (auto [candidate_ref, candidate_unit] :
       world.entity_view<Engine::Core::UnitComponent>()) {
    (void)candidate_unit;
    Engine::Core::Entity* candidate = &candidate_ref;
    if (candidate == &commander ||
        !Game::Systems::Combat::is_valid_enemy_unit(commander_unit, candidate, false)) {
      continue;
    }

    for (auto const& target : live_soldier_targets(*candidate)) {
      float const radius = std::min(target.body_radius, 0.45F) + forgiveness;
      auto const distance = ray_cylinder_distance(
          ray, target.position, radius, k_aim_body_height, max_range);
      if (!distance.has_value()) {
        continue;
      }
      if (best.has_value() && best->distance <= *distance) {
        continue;
      }
      QVector3D const point = ray.origin + (ray.direction * *distance);
      if (!has_clear_building_los(buildings, ray.origin, point)) {
        continue;
      }
      best = AimHit{.entity_id = target.entity_id,
                    .soldier_slot = target.soldier_slot,
                    .point = point,
                    .distance = *distance};
    }
  }
  return best;
}

auto scatter_ray(const AimRay& ray,
                 float spread_degrees,
                 std::uint32_t seed) -> AimRay {
  if (spread_degrees <= 0.0F) {
    return ray;
  }

  float const angle = hash_to_unit(seed) * 2.0F * std::numbers::pi_v<float>;

  float const magnitude = std::sqrt(hash_to_unit(seed ^ 0x5bf03635U)) * spread_degrees *
                          k_degrees_to_radians;

  QVector3D const world_up(0.0F, 1.0F, 0.0F);
  QVector3D right = QVector3D::crossProduct(ray.direction, world_up);
  if (right.lengthSquared() <= 1.0e-6F) {
    right = QVector3D(1.0F, 0.0F, 0.0F);
  }
  right.normalize();
  QVector3D const up = QVector3D::crossProduct(right, ray.direction).normalized();

  QVector3D const offset =
      (right * std::cos(angle) + up * std::sin(angle)) * std::tan(magnitude);
  return AimRay{.origin = ray.origin,
                .direction = (ray.direction + offset).normalized()};
}

auto resolve_bow_shot(Engine::Core::World& world,
                      Engine::Core::Entity& commander,
                      const Engine::Core::RpgCommanderAimComponent& aim,
                      const AimRay& sight,
                      float max_range) -> BowShot {
  BowShot shot;
  max_range = std::max(1.0F, max_range);

  constexpr float k_shot_forgiveness = 0.04F;
  float travel = max_range;
  if (auto const hit = raycast_enemy_bodies(
          world, commander, sight, max_range, k_shot_forgiveness)) {
    shot.hit = *hit;
    shot.hit_body = true;
    travel = hit->distance;
  } else {
    if (auto const ground = terrain_distance_along(world, sight, max_range)) {
      travel = std::min(travel, *ground);
    }
    float const wall_fraction = first_building_intersection_fraction(
        *Game::Session::services_for(world).building_collision,
        sight.origin,
        sight.origin + (sight.direction * travel));
    travel *= std::clamp(wall_fraction, 0.0F, 1.0F);
  }

  shot.impact = sight.origin + (sight.direction * std::max(travel, 0.25F));

  shot.start = bow_muzzle(commander, aim);
  shot.end = shot.impact;
  return shot;
}

} // namespace Game::Systems::RpgCombat
