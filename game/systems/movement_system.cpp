#include "movement_system.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <vector>

#include "../formation/army_formation_registry.h"
#include "../map/terrain_service.h"
#include "../units/spawn_type.h"
#include "../units/troop_config.h"
#include "combat_rules.h"
#include "combat_system/structure_combat.h"
#include "command_service.h"
#include "core/component.h"
#include "defensive_unit_layout_service.h"
#include "formation_combat_geometry.h"
#include "game/core/nav_profile.h"
#include "nav_grid.h"
#include "order_service.h"
#include "pathfinding.h"
#include "route_follow_system.h"
#include "walkability.h"

namespace Game::Systems {

namespace {

constexpr float hold_mode_turn_speed_degrees = 180.0F;

constexpr float k_duel_footwork_degrees_per_second = 18.0F;
constexpr float k_duel_footwork_period_seconds = 9.0F;
constexpr float k_duel_footwork_turn_degrees_per_second = 360.0F;
constexpr float k_duel_min_reach_sq = 0.04F;
constexpr float k_duel_measure_step_speed = 1.6F;
constexpr float k_duel_measure_gain_per_second = 4.5F;
constexpr float k_duel_measure_advance = 0.17F;
constexpr float k_duel_measure_retreat = -0.11F;
constexpr float k_duel_measure_breath = 0.05F;
constexpr float k_duel_measure_breath_period_seconds = 3.4F;
constexpr float k_duel_min_separation = 0.55F;
constexpr float k_duel_base_separation_fraction = 0.70F;
constexpr float k_duel_base_separation_min = 0.85F;
constexpr float k_duel_base_separation_max = 1.70F;
constexpr float desired_yaw_turn_speed_degrees = 720.0F;

constexpr float k_turn_speed_elephant_degrees = 110.0F;
constexpr float k_turn_speed_siege_degrees = 100.0F;
constexpr float k_turn_speed_cavalry_degrees = 300.0F;
constexpr float k_turn_speed_sheep_degrees = 210.0F;
constexpr float k_turn_speed_wolf_degrees = 340.0F;
constexpr float k_formation_heading_min_speed = 0.4F;
constexpr float k_formation_heading_speed_fraction = 0.25F;
constexpr float k_formation_intent_min_distance = 1.0F;
constexpr float full_translation_heading_error_degrees = 20.0F;
constexpr float stopped_translation_heading_error_degrees = 100.0F;

constexpr float k_motor_substep_cells = 0.45F;
constexpr int k_max_motor_substeps = 8;

constexpr float k_turning_translation_threshold = 0.35F;

auto body_turn_speed_degrees(Game::Units::SpawnType type) -> float {
  switch (type) {
  case Game::Units::SpawnType::Elephant:
    return k_turn_speed_elephant_degrees;
  case Game::Units::SpawnType::Catapult:
  case Game::Units::SpawnType::Ballista:
    return k_turn_speed_siege_degrees;
  case Game::Units::SpawnType::MountedKnight:
  case Game::Units::SpawnType::HorseArcher:
  case Game::Units::SpawnType::HorseSpearman:
    return k_turn_speed_cavalry_degrees;
  case Game::Units::SpawnType::Sheep:
    return k_turn_speed_sheep_degrees;
  case Game::Units::SpawnType::Wolf:
    return k_turn_speed_wolf_degrees;
  default:
    return desired_yaw_turn_speed_degrees;
  }
}

auto formation_turn_speed_degrees(const Engine::Core::Entity& entity,
                                  const Engine::Core::UnitComponent& unit,
                                  float single_body_turn_speed) -> float {
  float const turn_radius = FormationCombat::formation_turn_radius(entity);
  if (!FormationCombat::has_formation_slots(entity) || turn_radius <= 0.5F) {
    return single_body_turn_speed;
  }

  float const max_outer_speed = std::max(2.0F, unit.speed * 1.5F);
  float const derived =
      max_outer_speed / turn_radius * 180.0F / std::numbers::pi_v<float>;
  return std::clamp(derived, 30.0F, single_body_turn_speed);
}

struct HeadingReference {
  bool valid{false};
  float yaw{0.0F};
};

auto heading_reference(const Engine::Core::Entity& entity,
                       const Engine::Core::TransformComponent& transform,
                       const Engine::Core::MovementComponent& movement,
                       const Engine::Core::UnitComponent* unit) -> HeadingReference {
  bool const formation =
      unit != nullptr && FormationCombat::has_formation_slots(entity);
  if (formation && movement.get_has_target()) {
    float const intent_x = movement.get_target_x() - transform.position.x;
    float const intent_z = movement.get_target_y() - transform.position.z;
    if (intent_x * intent_x + intent_z * intent_z >
        k_formation_intent_min_distance * k_formation_intent_min_distance) {
      return {true,
              std::atan2(intent_x, intent_z) * 180.0F / std::numbers::pi_v<float>};
    }
  }
  float const vx = movement.get_vx();
  float const vz = movement.get_vz();
  float const speed2 = vx * vx + vz * vz;
  float const min_speed =
      formation ? std::max(k_formation_heading_min_speed,
                           unit->speed * k_formation_heading_speed_fraction)
                : 0.0F;
  if (speed2 <= std::max(1.0e-5F, min_speed * min_speed)) {
    return {};
  }
  return {true, std::atan2(vx, vz) * 180.0F / std::numbers::pi_v<float>};
}

auto heading_translation_scale(float yaw_degrees, HeadingReference reference) -> float {
  if (!reference.valid) {
    return 1.0F;
  }
  float const error =
      std::fabs(std::fmod((reference.yaw - yaw_degrees + 540.0F), 360.0F) - 180.0F);
  return 1.0F - std::clamp((error - full_translation_heading_error_degrees) /
                               (stopped_translation_heading_error_degrees -
                                full_translation_heading_error_degrees),
                           0.0F,
                           1.0F);
}

void apply_desired_yaw(Engine::Core::TransformComponent* transform,
                       float delta_time,
                       float turn_speed_degrees) {
  if ((transform == nullptr) || !transform->has_desired_yaw) {
    return;
  }

  float const current = transform->rotation.y;
  float const target_yaw = transform->desired_yaw;
  float const diff = std::fmod((target_yaw - current + 540.0F), 360.0F) - 180.0F;
  float const step = std::clamp(
      diff, -turn_speed_degrees * delta_time, turn_speed_degrees * delta_time);
  transform->rotation.y = current + step;

  float const remaining_diff =
      std::fmod((target_yaw - transform->rotation.y + 540.0F), 360.0F) - 180.0F;
  if (std::fabs(remaining_diff) < 0.5F) {
    transform->rotation.y = target_yaw;
    transform->has_desired_yaw = false;
  }
}

auto face_locked_structure(Engine::Core::World* world,
                           Engine::Core::TransformComponent& transform,
                           const Engine::Core::AttackComponent& attack,
                           const Engine::Core::UnitComponent* unit,
                           float delta_time) -> bool {
  if (world == nullptr || attack.melee_lock_target_id == 0) {
    return false;
  }
  auto* structure = world->get_entity(attack.melee_lock_target_id);
  if (structure == nullptr ||
      !structure->has_component<Engine::Core::BuildingComponent>()) {
    return false;
  }
  auto const* structure_transform =
      structure->get_component<Engine::Core::TransformComponent>();
  if (structure_transform == nullptr) {
    return false;
  }

  float const dx = structure_transform->position.x - transform.position.x;
  float const dz = structure_transform->position.z - transform.position.z;
  if ((dx * dx) + (dz * dz) < 0.000001F) {
    return false;
  }

  transform.desired_yaw = std::atan2(dx, dz) * 180.0F / std::numbers::pi_v<float>;
  transform.has_desired_yaw = true;
  apply_desired_yaw(&transform,
                    delta_time,
                    unit != nullptr ? body_turn_speed_degrees(unit->spawn_type)
                                    : desired_yaw_turn_speed_degrees);
  return true;
}

} // namespace

namespace {

void clamp_to_map_bounds(Engine::Core::TransformComponent& transform) {
  auto& terrain = Game::Map::TerrainService::instance();
  if (!terrain.is_initialized()) {
    return;
  }
  const Game::Map::TerrainHeightMap* hm = terrain.get_height_map();
  if (hm == nullptr) {
    return;
  }
  const float tile = hm->get_tile_size();
  const int w = hm->get_width();
  const int h = hm->get_height();
  if (w <= 0 || h <= 0) {
    return;
  }
  const float half_w = w * 0.5F - 0.5F;
  const float half_h = h * 0.5F - 0.5F;
  transform.position.x =
      std::clamp(transform.position.x, -half_w * tile, half_w * tile);
  transform.position.z =
      std::clamp(transform.position.z, -half_h * tile, half_h * tile);
}

void finalize_orientation(Engine::Core::Entity* entity,
                          Engine::Core::TransformComponent* transform,
                          Engine::Core::MovementComponent* movement,
                          float delta_time) {
  clamp_to_map_bounds(*transform);

  auto* terrain_ctx = entity->get_component<Engine::Core::TerrainContextComponent>();
  if (terrain_ctx != nullptr && terrain_ctx->audio_cooldown > 0.0F) {
    terrain_ctx->audio_cooldown =
        std::max(0.0F, terrain_ctx->audio_cooldown - delta_time);
  }

  if (entity->has_component<Engine::Core::BuildingComponent>()) {
    return;
  }

  auto const* unit = entity->get_component<Engine::Core::UnitComponent>();
  float const body_turn_speed = unit != nullptr
                                    ? body_turn_speed_degrees(unit->spawn_type)
                                    : desired_yaw_turn_speed_degrees;
  float const turn_speed =
      (unit != nullptr ? formation_turn_speed_degrees(*entity, *unit, body_turn_speed)
                       : body_turn_speed) *
      DefensiveUnitLayoutService::turn_speed_multiplier(*entity);

  bool const shell_holds_its_face =
      DefensiveUnitLayoutService::holds_position(*entity) && transform->has_desired_yaw;
  auto const reference = heading_reference(*entity, *transform, *movement, unit);
  if (reference.valid && !shell_holds_its_face) {
    float const target_yaw = reference.yaw;
    float const current = transform->rotation.y;
    float const diff = std::fmod((target_yaw - current + 540.0F), 360.0F) - 180.0F;
    float const step =
        std::clamp(diff, -turn_speed * delta_time, turn_speed * delta_time);
    transform->rotation.y = current + step;
  } else if (transform->has_desired_yaw) {
    float const current = transform->rotation.y;
    float const target_yaw = transform->desired_yaw;
    float const diff = std::fmod((target_yaw - current + 540.0F), 360.0F) - 180.0F;
    float const step =
        std::clamp(diff, -turn_speed * delta_time, turn_speed * delta_time);
    transform->rotation.y = current + step;
    if (std::fabs(diff) < 0.5F && !shell_holds_its_face) {
      transform->has_desired_yaw = false;
    }
  }
}

} // namespace

void MovementSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  if (auto* pathfinder = NavGrid::get_pathfinder()) {
    std::uint64_t const revision = pathfinder->obstruction_revision();
    if (revision != m_obstruction_revision) {
      m_obstruction_revision = revision;

      pathfinder->update_navigation_grid();
      auto entities = world->collect_entities_with<Engine::Core::MovementComponent>();
      repath_after_obstruction_release(*world, entities);
    }
  }

