#pragma once

#include <array>
#include <vector>

#include "../core/entity_id.h"

namespace Game::Session {

class SelectionService {
public:
  void select_unit(Engine::Core::EntityID unit_id);
  void deselect_unit(Engine::Core::EntityID unit_id);
  void clear_selection();

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

  static constexpr int k_control_group_count = 10;

  [[nodiscard]] static auto is_control_group(int group) -> bool {
    return group >= 0 && group < k_control_group_count;
  }
  void set_control_group(int group, std::vector<Engine::Core::EntityID> members);
  [[nodiscard]] auto
  control_group(int group) const -> const std::vector<Engine::Core::EntityID>&;
  void clear_control_groups();

private:
  std::vector<Engine::Core::EntityID> m_selected_units;
  Engine::Core::EntityID m_inspected_entity = Engine::Core::NULL_ENTITY;
  std::array<std::vector<Engine::Core::EntityID>, k_control_group_count>
      m_control_groups{};
};

} // namespace Game::Session
