#include "local_avoidance_system.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
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

// The steered velocity is a separate fact from the desired one. The motor stays
// the authority that accepts or rejects the displacement it implies.
void publish_steering(Engine::Core::SystemContext& context,
                      Engine::Core::EntityID entity_id,
                      float desired_x,
                      float desired_z,
                      float correction_x,
                      float correction_z,
                      std::uint32_t neighbor_count) {
  auto* facts = context.try_get<Engine::Core::MovementFactsComponent>(entity_id);
  if (facts == nullptr || !facts->desired.valid) {
    return;
  }
  facts->steering.valid = true;
  facts->steering.correction_x = correction_x;
  facts->steering.correction_z = correction_z;
  facts->steering.velocity_x = desired_x + correction_x;
  facts->steering.velocity_z = desired_z + correction_z;
  facts->steering.neighbor_count = neighbor_count;
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

} // namespace

auto LocalAvoidanceSystem::cell_key(int cell_x, int cell_z) -> std::int64_t {
  auto const high = static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell_x));
  auto const low = static_cast<std::uint32_t>(cell_z);
  return static_cast<std::int64_t>((high << 32U) | low);
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

  float const inv_cell_size = 1.0F / k_default_cell_size;
  for (std::int64_t const key : m_active_cell_keys) {
    if (auto bucket = m_grid.find(key); bucket != m_grid.end()) {
      bucket->second.clear();
    }
  }
  m_active_cell_keys.clear();
  m_circles.clear();
  m_grid.reserve(std::max(m_previous_cell_count, unit_ids.size() / 2U));
  m_active_cell_keys.reserve(std::max(m_previous_cell_count, unit_ids.size() / 2U));
  m_circles.reserve(unit_ids.size());

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
    circle.radius = CommandService::get_unit_radius(context.world(), entity_id);

    const auto* movement = context.try_get<Engine::Core::MovementComponent>(entity_id);
    const auto* facts =
        context.try_get<Engine::Core::MovementFactsComponent>(entity_id);
    if (movement != nullptr) {
      // Steering reads the route follower's desired velocity, never the motor's
      // integrated one. Correcting an already-integrated velocity and handing it
      // back for further integration is what let avoidance and movement take
      // turns overwriting the same number.
      bool const has_desired = facts != nullptr && facts->desired.valid;
      circle = UnitCircle{circle.id,
                          circle.x,
                          circle.z,
                          circle.radius,
                          has_desired ? facts->desired.velocity_x : 0.0F,
                          has_desired ? facts->desired.velocity_z : 0.0F,
                          circle.priority,
                          movement->get_has_target(),
                          movement->has_waypoints()};
    }

    circle.priority = compute_avoidance_priority(context, entity_id);

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

  m_diagnostics.units_processed = static_cast<std::uint32_t>(m_circles.size());

  std::uint32_t total_neighbors_checked = 0;
  std::uint32_t overlaps_detected = 0;

  for (std::size_t i = 0; i < m_circles.size(); ++i) {
    auto& ci = m_circles[i];
    if (!ci.is_moving) {
      continue;
    }
    CellKey const center = to_cell(ci.x, ci.z, inv_cell_size);
    float sep_x = 0.0F;
    float sep_z = 0.0F;
    int neighbor_count = 0;

    for (int dx = -1; dx <= 1; ++dx) {
      for (int dz = -1; dz <= 1; ++dz) {
        CellKey const neighbor_key{center.cx + dx, center.cz + dz};
        auto it = m_grid.find(cell_key(neighbor_key.cx, neighbor_key.cz));
        if (it == m_grid.end()) {
          continue;
        }
        for (std::size_t const j : it->second) {
          if (j == i) {
            continue;
          }
          auto& cj = m_circles[j];
          auto const* own_formation =
              context.try_get<Engine::Core::FormationModeComponent>(ci.id);
          auto const* other_formation =
              context.try_get<Engine::Core::FormationModeComponent>(cj.id);
          auto const* own_unit = context.try_get<Engine::Core::UnitComponent>(ci.id);
          auto const* other_unit = context.try_get<Engine::Core::UnitComponent>(cj.id);
          bool const same_friendly_formation_owner =
              own_formation != nullptr && other_formation != nullptr &&
              own_formation->formation_id != 0U &&
              other_formation->formation_id != 0U && own_unit != nullptr &&
              other_unit != nullptr && own_unit->owner_id != 0 &&
              own_unit->owner_id == other_unit->owner_id;
          bool const same_formation =
              own_formation != nullptr && other_formation != nullptr &&
              own_formation->formation_id != 0U &&
              own_formation->formation_id == other_formation->formation_id;
          if (same_formation || same_friendly_formation_owner) {

            continue;
          }
          float const ddx = ci.x - cj.x;
          float const ddz = ci.z - cj.z;
          float const dist_sq = ddx * ddx + ddz * ddz;
          float const min_dist = ci.radius + cj.radius + k_separation_radius;
          float const min_dist_sq = min_dist * min_dist;

          ++total_neighbors_checked;

          if (dist_sq < min_dist_sq) {
            float dist = std::sqrt(std::max(dist_sq, 0.0F));
            float nx = 0.0F;
            float nz = 0.0F;
            if (dist > 1e-6F) {
              nx = ddx / dist;
              nz = ddz / dist;
            } else {
              auto const seed =
                  static_cast<std::uint32_t>((ci.id * 73856093U) ^ (cj.id * 19349663U));
              float const angle = static_cast<float>(seed % 6283U) * 0.001F;
              nx = std::cos(angle);
              nz = std::sin(angle);
              dist = 0.0F;
            }
            float const overlap = min_dist - dist;

            float weight = 1.0F;
            if (cj.priority > ci.priority) {
              weight = 1.5F;
            } else if (cj.priority < ci.priority) {
              weight = 0.5F;
            }

            sep_x += nx * overlap * weight;
            sep_z += nz * overlap * weight;
            ++neighbor_count;
          }
        }
      }
    }

    if (neighbor_count > 0) {
      float const inv_n = 1.0F / static_cast<float>(neighbor_count);
      sep_x *= inv_n * k_separation_strength;
      sep_z *= inv_n * k_separation_strength;

      float const mag_sq = sep_x * sep_x + sep_z * sep_z;
      float const max_corr = k_max_steering_speed;
      if (mag_sq > max_corr * max_corr) {
        float const mag = std::sqrt(mag_sq);
        sep_x = sep_x / mag * max_corr;
        sep_z = sep_z / mag * max_corr;
      }

      float const speed = std::hypot(ci.vx, ci.vz);
      if (speed > 1.0e-4F) {
        float const forward_x = ci.vx / speed;
        float const forward_z = ci.vz / speed;
        float const forward_separation = sep_x * forward_x + sep_z * forward_z;
        float lateral_x = sep_x - forward_x * forward_separation;
        float lateral_z = sep_z - forward_z * forward_separation;
        float const lateral_length = std::hypot(lateral_x, lateral_z);
        if (lateral_length > 1.0e-4F) {
          Point const cell = NavGrid::world_to_grid(ci.x, ci.z);
          bool portal_constrains_lateral = ci.follows_navigation_path ||
                                           terrain.is_on_bridge(ci.x, ci.z) ||
                                           terrain.is_hill_entrance(cell.x, cell.y);
          if (!portal_constrains_lateral) {
            portal_constrains_lateral =
                point_is_in_navigation_passage(buildings, ci.x, ci.z);
          }
          float const probe_distance = std::max(0.75F, ci.radius + 0.25F);
          QVector3D const lateral_probe(
              ci.x + lateral_x / lateral_length * probe_distance,
              0.0F,
              ci.z + lateral_z / lateral_length * probe_distance);
          if (portal_constrains_lateral ||
              !NavGrid::is_world_position_walkable(lateral_probe)) {
            lateral_x = 0.0F;
            lateral_z = 0.0F;
          }
        }
        sep_x = lateral_x;
        sep_z = lateral_z;
      }

      publish_steering(context,
                       ci.id,
                       ci.vx,
                       ci.vz,
                       sep_x,
                       sep_z,
                       static_cast<std::uint32_t>(neighbor_count));
      ++overlaps_detected;
    } else {
      publish_steering(context, ci.id, ci.vx, ci.vz, 0.0F, 0.0F, 0U);
    }
  }

  m_diagnostics.overlaps_detected = overlaps_detected;
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
                                     BuildingComponent,
                                     PendingRemovalComponent>{},
                               Writes<MovementFactsComponent>{});
}

} // namespace Game::Systems
