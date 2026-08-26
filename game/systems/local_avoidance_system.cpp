#include "local_avoidance_system.h"

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <unordered_map>
#include <vector>

#include "../core/ambient_session.h"
#include "../core/component.h"
#include "../core/entity.h"
#include "../core/system_context.h"
#include "../core/world.h"
#include "../map/terrain_service.h"
#include "building_collision_registry.h"
#include "command_service.h"
#include "nav_grid.h"
#include "pathfinding.h"

namespace Game::Systems {

namespace {

struct CellKey {
  int cx;
  int cz;
};

auto to_cell(float x, float z, float inv_cell_size) -> CellKey {
  return {static_cast<int>(std::floor(x * inv_cell_size)),
          static_cast<int>(std::floor(z * inv_cell_size))};
}

constexpr std::array<float, LocalAvoidanceSystem::k_candidate_angle_count>
    k_candidate_angles_degrees{
        0.0F, -12.0F, 12.0F, -26.0F, 26.0F, -45.0F, 45.0F, -70.0F, 70.0F};

constexpr std::array<float, LocalAvoidanceSystem::k_candidate_speed_count>
    k_candidate_speed_scales{1.0F, 0.6F, 0.25F};

constexpr float k_deviation_weight = 1.0F;
constexpr float k_collision_weight = 40.0F;
constexpr float k_stopping_penalty = 1.6F;
constexpr float k_side_commitment_bonus = 0.45F;
constexpr float k_angle_commitment_bonus = 0.30F;

constexpr float k_deviation_rate_degrees_per_second = 150.0F;

constexpr float k_corridor_lateral_relief = 0.45F;

auto compute_avoidance_priority(Engine::Core::SystemContext& context,
                                Engine::Core::EntityID entity_id) -> std::uint8_t {
  if (context.has<Engine::Core::BuildingComponent>(entity_id)) {
    return 4;
  }
  const auto* atk = context.try_get<Engine::Core::AttackComponent>(entity_id);
  if (atk != nullptr && atk->in_melee_lock) {
    return 3;
  }
  const auto* intent =
      context.try_get<Engine::Core::MovementIntentComponent>(entity_id);
  if (intent != nullptr) {
    return intent->priority;
  }
  const auto* movement = context.try_get<Engine::Core::MovementComponent>(entity_id);
  if (movement != nullptr && movement->get_has_target()) {
    return 1;
  }
  return 2;
}

auto point_is_in_navigation_passage(const BuildingCollisionRegistry& buildings,
                                    float x,
                                    float z) -> bool {
  for (const auto& passage : buildings.navigation_passages()) {
    if (std::abs(x - passage.center_x) <= passage.width * 0.5F + 0.5F &&
        std::abs(z - passage.center_z) <= passage.depth * 0.5F + 0.5F) {
      return true;
    }
  }
  return false;
}

auto time_to_collision(float px,
                       float pz,
                       float vx,
                       float vz,
                       float combined_radius,
                       float horizon) -> float {
  float const distance_sq = px * px + pz * pz;
  float const radius_sq = combined_radius * combined_radius;
  if (distance_sq <= radius_sq) {
    return 0.0F;
  }

  float const a = vx * vx + vz * vz;
  if (a <= 1.0e-8F) {
    return -1.0F;
  }
  float const b = px * vx + pz * vz;
  if (b >= 0.0F) {
    return -1.0F;
  }
  float const c = distance_sq - radius_sq;
  float const discriminant = b * b - a * c;
  if (discriminant <= 0.0F) {
    return -1.0F;
  }
  float const t = (-b - std::sqrt(discriminant)) / a;
  if (t < 0.0F || t > horizon) {
    return -1.0F;
  }
  return t;
}

} // namespace

auto LocalAvoidanceSystem::cell_key(int cell_x, int cell_z) -> std::int64_t {
  auto const high = static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell_x));
  auto const low = static_cast<std::uint32_t>(cell_z);
  return static_cast<std::int64_t>((high << 32U) | low);
}