  m_duel_clock += delta_time;

  process_pending_path_requests(*world);
  world->each<Engine::Core::MovementComponent>(
      [this, world, delta_time](Engine::Core::EntityID id,
                                Engine::Core::MovementComponent&) {
        move_unit(world->get_entity(id), world, delta_time);
      });
}

void MovementSystem::process_pending_path_requests(Engine::Core::World& world) {
  std::size_t processed = 0;
  while (processed < k_path_requests_per_tick && !m_pending_path_requests.empty()) {
    PendingPathRequest request = std::move(m_pending_path_requests.front());
    m_pending_path_requests.pop_front();
    auto* entity = world.get_entity(request.entity_id);
    if (entity == nullptr) {
      ++processed;
      continue;
    }
    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    auto* movement = entity->get_component<Engine::Core::MovementComponent>();
    if (transform != nullptr && movement != nullptr) {

      if (movement->get_order_sequence() != request.order_sequence) {
        ++processed;
        continue;
      }
      float const current_goal_x = movement->get_has_requested_goal()
                                       ? movement->get_requested_goal_x()
                                       : movement->get_goal_x();
      float const current_goal_z = movement->get_has_requested_goal()
                                       ? movement->get_requested_goal_z()
                                       : movement->get_goal_y();
      float const goal_dx = current_goal_x - request.target.x();
      float const goal_dz = current_goal_z - request.target.z();
      if (goal_dx * goal_dx + goal_dz * goal_dz > 0.01F) {
        ++processed;
        continue;
      }
      assign_navigation_target(
          NavGrid::get_pathfinder(), *transform, *movement, request.target);
      movement->precise_arrival = request.precise_arrival;
    }
    ++processed;
  }
}

