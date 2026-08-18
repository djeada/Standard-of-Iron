#include "local_avoidance_system.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "../core/component.h"
#include "../core/entity.h"
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

auto compute_avoidance_priority(const Engine::Core::Entity& entity) -> std::uint8_t {
  if (entity.has_component<Engine::Core::BuildingComponent>()) {
    return 4;
  }
  const auto* atk = entity.get_component<Engine::Core::AttackComponent>();
  if (atk != nullptr && atk->in_melee_lock) {
    return 3;
  }
  const auto* intent = entity.get_component<Engine::Core::MovementIntentComponent>();
  if (intent != nullptr) {
    return intent->priority;
  }
  const auto* movement = entity.get_component<Engine::Core::MovementComponent>();
  if (movement != nullptr && movement->get_has_target()) {
    return 1;
  }
  return 2;
}

auto point_is_in_navigation_passage(float x, float z) -> bool {
  for (const auto& passage :
       BuildingCollisionRegistry::instance().navigation_passages()) {
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

void LocalAvoidanceSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr || delta_time <= 0.0F) {
    return;
  }

  m_diagnostics = {};

  auto const unit_ids = world->entities_with<Engine::Core::UnitComponent>();
  if (unit_ids.empty()) {
    return;
  }
  world->resolve_entities_into(unit_ids, m_query_scratch);

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

  for (auto* entity : m_query_scratch) {
    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (transform == nullptr || unit == nullptr || unit->health <= 0) {
      continue;
    }
    if (entity->has_component<Engine::Core::BuildingComponent>()) {
      continue;
    }
    if (entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    UnitCircle circle;
    circle.id = entity->get_id();
    circle.x = transform->position.x;
    circle.z = transform->position.z;
    circle.radius = CommandService::get_unit_radius(*world, entity->get_id());

    auto* movement = entity->get_component<Engine::Core::MovementComponent>();
    if (movement != nullptr) {
      circle = UnitCircle{circle.id,
                          circle.x,
                          circle.z,
                          circle.radius,
                          movement->get_vx(),
                          movement->get_vz(),
                          circle.priority,
                          movement->get_has_target(),
                          movement->has_waypoints()};
    }

    circle.priority = compute_avoidance_priority(*entity);

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
          auto& terrain = Game::Map::TerrainService::instance();
          Point const cell = NavGrid::world_to_grid(ci.x, ci.z);
          bool portal_constrains_lateral = ci.follows_navigation_path ||
                                           terrain.is_on_bridge(ci.x, ci.z) ||
                                           terrain.is_hill_entrance(cell.x, cell.y);
          if (!portal_constrains_lateral) {
            portal_constrains_lateral = point_is_in_navigation_passage(ci.x, ci.z);
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

      auto* entity = world->get_entity(ci.id);
      auto* movement = entity != nullptr
                           ? entity->get_component<Engine::Core::MovementComponent>()
                           : nullptr;
      if (movement != nullptr) {

        movement->set_manual_velocity(ci.vx + sep_x, ci.vz + sep_z);
      }
      ++overlaps_detected;
    }
  }

  m_diagnostics.overlaps_detected = overlaps_detected;
  if (!m_circles.empty()) {
    m_diagnostics.average_neighbors_checked =
        total_neighbors_checked / static_cast<std::uint32_t>(m_circles.size());
  }
}

} // namespace Game::Systems