void LocalAvoidanceSystem::build_index(Engine::Core::SystemContext& context) {
  float const inv_cell_size = 1.0F / k_default_cell_size;
  for (std::int64_t const key : m_active_cell_keys) {
    if (auto bucket = m_grid.find(key); bucket != m_grid.end()) {
      bucket->second.clear();
    }
  }
  m_active_cell_keys.clear();
  m_circles.clear();

  for (auto [entity_id, unit, transform] :
       context.view<Engine::Core::UnitComponent, Engine::Core::TransformComponent>()) {
    if (unit.health <= 0) {
      continue;
    }
    if (context.has<Engine::Core::BuildingComponent>(entity_id)) {
      continue;
    }
    if (context.has<Engine::Core::PendingRemovalComponent>(entity_id)) {
      continue;
    }

    UnitCircle circle;
    circle.id = entity_id;
    circle.x = transform.position.x;
    circle.z = transform.position.z;
    auto const radii = CommandService::get_unit_radii(context.world(), entity_id);
    circle.radius = radii.envelope;
    circle.core_radius = radii.core;
    circle.owner_id = unit.owner_id;
    circle.priority = compute_avoidance_priority(context, entity_id);

    const auto* movement = context.try_get<Engine::Core::MovementComponent>(entity_id);
    const auto* facts =
        context.try_get<Engine::Core::MovementFactsComponent>(entity_id);
    if (movement != nullptr) {
      circle.is_moving = movement->get_has_target();
      circle.follows_navigation_path = movement->has_waypoints();
    }
    if (facts != nullptr && facts->desired.valid) {

      circle.desired_vx = facts->desired.velocity_x;
      circle.desired_vz = facts->desired.velocity_z;
      circle.predicted_vx = facts->steering.valid ? facts->steering.velocity_x
                                                  : facts->desired.velocity_x;
      circle.predicted_vz = facts->steering.valid ? facts->steering.velocity_z
                                                  : facts->desired.velocity_z;
      circle.max_speed = std::max(facts->desired.speed_limit,
                                  std::hypot(circle.desired_vx, circle.desired_vz));
      circle.avoids = true;
    }
    if (const auto* formation =
            context.try_get<Engine::Core::FormationModeComponent>(entity_id)) {
      circle.formation_id = formation->formation_id;
    }

    if (const auto* attack = context.try_get<Engine::Core::AttackComponent>(entity_id);
        attack != nullptr && attack->in_melee_lock) {
      circle.engaged_target = attack->melee_lock_target_id;
    } else if (const auto* target =
                   context.try_get<Engine::Core::AttackTargetComponent>(entity_id)) {
      circle.engaged_target = target->target_id;
    }

    std::size_t const idx = m_circles.size();
    m_circles.push_back(circle);

    CellKey const key = to_cell(circle.x, circle.z, inv_cell_size);
    std::int64_t const packed_key = cell_key(key.cx, key.cz);
    auto& bucket = m_grid[packed_key];
    if (bucket.empty()) {
      m_active_cell_keys.push_back(packed_key);
    }
    bucket.push_back(idx);
  }
  m_previous_cell_count = std::max(m_previous_cell_count, m_active_cell_keys.size());
}

auto LocalAvoidanceSystem::gather_neighbors(std::size_t self,
                                            float horizon) -> std::size_t {
  m_neighbors.clear();
  m_neighbors.reserve(k_max_neighbors + 1U);
  auto const& me = m_circles[self];
  float const inv_cell_size = 1.0F / k_default_cell_size;
  CellKey const center = to_cell(me.x, me.z, inv_cell_size);

  auto const sorts_before = [this](const Neighbor& lhs, const Neighbor& rhs) {
    float const left = lhs.time_to_collision < 0.0F ? 1.0e9F : lhs.time_to_collision;
    float const right = rhs.time_to_collision < 0.0F ? 1.0e9F : rhs.time_to_collision;
    if (left != right) {
      return left < right;
    }
    return m_circles[lhs.index].id < m_circles[rhs.index].id;
  };

  std::uint32_t examined = 0;
  for (int dx = -1; dx <= 1; ++dx) {
    for (int dz = -1; dz <= 1; ++dz) {
      auto const found = m_grid.find(cell_key(center.cx + dx, center.cz + dz));
      if (found == m_grid.end()) {
        continue;
      }
      for (std::size_t const other : found->second) {
        if (other == self) {
          continue;
        }
        auto const& them = m_circles[other];
        if (them.id == me.engaged_target || me.id == them.engaged_target) {
          continue;
        }
        ++examined;

        float const px = them.x - me.x;
        float const pz = them.z - me.z;
        float const combined =
            me.radius + them.radius + LocalAvoidanceSystem::k_separation_radius;
        float const reach = combined + (me.max_speed + them.max_speed) * horizon;
        if (px * px + pz * pz > reach * reach) {
          continue;
        }

        float const ttc = time_to_collision(px,
                                            pz,
                                            them.predicted_vx - me.desired_vx,
                                            them.predicted_vz - me.desired_vz,
                                            combined,
                                            horizon);

        m_neighbors.push_back({other, ttc, 0.5F});
        std::push_heap(m_neighbors.begin(), m_neighbors.end(), sorts_before);
        if (m_neighbors.size() > k_max_neighbors) {
          std::pop_heap(m_neighbors.begin(), m_neighbors.end(), sorts_before);
          m_neighbors.pop_back();
        }
      }
    }
  }

  std::sort_heap(m_neighbors.begin(), m_neighbors.end(), sorts_before);

  for (auto& neighbor : m_neighbors) {
    auto const& them = m_circles[neighbor.index];
    float const px = them.x - me.x;
    float const pz = them.z - me.z;

    float response = 0.5F;
    if (!them.avoids || !them.is_moving) {
      response = 1.0F;
    } else if (them.priority > me.priority) {
      response = 0.75F;
    } else if (them.priority < me.priority) {
      response = 0.25F;
    } else {

      float const speed = std::hypot(me.desired_vx, me.desired_vz);
      if (speed > 1.0e-4F) {
        float const cross = (me.desired_vx * pz - me.desired_vz * px) / speed;
        response = cross < 0.0F ? 0.8F : 0.2F;
      }
    }

    if (them.formation_id != 0U && them.formation_id == me.formation_id) {
      response *= 0.6F;
    }
    neighbor.response = response;
  }

  return examined;
}

