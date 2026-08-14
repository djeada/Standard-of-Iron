#pragma once

#include <QVector3D>

#include <cstdint>
#include <vector>

#include "../core/component.h"
#include "nav_grid_types.h"

namespace Engine::Core {
class Entity;
class World;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Game::Systems {

struct GateBlocker {
  float min_x{0.0F};
  float max_x{0.0F};
  float min_z{0.0F};
  float max_z{0.0F};
  int owner_id{0};
  Engine::Core::EntityID entity_id{0};

  [[nodiscard]] auto contains(float world_x, float world_z) const -> bool {
    return world_x >= min_x && world_x <= max_x && world_z >= min_z && world_z <= max_z;
  }
};

class GateService {
public:
  using ManualMode = Engine::Core::GateComponent::ManualMode;

  struct GateExtent {
    float half_x{0.0F};
    float half_z{0.0F};
  };

  [[nodiscard]] static auto structure_extent(float rotation_y) -> GateExtent;

  [[nodiscard]] static auto passage_extent(float rotation_y) -> GateExtent;

  [[nodiscard]] static auto
  passage_blocker_bounds(float center_x, float center_z, float rotation_y) -> WorldRect;

  static void mark_gate_footprint_navigable(Engine::Core::EntityID entity_id);

  static void sync_gate_footprint(Engine::Core::EntityID entity_id, float rotation_y);

  [[nodiscard]] static auto is_gate(const Engine::Core::Entity& entity) -> bool;

  [[nodiscard]] static auto serves_owner(int gate_owner_id, int unit_owner_id) -> bool;

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
