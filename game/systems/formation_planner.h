#pragma once

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <unordered_map>
#include <vector>

#include "../core/component.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../units/spawn_type.h"
#include "command_service.h"
#include "formation_system.h"
#include "nation_registry.h"
#include "pathfinding.h"

namespace Game::Systems {

struct FormationResult {
  std::vector<QVector3D> positions;
  std::vector<float> facing_angles;
  float formation_facing = 0.0F;
  bool used_tactical_formation = false;
};

class FormationPlanner {
public:
  static auto spread_formation(int n,
                               const QVector3D& center,
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
  struct PlannedSlot {
    size_t original_idx{0};
    Engine::Core::EntityID entity_id{0};
    QVector3D local_offset;
    float facing_angle{0.0F};
    float unit_radius{0.0F};
  };

  struct OccupiedSlot {
    QVector3D position;
    float unit_radius{0.0F};
  };

  static auto is_position_walkable_for_radius(const QVector3D& position,
                                              float unit_radius) -> bool {
    return CommandService::is_world_position_walkable_for_radius(position, unit_radius);
  }

  static auto respects_spacing(const QVector3D& candidate,
                               float unit_radius,
                               const std::vector<OccupiedSlot>& occupied) -> bool {
    for (const auto& slot : occupied) {
      float const dx = candidate.x() - slot.position.x();
      float const dz = candidate.z() - slot.position.z();
      float const min_distance = unit_radius + slot.unit_radius;
      if (dx * dx + dz * dz < min_distance * min_distance) {
        return false;
      }
    }
    return true;
  }

  static auto find_nearest_walkable(const QVector3D& position,
                                    int max_search_radius = 5) -> QVector3D {
    return find_nearest_walkable_for_radius(position, 0.0F, max_search_radius);
  }

  static auto find_nearest_walkable_for_radius(const QVector3D& position,
                                               float unit_radius,
                                               int max_search_radius = 5) -> QVector3D {
    return CommandService::snap_to_walkable_ground_for_radius(
        position, unit_radius, max_search_radius);
  }

  static auto
  find_nearest_walkable_non_overlapping(const QVector3D& position,
                                        float unit_radius,
                                        const std::vector<OccupiedSlot>& occupied,
                                        int max_search_radius = 5) -> QVector3D {
    if (is_position_walkable_for_radius(position, unit_radius) &&
        respects_spacing(position, unit_radius, occupied)) {
      return position;
    }

    QVector3D const walkable_fallback =
        find_nearest_walkable_for_radius(position, unit_radius, max_search_radius);

    Point const center_grid = CommandService::world_to_grid(position.x(), position.z());

    for (int radius = 1; radius <= max_search_radius; ++radius) {
      for (int dx = -radius; dx <= radius; ++dx) {
        for (int dz = -radius; dz <= radius; ++dz) {
          if (std::abs(dx) != radius && std::abs(dz) != radius) {
            continue;
          }

          Point const candidate_grid{center_grid.x + dx, center_grid.y + dz};
          if (!CommandService::is_grid_walkable_for_radius(candidate_grid,
                                                           unit_radius)) {
            continue;
          }

          QVector3D candidate = CommandService::grid_to_world(candidate_grid);
          candidate.setY(position.y());
          if (respects_spacing(candidate, unit_radius, occupied)) {
            return candidate;
          }
        }
      }
    }

    return walkable_fallback;
  }

  static auto is_area_mostly_walkable(const QVector3D& center, float radius) -> bool {
    Point const center_grid = CommandService::world_to_grid(center.x(), center.z());

    int const check_radius = static_cast<int>(std::ceil(radius));
    int walkable_count = 0;
    int total_count = 0;

    for (int dx = -check_radius; dx <= check_radius; ++dx) {
      for (int dz = -check_radius; dz <= check_radius; ++dz) {
        Point const candidate{center_grid.x + dx, center_grid.y + dz};

        ++total_count;
        if (CommandService::is_grid_walkable_for_radius(candidate, 0.0F)) {
          ++walkable_count;
        }
      }
    }

    return walkable_count >= (total_count * 2 / 3);
  }

  static auto rotate_local_offset(const QVector3D& local,
                                  float yaw_degrees) -> QVector3D {
    constexpr float k_pi = 3.14159265358979323846F;
    float const yaw_radians = yaw_degrees * (k_pi / 180.0F);
    float const sin_yaw = std::sin(yaw_radians);
    float const cos_yaw = std::cos(yaw_radians);
    return {local.x() * cos_yaw + local.z() * sin_yaw,
            local.y(),
            -local.x() * sin_yaw + local.z() * cos_yaw};
  }

  static auto
  are_slots_walkable_at_center(const QVector3D& center,
                               const std::vector<PlannedSlot>& planned_slots) -> bool {
    for (const auto& slot : planned_slots) {
      if (!is_position_walkable_for_radius(center + slot.local_offset,
                                           slot.unit_radius)) {
        return false;
      }
    }
    return true;
  }

  static auto
  find_coherent_walkable_center(const QVector3D& preferred_center,
                                const std::vector<PlannedSlot>& planned_slots,
                                int max_search_radius = 15) -> QVector3D {
    if (planned_slots.empty()) {
      return preferred_center;
    }

    if (are_slots_walkable_at_center(preferred_center, planned_slots)) {
      return preferred_center;
    }

    Point const center_grid =
        CommandService::world_to_grid(preferred_center.x(), preferred_center.z());

    QVector3D best_center = preferred_center;
    int best_valid_count = -1;
    float best_distance_sq = 0.0F;
    bool found_partial_candidate = false;

    auto consider_candidate = [&](int grid_x, int grid_z) -> bool {
      QVector3D candidate_center = CommandService::grid_to_world({grid_x, grid_z});
      candidate_center.setY(preferred_center.y());
      int valid_count = 0;
      for (const auto& slot : planned_slots) {
        if (is_position_walkable_for_radius(candidate_center + slot.local_offset,
                                            slot.unit_radius)) {
          ++valid_count;
        }
      }

      float const distance_sq = (candidate_center - preferred_center).lengthSquared();
      if (valid_count == static_cast<int>(planned_slots.size())) {
        best_center = candidate_center;
        best_valid_count = valid_count;
        best_distance_sq = distance_sq;
        return true;
      }

      if (!found_partial_candidate || valid_count > best_valid_count ||
          (valid_count == best_valid_count && distance_sq < best_distance_sq)) {
        best_center = candidate_center;
        best_valid_count = valid_count;
        best_distance_sq = distance_sq;
        found_partial_candidate = true;
      }
      return false;
    };

    consider_candidate(center_grid.x, center_grid.y);

    for (int radius = 1; radius <= max_search_radius; ++radius) {
      for (int dx = -radius; dx <= radius; ++dx) {
        for (int dz = -radius; dz <= radius; ++dz) {
          if (std::abs(dx) != radius && std::abs(dz) != radius) {
            continue;
          }

          if (consider_candidate(center_grid.x + dx, center_grid.y + dz)) {
            return best_center;
          }
        }
      }
    }

    return best_center;
  }

public:
  static auto
  spread_formation_by_nation(Engine::Core::World& world,
                             const std::vector<Engine::Core::EntityID>& units,
                             const QVector3D& center,
                             float spacing = 1.0F) -> std::vector<QVector3D> {
    auto result = get_formation_with_facing(world, units, center, spacing);
    return result.positions;
  }

  static auto
  get_formation_with_facing(Engine::Core::World& world,
                            const std::vector<Engine::Core::EntityID>& units,
                            const QVector3D& center,
                            float spacing = 1.0F) -> FormationResult {
    FormationResult result;
    if (units.empty()) {
      return result;
    }

    result.positions = spread_formation(int(units.size()), center, spacing);
    result.facing_angles.resize(units.size(), 0.0F);

    QVector3D adjusted_center = center;
    float const estimated_formation_radius =
        std::sqrt(static_cast<float>(units.size())) * spacing * 2.0F;

    if (!is_area_mostly_walkable(center, estimated_formation_radius)) {
      adjusted_center = find_nearest_walkable(center, 15);
    }

    struct FormationGroup {
      FormationType formation_type{FormationType::Roman};
      std::vector<UnitFormationInfo> units;
      std::vector<FormationPosition> planned_positions;
      float average_current_x{0.0F};
      float min_x{0.0F};
      float max_x{0.0F};
    };

    std::vector<FormationGroup> groups;
    bool has_active_formation_unit = false;
    std::unordered_map<Engine::Core::EntityID, size_t> unit_to_original_idx;
    unit_to_original_idx.reserve(units.size());
    QVector3D active_position_sum(0.0F, 0.0F, 0.0F);
    int active_position_count = 0;

    for (size_t i = 0; i < units.size(); ++i) {
      auto unit_id = units[i];
      unit_to_original_idx[unit_id] = i;

      auto* entity = world.get_entity(unit_id);
      if (entity == nullptr) {
        continue;
      }

      auto* formation_mode =
          entity->get_component<Engine::Core::FormationModeComponent>();
      if ((formation_mode == nullptr) || !formation_mode->active) {
        continue;
      }

      auto* unit_comp = entity->get_component<Engine::Core::UnitComponent>();
      auto* transform = entity->get_component<Engine::Core::TransformComponent>();
      if (unit_comp == nullptr || transform == nullptr) {
        continue;
      }

      auto troop_type_opt = Game::Units::spawn_typeToTroopType(unit_comp->spawn_type);
      if (!troop_type_opt.has_value()) {
        continue;
      }

      const QVector3D current_position(
          transform->position.x, transform->position.y, transform->position.z);
      has_active_formation_unit = true;
      active_position_sum += current_position;
      ++active_position_count;

      FormationType formation_type = FormationType::Roman;
      auto* nation = NationRegistry::instance().get_nation(unit_comp->nation_id);
      if (nation != nullptr) {
        formation_type = nation->formation_type;
      }

      auto group_it = std::find_if(
          groups.begin(), groups.end(), [formation_type](const auto& group) {
            return group.formation_type == formation_type;
          });
      if (group_it == groups.end()) {
        groups.push_back({formation_type, {}});
        group_it = std::prev(groups.end());
      }

      UnitFormationInfo info;
      info.entity_id = unit_id;
      info.troop_type = troop_type_opt.value();
      info.current_position = current_position;
      group_it->units.push_back(info);
    }

    if (!has_active_formation_unit || groups.empty()) {
      result.positions = spread_formation(int(units.size()), adjusted_center, spacing);
      return result;
    }

    result.positions = spread_formation(int(units.size()), adjusted_center, spacing);

    QVector3D formation_centroid =
        active_position_count > 0
            ? active_position_sum / static_cast<float>(active_position_count)
            : adjusted_center;
    QVector3D formation_forward = adjusted_center - formation_centroid;
    formation_forward.setY(0.0F);
    if (formation_forward.lengthSquared() <= 1.0e-4F) {
      formation_forward = QVector3D(0.0F, 0.0F, 1.0F);
    } else {
      formation_forward.normalize();
    }
    QVector3D formation_right(formation_forward.z(), 0.0F, -formation_forward.x());
    if (formation_right.lengthSquared() <= 1.0e-4F) {
      formation_right = QVector3D(1.0F, 0.0F, 0.0F);
    } else {
      formation_right.normalize();
    }
    result.formation_facing = std::atan2(formation_forward.x(), formation_forward.z()) *
                              180.0F / 3.14159265358979323846F;

    for (auto& group : groups) {
      std::sort(group.units.begin(),
                group.units.end(),
                [](const UnitFormationInfo& a, const UnitFormationInfo& b) {
                  if (a.troop_type != b.troop_type) {
                    return static_cast<int>(a.troop_type) <
                           static_cast<int>(b.troop_type);
                  }
                  return a.entity_id < b.entity_id;
                });

      float current_x_sum = 0.0F;
      for (const auto& unit : group.units) {
        current_x_sum += QVector3D::dotProduct(
            unit.current_position - formation_centroid, formation_right);
      }
      group.average_current_x =
          group.units.empty() ? 0.0F
                              : current_x_sum / static_cast<float>(group.units.size());

      group.planned_positions =
          FormationSystem::instance().get_formation_positions_with_facing(
              group.formation_type,
              group.units,
              QVector3D(0.0F, adjusted_center.y(), 0.0F),
              spacing);
      if (!group.planned_positions.empty()) {
        group.min_x = group.planned_positions.front().position.x();
        group.max_x = group.min_x;
        for (const auto& fpos : group.planned_positions) {
          group.min_x = std::min(group.min_x, fpos.position.x());
          group.max_x = std::max(group.max_x, fpos.position.x());
        }
      }
    }

    std::stable_sort(groups.begin(),
                     groups.end(),
                     [](const FormationGroup& a, const FormationGroup& b) {
                       if (a.average_current_x != b.average_current_x) {
                         return a.average_current_x < b.average_current_x;
                       }
                       return static_cast<int>(a.formation_type) <
                              static_cast<int>(b.formation_type);
                     });

    float const group_gap = spacing * 5.0F;
    float total_width = 0.0F;
    for (const auto& group : groups) {
      total_width += group.max_x - group.min_x;
    }
    if (groups.size() > 1) {
      total_width += group_gap * static_cast<float>(groups.size() - 1);
    }

    std::vector<PlannedSlot> planned_slots;
    planned_slots.reserve(units.size());

    float left_edge = adjusted_center.x() - total_width * 0.5F;
    for (size_t group_idx = 0; group_idx < groups.size(); ++group_idx) {
      auto& group = groups[group_idx];
      float const group_width = group.max_x - group.min_x;
      float const x_shift =
          (groups.size() == 1) ? adjusted_center.x() : left_edge - group.min_x;

      for (auto fpos : group.planned_positions) {
        QVector3D const local_position(
            fpos.position.x() + x_shift - adjusted_center.x(), 0.0F, fpos.position.z());
        QVector3D const rotated =
            rotate_local_offset(local_position, result.formation_facing);
        auto it = unit_to_original_idx.find(fpos.entity_id);
        if (it != unit_to_original_idx.end()) {
          size_t const original_idx = it->second;
          planned_slots.push_back(
              {original_idx,
               fpos.entity_id,
               rotated,
               result.formation_facing + fpos.facing_angle,
               CommandService::get_unit_radius(world, fpos.entity_id)});
        }
      }

      left_edge += group_width + group_gap;
    }

    QVector3D const coherent_center =
        find_coherent_walkable_center(adjusted_center, planned_slots, 18);

    std::stable_sort(
        planned_slots.begin(),
        planned_slots.end(),
        [](const PlannedSlot& a, const PlannedSlot& b) {
          if (a.local_offset.z() != b.local_offset.z()) {
            return a.local_offset.z() > b.local_offset.z();
          }
          if (std::abs(a.local_offset.x()) != std::abs(b.local_offset.x())) {
            return std::abs(a.local_offset.x()) < std::abs(b.local_offset.x());
          }
          return a.entity_id < b.entity_id;
        });

    std::vector<OccupiedSlot> occupied_slots;
    occupied_slots.reserve(planned_slots.size());
    for (const auto& slot : planned_slots) {
      QVector3D final_position = find_nearest_walkable_non_overlapping(
          coherent_center + slot.local_offset, slot.unit_radius, occupied_slots, 10);
      result.positions[slot.original_idx] = final_position;
      result.facing_angles[slot.original_idx] = slot.facing_angle;
      occupied_slots.push_back({final_position, slot.unit_radius});
    }

    result.used_tactical_formation = true;

    return result;
  }
};

} // namespace Game::Systems