auto MovementSystem::enqueue_pending_path_request(Engine::Core::EntityID entity_id,
                                                  const QVector3D& target,
                                                  bool precise_arrival,
                                                  std::uint64_t navigation_revision,
                                                  std::uint64_t order_sequence)
    -> bool {
  cancel_pending_path_request(entity_id);
  if (m_pending_path_requests.size() >= k_max_pending_path_requests) {
    Engine::Core::count_nav(Engine::Core::NavCounter::RequestsDropped);
    return false;
  }
  Engine::Core::count_nav(Engine::Core::NavCounter::RequestsQueued);
  m_pending_path_requests.push_back(
      {entity_id, target, navigation_revision, order_sequence, precise_arrival});
  return true;
}

void MovementSystem::cancel_pending_path_request(Engine::Core::EntityID entity_id) {
  std::erase_if(m_pending_path_requests, [entity_id](auto const& request) {
    return request.entity_id == entity_id;
  });
}

void MovementSystem::repath_after_obstruction_release(
    Engine::Core::World& world, const std::vector<Engine::Core::Entity*>& movers) {
  auto* pathfinder = NavGrid::get_pathfinder();
  auto const release = pathfinder != nullptr ? pathfinder->last_obstruction_release()
                                             : Pathfinding::ObstructionRelease{};

  struct RepathCandidate {
    Engine::Core::EntityID entity_id{0};
    QVector3D goal;
    float distance_to_release_sq{0.0F};
  };

  std::vector<RepathCandidate> candidates;
  candidates.reserve(movers.size());

  for (auto* entity : movers) {
    if (entity == nullptr ||
        entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto const* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || unit->health <= 0) {
      continue;
    }

    auto* movement = entity->get_component<Engine::Core::MovementComponent>();
    if (movement == nullptr || !movement->get_has_target()) {
      continue;
    }

    RepathCandidate candidate;
    candidate.entity_id = entity->get_id();
    candidate.goal =
        movement->get_has_requested_goal()
            ? QVector3D(movement->get_requested_goal_x(),
                        0.0F,
                        movement->get_requested_goal_z())
            : QVector3D(movement->get_goal_x(), 0.0F, movement->get_goal_y());

    auto const* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (release.located && transform != nullptr) {
      float const dx = transform->position.x - release.center.x();
      float const dz = transform->position.z - release.center.z();
      candidate.distance_to_release_sq = (dx * dx) + (dz * dz);
    }
    candidates.push_back(candidate);
  }

  if (release.located && candidates.size() > k_path_requests_per_tick) {
    std::partial_sort(candidates.begin(),
                      candidates.begin() +
                          static_cast<std::ptrdiff_t>(k_path_requests_per_tick),
                      candidates.end(),
                      [](const RepathCandidate& lhs, const RepathCandidate& rhs) {
                        return lhs.distance_to_release_sq < rhs.distance_to_release_sq;
                      });
  }

  std::uint64_t const navigation_revision =
      pathfinder != nullptr ? pathfinder->navigation_revision() : 0U;
  std::size_t repathed_now = 0;

  for (auto const& candidate : candidates) {
    auto* entity = world.get_entity(candidate.entity_id);
    if (entity == nullptr) {
      continue;
    }
    auto* movement = entity->get_component<Engine::Core::MovementComponent>();
    if (movement == nullptr) {
      continue;
    }

    if (repathed_now < k_path_requests_per_tick) {
      if (!retarget_unit(world, candidate.entity_id, candidate.goal)) {
        continue;
      }
      ++repathed_now;
    } else if (!enqueue_pending_path_request(candidate.entity_id,
                                             candidate.goal,
                                             movement->get_precise_arrival(),
                                             navigation_revision,
                                             movement->get_order_sequence())) {
      continue;
    }

    movement->stuck_ref_valid = false;
    movement->stuck_timer = 0.0F;
  }
}

