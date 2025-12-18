#pragma once

#include "../core/component.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../units/spawn_type.h"
#include "command_service.h"
#include "formation_system.h"
#include "nation_registry.h"
#include "pathfinding.h"
#include <QVector3D>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace Game::Systems {

struct FormationResult {
  std::vector<QVector3D> positions;
  std::vector<float> facing_angles;
  float formation_facing = 0.0F;
};

class FormationPlanner {
public:
  static auto spread_formation(int n, const QVector3D &center,
                               float spacing = 1.0F) -> std::vector<QVector3D> {
    std::vector<QVector3D> out;
    out.reserve(n);
    if (n <= 0) {
      return out;
    }
    int const side = std::ceil(std::sqrt(float(n)));
    for (int i = 0; i < n; ++i) {
      int const gx = i % side;
      int const gy = i / side;
      float const ox = (gx - (side - 1) * 0.5F) * spacing;
      float const oz = (gy - (side - 1) * 0.5F) * spacing;
      out.emplace_back(center.x() + ox, center.y(), center.z() + oz);
    }
    return out;
  }

private:
  static auto find_nearest_walkable(const QVector3D &position,
                                    Pathfinding *pathfinder,
                                    int max_search_radius = 5) -> QVector3D {
    if (pathfinder == nullptr) {
      return position;
    }

    float const offset_x = pathfinder->get_grid_offset_x();
    float const offset_z = pathfinder->get_grid_offset_z();

    int const center_grid_x =
        static_cast<int>(std::round(position.x() - offset_x));
    int const center_grid_z =
        static_cast<int>(std::round(position.z() - offset_z));

    if (pathfinder->is_walkable(center_grid_x, center_grid_z)) {
      return position;
    }

    for (int radius = 1; radius <= max_search_radius; ++radius) {
      for (int dx = -radius; dx <= radius; ++dx) {
        for (int dz = -radius; dz <= radius; ++dz) {
          if (std::abs(dx) != radius && std::abs(dz) != radius) {
            continue;
          }

          int const test_x = center_grid_x + dx;
          int const test_z = center_grid_z + dz;

          if (pathfinder->is_walkable(test_x, test_z)) {
            return QVector3D(static_cast<float>(test_x) + offset_x,
                             position.y(),
                             static_cast<float>(test_z) + offset_z);
          }
        }
      }
    }

    return position;
  }

  static auto is_area_mostly_walkable(const QVector3D &center,
                                      Pathfinding *pathfinder,
                                      float radius) -> bool {
    if (pathfinder == nullptr) {
      return true;
    }

    float const offset_x = pathfinder->get_grid_offset_x();
    float const offset_z = pathfinder->get_grid_offset_z();

    int const center_grid_x =
        static_cast<int>(std::round(center.x() - offset_x));
    int const center_grid_z =
        static_cast<int>(std::round(center.z() - offset_z));

    int const check_radius = static_cast<int>(std::ceil(radius));
    int walkable_count = 0;
    int total_count = 0;

    for (int dx = -check_radius; dx <= check_radius; ++dx) {
      for (int dz = -check_radius; dz <= check_radius; ++dz) {
        int const test_x = center_grid_x + dx;
        int const test_z = center_grid_z + dz;

        ++total_count;
        if (pathfinder->is_walkable(test_x, test_z)) {
          ++walkable_count;
        }
      }
    }

    return walkable_count >= (total_count * 2 / 3);
  }

public:
  static auto
  spread_formation_by_nation(Engine::Core::World &world,
                             const std::vector<Engine::Core::EntityID> &units,
                             const QVector3D &center,
                             float spacing = 1.0F) -> std::vector<QVector3D> {
    auto result = get_formation_with_facing(world, units, center, spacing);
    return result.positions;
  }

