#pragma once

#include "../core/component.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../units/spawn_type.h"
#include "formation_system.h"
#include "nation_registry.h"
#include <QVector3D>
#include <cmath>
#include <vector>

namespace Game::Systems {

class FormationPlanner {
public:
  static auto spreadFormation(int n, const QVector3D &center,
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

  static auto
  spread_formation_by_nation(Engine::Core::World &world,
                             const std::vector<Engine::Core::EntityID> &units,
                             const QVector3D &center,
                             float spacing = 1.0F) -> std::vector<QVector3D> {
    if (units.empty()) {
      return {};
    }

    bool all_in_formation_mode = true;
    FormationType formation_type = FormationType::Roman;
    bool formation_type_determined = false;

    // First pass: check if all units are in formation mode and determine nation
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
              NationRegistry::instance().getNation(unit_comp->nation_id);
          if (nation != nullptr) {
            formation_type = nation->formation_type;
            formation_type_determined = true;
          }
        }
      }
    }

    if (!all_in_formation_mode || !formation_type_determined) {
      return spreadFormation(int(units.size()), center, spacing);
    }

    // Gather unit information for advanced formation
    std::vector<UnitFormationInfo> unit_infos;
    unit_infos.reserve(units.size());

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
      return spreadFormation(int(units.size()), center, spacing);
    }

    // Get formation positions with facing information
    auto formation_positions =
        FormationSystem::instance().get_formation_positions_with_facing(
            formation_type, unit_infos, center, spacing);

    // Convert to simple positions (facing will be handled later via transform)
    std::vector<QVector3D> positions;
    positions.reserve(formation_positions.size());
    for (const auto &fpos : formation_positions) {
      positions.push_back(fpos.position);
    }

    return positions;
  }
};

} // namespace Game::Systems
