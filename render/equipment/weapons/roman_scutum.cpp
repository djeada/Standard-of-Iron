#include "roman_scutum.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

#include "../../geom/transforms.h"
#include "../../gl/mesh.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/humanoid_renderer_base.h"
#include "../../humanoid/humanoid_spec.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../humanoid/skeleton.h"
#include "../../humanoid/style_palette.h"
#include "../../render_archetype.h"
#include "../attachment_builder.h"
#include "../equipment_submit.h"
#include "shield_anchor.h"

namespace Render::GL {

using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;
using Render::GL::Humanoid::saturate_color;

namespace {

constexpr std::uint8_t k_red_face = 0;
constexpr std::uint8_t k_wood_back = 1;
constexpr std::uint8_t k_brass_spine = 2;
constexpr std::uint8_t k_brass_boss_plate = 3;
constexpr std::uint8_t k_iron_boss = 4;
constexpr std::uint8_t k_rawhide_rim = 5;
constexpr std::uint8_t k_leather_grip = 6;

constexpr float k_half_width = 0.36F;
constexpr float k_half_height = 0.55F;
constexpr float k_corner_radius = 0.12F;
constexpr float k_curve_depth = 0.082F;
constexpr float k_board_thickness = 0.034F;
constexpr float k_boss_plate_half_width = 0.105F;
constexpr float k_boss_plate_half_height = 0.18F;
constexpr float k_boss_plate_half_depth = 0.010F;
constexpr float k_spine_half_width = 0.018F;
constexpr float k_spine_half_height = 0.43F;
constexpr float k_spine_half_depth = 0.008F;
constexpr float k_boss_radius = 0.073F;
constexpr int k_face_columns = 16;
constexpr int k_face_rows = 20;
constexpr int k_outline_samples = 16;

struct OutlinePoint {
  float x{0.0F};
  float y{0.0F};
};

auto lerp(float a, float b, float t) -> float {
  return a + (b - a) * t;
}

auto scutum_half_width_at(float y) -> float {
  float const abs_y = std::abs(y);
  float const straight_half_height = k_half_height - k_corner_radius;
  if (abs_y <= straight_half_height) {
    return k_half_width;
  }

  float const core_half_width = k_half_width - k_corner_radius;
  float const dy = std::min(abs_y - straight_half_height, k_corner_radius);
  float const dx =
      std::sqrt(std::max(k_corner_radius * k_corner_radius - dy * dy, 0.0F));
  return core_half_width + dx;
}

auto scutum_front_depth(float x, float row_half_width) -> float {
  if (row_half_width <= 1e-4F) {
    return 0.0F;
  }
  float const normalized_x = std::clamp(x / row_half_width, -1.0F, 1.0F);
  return k_curve_depth * (1.0F - normalized_x * normalized_x);
}

auto scutum_face_point(float x, float y, float z_offset = 0.0F) -> QVector3D {
  float const row_half_width = scutum_half_width_at(y);
  float const clamped_x = std::clamp(x, -row_half_width, row_half_width);
  return {clamped_x, y, scutum_front_depth(clamped_x, row_half_width) + z_offset};
}

auto scutum_face_sample(float u, float v, float z_offset = 0.0F) -> QVector3D {
  float const y = lerp(-k_half_height, k_half_height, v);
  float const row_half_width = scutum_half_width_at(y);
  float const x = lerp(-row_half_width, row_half_width, u);
  return scutum_face_point(x, y, z_offset);
}

auto scutum_face_normal(float u, float v, bool back) -> QVector3D {
  float const du = 1.0F / static_cast<float>(k_face_columns);
  float const dv = 1.0F / static_cast<float>(k_face_rows);
  float const z_offset = back ? -k_board_thickness : 0.0F;

  auto sample = [&](float sample_u, float sample_v) {
    return scutum_face_sample(
        std::clamp(sample_u, 0.0F, 1.0F), std::clamp(sample_v, 0.0F, 1.0F), z_offset);
  };

  QVector3D const tangent_u = sample(u + du, v) - sample(u - du, v);
  QVector3D const tangent_v = sample(u, v + dv) - sample(u, v - dv);
  QVector3D normal = QVector3D::crossProduct(tangent_u, tangent_v);
  if (back) {
    normal = -normal;
  }
  if (normal.lengthSquared() <= 1e-6F) {
    return back ? QVector3D(0.0F, 0.0F, -1.0F) : QVector3D(0.0F, 0.0F, 1.0F);
  }
  normal.normalize();
  return normal;
}

auto create_scutum_face_mesh(bool back) -> std::unique_ptr<Mesh> {
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;
  vertices.reserve((k_face_columns + 1) * (k_face_rows + 1));
  indices.reserve(k_face_columns * k_face_rows * 6);

  float const z_offset = back ? -k_board_thickness : 0.0F;
  for (int row = 0; row <= k_face_rows; ++row) {
    float const v = static_cast<float>(row) / static_cast<float>(k_face_rows);
    for (int col = 0; col <= k_face_columns; ++col) {
      float const u = static_cast<float>(col) / static_cast<float>(k_face_columns);
      QVector3D const p = scutum_face_sample(u, v, z_offset);
      QVector3D const n = scutum_face_normal(u, v, back);
      vertices.push_back({{p.x(), p.y(), p.z()}, {n.x(), n.y(), n.z()}, {u, v}});
    }
  }

  int const stride = k_face_columns + 1;
  for (int row = 0; row < k_face_rows; ++row) {
    for (int col = 0; col < k_face_columns; ++col) {
      auto const a = static_cast<unsigned int>(row * stride + col);
      unsigned int const b = a + 1U;
      auto const c = static_cast<unsigned int>((row + 1) * stride + col + 1);
      auto const d = static_cast<unsigned int>((row + 1) * stride + col);
      if (back) {
        indices.push_back(a);
        indices.push_back(d);
        indices.push_back(c);
        indices.push_back(c);
        indices.push_back(b);
        indices.push_back(a);
      } else {
        indices.push_back(a);
        indices.push_back(b);
        indices.push_back(c);
        indices.push_back(c);
        indices.push_back(d);
        indices.push_back(a);
      }
    }
  }

  return std::make_unique<Mesh>(vertices, indices);
}

auto build_scutum_outline() -> std::vector<OutlinePoint> {
  std::vector<OutlinePoint> outline;
  outline.reserve((k_outline_samples + 1) * 2);

  for (int i = 0; i <= k_outline_samples; ++i) {
    float const t = static_cast<float>(i) / static_cast<float>(k_outline_samples);
    float const y = lerp(-k_half_height, k_half_height, t);
    outline.push_back({scutum_half_width_at(y), y});
  }
  for (int i = k_outline_samples; i >= 0; --i) {
    float const t = static_cast<float>(i) / static_cast<float>(k_outline_samples);
    float const y = lerp(-k_half_height, k_half_height, t);
    outline.push_back({-scutum_half_width_at(y), y});
  }
  return outline;
}

auto create_scutum_edge_mesh() -> std::unique_ptr<Mesh> {
  std::vector<OutlinePoint> const outline = build_scutum_outline();
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;
  vertices.reserve(outline.size() * 2);
  indices.reserve(outline.size() * 6);

  for (std::size_t i = 0; i < outline.size(); ++i) {
    OutlinePoint const& prev = outline[(i + outline.size() - 1U) % outline.size()];
    OutlinePoint const& curr = outline[i];
    OutlinePoint const& next = outline[(i + 1U) % outline.size()];

    QVector3D const tangent(next.x - prev.x, next.y - prev.y, 0.0F);
    QVector3D normal(tangent.y(), -tangent.x(), 0.0F);
    if (normal.lengthSquared() <= 1e-6F) {
      normal = QVector3D(curr.x >= 0.0F ? 1.0F : -1.0F, 0.0F, 0.0F);
    } else {
      normal.normalize();
    }

    float const u = static_cast<float>(i) / static_cast<float>(outline.size() - 1U);
    vertices.push_back(
        {{curr.x, curr.y, 0.0F}, {normal.x(), normal.y(), normal.z()}, {u, 0.0F}});
    vertices.push_back({{curr.x, curr.y, -k_board_thickness},
                        {normal.x(), normal.y(), normal.z()},
                        {u, 1.0F}});
  }

  for (std::size_t i = 0; i < outline.size(); ++i) {
    std::size_t const next = (i + 1U) % outline.size();
    auto const front_a = static_cast<unsigned int>(i * 2U);
    unsigned int const back_a = front_a + 1U;
    auto const front_b = static_cast<unsigned int>(next * 2U);
    unsigned int const back_b = front_b + 1U;

    indices.push_back(front_a);
    indices.push_back(back_a);
    indices.push_back(back_b);
    indices.push_back(back_b);
    indices.push_back(front_b);
    indices.push_back(front_a);
  }

  return std::make_unique<Mesh>(vertices, indices);
}

auto roman_scutum_front_mesh() -> Mesh* {
  static std::unique_ptr<Mesh> const mesh = create_scutum_face_mesh(false);
  return mesh.get();
}

auto roman_scutum_back_mesh() -> Mesh* {
  static std::unique_ptr<Mesh> const mesh = create_scutum_face_mesh(true);
  return mesh.get();
}

auto roman_scutum_edge_mesh() -> Mesh* {
  static std::unique_ptr<Mesh> const mesh = create_scutum_edge_mesh();
  return mesh.get();
}

} // namespace

auto roman_scutum_archetype() -> const RenderArchetype& {
  static const RenderArchetype k_archetype = []() {
    RenderArchetypeBuilder builder{"roman_scutum"};

    builder.add_palette_mesh(roman_scutum_front_mesh(), QMatrix4x4{}, k_red_face);
    builder.add_palette_mesh(roman_scutum_back_mesh(), QMatrix4x4{}, k_wood_back);
    builder.add_palette_mesh(roman_scutum_edge_mesh(), QMatrix4x4{}, k_rawhide_rim);

    QMatrix4x4 spine_m;
    spine_m.translate(0.0F, 0.0F, k_curve_depth * 0.52F);
    spine_m.scale(k_spine_half_width, k_spine_half_height, k_spine_half_depth);
    builder.add_palette_mesh(get_unit_cube(), spine_m, k_brass_spine);

    QMatrix4x4 boss_plate_m;
    boss_plate_m.translate(0.0F, 0.0F, k_curve_depth + 0.010F);
    boss_plate_m.scale(
        k_boss_plate_half_width, k_boss_plate_half_height, k_boss_plate_half_depth);
    builder.add_palette_mesh(get_unit_cube(), boss_plate_m, k_brass_boss_plate);

    builder.add_palette_mesh(
        get_unit_sphere(),
        sphere_at(QVector3D(0.0F, 0.0F, k_curve_depth + 0.030F), k_boss_radius),
        k_iron_boss);

    builder.add_palette_mesh(
        get_unit_cylinder(),
        cylinder_between(QVector3D(-0.055F, 0.0F, -k_board_thickness - 0.020F),
                         QVector3D(0.055F, 0.0F, -k_board_thickness - 0.020F),
                         0.012F),
        k_leather_grip);

    return std::move(builder).build();
  }();
  return k_archetype;
}

namespace {

auto shield_basis_transform(const QMatrix4x4& parent,
                            const AttachmentFrame& shield_frame) -> QMatrix4x4 {
  return parent * attachment_frame_transform(shield_frame);
}

auto scutum_local_pose() -> QMatrix4x4 {
  QMatrix4x4 pose;
  pose.rotate(90.0F, 0.0F, 1.0F, 0.0F);
  pose.translate(0.0F, 0.0F, 0.07F);
  return pose;
}

} // namespace

void RomanScutumRenderer::render(const DrawContext& ctx,
                                 const BodyFrames& frames,
                                 const HumanoidPalette& palette,
                                 const HumanoidAnimationContext& anim,
                                 EquipmentBatch& batch) {
  submit({}, ctx, frames, palette, anim, batch);
}

void RomanScutumRenderer::submit(const RomanScutumConfig&,
                                 const DrawContext& ctx,
                                 const BodyFrames& frames,
                                 const HumanoidPalette& palette,
                                 const HumanoidAnimationContext&,
                                 EquipmentBatch& batch) {
  if (frames.hand_l.radius <= 0.0F) {
    return;
  }
  AttachmentFrame const shield_frame = resolve_left_shield_frame(frames);
  QMatrix4x4 pose_adjustment;
  if (frames.shield_l.radius > 0.0F) {
    pose_adjustment = bind_left_shield_pose_calibration();
  }

  QVector3D const shield_red = saturate_color(QVector3D(0.72F, 0.13F, 0.10F));
  QVector3D const wood_back = saturate_color(QVector3D(0.46F, 0.38F, 0.29F));
  QVector3D const brass_dark = saturate_color(QVector3D(0.63F, 0.53F, 0.24F));
  QVector3D const brass_light = saturate_color(QVector3D(0.74F, 0.63F, 0.32F));
  QVector3D const iron_color = saturate_color(QVector3D(0.58F, 0.58F, 0.60F));
  QVector3D const rawhide_color = saturate_color(QVector3D(0.64F, 0.57F, 0.44F));
  QVector3D const grip_color =
      saturate_color(palette.leather * QVector3D(0.92F, 0.72F, 0.54F));

  std::array<QVector3D, k_roman_scutum_role_count> const palette_slots{
      shield_red,
      wood_back,
      brass_dark,
      brass_light,
      iron_color,
      rawhide_color,
      grip_color,
  };

  append_equipment_archetype(batch,
                             roman_scutum_archetype(),
                             shield_basis_transform(ctx.model, shield_frame) *
                                 pose_adjustment * scutum_local_pose(),
                             palette_slots);
}

auto roman_scutum_fill_role_colors(const HumanoidPalette& palette,
                                   QVector3D* out,
                                   std::size_t max) -> std::uint32_t {
  if (max < k_roman_scutum_role_count) {
    return 0;
  }
  QVector3D const shield_red = saturate_color(QVector3D(0.72F, 0.13F, 0.10F));
  QVector3D const wood_back = saturate_color(QVector3D(0.46F, 0.38F, 0.29F));
  QVector3D const brass_dark = saturate_color(QVector3D(0.63F, 0.53F, 0.24F));
  QVector3D const brass_light = saturate_color(QVector3D(0.74F, 0.63F, 0.32F));
  QVector3D const iron_color = saturate_color(QVector3D(0.58F, 0.58F, 0.60F));
  QVector3D const rawhide_color = saturate_color(QVector3D(0.64F, 0.57F, 0.44F));
  QVector3D const grip_color =
      saturate_color(palette.leather * QVector3D(0.92F, 0.72F, 0.54F));
  out[k_red_face] = shield_red;
  out[k_wood_back] = wood_back;
  out[k_brass_spine] = brass_dark;
  out[k_brass_boss_plate] = brass_light;
  out[k_iron_boss] = iron_color;
  out[k_rawhide_rim] = rawhide_color;
  out[k_leather_grip] = grip_color;
  return k_roman_scutum_role_count;
}

auto roman_scutum_make_static_attachment(std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec {
  constexpr auto k_bone = Render::Humanoid::HumanoidBone::HandL;
  QMatrix4x4 const bind_bone =
      Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(k_bone)];
  auto const& bind_shield = Render::Humanoid::humanoid_bind_body_frames().shield_l;
  QMatrix4x4 const bind_socket = attachment_frame_transform(bind_shield);
  auto spec = Render::Equipment::build_socket_static_attachment({
      .archetype = &roman_scutum_archetype(),
      .socket_bone_index = static_cast<std::uint16_t>(k_bone),
      .bind_bone_transform = bind_bone,
      .bind_socket_transform = bind_socket,
      .mesh_from_socket = bind_left_shield_pose_calibration() * scutum_local_pose(),
  });
  spec.palette_role_remap[k_red_face] = base_role_byte;
  spec.palette_role_remap[k_wood_back] = static_cast<std::uint8_t>(base_role_byte + 1U);
  spec.palette_role_remap[k_brass_spine] =
      static_cast<std::uint8_t>(base_role_byte + 2U);
  spec.palette_role_remap[k_brass_boss_plate] =
      static_cast<std::uint8_t>(base_role_byte + 3U);
  spec.palette_role_remap[k_iron_boss] = static_cast<std::uint8_t>(base_role_byte + 4U);
  spec.palette_role_remap[k_rawhide_rim] =
      static_cast<std::uint8_t>(base_role_byte + 5U);
  spec.palette_role_remap[k_leather_grip] =
      static_cast<std::uint8_t>(base_role_byte + 6U);
  return spec;
}

} // namespace Render::GL
