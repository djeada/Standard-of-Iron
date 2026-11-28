#pragma once

#include <cstdint>
#include <optional>

#include "app/core/presentation_frame.h"
#include "render/entity/rpg_telegraph_renderer.h"

namespace Engine::Core {
class World;
}

namespace Render::GL {
class FrameSubmitter;
}

namespace App::Core {

class CommanderOverlayPass {
public:
  void render(const std::optional<CommanderOverlayFrame>& overlay,
              Engine::Core::World& snapshot,
              Render::GL::FrameSubmitter& renderer,
              float anim_time);

private:
  Render::GL::RpgTelegraphRenderer m_telegraphs;
  std::uint64_t m_drawn_session{0};
};

} // namespace App::Core
