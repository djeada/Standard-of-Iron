#pragma once

#include <QVector3D>

#include <cstdint>
#include <vector>

#include "../core/component_gameplay.h"
#include "nav_grid_types.h"

namespace Engine::Core {
class Entity;
class World;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Game::Systems {

class GateService {
public:
  using ManualMode = Engine::Core::GateComponent::ManualMode;

  struct GateExtent {
    float half_x{0.0F};
    float half_z{0.0F};
  };

  [[nodiscard]] static auto structure_extent(float rotation_y) -> GateExtent;

  [[nodiscard]] static auto passage_extent(float rotation_y) -> GateExtent;

  [[nodiscard]] static auto lane_half_width() -> float;

  [[nodiscard]] static auto lane_center(float center_x, float center_z) -> QVector3D;

  [[nodiscard]] static auto lane_extent(float rotation_y) -> GateExtent;

  [[nodiscard]] static auto
  passage_blocker_bounds(float center_x, float center_z, float rotation_y) -> WorldRect;

  static void sync_gate_footprint(Engine::Core::World& world,
                                  Engine::Core::EntityID entity_id,
                                  float rotation_y);

  [[nodiscard]] static auto is_gate(const Engine::Core::Entity& entity) -> bool;

  [[nodiscard]] static auto serves_owner(const Engine::Core::World& world,
                                         int gate_owner_id,
                                         int unit_owner_id) -> bool;

  [[nodiscard]] static auto
  gate_at(Engine::Core::World& world,
          Engine::Core::EntityID entity_id) -> Engine::Core::Entity*;

  static void refresh_blockers(Engine::Core::World& world);
  static void clear_blockers();
  [[nodiscard]] static auto blockers() -> const std::vector<GateBlocker>&;

  [[nodiscard]] static auto blocks_move(const QVector3D& current,
                                        const QVector3D& target) -> bool;

  [[nodiscard]] static auto blocks_line(const QVector3D& from,
                                        const QVector3D& to) -> bool;

  static auto set_manual_mode(Engine::Core::Entity& gate, ManualMode mode) -> bool;
  static auto cycle_manual_mode(Engine::Core::Entity& gate) -> ManualMode;
};

} // namespace Game::Systems