namespace {

[[nodiscard]] auto duel_footwork_body(const Engine::Core::Entity& entity) -> bool {
  if (entity.has_component<Engine::Core::BuildingComponent>() ||
      entity.has_component<Engine::Core::ElephantComponent>()) {
    return false;
  }
  auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr || Game::Units::is_cavalry(unit->spawn_type)) {
    return false;
  }
  return !FormationCombat::has_formation_slots(entity);
}

[[nodiscard]] auto duel_measure_target(const Engine::Core::Entity& entity,
                                       float clock,
                                       float breath_phase) -> float {
  auto const* action =
      entity.get_component<Engine::Core::RpgCommanderActionComponent>();
  if (action != nullptr && action->action_running && action->combat_action_id != 0U) {
    float const t = std::clamp(action->normalized_action_time, 0.0F, 1.0F);
    if (t < 0.12F) {
      return k_duel_measure_retreat * 0.4F;
    }
    if (t < 0.62F) {
      float const s = (t - 0.12F) / 0.50F;
      float const eased = s * s * (3.0F - 2.0F * s);
      return k_duel_measure_retreat * 0.4F +
             (k_duel_measure_advance - k_duel_measure_retreat * 0.4F) * eased;
    }
    float const s = std::clamp((t - 0.62F) / 0.33F, 0.0F, 1.0F);
    return k_duel_measure_advance +
           (k_duel_measure_retreat - k_duel_measure_advance) * s;
  }
  float const breath = std::sin(clock * 2.0F * std::numbers::pi_v<float> /
                                    k_duel_measure_breath_period_seconds +
                                breath_phase);
  return k_duel_measure_retreat * 0.5F + breath * k_duel_measure_breath;
}

} // namespace

namespace {

struct SweepResult {
  float accepted_dx{0.0F};
  float accepted_dz{0.0F};
  float rejected_dx{0.0F};
  float rejected_dz{0.0F};
  float accepted_fraction{1.0F};
  bool contact{false};
  bool blocked{false};
  float normal_x{0.0F};
  float normal_z{0.0F};
};

template <typename AllowedFn>
auto sweep_body(float origin_x,
                float origin_z,
                float delta_x,
                float delta_z,
                float substep_length,
                const AllowedFn& allowed) -> SweepResult {
  SweepResult result;
  float const total = std::hypot(delta_x, delta_z);
  if (total <= 1.0e-6F) {
    return result;
  }

  int const substeps =
      std::clamp(static_cast<int>(std::ceil(total / std::max(0.02F, substep_length))),
                 1,
                 k_max_motor_substeps);

  float x = origin_x;
  float z = origin_z;
  float remaining_x = delta_x;
  float remaining_z = delta_z;

  for (int step = 0; step < substeps; ++step) {
    float const fraction = 1.0F / static_cast<float>(substeps - step);
    float const step_x = remaining_x * fraction;
    float const step_z = remaining_z * fraction;
    if (std::hypot(step_x, step_z) <= 1.0e-7F) {
      break;
    }

    if (allowed(x, z, x + step_x, z + step_z)) {
      x += step_x;
      z += step_z;
      remaining_x -= step_x;
      remaining_z -= step_z;
      continue;
    }

    bool const x_clear = allowed(x, z, x + step_x, z);
    bool const z_clear = allowed(x, z, x, z + step_z);

    float normal_x = 0.0F;
    float normal_z = 0.0F;
    if (x_clear && (!z_clear || std::abs(step_x) >= std::abs(step_z))) {
      x += step_x;
      normal_z = step_z > 0.0F ? -1.0F : 1.0F;
    } else if (z_clear) {
      z += step_z;
      normal_x = step_x > 0.0F ? -1.0F : 1.0F;
    } else {
      result.blocked = true;
      result.contact = true;
      float const length = std::hypot(remaining_x, remaining_z);
      if (length > 1.0e-6F) {
        result.normal_x = -remaining_x / length;
        result.normal_z = -remaining_z / length;
      }
      break;
    }

    result.contact = true;
    result.normal_x = normal_x;
    result.normal_z = normal_z;

    remaining_x -= step_x;
    remaining_z -= step_z;
    float const into = remaining_x * normal_x + remaining_z * normal_z;
    if (into < 0.0F) {
      remaining_x -= normal_x * into;
      remaining_z -= normal_z * into;
    }
  }

  result.accepted_dx = x - origin_x;
  result.accepted_dz = z - origin_z;
  result.rejected_dx = delta_x - result.accepted_dx;
  result.rejected_dz = delta_z - result.accepted_dz;
  result.accepted_fraction = std::clamp(
      std::hypot(result.accepted_dx, result.accepted_dz) / total, 0.0F, 1.0F);
  return result;
}

struct MotorLimits {
  float max_speed{0.0F};
  float acceleration{0.0F};
  float damping{6.0F};
};

auto motor_limits(const Engine::Core::Entity& entity,
                  const Engine::Core::UnitComponent& unit,
                  const Engine::Core::StaminaComponent* stamina) -> MotorLimits {
  MotorLimits limits;
  limits.max_speed = formation_navigation_speed(entity, unit, stamina);
  limits.acceleration = limits.max_speed * 4.0F;
  return limits;
}

class MotorCollision {
public:
  MotorCollision(const Engine::Core::Entity& entity,
                 float origin_x,
                 float origin_z,
                 bool respect_body_radius = false)
      : m_entity(&entity)
      , m_respect_body_radius(respect_body_radius) {
    QVector3D const origin(origin_x, 0.0F, origin_z);
    m_valid_tile = allowed_here(origin);
    if (!m_valid_tile) {
      m_trapped_depth = Walkability::penetration(origin, body_profile(entity));
    }
  }

