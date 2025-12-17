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
  constexpr float handle_offset = -0.18F;
  constexpr float guard_position_ratio = 0.15F;
  constexpr float handle_width_ratio = 0.3F;

  const float angles[] = {0.785398F, -0.785398F};
  const float x_offsets[] = {-handle_offset, handle_offset};

  for (int sword_idx = 0; sword_idx < 2; ++sword_idx) {
    float const angle = angles[sword_idx];
    float const cos_a = std::cos(angle);
    float const sin_a = std::sin(angle);
    float const x_offset = x_offsets[sword_idx];

    size_t const base = verts.size();

    float const blade_half_width = sword_width * 0.5F;
    QVector3D const n(0, 0, 1);

    float blade_verts[4][2] = {{-blade_half_width, 0.0F},
                               {blade_half_width, 0.0F},
                               {blade_tip_width, blade_length},
                               {-blade_tip_width, blade_length}};

    for (int i = 0; i < 4; ++i) {
      float const local_x = blade_verts[i][0];
      float const local_y = blade_verts[i][1];

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

    size_t const handle_base = verts.size();
    float const handle_half_width = sword_width * handle_width_ratio;
    float const handle_start = -handle_length;

    float handle_verts[4][2] = {{-handle_half_width, handle_start},
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

  QVector3D const n(0, 0, 1);

  constexpr float shield_width = 0.42F;
  constexpr float shield_height = 0.62F;

  constexpr float inner_scale = 0.82F;

  constexpr float face_curve_z = 0.025F;

  constexpr float boss_radius = 0.095F;
  constexpr float boss_height = 0.04F;

  auto add_vert = [&](float x, float y, float u, float v, float z = 0.0F) {
    verts.push_back({{x, y, z}, {n.x(), n.y(), n.z()}, {u, v}});
    return static_cast<unsigned int>(verts.size() - 1);
  };

  auto uv_for = [&](float x, float y) {
    float u = (x / shield_width) * 0.5F + 0.5F;
    float v = (y / shield_height) * 0.5F + 0.5F;
    return QVector2D(u, v);
  };

  const std::vector<QVector2D> outline = {
      {-0.55F * shield_width, 0.55F * shield_height},
      {-0.20F * shield_width, 0.60F * shield_height},
      {0.20F * shield_width, 0.60F * shield_height},
      {0.55F * shield_width, 0.55F * shield_height},

      {0.65F * shield_width, 0.15F * shield_height},
      {0.55F * shield_width, -0.25F * shield_height},
      {0.25F * shield_width, -0.60F * shield_height},

      {0.00F, -0.85F * shield_height},

      {-0.25F * shield_width, -0.60F * shield_height},
      {-0.55F * shield_width, -0.25F * shield_height},
      {-0.65F * shield_width, 0.15F * shield_height},
  };

  std::vector<unsigned int> outer_idx;
  std::vector<unsigned int> inner_idx;

  for (auto const &p : outline) {
    auto uv = uv_for(p.x(), p.y());
    outer_idx.push_back(add_vert(p.x(), p.y(), uv.x(), uv.y(), 0.0F));

    QVector2D ip = p * inner_scale;

    float z = face_curve_z * (1.0F - std::abs(ip.y()) / shield_height);
    auto iuv = uv_for(ip.x(), ip.y());

    inner_idx.push_back(add_vert(ip.x(), ip.y(), iuv.x(), iuv.y(), z));
  }

  for (size_t i = 0; i < outer_idx.size(); ++i) {
    size_t next = (i + 1) % outer_idx.size();

    idx.insert(idx.end(), {outer_idx[i], outer_idx[next], inner_idx[i],
                           inner_idx[i], outer_idx[next], inner_idx[next]});
  }

  unsigned int face_center =
      add_vert(0.0F, -shield_height * 0.05F, 0.5F, 0.45F, face_curve_z);

  for (size_t i = 0; i < inner_idx.size(); ++i) {
    size_t next = (i + 1) % inner_idx.size();
    idx.insert(idx.end(), {face_center, inner_idx[i], inner_idx[next]});
  }

  constexpr int boss_segments = 18;

  unsigned int boss_center =
      add_vert(0.0F, shield_height * 0.08F, 0.5F, 0.58F, boss_height);

  std::vector<unsigned int> boss_ring;
  boss_ring.reserve(boss_segments + 1);

  for (int i = 0; i <= boss_segments; ++i) {
    float a = (i / float(boss_segments)) * 2.0F * k_pi;
    float x = boss_radius * std::cos(a);
    float y = shield_height * 0.08F + boss_radius * std::sin(a);
    auto uv = uv_for(x, y);
    boss_ring.push_back(add_vert(x, y, uv.x(), uv.y(), boss_height));
  }

  for (int i = 0; i < boss_segments; ++i) {
    idx.insert(idx.end(), {boss_center, boss_ring[i], boss_ring[i + 1]});
  }

  return std::make_unique<Mesh>(verts, idx);
}

auto ModeIndicator::create_hold_mode_mesh()
    -> std::unique_ptr<Render::GL::Mesh> {
  using namespace Render::GL;

  std::vector<Vertex> verts;
  std::vector<unsigned int> idx;

  constexpr float anchor_height = 0.9F;

  constexpr float ring_outer = 0.11F;
  constexpr float ring_inner = 0.065F;

  constexpr float shank_width = 0.12F;

  constexpr float cross_width = 0.32F;
  constexpr float cross_height = 0.11F;

  constexpr float fluke_span = 0.48F;
  constexpr float fluke_drop = 0.28F;

  QVector3D const n(0, 0, 1);

  auto add = [&](float x, float y, float u, float v) {
    verts.push_back({{x, y, 0.0F}, {n.x(), n.y(), n.z()}, {u, v}});
    return static_cast<unsigned int>(verts.size() - 1);
  };

  constexpr int ring_segments = 18;
  float const ring_y = anchor_height * 0.32F;

  for (int i = 0; i < ring_segments; ++i) {
    float a0 = (i / float(ring_segments)) * 2.0F * k_pi;
    float a1 = ((i + 1) / float(ring_segments)) * 2.0F * k_pi;

    float x0o = ring_outer * std::cos(a0);
    float y0o = ring_y + ring_outer * std::sin(a0);
    float x1o = ring_outer * std::cos(a1);
    float y1o = ring_y + ring_outer * std::sin(a1);

    float x0i = ring_inner * std::cos(a0);
    float y0i = ring_y + ring_inner * std::sin(a0);
    float x1i = ring_inner * std::cos(a1);
    float y1i = ring_y + ring_inner * std::sin(a1);

    unsigned int b = verts.size();
    verts.push_back({{x0o, y0o, 0.0F}, {n.x(), n.y(), n.z()}, {0.0F, 1.0F}});
    verts.push_back({{x1o, y1o, 0.0F}, {n.x(), n.y(), n.z()}, {1.0F, 1.0F}});
    verts.push_back({{x1i, y1i, 0.0F}, {n.x(), n.y(), n.z()}, {1.0F, 0.0F}});
    verts.push_back({{x0i, y0i, 0.0F}, {n.x(), n.y(), n.z()}, {0.0F, 0.0F}});

    idx.insert(idx.end(), {b + 0, b + 1, b + 2, b + 2, b + 3, b + 0});
  }

  float const shank_half = shank_width * 0.5F;
  float const shank_top = ring_y - ring_inner * 0.9F;
  float const shank_bottom = -anchor_height * 0.18F;

  unsigned int shank_base = verts.size();
  verts.push_back(
      {{-shank_half, shank_top, 0.0F}, {n.x(), n.y(), n.z()}, {0.0F, 1.0F}});
  verts.push_back(
      {{shank_half, shank_top, 0.0F}, {n.x(), n.y(), n.z()}, {1.0F, 1.0F}});
  verts.push_back(
      {{shank_half, shank_bottom, 0.0F}, {n.x(), n.y(), n.z()}, {1.0F, 0.0F}});
  verts.push_back(
      {{-shank_half, shank_bottom, 0.0F}, {n.x(), n.y(), n.z()}, {0.0F, 0.0F}});

  idx.insert(idx.end(), {shank_base + 0, shank_base + 1, shank_base + 2,
                         shank_base + 2, shank_base + 3, shank_base + 0});

  float const cross_y = shank_bottom;
  float const cross_half_w = cross_width * 0.5F;
  float const cross_half_h = cross_height * 0.5F;

  unsigned int cross_base = verts.size();
  verts.push_back({{-cross_half_w, cross_y - cross_half_h, 0.0F},
                   {n.x(), n.y(), n.z()},
                   {0.0F, 0.5F}});
  verts.push_back({{cross_half_w, cross_y - cross_half_h, 0.0F},
                   {n.x(), n.y(), n.z()},
                   {1.0F, 0.5F}});
  verts.push_back({{cross_half_w, cross_y + cross_half_h, 0.0F},
                   {n.x(), n.y(), n.z()},
                   {1.0F, 0.5F}});
  verts.push_back({{-cross_half_w, cross_y + cross_half_h, 0.0F},
                   {n.x(), n.y(), n.z()},
                   {0.0F, 0.5F}});

  idx.insert(idx.end(), {cross_base + 0, cross_base + 1, cross_base + 2,
                         cross_base + 2, cross_base + 3, cross_base + 0});

  float const fluke_y = cross_y - fluke_drop;
  float const fluke_tip_y = -anchor_height * 0.58F;

  auto add_tri = [&](QVector2D a, QVector2D b, QVector2D c) {
    unsigned int ia = add(a.x(), a.y(), 0.0F, 0.0F);
    unsigned int ib = add(b.x(), b.y(), 1.0F, 0.5F);
    unsigned int ic = add(c.x(), c.y(), 0.0F, 1.0F);
    idx.insert(idx.end(), {ia, ib, ic});
  };

  add_tri({-cross_half_w * 0.9F, cross_y}, {-fluke_span * 0.6F, fluke_y},
          {-fluke_span, fluke_tip_y});

  add_tri({cross_half_w * 0.9F, cross_y}, {fluke_span * 0.6F, fluke_y},
          {fluke_span, fluke_tip_y});

  add_tri({-shank_half * 0.8F, cross_y}, {shank_half * 0.8F, cross_y},
          {0.0F, fluke_tip_y});

  return std::make_unique<Mesh>(verts, idx);
}

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
  constexpr float arrow_end_ratio = 0.85F;

  QVector3D const n(0, 0, 1);

  for (int arrow = 0; arrow < 2; ++arrow) {

    float const start_angle = arrow == 0 ? 0.0F : k_pi;
    float const end_angle =
        arrow == 0 ? k_pi * arrow_end_ratio : k_pi * (1.0F + arrow_end_ratio);
    int const segments = circle_segments / 2;

    for (int i = 0; i < segments; ++i) {
      float const t1 = i / float(segments);
      float const t2 = (i + 1) / float(segments);
      float const angle1 = start_angle + (end_angle - start_angle) * t1;
      float const angle2 = start_angle + (end_angle - start_angle) * t2;

      float const x1_inner =
          (circle_radius - arrow_width * 0.5F) * std::cos(angle1);
      float const y1_inner =
          (circle_radius - arrow_width * 0.5F) * std::sin(angle1);
      float const x1_outer =
          (circle_radius + arrow_width * 0.5F) * std::cos(angle1);
      float const y1_outer =
          (circle_radius + arrow_width * 0.5F) * std::sin(angle1);

      float const x2_inner =
          (circle_radius - arrow_width * 0.5F) * std::cos(angle2);
      float const y2_inner =
          (circle_radius - arrow_width * 0.5F) * std::sin(angle2);
      float const x2_outer =
          (circle_radius + arrow_width * 0.5F) * std::cos(angle2);
      float const y2_outer =
          (circle_radius + arrow_width * 0.5F) * std::sin(angle2);

      size_t const base = verts.size();
      verts.push_back(
          {{x1_inner, y1_inner, 0.0F}, {n.x(), n.y(), n.z()}, {0.0F, 0.0F}});
      verts.push_back(
          {{x1_outer, y1_outer, 0.0F}, {n.x(), n.y(), n.z()}, {1.0F, 0.0F}});
      verts.push_back(
          {{x2_outer, y2_outer, 0.0F}, {n.x(), n.y(), n.z()}, {1.0F, 1.0F}});
      verts.push_back(
          {{x2_inner, y2_inner, 0.0F}, {n.x(), n.y(), n.z()}, {0.0F, 1.0F}});

      idx.push_back(base + 0);
      idx.push_back(base + 1);
      idx.push_back(base + 2);
      idx.push_back(base + 2);
      idx.push_back(base + 3);
      idx.push_back(base + 0);
    }

    float const arrow_angle = end_angle;
    float const arrow_x = circle_radius * std::cos(arrow_angle);
    float const arrow_y = circle_radius * std::sin(arrow_angle);

    float const tangent_x = -std::sin(arrow_angle);
    float const tangent_y = std::cos(arrow_angle);

    float const normal_x = std::cos(arrow_angle);
    float const normal_y = std::sin(arrow_angle);

    float const tip_x = arrow_x + tangent_x * arrow_head_length;
    float const tip_y = arrow_y + tangent_y * arrow_head_length;

    float const base1_x = arrow_x + normal_x * arrow_head_width * 0.5F;
    float const base1_y = arrow_y + normal_y * arrow_head_width * 0.5F;
    float const base2_x = arrow_x - normal_x * arrow_head_width * 0.5F;
    float const base2_y = arrow_y - normal_y * arrow_head_width * 0.5F;

    size_t const arrow_base = verts.size();
    verts.push_back(
        {{tip_x, tip_y, 0.0F}, {n.x(), n.y(), n.z()}, {0.5F, 1.0F}});
    verts.push_back(
        {{base1_x, base1_y, 0.0F}, {n.x(), n.y(), n.z()}, {1.0F, 0.0F}});
    verts.push_back(
        {{base2_x, base2_y, 0.0F}, {n.x(), n.y(), n.z()}, {0.0F, 0.0F}});

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
