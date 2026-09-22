#pragma once

#include <vector>

#include "../core/entity_id.h"

namespace Game::Session {

// Which units this client has selected, and which one it is inspecting.
//
// This is client state, not match state: two players in one match select
// different units and a spectator selects none, so it is deliberately absent
// from the world and from the replay digest. It lived in the world as a
// Systems::SelectionSystem with an empty update() and an access() declaring
// neither reads nor writes -- a system in name only.
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

private:
  std::vector<Engine::Core::EntityID> m_selected_units;
  Engine::Core::EntityID m_inspected_entity = Engine::Core::NULL_ENTITY;
};

} // namespace Game::Session