  [[nodiscard]] auto was_on_valid_tile() const -> bool { return m_valid_tile; }

  [[nodiscard]] auto point_allowed(float wx, float wz) const -> bool {
    QVector3D const point(wx, 0.0F, wz);
    if (m_valid_tile) {
      return allowed_here(point);
    }
    if (m_trapped_depth > 0.0F) {

      return Walkability::penetration(point, body_profile(*m_entity)) < m_trapped_depth;
    }
    return allowed_here(point);
  }

  [[nodiscard]] auto
  step_allowed(float from_x, float from_z, float to_x, float to_z) const -> bool {
    if (!point_allowed(to_x, to_z)) {
      return false;
    }
    Point const from_cell = NavGrid::world_to_grid(from_x, from_z);
    Point const to_cell = NavGrid::world_to_grid(to_x, to_z);
    if (from_cell.x == to_cell.x || from_cell.y == to_cell.y) {
      return true;
    }
    QVector3D const across_x = NavGrid::grid_to_world({to_cell.x, from_cell.y});
    QVector3D const across_z = NavGrid::grid_to_world({from_cell.x, to_cell.y});
    return point_allowed(across_x.x(), across_x.z()) &&
           point_allowed(across_z.x(), across_z.z());
  }

  [[nodiscard]] static auto
  body_profile(const Engine::Core::Entity& entity) -> BodyProfile {
    BodyProfile profile;
    if (auto const* movement = entity.get_component<Engine::Core::MovementComponent>();
        movement != nullptr) {
      profile.radius = movement->get_navigation_clearance();
      profile.passability = movement->get_can_enter_forest()
                                ? Pathfinding::Passability::Light
                                : Pathfinding::Passability::Heavy;
    }
    if (auto const* commander =
            entity.get_component<Engine::Core::CommanderComponent>();
        commander != nullptr && commander->fpv_controlled) {
      profile.stops_at_building_facade = true;
    }
    return profile;
  }

private:
  [[nodiscard]] auto allowed_here(const QVector3D& point) const -> bool {
    if (m_respect_body_radius) {
      return Walkability::can_stand(point, body_profile(*m_entity));
    }
    return is_movement_point_allowed(point, *m_entity);
  }

  const Engine::Core::Entity* m_entity;
  bool m_respect_body_radius{false};
  bool m_valid_tile{true};
  float m_trapped_depth{0.0F};
};

void unstick_body(const Engine::Core::Entity& entity,
                  Engine::Core::TransformComponent& transform,
                  float delta_time) {
  if (entity.has_component<Engine::Core::BuildingComponent>()) {
    return;
  }

  if (auto const* commander = entity.get_component<Engine::Core::CommanderComponent>();
      commander != nullptr && commander->jump_active) {
    return;
  }

  QVector3D const here(transform.position.x, 0.0F, transform.position.z);
  if (is_movement_point_allowed(here, entity)) {
    return;
  }

  BodyProfile profile = MotorCollision::body_profile(entity);
  profile.radius = 0.0F;
  if (Walkability::penetration(here, profile) <= 0.0F) {
    return;
  }

  constexpr float k_escape_search_radius = 16.0F;
  auto const escape =
      Walkability::nearest_standable(here, profile, k_escape_search_radius);
  if (!escape.has_value()) {
    return;
  }
  float const dx = escape->x() - here.x();
  float const dz = escape->z() - here.z();
  float const distance = std::hypot(dx, dz);
  if (distance <= 1.0e-4F) {
    return;
  }

  auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
  float const speed = unit != nullptr ? std::max(0.5F, unit->speed) : 1.5F;
  float const travel = std::min(distance, speed * std::max(delta_time, 0.0F));
  transform.position.x += dx / distance * travel;
  transform.position.z += dz / distance * travel;
}

auto slide_body_to(const Engine::Core::Entity& entity,
                   Engine::Core::TransformComponent& transform,
                   float target_x,
                   float target_z,
                   bool respect_body_radius = false) -> SweepResult {
  MotorCollision const collision(
      entity, transform.position.x, transform.position.z, respect_body_radius);
  auto const* pathfinder = NavGrid::get_pathfinder();
  float const cell_size = pathfinder != nullptr ? pathfinder->grid_cell_size() : 1.0F;
  auto const sweep = sweep_body(transform.position.x,
                                transform.position.z,
                                target_x - transform.position.x,
                                target_z - transform.position.z,
                                std::max(0.05F, cell_size * k_motor_substep_cells),
                                [&collision](float fx, float fz, float tx, float tz) {
                                  return collision.step_allowed(fx, fz, tx, tz);
                                });
  transform.position.x += sweep.accepted_dx;
  transform.position.z += sweep.accepted_dz;
  return sweep;
}

} // namespace

