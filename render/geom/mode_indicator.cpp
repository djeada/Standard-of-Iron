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

// Attack mode: crossed swords
auto ModeIndicator::create_attack_mode_mesh()
    -> std::unique_ptr<Render::GL::Mesh> {
  using namespace Render::GL;
  std::vector<Vertex> verts;
  std::vector<unsigned int> idx;

  constexpr float sword_length = 0.9F;
  constexpr float sword_width = 0.1F;
  constexpr float blade_length = 0.65F;
  constexpr float handle_length = 0.2F;
  constexpr float cross_guard_width = 0.25F;
  constexpr float cross_guard_height = 0.06F;
  constexpr float blade_tip_width = 0.03F;
  constexpr float handle_offset = 0.25F; // Offset handles so swords cross at top
  constexpr float guard_position_ratio = 0.15F;
  constexpr float handle_width_ratio = 0.3F;

  const float angles[] = {0.785398F, -0.785398F}; // 45 degrees
  const float x_offsets[] = {-handle_offset, handle_offset}; // Separate handles

  for (int sword_idx = 0; sword_idx < 2; ++sword_idx) {
    float const angle = angles[sword_idx];
    float const cos_a = std::cos(angle);
    float const sin_a = std::sin(angle);
    float const x_offset = x_offsets[sword_idx];

    size_t const base = verts.size();

    float const blade_half_width = sword_width * 0.5F;
    QVector3D const n(0, 0, 1);

    // Blade starts at handle position and extends outward
    float blade_verts[4][2] = {{-blade_half_width, 0.0F},
                               {blade_half_width, 0.0F},
                               {blade_tip_width, blade_length},
                               {-blade_tip_width, blade_length}};

    for (int i = 0; i < 4; ++i) {
      float const local_x = blade_verts[i][0];
      float const local_y = blade_verts[i][1];

      // Apply rotation and offset
      float const world_x = local_x * cos_a - local_y * sin_a + x_offset;
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

    // Cross-guard
    size_t const guard_base = verts.size();
    float const guard_half_width = cross_guard_width * 0.5F;
    float const guard_half_height = cross_guard_height * 0.5F;
    float const guard_y = blade_length * guard_position_ratio;

    float guard_verts[4][2] = {
        {-guard_half_width, guard_y - guard_half_height},
        {guard_half_width, guard_y - guard_half_height},
        {guard_half_width, guard_y + guard_half_height},
        {-guard_half_width, guard_y + guard_half_height}};

    for (int i = 0; i < 4; ++i) {
      float const local_x = guard_verts[i][0];
      float const local_y = guard_verts[i][1];
      float const world_x = local_x * cos_a - local_y * sin_a + x_offset;
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

    // Handle below guard
    size_t const handle_base = verts.size();
    float const handle_half_width = sword_width * handle_width_ratio;
    float const handle_start = -handle_length;

    float handle_verts[4][2] = {
        {-handle_half_width, handle_start},
        {handle_half_width, handle_start},
        {handle_half_width, 0.0F},
        {-handle_half_width, 0.0F}};

    for (int i = 0; i < 4; ++i) {
      float const local_x = handle_verts[i][0];
      float const local_y = handle_verts[i][1];
      float const world_x = local_x * cos_a - local_y * sin_a + x_offset;
      float const world_y = local_x * sin_a + local_y * cos_a;

      verts.push_back({{world_x, world_y, 0.0F},
                       {n.x(), n.y(), n.z()},
                       {(i % 2) == 0 ? 0.0F : 1.0F, i < 2 ? 0.0F : 1.0F}});
    }

    idx.push_back(handle_base + 0);
    idx.push_back(handle_base + 1);
    idx.push_back(handle_base + 2);
    idx.push_back(handle_base + 2);
    idx.push_back(handle_base + 3);
    idx.push_back(handle_base + 0);
  }

  return std::make_unique<Mesh>(verts, idx);
}

auto ModeIndicator::create_guard_mode_mesh()
    -> std::unique_ptr<Render::GL::Mesh> {
  using namespace Render::GL;
  std::vector<Vertex> verts;
  std::vector<unsigned int> idx;

  constexpr float shield_width = 0.65F;
  constexpr float shield_height = 0.75F;
  constexpr int segments = 16;
  constexpr float boss_radius = 0.12F;

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

  // Add center boss (raised decorative circle)
  constexpr int boss_segments = 12;
  size_t const boss_center = verts.size();
  verts.push_back({{0.0F, 0.0F, 0.0F}, {n.x(), n.y(), n.z()}, {0.5F, 0.5F}});

  for (int i = 0; i <= boss_segments; ++i) {
    float const angle = (i / float(boss_segments)) * 2.0F * k_pi;
    float const x = boss_radius * std::cos(angle);
    float const y = boss_radius * std::sin(angle);
    verts.push_back({{x, y, 0.0F}, {n.x(), n.y(), n.z()}, {0.5F, 0.8F}});
  }

  for (int i = 0; i < boss_segments; ++i) {
    idx.push_back(boss_center);
    idx.push_back(boss_center + 1 + i);
    idx.push_back(boss_center + 1 + i + 1);
  }

  return std::make_unique<Mesh>(verts, idx);
}

// Hold mode: anchor shape
auto ModeIndicator::create_hold_mode_mesh()
    -> std::unique_ptr<Render::GL::Mesh> {
  using namespace Render::GL;
  std::vector<Vertex> verts;
  std::vector<unsigned int> idx;

  constexpr float anchor_width = 0.6F;
  constexpr float anchor_height = 0.8F;
  constexpr float ring_outer_radius = 0.1F;
  constexpr float ring_inner_radius = 0.06F;
  constexpr float shank_width = 0.05F;
  constexpr float fluke_width = 0.32F;
  constexpr float fluke_height = 0.18F;
  constexpr float stock_width = 0.22F;

  QVector3D const n(0, 0, 1);

  // Ring at top (hollow circle)
  constexpr int ring_segments = 16;
  float const ring_y = anchor_height * 0.65F;
  
  // Outer ring
  for (int i = 0; i < ring_segments; ++i) {
    float const angle1 = (i / float(ring_segments)) * 2.0F * k_pi;
    float const angle2 = ((i + 1) / float(ring_segments)) * 2.0F * k_pi;
    
    float const x1_outer = ring_outer_radius * std::cos(angle1);
    float const y1_outer = ring_y + ring_outer_radius * std::sin(angle1);
    float const x2_outer = ring_outer_radius * std::cos(angle2);
    float const y2_outer = ring_y + ring_outer_radius * std::sin(angle2);
    
    float const x1_inner = ring_inner_radius * std::cos(angle1);
    float const y1_inner = ring_y + ring_inner_radius * std::sin(angle1);
    float const x2_inner = ring_inner_radius * std::cos(angle2);
    float const y2_inner = ring_y + ring_inner_radius * std::sin(angle2);
    
    size_t const base = verts.size();
    verts.push_back({{x1_outer, y1_outer, 0.0F}, {n.x(), n.y(), n.z()}, {0.0F, 1.0F}});
    verts.push_back({{x2_outer, y2_outer, 0.0F}, {n.x(), n.y(), n.z()}, {1.0F, 1.0F}});
    verts.push_back({{x2_inner, y2_inner, 0.0F}, {n.x(), n.y(), n.z()}, {1.0F, 0.0F}});
    verts.push_back({{x1_inner, y1_inner, 0.0F}, {n.x(), n.y(), n.z()}, {0.0F, 0.0F}});
    
    idx.push_back(base + 0);
    idx.push_back(base + 1);
    idx.push_back(base + 2);
    idx.push_back(base + 2);
    idx.push_back(base + 3);
    idx.push_back(base + 0);
  }

  // Shank (vertical bar)
  float const shank_half = shank_width * 0.5F;
  size_t const shank_base = verts.size();
  verts.push_back({{-shank_half, ring_y - ring_inner_radius, 0.0F},
                   {n.x(), n.y(), n.z()},
                   {0.0F, 1.0F}});
  verts.push_back({{shank_half, ring_y - ring_inner_radius, 0.0F},
                   {n.x(), n.y(), n.z()},
                   {1.0F, 1.0F}});
  verts.push_back(
      {{shank_half, -anchor_height * 0.25F, 0.0F}, {n.x(), n.y(), n.z()}, {1.0F, 0.0F}});
  verts.push_back({{-shank_half, -anchor_height * 0.25F, 0.0F},
                   {n.x(), n.y(), n.z()},
                   {0.0F, 0.0F}});

  idx.push_back(shank_base + 0);
  idx.push_back(shank_base + 1);
  idx.push_back(shank_base + 2);
  idx.push_back(shank_base + 2);
  idx.push_back(shank_base + 3);
  idx.push_back(shank_base + 0);

  // Stock (horizontal bar at bottom)
  float const stock_y = -anchor_height * 0.25F;
  float const stock_half = stock_width * 0.5F;
  float const stock_height = shank_width;
  
  size_t const stock_base = verts.size();
  verts.push_back({{-stock_half, stock_y - stock_height * 0.5F, 0.0F},
                   {n.x(), n.y(), n.z()},
                   {0.0F, 0.5F}});
  verts.push_back({{stock_half, stock_y - stock_height * 0.5F, 0.0F},
                   {n.x(), n.y(), n.z()},
                   {1.0F, 0.5F}});
  verts.push_back({{stock_half, stock_y + stock_height * 0.5F, 0.0F},
                   {n.x(), n.y(), n.z()},
                   {1.0F, 0.5F}});
  verts.push_back({{-stock_half, stock_y + stock_height * 0.5F, 0.0F},
                   {n.x(), n.y(), n.z()},
                   {0.0F, 0.5F}});

  idx.push_back(stock_base + 0);
  idx.push_back(stock_base + 1);
  idx.push_back(stock_base + 2);
  idx.push_back(stock_base + 2);
  idx.push_back(stock_base + 3);
  idx.push_back(stock_base + 0);

  // Flukes (curved arms at bottom) - more realistic shape
  float const fluke_y_start = stock_y;
  float const fluke_y_end = stock_y - fluke_height;
  
  // Left fluke - curved outward
  constexpr int fluke_segments = 8;
  for (int i = 0; i < fluke_segments; ++i) {
    float const t1 = i / float(fluke_segments);
    float const t2 = (i + 1) / float(fluke_segments);
    
    // Curve from center outward and downward
    float const x1_inner = -shank_half - (fluke_width - shank_half) * t1 * t1;
    float const y1 = fluke_y_start - fluke_height * t1;
    float const x2_inner = -shank_half - (fluke_width - shank_half) * t2 * t2;
    float const y2 = fluke_y_start - fluke_height * t2;
    
    float const fluke_thickness = shank_width * (1.0F + t1 * 0.5F);
    float const x1_outer = x1_inner - fluke_thickness * 0.5F;
    float const x2_outer = x2_inner - fluke_thickness * 0.5F;
    
    size_t const base = verts.size();
    verts.push_back({{x1_inner, y1, 0.0F}, {n.x(), n.y(), n.z()}, {0.0F, 1.0F}});
    verts.push_back({{x2_inner, y2, 0.0F}, {n.x(), n.y(), n.z()}, {1.0F, 1.0F}});
    verts.push_back({{x2_outer, y2, 0.0F}, {n.x(), n.y(), n.z()}, {1.0F, 0.0F}});
    verts.push_back({{x1_outer, y1, 0.0F}, {n.x(), n.y(), n.z()}, {0.0F, 0.0F}});
    
    idx.push_back(base + 0);
    idx.push_back(base + 1);
    idx.push_back(base + 2);
    idx.push_back(base + 2);
    idx.push_back(base + 3);
    idx.push_back(base + 0);
  }
  
  // Right fluke - curved outward (mirror of left)
  for (int i = 0; i < fluke_segments; ++i) {
    float const t1 = i / float(fluke_segments);
    float const t2 = (i + 1) / float(fluke_segments);
    
    float const x1_inner = shank_half + (fluke_width - shank_half) * t1 * t1;
    float const y1 = fluke_y_start - fluke_height * t1;
    float const x2_inner = shank_half + (fluke_width - shank_half) * t2 * t2;
    float const y2 = fluke_y_start - fluke_height * t2;
    
    float const fluke_thickness = shank_width * (1.0F + t1 * 0.5F);
    float const x1_outer = x1_inner + fluke_thickness * 0.5F;
    float const x2_outer = x2_inner + fluke_thickness * 0.5F;
    
    size_t const base = verts.size();
    verts.push_back({{x1_inner, y1, 0.0F}, {n.x(), n.y(), n.z()}, {0.0F, 1.0F}});
    verts.push_back({{x2_inner, y2, 0.0F}, {n.x(), n.y(), n.z()}, {1.0F, 1.0F}});
    verts.push_back({{x2_outer, y2, 0.0F}, {n.x(), n.y(), n.z()}, {1.0F, 0.0F}});
    verts.push_back({{x1_outer, y1, 0.0F}, {n.x(), n.y(), n.z()}, {0.0F, 0.0F}});
    
    idx.push_back(base + 0);
    idx.push_back(base + 1);
    idx.push_back(base + 2);
    idx.push_back(base + 2);
    idx.push_back(base + 3);
    idx.push_back(base + 0);
  }

  return std::make_unique<Mesh>(verts, idx);
}

// Patrol mode: circular arrows indicating patrol route
auto ModeIndicator::create_patrol_mode_mesh()
    -> std::unique_ptr<Render::GL::Mesh> {
  using namespace Render::GL;
  std::vector<Vertex> verts;
  std::vector<unsigned int> idx;

  constexpr float circle_radius = 0.3F;
  constexpr float arrow_width = 0.08F;
  constexpr float arrow_head_length = 0.15F;
  constexpr float arrow_head_width = 0.15F;
  constexpr int circle_segments = 24;
  constexpr float arrow_end_ratio = 0.85F; // How far around the circle the arrow extends

  QVector3D const n(0, 0, 1);

  // Create two circular arrow paths (top and bottom half of circle)
  for (int arrow = 0; arrow < 2; ++arrow) {
    // Determine which half of the circle (top or bottom)
    float const start_angle = arrow == 0 ? 0.0F : k_pi;
    float const end_angle = arrow == 0 ? k_pi * arrow_end_ratio : k_pi * (1.0F + arrow_end_ratio);
    int const segments = circle_segments / 2;

    // Draw the curved path
    for (int i = 0; i < segments; ++i) {
      float const t1 = i / float(segments);
      float const t2 = (i + 1) / float(segments);
      float const angle1 = start_angle + (end_angle - start_angle) * t1;
      float const angle2 = start_angle + (end_angle - start_angle) * t2;

      // Inner and outer points of the arrow path
      float const x1_inner = (circle_radius - arrow_width * 0.5F) * std::cos(angle1);
      float const y1_inner = (circle_radius - arrow_width * 0.5F) * std::sin(angle1);
      float const x1_outer = (circle_radius + arrow_width * 0.5F) * std::cos(angle1);
      float const y1_outer = (circle_radius + arrow_width * 0.5F) * std::sin(angle1);

      float const x2_inner = (circle_radius - arrow_width * 0.5F) * std::cos(angle2);
      float const y2_inner = (circle_radius - arrow_width * 0.5F) * std::sin(angle2);
      float const x2_outer = (circle_radius + arrow_width * 0.5F) * std::cos(angle2);
      float const y2_outer = (circle_radius + arrow_width * 0.5F) * std::sin(angle2);

      size_t const base = verts.size();
      verts.push_back({{x1_inner, y1_inner, 0.0F}, {n.x(), n.y(), n.z()}, {0.0F, 0.0F}});
      verts.push_back({{x1_outer, y1_outer, 0.0F}, {n.x(), n.y(), n.z()}, {1.0F, 0.0F}});
      verts.push_back({{x2_outer, y2_outer, 0.0F}, {n.x(), n.y(), n.z()}, {1.0F, 1.0F}});
      verts.push_back({{x2_inner, y2_inner, 0.0F}, {n.x(), n.y(), n.z()}, {0.0F, 1.0F}});

      idx.push_back(base + 0);
      idx.push_back(base + 1);
      idx.push_back(base + 2);
      idx.push_back(base + 2);
      idx.push_back(base + 3);
      idx.push_back(base + 0);
    }

    // Add arrowhead at the end
    float const arrow_angle = end_angle;
    float const arrow_x = circle_radius * std::cos(arrow_angle);
    float const arrow_y = circle_radius * std::sin(arrow_angle);
    
    // Direction perpendicular to circle (tangent direction for arrow)
    float const tangent_x = -std::sin(arrow_angle);
    float const tangent_y = std::cos(arrow_angle);
    
    // Direction pointing outward from circle center
    float const normal_x = std::cos(arrow_angle);
    float const normal_y = std::sin(arrow_angle);

    // Arrow head tip
    float const tip_x = arrow_x + tangent_x * arrow_head_length;
    float const tip_y = arrow_y + tangent_y * arrow_head_length;

    // Arrow head base corners
    float const base1_x = arrow_x + normal_x * arrow_head_width * 0.5F;
    float const base1_y = arrow_y + normal_y * arrow_head_width * 0.5F;
    float const base2_x = arrow_x - normal_x * arrow_head_width * 0.5F;
    float const base2_y = arrow_y - normal_y * arrow_head_width * 0.5F;

    size_t const arrow_base = verts.size();
    verts.push_back({{tip_x, tip_y, 0.0F}, {n.x(), n.y(), n.z()}, {0.5F, 1.0F}});
    verts.push_back({{base1_x, base1_y, 0.0F}, {n.x(), n.y(), n.z()}, {1.0F, 0.0F}});
    verts.push_back({{base2_x, base2_y, 0.0F}, {n.x(), n.y(), n.z()}, {0.0F, 0.0F}});

    idx.push_back(arrow_base + 0);
    idx.push_back(arrow_base + 1);
    idx.push_back(arrow_base + 2);
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
