#include "mode_indicator.h"
#include "../gl/mesh.h"
#include <QVector3D>
#include <cmath>
#include <cstddef>
#include <memory>
#include <vector>

namespace {
constexpr float k_pi = 3.14159265358979323846F;
}

namespace Render::Geom {

std::unique_ptr<Render::GL::Mesh> ModeIndicator::s_attack_mesh;
std::unique_ptr<Render::GL::Mesh> ModeIndicator::s_guard_mesh;
std::unique_ptr<Render::GL::Mesh> ModeIndicator::s_hold_mesh;
std::unique_ptr<Render::GL::Mesh> ModeIndicator::s_patrol_mesh;

// Attack mode: crossed swords (reusing existing create_hold_mode_mesh)
auto ModeIndicator::create_attack_mode_mesh()
    -> std::unique_ptr<Render::GL::Mesh> {
  using namespace Render::GL;
  std::vector<Vertex> verts;
  std::vector<unsigned int> idx;

  constexpr float sword_length = 0.8F;
  constexpr float sword_width = 0.12F;
  constexpr float blade_length = 0.6F;
  constexpr float handle_length = 0.2F;
  constexpr float cross_guard_width = 0.2F;
  constexpr float cross_guard_height = 0.05F;

  const float angles[] = {0.785398F, -0.785398F};

  for (int sword_idx = 0; sword_idx < 2; ++sword_idx) {
    float const angle = angles[sword_idx];
    float const cos_a = std::cos(angle);
    float const sin_a = std::sin(angle);

    size_t const base = verts.size();

    float const blade_half_width = sword_width * 0.5F;
    QVector3D const n(0, 0, 1);

    float blade_verts[4][2] = {{-blade_half_width, 0.0F},
                               {blade_half_width, 0.0F},
                               {blade_half_width, blade_length},
                               {-blade_half_width, blade_length}};

    for (int i = 0; i < 4; ++i) {
      float const local_x = blade_verts[i][0];
      float const local_y = blade_verts[i][1];

      float const world_x = local_x * cos_a - local_y * sin_a;
      float const world_y = local_x * sin_a + local_y * cos_a;

      verts.push_back({{world_x, world_y, 0.0F},
                       {n.x(), n.y(), n.z()},
                       {(i % 2) == 0 ? 0.0F : 1.0F, i < 2 ? 0.0F : 1.0F}});
    }

    idx.push_back(base + 0);
    idx.push_back(base + 1);
    idx.push_back(base + 2);
    idx.push_back(base + 2);
    idx.push_back(base + 3);
    idx.push_back(base + 0);

    size_t const guard_base = verts.size();
    float const guard_half_width = cross_guard_width * 0.5F;
    float const guard_half_height = cross_guard_height * 0.5F;
    float const guard_y = blade_length * 0.2F;

    float guard_verts[4][2] = {
        {-guard_half_width, guard_y - guard_half_height},
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

    idx.push_back(guard_base + 0);
    idx.push_back(guard_base + 1);
    idx.push_back(guard_base + 2);
    idx.push_back(guard_base + 2);
    idx.push_back(guard_base + 3);
    idx.push_back(guard_base + 0);
  }

  return std::make_unique<Mesh>(verts, idx);
}

auto ModeIndicator::create_guard_mode_mesh()
    -> std::unique_ptr<Render::GL::Mesh> {
  using namespace Render::GL;
  std::vector<Vertex> verts;
  std::vector<unsigned int> idx;

  constexpr float shield_width = 0.6F;
  constexpr float shield_height = 0.7F;
  constexpr int segments = 12;

  QVector3D const n(0, 0, 1);

  float const half_width = shield_width * 0.5F;
  float const top_height = shield_height * 0.4F;
  float const bottom_height = shield_height * 0.6F;

  size_t const center_idx = verts.size();
  verts.push_back({{0.0F, 0.0F, 0.0F}, {n.x(), n.y(), n.z()}, {0.5F, 0.5F}});

  for (int i = 0; i <= segments; ++i) {
    float const angle = k_pi * (i / float(segments));
    float const x = half_width * std::cos(angle);
    float const y = top_height * std::sin(angle);
    verts.push_back({{x, y, 0.0F}, {n.x(), n.y(), n.z()}, {0.5F, 1.0F}});
  }

  size_t const bottom_idx = verts.size();
  verts.push_back(
      {{0.0F, -bottom_height, 0.0F}, {n.x(), n.y(), n.z()}, {0.5F, 0.0F}});

  for (int i = 0; i < segments; ++i) {
    idx.push_back(center_idx);
    idx.push_back(center_idx + 1 + i);
    idx.push_back(center_idx + 1 + i + 1);
  }

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

// Hold mode: anchor shape
auto ModeIndicator::create_hold_mode_mesh()
    -> std::unique_ptr<Render::GL::Mesh> {
  using namespace Render::GL;
  std::vector<Vertex> verts;
  std::vector<unsigned int> idx;

  constexpr float anchor_width = 0.5F;
  constexpr float anchor_height = 0.7F;
  constexpr float ring_radius = 0.15F;
  constexpr float shank_width = 0.08F;
  constexpr float fluke_width = 0.25F;
  constexpr float fluke_height = 0.2F;

  QVector3D const n(0, 0, 1);

  // Ring at top (circle)
  constexpr int ring_segments = 12;
  size_t const ring_center = verts.size();
  verts.push_back({{0.0F, anchor_height * 0.7F, 0.0F},
                   {n.x(), n.y(), n.z()},
                   {0.5F, 0.5F}});

  for (int i = 0; i <= ring_segments; ++i) {
    float const angle = (i / float(ring_segments)) * 2.0F * k_pi;
    float const x = ring_radius * std::cos(angle);
    float const y = anchor_height * 0.7F + ring_radius * std::sin(angle);
    verts.push_back({{x, y, 0.0F}, {n.x(), n.y(), n.z()}, {0.5F, 1.0F}});
  }

  for (int i = 0; i < ring_segments; ++i) {
    idx.push_back(ring_center);
    idx.push_back(ring_center + 1 + i);
    idx.push_back(ring_center + 1 + i + 1);
  }

  // Shank (vertical bar)
  float const shank_half = shank_width * 0.5F;
  size_t const shank_base = verts.size();
  verts.push_back({{-shank_half, anchor_height * 0.6F, 0.0F},
                   {n.x(), n.y(), n.z()},
                   {0.0F, 1.0F}});
  verts.push_back({{shank_half, anchor_height * 0.6F, 0.0F},
                   {n.x(), n.y(), n.z()},
                   {1.0F, 1.0F}});
  verts.push_back(
      {{shank_half, -anchor_height * 0.3F, 0.0F}, {n.x(), n.y(), n.z()}, {1.0F, 0.0F}});
  verts.push_back({{-shank_half, -anchor_height * 0.3F, 0.0F},
                   {n.x(), n.y(), n.z()},
                   {0.0F, 0.0F}});

  idx.push_back(shank_base + 0);
  idx.push_back(shank_base + 1);
  idx.push_back(shank_base + 2);
  idx.push_back(shank_base + 2);
  idx.push_back(shank_base + 3);
  idx.push_back(shank_base + 0);

  // Flukes (arms at bottom)
  float const fluke_y = -anchor_height * 0.3F;

  // Left fluke
  size_t const left_fluke_base = verts.size();
  verts.push_back({{-shank_half, fluke_y, 0.0F}, {n.x(), n.y(), n.z()}, {0.0F, 0.5F}});
  verts.push_back(
      {{-fluke_width, fluke_y - fluke_height, 0.0F}, {n.x(), n.y(), n.z()}, {0.0F, 0.0F}});
  verts.push_back({{-shank_half, fluke_y - fluke_height * 0.5F, 0.0F},
                   {n.x(), n.y(), n.z()},
                   {0.5F, 0.0F}});

  idx.push_back(left_fluke_base + 0);
  idx.push_back(left_fluke_base + 1);
  idx.push_back(left_fluke_base + 2);

  // Right fluke
  size_t const right_fluke_base = verts.size();
  verts.push_back({{shank_half, fluke_y, 0.0F}, {n.x(), n.y(), n.z()}, {1.0F, 0.5F}});
  verts.push_back(
      {{fluke_width, fluke_y - fluke_height, 0.0F}, {n.x(), n.y(), n.z()}, {1.0F, 0.0F}});
  verts.push_back({{shank_half, fluke_y - fluke_height * 0.5F, 0.0F},
                   {n.x(), n.y(), n.z()},
                   {0.5F, 0.0F}});

  idx.push_back(right_fluke_base + 0);
  idx.push_back(right_fluke_base + 1);
  idx.push_back(right_fluke_base + 2);

  return std::make_unique<Mesh>(verts, idx);
}

// Patrol mode: footsteps
auto ModeIndicator::create_patrol_mode_mesh()
    -> std::unique_ptr<Render::GL::Mesh> {
  using namespace Render::GL;
  std::vector<Vertex> verts;
  std::vector<unsigned int> idx;

  constexpr float foot_length = 0.25F;
  constexpr float foot_width = 0.15F;
  constexpr float step_offset = 0.2F;

  QVector3D const n(0, 0, 1);

  // Create two footprints offset from each other
  for (int foot = 0; foot < 2; ++foot) {
    float const x_offset = (foot == 0) ? -step_offset : step_offset;
    float const y_offset = (foot == 0) ? 0.15F : -0.15F;

    size_t const base = verts.size();

    // Footprint shape (ellipse)
    constexpr int segments = 8;
    verts.push_back({{x_offset, y_offset, 0.0F}, {n.x(), n.y(), n.z()}, {0.5F, 0.5F}});

    for (int i = 0; i <= segments; ++i) {
      float const angle = (i / float(segments)) * 2.0F * k_pi;
      float const x = x_offset + (foot_width * 0.5F) * std::cos(angle);
      float const y = y_offset + (foot_length * 0.5F) * std::sin(angle);
      verts.push_back({{x, y, 0.0F}, {n.x(), n.y(), n.z()}, {0.5F, 1.0F}});
    }

    for (int i = 0; i < segments; ++i) {
      idx.push_back(base);
      idx.push_back(base + 1 + i);
      idx.push_back(base + 1 + i + 1);
    }
  }

  return std::make_unique<Mesh>(verts, idx);
}

auto ModeIndicator::get_attack_mode_mesh() -> Render::GL::Mesh * {
  if (!s_attack_mesh) {
    s_attack_mesh = create_attack_mode_mesh();
  }
  return s_attack_mesh.get();
}

auto ModeIndicator::get_guard_mode_mesh() -> Render::GL::Mesh * {
  if (!s_guard_mesh) {
    s_guard_mesh = create_guard_mode_mesh();
  }
  return s_guard_mesh.get();
}

auto ModeIndicator::get_hold_mode_mesh() -> Render::GL::Mesh * {
  if (!s_hold_mesh) {
    s_hold_mesh = create_hold_mode_mesh();
  }
  return s_hold_mesh.get();
}

auto ModeIndicator::get_patrol_mode_mesh() -> Render::GL::Mesh * {
  if (!s_patrol_mesh) {
    s_patrol_mesh = create_patrol_mode_mesh();
  }
  return s_patrol_mesh.get();
}

} // namespace Render::Geom