auto MovementSystem::apply_duel_footwork(Engine::Core::Entity* entity,
                                         Engine::Core::World* world,
                                         Engine::Core::TransformComponent& transform,
                                         Engine::Core::AttackComponent& attack,
                                         float delta_time) const -> bool {
  if (world == nullptr || !duel_footwork_body(*entity)) {
    return false;
  }

  auto* opponent = world->get_entity(attack.melee_lock_target_id);
  if (opponent == nullptr || !duel_footwork_body(*opponent)) {
    return false;
  }

  auto const* opponent_attack =
      opponent->get_component<Engine::Core::AttackComponent>();
  if (opponent_attack == nullptr || !opponent_attack->in_melee_lock ||
      opponent_attack->melee_lock_target_id != entity->get_id()) {
    return false;
  }

  auto const* opponent_transform =
      opponent->get_component<Engine::Core::TransformComponent>();
  if (opponent_transform == nullptr) {
    return false;
  }

  float const rx = transform.position.x - opponent_transform->position.x;
  float const rz = transform.position.z - opponent_transform->position.z;
  if ((rx * rx) + (rz * rz) < k_duel_min_reach_sq) {
    return false;
  }

  auto const lhs = std::min(entity->get_id(), opponent->get_id());
  auto const rhs = std::max(entity->get_id(), opponent->get_id());
  float const direction = (((lhs + rhs) & 1U) == 0U) ? 1.0F : -1.0F;
  float const sway = std::sin(m_duel_clock * 2.0F * std::numbers::pi_v<float> /
                              k_duel_footwork_period_seconds);
  float const angle = direction * sway * k_duel_footwork_degrees_per_second *
                      delta_time * std::numbers::pi_v<float> / 180.0F;

  float const cos_a = std::cos(angle);
  float const sin_a = std::sin(angle);

  slide_body_to(*entity,
                transform,
                opponent_transform->position.x + (rx * cos_a) - (rz * sin_a),
                opponent_transform->position.z + (rx * sin_a) + (rz * cos_a),
                true);

  {
    float const breath_phase = static_cast<float>(entity->get_id() % 17U) / 17.0F *
                               2.0F * std::numbers::pi_v<float>;
    float const advance = duel_measure_target(*entity, m_duel_clock, breath_phase);

    float const own_reach = attack.melee_range;
    float const opponent_reach =
        opponent_attack != nullptr ? opponent_attack->melee_range : own_reach;
    float const base_separation = std::clamp(0.5F * (own_reach + opponent_reach) *
                                                 k_duel_base_separation_fraction,
                                             k_duel_base_separation_min,
                                             k_duel_base_separation_max);
    float const desired_separation =
        std::max(k_duel_min_separation, base_separation - advance);
    float const to_x = opponent_transform->position.x - transform.position.x;
    float const to_z = opponent_transform->position.z - transform.position.z;
    float const separation = std::hypot(to_x, to_z);
    if (separation > 0.0001F) {
      float const error = separation - desired_separation;
      float const max_step = k_duel_measure_step_speed * delta_time;
      float step = std::clamp(
          error * k_duel_measure_gain_per_second * delta_time, -max_step, max_step);
      step = std::min(step, std::max(0.0F, separation - k_duel_min_separation));
      auto const measure =
          slide_body_to(*entity,
                        transform,
                        transform.position.x + (to_x / separation * step),
                        transform.position.z + (to_z / separation * step),
                        true);
      attack.melee_footwork_offset =
          std::copysign(std::hypot(measure.accepted_dx, measure.accepted_dz), step);
    }
  }

  float const face_x = opponent_transform->position.x - transform.position.x;
  float const face_z = opponent_transform->position.z - transform.position.z;
  float const target_yaw =
      std::atan2(face_x, face_z) * 180.0F / std::numbers::pi_v<float>;
  float const current = transform.rotation.y;
  float const diff = std::fmod((target_yaw - current + 540.0F), 360.0F) - 180.0F;
  transform.rotation.y =
      current + std::clamp(diff,
                           -k_duel_footwork_turn_degrees_per_second * delta_time,
                           k_duel_footwork_turn_degrees_per_second * delta_time);
  transform.desired_yaw = transform.rotation.y;
  transform.has_desired_yaw = false;
  return true;
}

