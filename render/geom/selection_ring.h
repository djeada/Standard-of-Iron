#pragma once

#include <array>
#include <memory>

#include "../../game/accessibility/team_identity.h"
#include "../gl/mesh.h"

namespace Render::Geom {

class SelectionRing {
public:
  static auto get() -> Render::GL::Mesh*;

  static auto get(Game::Accessibility::TeamPattern pattern) -> Render::GL::Mesh*;

private:
  static std::array<std::unique_ptr<Render::GL::Mesh>,
                    Game::Accessibility::k_team_pattern_count>
      s_meshes;
};

} // namespace Render::Geom
