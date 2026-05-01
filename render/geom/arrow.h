
#pragma once
#include "../gl/mesh.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>

namespace Render {
namespace Geom {
class Arrow {
public:
  static auto get_shaft() -> GL::Mesh *;
  static auto get_tip() -> GL::Mesh *;

  static constexpr float k_arrow_z_scale = 0.55F;
  static constexpr float k_arrow_xy_scale = 0.36F;
  static constexpr float k_arrow_z_translate_factor = 0.5F;
  static constexpr float k_fletch_z_offset_factor = 0.2F;
  static constexpr float k_fletch_xy_scale = 0.75F;
  static constexpr float k_fletch_z_scale = 0.15F;

  static auto shaft_color(const QVector3D &team_color) -> QVector3D {
    return {std::clamp(team_color.x() * 0.6F + 0.35F, 0.0F, 1.0F),
            std::clamp(team_color.y() * 0.55F + 0.30F, 0.0F, 1.0F),
            std::clamp(team_color.z() * 0.5F + 0.15F, 0.0F, 1.0F)};
  }

  static auto fletch_color(const QVector3D &team_color) -> QVector3D {
    return {std::clamp(team_color.x() * 0.9F + 0.1F, 0.0F, 1.0F),
            std::clamp(team_color.y() * 0.9F + 0.1F, 0.0F, 1.0F),
            std::clamp(team_color.z() * 0.9F + 0.1F, 0.0F, 1.0F)};
  }
};
} // namespace Geom

namespace GL {
class Renderer;
class ResourceManager;
} // namespace GL
} // namespace Render

namespace Game::Systems {
class ArrowSystem;
}

namespace Render::GL {
void render_arrows(Renderer *renderer, ResourceManager *resources,
                   const Game::Systems::ArrowSystem &arrow_system);
}
