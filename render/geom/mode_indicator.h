#pragma once

#include "../gl/mesh.h"
#include <QVector3D>
#include <memory>

namespace Render::Geom {

// Mode type constants for rendering
constexpr int k_mode_type_hold = 0;
constexpr int k_mode_type_guard = 1;

// Visual constants for mode indicators
constexpr float k_indicator_height_base = 2.0F;    // Height above unit
constexpr float k_indicator_size = 0.4F;           // Size of indicator mesh
constexpr float k_indicator_alpha = 0.85F;         // Transparency level
constexpr float k_indicator_height_multiplier = 2.0F; // Scale multiplier
constexpr float k_frustum_cull_margin = 1.5F;      // Culling margin beyond NDC

// Mode indicator colors
const QVector3D k_hold_mode_color(1.0F, 0.3F, 0.3F);  // Red for hold mode
const QVector3D k_guard_mode_color(0.3F, 0.5F, 1.0F); // Blue for guard mode

class ModeIndicator {
public:
  static auto get_hold_mode_mesh() -> Render::GL::Mesh *;
  static auto get_guard_mode_mesh() -> Render::GL::Mesh *;

private:
  static auto create_hold_mode_mesh() -> std::unique_ptr<Render::GL::Mesh>;
  static auto create_guard_mode_mesh() -> std::unique_ptr<Render::GL::Mesh>;

  static std::unique_ptr<Render::GL::Mesh> s_hold_mesh;
  static std::unique_ptr<Render::GL::Mesh> s_guard_mesh;
};

} // namespace Render::Geom
