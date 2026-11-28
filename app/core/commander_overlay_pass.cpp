#include "app/core/commander_overlay_pass.h"

#include "game/core/world.h"

namespace App::Core {

void CommanderOverlayPass::render(const std::optional<CommanderOverlayFrame>& overlay,
                                  Engine::Core::World& snapshot,
                                  Render::GL::FrameSubmitter& renderer,
                                  float anim_time) {
  if (!overlay.has_value() || overlay->session != m_drawn_session) {
    m_telegraphs.clear();
    m_drawn_session = overlay.has_value() ? overlay->session : 0;
  }
  if (!overlay.has_value() || !snapshot.is_render_snapshot()) {
    return;
  }
  m_telegraphs.render(&renderer,
                      &snapshot,
                      overlay->commander_id,
                      overlay->locked_target_id,
                      anim_time);
}

} // namespace App::Core