void LocalAvoidanceSystem::run(Engine::Core::SystemContext& context) {
  const float delta_time = context.delta_time();
  if (delta_time <= 0.0F) {
    return;
  }

  m_diagnostics = {};

  auto const unit_ids = context.entities_with<Engine::Core::UnitComponent>();
  if (unit_ids.empty()) {
    return;
  }

  const auto& services = Game::Session::services_for(context.world());
  const auto& terrain = *services.terrain;
  const auto& buildings = *services.building_collision;

  m_grid.reserve(std::max(m_previous_cell_count, unit_ids.size() / 2U));
  m_active_cell_keys.reserve(std::max(m_previous_cell_count, unit_ids.size() / 2U));
  m_circles.reserve(unit_ids.size());
  build_index(context);

  m_diagnostics.units_processed = static_cast<std::uint32_t>(m_circles.size());

  std::uint32_t total_neighbors_checked = 0;
  std::uint32_t overlaps_detected = 0;
  std::uint32_t units_steered = 0;
  std::uint32_t candidates_evaluated = 0;

  for (std::size_t self = 0; self < m_circles.size(); ++self) {
    auto const& me = m_circles[self];
    if (!me.avoids) {

      if (auto* stale = context.try_get<Engine::Core::MovementFactsComponent>(me.id)) {
        stale->steering.valid = false;
      }
      continue;
    }

    auto* facts = context.try_get<Engine::Core::MovementFactsComponent>(me.id);
    if (facts == nullptr || !facts->desired.valid) {
      continue;
    }
    facts->steering = {};

    facts->steering.valid = true;
    facts->steering.velocity_x = me.desired_vx;
    facts->steering.velocity_z = me.desired_vz;
    facts->steering.correction_x = 0.0F;
    facts->steering.correction_z = 0.0F;
    facts->steering.separation_x = 0.0F;
    facts->steering.separation_z = 0.0F;
    facts->steering.result = Engine::Core::SteeringResult::Unconstrained;

    total_neighbors_checked +=
        static_cast<std::uint32_t>(gather_neighbors(self, k_time_horizon_seconds));
    facts->steering.neighbor_count = static_cast<std::uint32_t>(m_neighbors.size());

    float const desired_speed = std::hypot(me.desired_vx, me.desired_vz);

    Point const own_cell = NavGrid::world_to_grid(me.x, me.z);
    bool corridor_constrained = me.follows_navigation_path ||
                                terrain.is_on_bridge(me.x, me.z) ||
                                terrain.is_hill_entrance(own_cell.x, own_cell.y);
    if (!corridor_constrained) {
      corridor_constrained = point_is_in_navigation_passage(buildings, me.x, me.z);
    }
    if (!corridor_constrained && me.formation_id != 0U) {

      for (auto const& neighbor : m_neighbors) {
        if (m_circles[neighbor.index].formation_id == me.formation_id) {
          corridor_constrained = true;
          break;
        }
      }
    }

    float separation_x = 0.0F;
    float separation_z = 0.0F;
    for (auto const& neighbor : m_neighbors) {
      auto const& them = m_circles[neighbor.index];
      float const px = me.x - them.x;
      float const pz = me.z - them.z;

      float const combined = me.core_radius + them.core_radius;
      float const distance = std::hypot(px, pz);
      if (distance >= combined || combined <= 1.0e-4F) {
        continue;
      }
      float nx = 0.0F;
      float nz = 0.0F;
      if (distance > 1.0e-5F) {
        nx = px / distance;
        nz = pz / distance;
      } else {

        bool const first = me.id < them.id;
        nx = first ? 1.0F : -1.0F;
      }
      float const overlap = combined - distance;
      separation_x += nx * overlap * neighbor.response;
      separation_z += nz * overlap * neighbor.response;
      ++overlaps_detected;
    }
    if (corridor_constrained && desired_speed > 1.0e-4F) {

      float const tangent_x = me.desired_vx / desired_speed;
      float const tangent_z = me.desired_vz / desired_speed;
      float const along = separation_x * tangent_x + separation_z * tangent_z;
      float const lateral_x = separation_x - (tangent_x * along);
      float const lateral_z = separation_z - (tangent_z * along);
      separation_x = (tangent_x * along) + (lateral_x * k_corridor_lateral_relief);
      separation_z = (tangent_z * along) + (lateral_z * k_corridor_lateral_relief);
    }

    if (float const magnitude = std::hypot(separation_x, separation_z);
        magnitude > 1.0e-5F) {
      float const scale =
          std::min(k_overlap_correction_speed, magnitude / delta_time) / magnitude;
      facts->steering.separation_x = separation_x * scale;
      facts->steering.separation_z = separation_z * scale;
      facts->steering.result = Engine::Core::SteeringResult::Separating;
    }

    float nearest_ttc = -1.0F;
    for (auto const& neighbor : m_neighbors) {
      if (neighbor.time_to_collision < 0.0F) {
        continue;
      }
      if (nearest_ttc < 0.0F || neighbor.time_to_collision < nearest_ttc) {
        nearest_ttc = neighbor.time_to_collision;
      }
    }
    facts->steering.nearest_time_to_collision = nearest_ttc;

    if (nearest_ttc < 0.0F) {
      float const relax = k_deviation_rate_degrees_per_second * delta_time;
      facts->passing.deviation_degrees =
          facts->passing.deviation_degrees > 0.0F
              ? std::max(0.0F, facts->passing.deviation_degrees - relax)
              : std::min(0.0F, facts->passing.deviation_degrees + relax);
      if (facts->passing.side != 0) {
        facts->passing.held_seconds += delta_time;
        if (facts->passing.held_seconds > k_passing_side_hold_seconds) {
          facts->passing.side = 0;
          facts->passing.held_seconds = 0.0F;
        }
      }
      facts->steering.passing_side = facts->passing.side;
      continue;
    }

    if (desired_speed <= 1.0e-4F) {
      facts->steering.passing_side = facts->passing.side;
      continue;
    }
    float const desired_angle = std::atan2(me.desired_vx, me.desired_vz);

    float best_cost = std::numeric_limits<float>::infinity();
    float best_vx = me.desired_vx;
    float best_vz = me.desired_vz;
    std::int8_t best_side = 0;
    std::int8_t best_angle_index = -1;

    for (int angle_index = 0; angle_index < k_candidate_angle_count; ++angle_index) {
      float const offset_degrees = k_candidate_angles_degrees[angle_index];
      float const angle =
          desired_angle + offset_degrees * std::numbers::pi_v<float> / 180.0F;
      float const dir_x = std::sin(angle);
      float const dir_z = std::cos(angle);
      auto const side = static_cast<std::int8_t>(
          offset_degrees > 0.0F ? 1 : (offset_degrees < 0.0F ? -1 : 0));

      for (int speed_index = 0; speed_index < k_candidate_speed_count; ++speed_index) {
        float const speed = desired_speed * k_candidate_speed_scales[speed_index];
        float const vx = dir_x * speed;
        float const vz = dir_z * speed;
        ++candidates_evaluated;

        float cost =
            k_deviation_weight * std::hypot(vx - me.desired_vx, vz - me.desired_vz);

        cost += k_stopping_penalty * (1.0F - k_candidate_speed_scales[speed_index]) *
                desired_speed;
        if (side != 0 && side == facts->passing.side) {
          cost -= k_side_commitment_bonus * desired_speed;
        }

        if (angle_index == facts->passing.angle_index) {
          cost -= k_angle_commitment_bonus * desired_speed;
        }

        for (auto const& neighbor : m_neighbors) {
          auto const& them = m_circles[neighbor.index];
          float const combined = me.radius + them.radius + k_separation_radius;

          if (std::hypot(them.x - me.x, them.z - me.z) < me.radius + them.radius) {
            continue;
          }
          float const ttc =
              time_to_collision(them.x - me.x,
                                them.z - me.z,
                                (them.predicted_vx - vx) * neighbor.response,
                                (them.predicted_vz - vz) * neighbor.response,
                                combined,
                                k_time_horizon_seconds);
          if (ttc < 0.0F) {
            continue;
          }
          cost += k_collision_weight * neighbor.response / std::max(0.05F, ttc);
        }

        if (cost < best_cost) {
          best_cost = cost;
          best_vx = vx;
          best_vz = vz;
          best_side = side;
          best_angle_index = static_cast<std::int8_t>(angle_index);
        }
      }
    }

    float const lateral_x = best_vx - me.desired_vx;
    float const lateral_z = best_vz - me.desired_vz;
    if (float const lateral = std::hypot(lateral_x, lateral_z); lateral > 1.0e-4F) {
      bool const constrained = corridor_constrained;
      float const probe_distance = std::max(0.75F, me.radius + 0.25F);
      QVector3D const probe(me.x + lateral_x / lateral * probe_distance,
                            0.0F,
                            me.z + lateral_z / lateral * probe_distance);
      if (constrained || !NavGrid::is_world_position_walkable(probe)) {
        float const scale = std::min(
            1.0F, std::hypot(best_vx, best_vz) / std::max(1.0e-4F, desired_speed));
        best_vx = me.desired_vx * scale;
        best_vz = me.desired_vz * scale;
        best_side = 0;
        best_angle_index = 0;
      }
    }

    if (best_side != 0 && facts->passing.side != best_side) {
      if (facts->passing.side == 0 ||
          facts->passing.held_seconds > k_passing_side_hold_seconds) {
        facts->passing.side = best_side;
        facts->passing.held_seconds = 0.0F;
      }
    } else {
      facts->passing.held_seconds += delta_time;
    }

    {
      float const chosen_speed = std::hypot(best_vx, best_vz);
      float deviation = 0.0F;
      if (chosen_speed > 1.0e-4F) {
        deviation = (std::atan2(best_vx, best_vz) - desired_angle) * 180.0F /
                    std::numbers::pi_v<float>;
        deviation = std::fmod(deviation + 540.0F, 360.0F) - 180.0F;
      }
      float const max_step = k_deviation_rate_degrees_per_second * delta_time;
      float const change =
          std::clamp(deviation - facts->passing.deviation_degrees, -max_step, max_step);
      float const limited = facts->passing.deviation_degrees + change;
      facts->passing.deviation_degrees = limited;
      if (chosen_speed > 1.0e-4F) {
        float const angle =
            desired_angle + limited * std::numbers::pi_v<float> / 180.0F;
        best_vx = std::sin(angle) * chosen_speed;
        best_vz = std::cos(angle) * chosen_speed;
      }
    }

    facts->passing.angle_index = best_angle_index;
    facts->steering.velocity_x = best_vx;
    facts->steering.velocity_z = best_vz;
    facts->steering.correction_x = best_vx - me.desired_vx;
    facts->steering.correction_z = best_vz - me.desired_vz;
    facts->steering.passing_side = facts->passing.side;

    float const steered_speed = std::hypot(best_vx, best_vz);
    if (std::hypot(facts->steering.correction_x, facts->steering.correction_z) >
        1.0e-4F) {
      ++units_steered;
      if (facts->steering.result != Engine::Core::SteeringResult::Separating) {
        facts->steering.result =
            steered_speed < desired_speed * 0.5F
                ? Engine::Core::SteeringResult::Yielded
                : (best_side != 0 ? Engine::Core::SteeringResult::Deviated
                                  : Engine::Core::SteeringResult::Slowed);
      }
    }
  }

  m_diagnostics.overlaps_detected = overlaps_detected;
  m_diagnostics.units_steered = units_steered;
  m_diagnostics.candidates_evaluated = candidates_evaluated;
  if (!m_circles.empty()) {
    m_diagnostics.average_neighbors_checked =
        total_neighbors_checked / static_cast<std::uint32_t>(m_circles.size());
  }
}

auto LocalAvoidanceSystem::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(Reads<UnitComponent,
                                     TransformComponent,
                                     AttackComponent,
                                     MovementIntentComponent,
                                     MovementComponent,
                                     FormationModeComponent,
                                     BuildingComponent,
                                     PendingRemovalComponent>{},
                               Writes<MovementFactsComponent>{});
}

} // namespace Game::Systems
