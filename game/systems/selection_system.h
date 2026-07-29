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

  void select_unit(Engine::Core::EntityID unit_id);
  void deselect_unit(Engine::Core::EntityID unit_id);
  void clear_selection();
  void select_units_in_area(float x1, float y1, float x2, float y2);

  [[nodiscard]] auto
  get_selected_units() const -> const std::vector<Engine::Core::EntityID>& {
    return m_selected_units;
  }

private:
  std::vector<Engine::Core::EntityID> m_selected_units;
  static auto is_unit_in_area(
      Engine::Core::Entity* entity, float x1, float y1, float x2, float y2) -> bool;
};

} // namespace Game::Systems
