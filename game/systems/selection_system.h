#pragma once

#include <functional>
#include <vector>

#include "../core/entity.h"
#include "../core/system.h"

namespace Engine::Core {
class Entity;
class World;
} // namespace Engine::Core

namespace Game::Systems {

class PickingService;

class SelectionSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;

  [[nodiscard]] auto access() const -> Engine::Core::SystemAccess override;

  void select_unit(Engine::Core::EntityID unit_id);
  void deselect_unit(Engine::Core::EntityID unit_id);
  void clear_selection();
  void select_units_in_area(float x1, float y1, float x2, float y2);

  [[nodiscard]] auto
  get_selected_units() const -> const std::vector<Engine::Core::EntityID>& {
    return m_selected_units;
  }

  void set_inspected_entity(Engine::Core::EntityID entity_id) {
    m_inspected_entity = entity_id;
  }
  void clear_inspected_entity() { m_inspected_entity = Engine::Core::NULL_ENTITY; }
  [[nodiscard]] auto inspected_entity() const -> Engine::Core::EntityID {
    return m_inspected_entity;
  }

private:
  std::vector<Engine::Core::EntityID> m_selected_units;
  Engine::Core::EntityID m_inspected_entity = Engine::Core::NULL_ENTITY;
  static auto is_unit_in_area(
      Engine::Core::Entity* entity, float x1, float y1, float x2, float y2) -> bool;
};

} // namespace Game::Systems
