#include "local_avoidance_system.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

#include "../core/component.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "command_service.h"

namespace Game::Systems {

namespace {

struct UnitCircle {
  Engine::Core::EntityID id{0};
  float x{0.0F};
  float z{0.0F};
  float radius{0.5F};
  float vx{0.0F};
  float vz{0.0F};
  std::uint8_t priority{0};
  bool is_moving{false};
};

struct CellKey {
  int cx;
  int cz;

  auto operator==(const CellKey& other) const -> bool {
    return cx == other.cx && cz == other.cz;
  }
};

struct CellKeyHash {
  auto operator()(const CellKey& key) const -> std::size_t {
    return std::hash<int>()(key.cx) ^ (std::hash<int>()(key.cz) << 16);
  }
};

auto to_cell(float x, float z, float inv_cell_size) -> CellKey {
  return {static_cast<int>(std::floor(x * inv_cell_size)),
          static_cast<int>(std::floor(z * inv_cell_size))};
}

auto compute_avoidance_priority(const Engine::Core::Entity& entity) -> std::uint8_t {
  if (entity.has_component<Engine::Core::BuildingComponent>()) {
    return 4;
  }
  auto* atk = entity.get_component<Engine::Core::AttackComponent>();
  if (atk != nullptr && atk->in_melee_lock) {
    return 3;
  }
  auto* intent = entity.get_component<Engine::Core::MovementIntentComponent>();
  if (intent != nullptr) {
    return intent->priority;
  }
  auto* movement = entity.get_component<Engine::Core::MovementComponent>();
  if (movement != nullptr && movement->has_target) {
    return 1;
  }
  return 2; // stationary default
}

} // namespace

void LocalAvoidanceSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr || delta_time <= 0.0F) {
    return;
  }

  m_diagnostics = {};

  auto entities = world->get_entities_with<Engine::Core::UnitComponent>();
  if (entities.empty()) {
    return;
  }

  // Build transient spatial hash of unit circles.
  float const inv_cell_size = 1.0F / k_default_cell_size;
  std::unordered_map<CellKey, std::vector<std::size_t>, CellKeyHash> grid;
  std::vector<UnitCircle> circles;
  circles.reserve(entities.size());

  for (auto* entity : entities) {
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
      circle.vx = movement->vx;
      circle.vz = movement->vz;
      circle.is_moving = movement->has_target;
    }

    circle.priority = compute_avoidance_priority(*entity);

    std::size_t idx = circles.size();
    circles.push_back(circle);

    CellKey key = to_cell(circle.x, circle.z, inv_cell_size);
    grid[key].push_back(idx);
  }

  m_diagnostics.units_processed = static_cast<std::uint32_t>(circles.size());

  // Apply soft separation.
  std::uint32_t total_neighbors_checked = 0;
  std::uint32_t corrections = 0;

  for (std::size_t i = 0; i < circles.size(); ++i) {
    auto& ci = circles[i];
    if (!ci.is_moving) {
      continue; // Only moving units get displaced
    }

    CellKey center = to_cell(ci.x, ci.z, inv_cell_size);
    float sep_x = 0.0F;
    float sep_z = 0.0F;
    int neighbor_count = 0;

    for (int dx = -1; dx <= 1; ++dx) {
      for (int dz = -1; dz <= 1; ++dz) {
        CellKey neighbor_key{center.cx + dx, center.cz + dz};
        auto it = grid.find(neighbor_key);
        if (it == grid.end()) {
          continue;
        }
        for (std::size_t j : it->second) {
          if (j == i) {
            continue;
          }
          auto& cj = circles[j];
          float const ddx = ci.x - cj.x;
          float const ddz = ci.z - cj.z;
          float const dist_sq = ddx * ddx + ddz * ddz;
          float const min_dist = ci.radius + cj.radius + k_separation_radius;
          float const min_dist_sq = min_dist * min_dist;

          ++total_neighbors_checked;

          if (dist_sq < min_dist_sq && dist_sq > 1e-6F) {
            float const dist = std::sqrt(dist_sq);
            float const overlap = min_dist - dist;
            float const nx = ddx / dist;
            float const nz = ddz / dist;

            // Higher priority units push more; lower priority yields more.
            float weight = 1.0F;
            if (cj.priority > ci.priority) {
              weight = 1.5F; // yield to higher priority
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

      // Clamp correction magnitude.
      float const mag_sq = sep_x * sep_x + sep_z * sep_z;
      float const max_corr = k_max_correction_per_tick;
      if (mag_sq > max_corr * max_corr) {
        float const mag = std::sqrt(mag_sq);
        sep_x = sep_x / mag * max_corr;
        sep_z = sep_z / mag * max_corr;
      }

      // Apply correction to entity.
      auto* entity = world->get_entity(ci.id);
      if (entity != nullptr) {
        auto* transform = entity->get_component<Engine::Core::TransformComponent>();
        if (transform != nullptr) {
          float const new_x = transform->position.x + sep_x * delta_time;
          float const new_z = transform->position.z + sep_z * delta_time;

          // Validate against terrain before committing.
          QVector3D const candidate(new_x, 0.0F, new_z);
          if (CommandService::is_world_position_walkable_for_radius(candidate,
                                                                    ci.radius)) {
            transform->position.x = new_x;
            transform->position.z = new_z;
            ++corrections;
          }
        }
      }
    }
  }

  m_diagnostics.corrections_applied = corrections;
  if (!circles.empty()) {
    m_diagnostics.average_neighbors_checked =
        total_neighbors_checked / static_cast<std::uint32_t>(circles.size());
  }
}

} // namespace Game::Systems
