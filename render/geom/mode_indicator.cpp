#include "mode_indicator.h"
#include "gl/mesh.h"
#include <QVector3D>
#include <cmath>
#include <cstddef>
#include <memory>
#include <vector>

namespace Render::Geom {

std::unique_ptr<Render::GL::Mesh> ModeIndicator::s_hold_mesh;
std::unique_ptr<Render::GL::Mesh> ModeIndicator::s_guard_mesh;

// Create crossed swords mesh for hold/fight mode
auto ModeIndicator::create_hold_mode_mesh()
    -> std::unique_ptr<Render::GL::Mesh> {
  using namespace Render::GL;
  std::vector<Vertex> verts;
  std::vector<unsigned int> idx;

  // Constants for sword geometry
  constexpr float sword_length = 0.8F;
  constexpr float sword_width = 0.12F;
  constexpr float blade_length = 0.6F;
  constexpr float handle_length = 0.2F;
  constexpr float cross_guard_width = 0.2F;
  constexpr float cross_guard_height = 0.05F;

  // We'll create two swords crossed in an X pattern
  // Sword 1: rotated 45 degrees, Sword 2: rotated -45 degrees
  const float angles[] = {0.785398F, -0.785398F}; // 45 and -45 degrees

  for (int sword_idx = 0; sword_idx < 2; ++sword_idx) {
    float const angle = angles[sword_idx];
    float const cos_a = std::cos(angle);
    float const sin_a = std::sin(angle);

    size_t const base = verts.size();

    // Blade vertices (flat quad)
    float const blade_half_width = sword_width * 0.5F;
    QVector3D const n(0, 0, 1); // Normal pointing towards camera

    // Blade quad vertices in local space
    float blade_verts[4][2] = {{-blade_half_width, 0.0F},
                               {blade_half_width, 0.0F},
                               {blade_half_width, blade_length},
                               {-blade_half_width, blade_length}};

    // Transform and add blade vertices
    for (int i = 0; i < 4; ++i) {
      float const local_x = blade_verts[i][0];
      float const local_y = blade_verts[i][1];

      // Rotate around origin
      float const world_x = local_x * cos_a - local_y * sin_a;
      float const world_y = local_x * sin_a + local_y * cos_a;

      verts.push_back({{world_x, world_y, 0.0F},
                       {n.x(), n.y(), n.z()},
                       {(i % 2) == 0 ? 0.0F : 1.0F, i < 2 ? 0.0F : 1.0F}});
    }

    // Blade indices
    idx.push_back(base + 0);
    idx.push_back(base + 1);
    idx.push_back(base + 2);
    idx.push_back(base + 2);
    idx.push_back(base + 3);
    idx.push_back(base + 0);

    // Cross guard (small horizontal bar)
    size_t const guard_base = verts.size();
    float const guard_half_width = cross_guard_width * 0.5F;
    float const guard_half_height = cross_guard_height * 0.5F;
    float const guard_y = blade_length * 0.2F;

    float guard_verts[4][2] = {{-guard_half_width, guard_y - guard_half_height},
                               {guard_half_width, guard_y - guard_half_height},
                               {guard_half_width, guard_y + guard_half_height},
                               {-guard_half_width, guard_y + guard_half_height}};

    for (int i = 0; i < 4; ++i) {
      float const local_x = guard_verts[i][0];
      float const local_y = guard_verts[i][1];
      float const world_x = local_x * cos_a - local_y * sin_a;
      float const world_y = local_x * sin_a + local_y * cos_a;

      verts.push_back({{world_x, world_y, 0.0F},
                       {n.x(), n.y(), n.z()},
                       {(i % 2) == 0 ? 0.0F : 1.0F, i < 2 ? 0.0F : 1.0F}});
    }

    // Cross guard indices
    idx.push_back(guard_base + 0);
    idx.push_back(guard_base + 1);
    idx.push_back(guard_base + 2);
    idx.push_back(guard_base + 2);
    idx.push_back(guard_base + 3);
    idx.push_back(guard_base + 0);
  }

  return std::make_unique<Mesh>(verts, idx);
}

// Create shield mesh for guard/defend mode
auto ModeIndicator::create_guard_mode_mesh()
    -> std::unique_ptr<Render::GL::Mesh> {
  using namespace Render::GL;
  std::vector<Vertex> verts;
  std::vector<unsigned int> idx;

  // Shield parameters
  constexpr float shield_width = 0.6F;
  constexpr float shield_height = 0.7F;
  constexpr int segments = 12;

  QVector3D const n(0, 0, 1); // Normal pointing towards camera

  // Create shield as a rounded rectangle/kite shape
  // Top half: semicircle
  // Bottom half: pointed triangle

  float const half_width = shield_width * 0.5F;
  float const top_height = shield_height * 0.4F;
  float const bottom_height = shield_height * 0.6F;

  size_t const center_idx = verts.size();
  verts.push_back(
      {{0.0F, 0.0F, 0.0F}, {n.x(), n.y(), n.z()}, {0.5F, 0.5F}}); // Center

  // Top semicircle vertices
  for (int i = 0; i <= segments; ++i) {
    float const angle = 3.14159265F * (i / float(segments)); // 0 to PI
    float const x = half_width * std::cos(angle);
    float const y = top_height * std::sin(angle);
    verts.push_back({{x, y, 0.0F}, {n.x(), n.y(), n.z()}, {0.5F, 1.0F}});
  }

  // Bottom point
  size_t const bottom_idx = verts.size();
  verts.push_back(
      {{0.0F, -bottom_height, 0.0F}, {n.x(), n.y(), n.z()}, {0.5F, 0.0F}});

  // Triangles for top half
  for (int i = 0; i < segments; ++i) {
    idx.push_back(center_idx);
    idx.push_back(center_idx + 1 + i);
    idx.push_back(center_idx + 1 + i + 1);
  }

  // Triangles for bottom half (left and right sides)
  size_t const leftmost_idx = center_idx + 1;
  size_t const rightmost_idx = center_idx + 1 + segments;

  idx.push_back(center_idx);
  idx.push_back(leftmost_idx);
  idx.push_back(bottom_idx);

  idx.push_back(center_idx);
  idx.push_back(bottom_idx);
  idx.push_back(rightmost_idx);

  return std::make_unique<Mesh>(verts, idx);
}

auto ModeIndicator::get_hold_mode_mesh() -> Render::GL::Mesh * {
  if (!s_hold_mesh) {
    s_hold_mesh = create_hold_mode_mesh();
  }
  return s_hold_mesh.get();
}

auto ModeIndicator::get_guard_mode_mesh() -> Render::GL::Mesh * {
  if (!s_guard_mesh) {
    s_guard_mesh = create_guard_mode_mesh();
  }
  return s_guard_mesh.get();
}

} // namespace Render::Geom