  static auto
  get_formation_with_facing(Engine::Core::World &world,
                            const std::vector<Engine::Core::EntityID> &units,
                            const QVector3D &center,
                            float spacing = 1.0F) -> FormationResult {
    FormationResult result;
    if (units.empty()) {
      return result;
    }

    result.positions.resize(units.size(), center);
    result.facing_angles.resize(units.size(), 0.0F);

    auto *pathfinder = CommandService::getPathfinder();

    QVector3D adjusted_center = center;
    if (pathfinder != nullptr) {
      float const estimated_formation_radius =
          std::sqrt(static_cast<float>(units.size())) * spacing * 2.0F;

      if (!is_area_mostly_walkable(center, pathfinder,
                                   estimated_formation_radius)) {
        adjusted_center = find_nearest_walkable(center, pathfinder, 15);
      }
    }

    bool all_in_formation_mode = true;
    FormationType formation_type = FormationType::Roman;
    bool formation_type_determined = false;

    for (auto unit_id : units) {
      auto *entity = world.get_entity(unit_id);
      if (entity == nullptr) {
        all_in_formation_mode = false;
        continue;
      }

      auto *formation_mode =
          entity->get_component<Engine::Core::FormationModeComponent>();
      if ((formation_mode == nullptr) || !formation_mode->active) {
        all_in_formation_mode = false;
        continue;
      }

      if (!formation_type_determined) {
        auto *unit_comp = entity->get_component<Engine::Core::UnitComponent>();
        if (unit_comp != nullptr) {
          auto *nation =
              NationRegistry::instance().get_nation(unit_comp->nation_id);
          if (nation != nullptr) {
            formation_type = nation->formation_type;
            formation_type_determined = true;
          }
        }
      }
    }

    if (!all_in_formation_mode || !formation_type_determined) {
      result.positions =
          spread_formation(int(units.size()), adjusted_center, spacing);
      result.facing_angles.assign(units.size(), 0.0F);
      return result;
    }

    std::vector<UnitFormationInfo> unit_infos;
    unit_infos.reserve(units.size());

    std::unordered_map<Engine::Core::EntityID, size_t> unit_to_original_idx;
    for (size_t i = 0; i < units.size(); ++i) {
      unit_to_original_idx[units[i]] = i;
    }

    for (auto unit_id : units) {
      auto *entity = world.get_entity(unit_id);
      if (entity == nullptr) {
        continue;
      }

      auto *unit_comp = entity->get_component<Engine::Core::UnitComponent>();
      auto *transform =
          entity->get_component<Engine::Core::TransformComponent>();

      if (unit_comp != nullptr && transform != nullptr) {
        auto troop_type_opt =
            Game::Units::spawn_typeToTroopType(unit_comp->spawn_type);
        if (troop_type_opt.has_value()) {
          UnitFormationInfo info;
          info.entity_id = unit_id;
          info.troop_type = troop_type_opt.value();
          info.current_position =
              QVector3D(transform->position.x, transform->position.y,
                        transform->position.z);
          unit_infos.push_back(info);
        }
      }
    }

    if (unit_infos.empty()) {
      result.positions =
          spread_formation(int(units.size()), adjusted_center, spacing);
      result.facing_angles.assign(units.size(), 0.0F);
      return result;
    }

    std::sort(unit_infos.begin(), unit_infos.end(),
              [](const UnitFormationInfo &a, const UnitFormationInfo &b) {
                if (a.troop_type != b.troop_type) {
                  return static_cast<int>(a.troop_type) <
                         static_cast<int>(b.troop_type);
                }
                return a.entity_id < b.entity_id;
              });

    auto formation_positions =
        FormationSystem::instance().get_formation_positions_with_facing(
            formation_type, unit_infos, adjusted_center, spacing);

    for (const auto &fpos : formation_positions) {
      auto it = unit_to_original_idx.find(fpos.entity_id);
      if (it != unit_to_original_idx.end()) {
        size_t const original_idx = it->second;
        result.positions[original_idx] = fpos.position;
        result.facing_angles[original_idx] = fpos.facing_angle;
      }
    }

    result.formation_facing = 0.0F;

    return result;
  }
};

} // namespace Game::Systems
