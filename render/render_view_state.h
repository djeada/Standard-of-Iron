#pragma once

#include <cstdint>

#include "game/core/entity.h"
#include "render/world_render_mode.h"

namespace Render::GL {

// Who is watching, and how. None of this is simulation state: it is the set of
// presentation choices a frame is drawn under, and the rules that follow from
// them. It lives apart from Renderer so those rules can be read and tested
// without standing up an OpenGL context.
class RenderViewState {
public:
  void set_hovered_entity_id(Engine::Core::EntityID id) noexcept {
    m_hovered_entity_id = id;
  }
  [[nodiscard]] auto hovered_entity_id() const noexcept -> Engine::Core::EntityID {
    return m_hovered_entity_id;
  }

  // The player whose orders and selection this view belongs to.
  void set_local_owner_id(int owner_id) noexcept { m_local_owner_id = owner_id; }
  [[nodiscard]] auto local_owner_id() const noexcept -> int { return m_local_owner_id; }

  // A spectator has no side, so no side's orders are theirs to see.
  void set_order_marker_spectator_mode(bool enabled) noexcept {
    m_order_marker_spectator_mode = enabled;
  }
  [[nodiscard]] auto order_marker_spectator_mode() const noexcept -> bool {
    return m_order_marker_spectator_mode;
  }

  // Rally and patrol markers are UI, not world geometry: they belong to the
  // local player, and a cinematic frame carries no UI at all.
  [[nodiscard]] auto
  order_markers_visible_for_owner(int owner_id) const noexcept -> bool {
    if (m_cinematic_mode) {
      return false;
    }
    return !m_order_marker_spectator_mode && owner_id == m_local_owner_id;
  }

  // Captures and promo shots want the scene without overlays, and without the
  // distance-based detail drops a player would never notice in motion.
  void set_cinematic_mode(bool enabled) noexcept { m_cinematic_mode = enabled; }
  [[nodiscard]] auto cinematic_mode() const noexcept -> bool {
    return m_cinematic_mode;
  }

  void set_force_full_creature_lod(bool enabled) noexcept {
    m_force_full_creature_lod = enabled;
  }
  [[nodiscard]] auto force_full_creature_lod() const noexcept -> bool {
    return m_force_full_creature_lod;
  }

  void set_world_render_mode(WorldRenderMode mode) noexcept {
    m_world_render_mode = mode;
  }
  [[nodiscard]] auto world_render_mode() const noexcept -> WorldRenderMode {
    return m_world_render_mode;
  }

  // The entity the RPG chase camera sits behind, when in RPG mode.
  void set_rpg_camera_focus(Engine::Core::EntityID entity_id) noexcept {
    m_rpg_camera_focus_id = entity_id;
  }
  [[nodiscard]] auto rpg_camera_focus() const noexcept -> Engine::Core::EntityID {
    return m_rpg_camera_focus_id;
  }

private:
  Engine::Core::EntityID m_hovered_entity_id = 0;
  Engine::Core::EntityID m_rpg_camera_focus_id = 0;
  WorldRenderMode m_world_render_mode = WorldRenderMode::Rts;
  int m_local_owner_id = 1;
  bool m_order_marker_spectator_mode = false;
  bool m_force_full_creature_lod = false;
  bool m_cinematic_mode = false;
};

} // namespace Render::GL