void MovementSystem::move_unit(Engine::Core::Entity* entity,
                               Engine::Core::World* world,
                               float delta_time) {
  auto* transform = entity->get_component<Engine::Core::TransformComponent>();
  auto* movement = entity->get_component<Engine::Core::MovementComponent>();
  auto* unit = entity->get_component<Engine::Core::UnitComponent>();

  if ((transform == nullptr) || (movement == nullptr) || (unit == nullptr)) {
    return;
  }

  auto* facts =
      Engine::Core::get_or_add_component<Engine::Core::MovementFactsComponent>(entity);
  if (facts == nullptr) {
    return;
  }

  float const previous_x = transform->position.x;
  float const previous_z = transform->position.z;

  if (unit->health <= 0 ||
      entity->has_component<Engine::Core::PendingRemovalComponent>()) {
    return;
  }

  unstick_body(*entity, *transform, delta_time);

  MovementGate const gate = classify_movement_gate(*entity);

  if (gate == MovementGate::DirectControl) {
    if (auto const* commander =
            entity->get_component<Engine::Core::CommanderComponent>();
        commander != nullptr && commander->fpv_controlled && !commander->jump_active) {
      movement->has_target = false;
      movement->clear_path();
      OrderService::clear_player_order_intent(entity);
      movement->vx = 0.0F;
      movement->vz = 0.0F;
      facts->progress.state = Engine::Core::MovementOrderState::Cancelled;
    }
    return;
  }

  auto* hold_mode = entity->get_component<Engine::Core::HoldModeComponent>();
  if (gate == MovementGate::HoldMode) {
    bool in_hold_mode = false;
    if (hold_mode != nullptr) {
      if (hold_mode->exit_cooldown > 0.0F) {
        hold_mode->exit_cooldown =
            std::max(0.0F, hold_mode->exit_cooldown - delta_time);
      }

      if (hold_mode->active) {
        movement->has_target = false;
        movement->clear_path();
        OrderService::clear_player_order_intent(entity);
        movement->vx = 0.0F;
        movement->vz = 0.0F;
        in_hold_mode = true;

        if (hold_mode->kneel_duration > 0.0F &&
            hold_mode->kneel_entry_progress < 1.0F) {
          hold_mode->kneel_entry_progress = std::min(
              1.0F,
              hold_mode->kneel_entry_progress + delta_time / hold_mode->kneel_duration);
        }
      } else {
        hold_mode->kneel_entry_progress = 0.0F;
      }

      if (hold_mode->exit_cooldown > 0.0F && !in_hold_mode) {
        movement->vx = 0.0F;
        movement->vz = 0.0F;
        return;
      }
    }

    if (in_hold_mode) {
      facts->progress.state = Engine::Core::MovementOrderState::Idle;
      if (!entity->has_component<Engine::Core::BuildingComponent>()) {
        apply_desired_yaw(transform,
                          delta_time,
                          formation_turn_speed_degrees(
                              *entity,
                              *unit,
                              std::min(hold_mode_turn_speed_degrees,
                                       body_turn_speed_degrees(unit->spawn_type))));
      }
    }
    return;
  }

  if (gate == MovementGate::MeleeLock) {
    auto* atk = entity->get_component<Engine::Core::AttackComponent>();
    movement->has_target = false;
    OrderService::clear_player_order_intent(entity);
    movement->vx = 0.0F;
    movement->vz = 0.0F;
    movement->clear_path();
    facts->progress.state = Engine::Core::MovementOrderState::Idle;
    if (atk != nullptr &&
        !apply_duel_footwork(entity, world, *transform, *atk, delta_time) &&
        !face_locked_structure(world, *transform, *atk, unit, delta_time)) {
      transform->desired_yaw = transform->rotation.y;
      transform->has_desired_yaw = false;
    }

    clamp_to_map_bounds(*transform);
    facts->motor.valid = true;
    facts->motor.accepted_dx = transform->position.x - previous_x;
    facts->motor.accepted_dz = transform->position.z - previous_z;
    facts->motor.accepted_vx = facts->motor.accepted_dx / std::max(1.0e-5F, delta_time);
    facts->motor.accepted_vz = facts->motor.accepted_dz / std::max(1.0e-5F, delta_time);
    return;
  }

  if (gate == MovementGate::BuilderBypass) {
    auto* builder_prod =
        entity->get_component<Engine::Core::BuilderProductionComponent>();
    float const dx = builder_prod->bypass_target_x - transform->position.x;
    float const dz = builder_prod->bypass_target_z - transform->position.z;
    float const dist_sq = dx * dx + dz * dz;

    float const dist = std::sqrt(std::max(dist_sq, 0.0001F));
    float const bypass_step =
        std::max(max_navigation_speed(*unit, nullptr) * delta_time, 0.01F);

    if (dist <= bypass_step) {
      transform->position.x = builder_prod->bypass_target_x;
      transform->position.z = builder_prod->bypass_target_z;
      builder_prod->bypass_movement_active = false;
      movement->vx = 0.0F;
      movement->vz = 0.0F;
      movement->has_target = false;
      movement->clear_path();
      OrderService::clear_player_order_intent(entity);
      facts->progress.state = Engine::Core::MovementOrderState::Arrived;
    } else {
      float const nx = dx / dist;
      float const nz = dz / dist;
      float const base_speed = max_navigation_speed(*unit, nullptr);
      movement->vx = nx * base_speed;
      movement->vz = nz * base_speed;

      transform->position.x += movement->vx * delta_time;
      transform->position.z += movement->vz * delta_time;

      float const target_yaw =
          std::atan2(movement->vx, movement->vz) * 180.0F / std::numbers::pi_v<float>;
      float const current = transform->rotation.y;
      float const diff = std::fmod((target_yaw - current + 540.0F), 360.0F) - 180.0F;
      float const turn_speed = body_turn_speed_degrees(unit->spawn_type);
      float const step =
          std::clamp(diff, -turn_speed * delta_time, turn_speed * delta_time);
      transform->rotation.y = current + step;
      facts->progress.state = Engine::Core::MovementOrderState::Following;
    }

    facts->motor.valid = true;
    facts->motor.accepted_dx = transform->position.x - previous_x;
    facts->motor.accepted_dz = transform->position.z - previous_z;
    facts->motor.accepted_vx = facts->motor.accepted_dx / std::max(1.0e-5F, delta_time);
    facts->motor.accepted_vz = facts->motor.accepted_dz / std::max(1.0e-5F, delta_time);
    return;
  }

  auto* stamina = entity->get_component<Engine::Core::StaminaComponent>();
  MotorLimits const limits = motor_limits(*entity, *unit, stamina);

  float target_vx = 0.0F;
  float target_vz = 0.0F;
  bool have_target_velocity = false;
  if (facts->desired.valid && facts->steering.valid) {
    target_vx = facts->steering.velocity_x;
    target_vz = facts->steering.velocity_z;
    have_target_velocity = true;
  } else if (facts->desired.valid) {
    target_vx = facts->desired.velocity_x;
    target_vz = facts->desired.velocity_z;
    have_target_velocity = true;
  }

  if (!have_target_velocity) {
    movement->vx *= std::max(0.0F, 1.0F - limits.damping * delta_time);
    movement->vz *= std::max(0.0F, 1.0F - limits.damping * delta_time);
  } else {
    float const ax = (target_vx - movement->vx) * limits.acceleration;
    float const az = (target_vz - movement->vz) * limits.acceleration;
    movement->vx += ax * delta_time;
    movement->vz += az * delta_time;
    movement->vx *= std::max(0.0F, 1.0F - 0.5F * limits.damping * delta_time);
    movement->vz *= std::max(0.0F, 1.0F - 0.5F * limits.damping * delta_time);
  }

  QVector3D const current_pos_3d(transform->position.x, 0.0F, transform->position.z);
  bool const was_on_valid_tile = is_movement_point_allowed(current_pos_3d, *entity);

  float const old_x = transform->position.x;
  float const old_z = transform->position.z;

  auto const heading = heading_reference(*entity, *transform, *movement, unit);
  float const translation_scale =
      was_on_valid_tile ? heading_translation_scale(transform->rotation.y, heading)
                        : 1.0F;
  if (translation_scale < k_turning_translation_threshold &&
      facts->progress.state == Engine::Core::MovementOrderState::Following) {
    facts->progress.state = Engine::Core::MovementOrderState::Turning;
  }

  float translated_vx = movement->vx * translation_scale;
  float translated_vz = movement->vz * translation_scale;

  if (auto const* traversal =
          entity->get_component<Engine::Core::UnitTraversalLayoutStateComponent>();
      traversal != nullptr && traversal->root_motion_blocked && world != nullptr &&
      world->presentation_enabled() &&
      entity->has_component<Engine::Core::RenderableComponent>()) {
    translated_vx = 0.0F;
    translated_vz = 0.0F;
    movement->vx = 0.0F;
    movement->vz = 0.0F;
    facts->progress.state = Engine::Core::MovementOrderState::Yielding;
  }

  MotorCollision const collision(*entity, old_x, old_z);

  auto const* pathfinder = NavGrid::get_pathfinder();
  float const cell_size = pathfinder != nullptr ? pathfinder->grid_cell_size() : 1.0F;
  auto const sweep = sweep_body(old_x,
                                old_z,
                                translated_vx * delta_time,
                                translated_vz * delta_time,
                                std::max(0.05F, cell_size * k_motor_substep_cells),
                                [&collision](float fx, float fz, float tx, float tz) {
                                  return collision.step_allowed(fx, fz, tx, tz);
                                });

  transform->position.x = old_x + sweep.accepted_dx;
  transform->position.z = old_z + sweep.accepted_dz;

  if (sweep.blocked) {
    movement->vx = 0.0F;
    movement->vz = 0.0F;
  } else if (sweep.contact) {

    float const into = movement->vx * sweep.normal_x + movement->vz * sweep.normal_z;
    if (into < 0.0F) {
      movement->vx -= sweep.normal_x * into;
      movement->vz -= sweep.normal_z * into;
    }
  }

  float const stepped_x = transform->position.x - old_x;
  float const stepped_z = transform->position.z - old_z;
  movement->travelled += std::sqrt((stepped_x * stepped_x) + (stepped_z * stepped_z));

  constexpr float k_travelled_wrap = 4096.0F;
  if (movement->travelled >= k_travelled_wrap) {
    movement->travelled -= k_travelled_wrap;
  }

  facts->motor.valid = true;
  facts->motor.accepted_dx = stepped_x;
  facts->motor.accepted_dz = stepped_z;
  facts->motor.accepted_vx = stepped_x / std::max(1.0e-5F, delta_time);
  facts->motor.accepted_vz = stepped_z / std::max(1.0e-5F, delta_time);
  facts->motor.rejected_dx = sweep.rejected_dx;
  facts->motor.rejected_dz = sweep.rejected_dz;
  facts->motor.accepted_fraction = sweep.accepted_fraction;
  facts->motor.blocked = sweep.blocked;
  facts->motor.has_contact = sweep.contact;
  facts->motor.contact_nx = sweep.normal_x;
  facts->motor.contact_nz = sweep.normal_z;

  QVector3D const settled_pos(transform->position.x, 0.0F, transform->position.z);
  facts->motor.penetration_depth =
      is_movement_point_allowed(settled_pos, *entity)
          ? 0.0F
          : Walkability::penetration(settled_pos,
                                     MotorCollision::body_profile(*entity));

  if (sweep.blocked) {
    ++facts->progress.blocked_steps;
  } else if (facts->motor.accepted_fraction > 0.5F) {
    facts->progress.blocked_steps = 0;
  }

  finalize_orientation(entity, transform, movement, delta_time);
}

auto MovementSystem::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(Reads<UnitComponent,
                                     BuildingComponent,
                                     CommanderComponent,
                                     GuardModeComponent,
                                     HoldModeComponent,
                                     ElephantComponent,
                                     RpgCommanderActionComponent,
                                     BuilderProductionComponent,
                                     UnitTraversalLayoutStateComponent,
                                     RenderableComponent,
                                     PendingRemovalComponent>{},
                               Writes<MovementComponent,
                                      MovementFactsComponent,
                                      TransformComponent,
                                      AttackComponent,
                                      StaminaComponent,
                                      TerrainContextComponent>{});
}

} // namespace Game::Systems
