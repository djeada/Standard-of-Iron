
#pragma once
#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>

#include "render/gl/mesh.h"

namespace Render {
namespace Geom {
class Arrow {
public:
  static auto get_shaft() -> GL::Mesh*;
  static auto get_tip() -> GL::Mesh*;
  static auto get_fletching() -> GL::Mesh*;

  static constexpr float k_shaft_length = 0.64F;
  static constexpr float k_tip_length = 0.16F;
  static constexpr float k_total_length = k_shaft_length + k_tip_length;
  static constexpr float k_shaft_radius = 0.0175F;
  static constexpr float k_ferrule_radius = 0.0225F;
  static constexpr float k_head_half_width = 0.0475F;
  static constexpr float k_head_half_thickness = 0.0105F;
  static constexpr float k_fletch_start_z = 0.046F;
  static constexpr float k_fletch_end_z = 0.262F;
  static constexpr float k_fletch_peak_radius = 0.068F;
  static constexpr float k_fletch_helical_deg = 9.0F;
  static constexpr int k_fletch_vanes = 3;
  static constexpr float k_arrow_z_scale = 0.42F;
  static constexpr float k_arrow_xy_scale = 0.40F;
  static constexpr float k_arrow_z_translate_factor = k_total_length * 0.5F;
  static constexpr float k_fletch_z_offset_factor = 0.0F;
  static constexpr float k_fletch_xy_scale = 1.0F;
  static constexpr float k_fletch_z_scale = 1.0F;

  static constexpr float k_head_center_z = (k_shaft_length + k_total_length) * 0.5F;
  static constexpr float k_shaft_glow_xy_scale = 1.75F;
  static constexpr float k_shaft_glow_alpha = 0.13F;
  static constexpr float k_head_glow_xy_scale = 1.90F;
  static constexpr float k_head_glow_z_scale = 1.32F;
  static constexpr float k_head_glow_alpha = 0.30F;

  static auto shaft_color(const QVector3D& team_color) -> QVector3D {
    constexpr float k_wood_r = 0.640F;
    constexpr float k_wood_g = 0.502F;
    constexpr float k_wood_b = 0.318F;
    constexpr float k_team_tint = 0.12F;
    return {std::clamp(k_wood_r + team_color.x() * k_team_tint, 0.0F, 1.0F),
            std::clamp(k_wood_g + team_color.y() * k_team_tint, 0.0F, 1.0F),
            std::clamp(k_wood_b + team_color.z() * k_team_tint, 0.0F, 1.0F)};
  }

  static auto fletch_color(const QVector3D& team_color) -> QVector3D {
    return {std::clamp(team_color.x() * 0.82F + 0.13F, 0.0F, 1.0F),
            std::clamp(team_color.y() * 0.82F + 0.13F, 0.0F, 1.0F),
            std::clamp(team_color.z() * 0.82F + 0.12F, 0.0F, 1.0F)};
  }

  static auto glow_color(const QVector3D& team_color) -> QVector3D {
    QVector3D const fletch = fletch_color(team_color);
    return {std::clamp(fletch.x() * 0.22F + 0.70F, 0.0F, 1.0F),
            std::clamp(fletch.y() * 0.22F + 0.66F, 0.0F, 1.0F),
            std::clamp(fletch.z() * 0.22F + 0.56F, 0.0F, 1.0F)};
  }

  static auto tip_color(float brightness = 1.0F) -> QVector3D {
    return {std::clamp(0.700F * brightness + 0.06F, 0.0F, 1.0F),
            std::clamp(0.722F * brightness + 0.06F, 0.0F, 1.0F),
            std::clamp(0.770F * brightness + 0.05F, 0.0F, 1.0F)};
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
void render_arrows(Renderer* renderer,
                   ResourceManager* resources,
                   const Game::Systems::ArrowSystem& arrow_system);
}
