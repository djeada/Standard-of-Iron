#include "cloak_renderer.h"

#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_spec.h"
#include "../../static_attachment_spec.h"
#include "../attachment_builder.h"
#include "../equipment_submit.h"
#include "torso_local_archetype_utils.h"

namespace Render::GL {

namespace {

enum CloakPaletteSlot : std::uint8_t {
  k_cloak_cloth_slot = 0U,
  k_cloak_trim_slot = 1U,
};

constexpr float k_pi = std::numbers::pi_v<float>;

struct ClothSurface {
  std::vector<QVector3D> positions;
  std::vector<QVector2D> uvs;
  std::vector<unsigned int> indices;
};

struct CloakPlacement {
  QMatrix4x4 shoulder_model;
  QMatrix4x4 drape_model;
  QMatrix4x4 clasp_model;
  bool has_clasp = false;
};

auto safe_normalized(const QVector3D& value, const QVector3D& fallback) -> QVector3D {
  if (value.lengthSquared() <= 1e-8F) {
    return fallback;
  }
  return value.normalized();
}

void append_grid_indices(std::vector<unsigned int>& indices,
                         int width_segments,
                         int height_segments) {
  int const row_stride = width_segments + 1;
  for (int y = 0; y < height_segments; ++y) {
    for (int x = 0; x < width_segments; ++x) {
      auto const top_left = static_cast<unsigned int>(y * row_stride + x);
      unsigned int const top_right = top_left + 1U;
      auto const bottom_left = static_cast<unsigned int>((y + 1) * row_stride + x);
      unsigned int const bottom_right = bottom_left + 1U;

      indices.push_back(top_left);
      indices.push_back(bottom_left);
      indices.push_back(top_right);

      indices.push_back(top_right);
      indices.push_back(bottom_left);
      indices.push_back(bottom_right);
    }
  }
}

auto build_normals(const ClothSurface& surface) -> std::vector<QVector3D> {
  std::vector<QVector3D> normals(surface.positions.size(), QVector3D(0.0F, 0.0F, 0.0F));
  for (std::size_t i = 0; i + 2 < surface.indices.size(); i += 3) {
    unsigned int const ia = surface.indices[i];
    unsigned int const ib = surface.indices[i + 1];
    unsigned int const ic = surface.indices[i + 2];
    if (ia >= surface.positions.size() || ib >= surface.positions.size() ||
        ic >= surface.positions.size()) {
      continue;
    }
    QVector3D const edge_ab = surface.positions[ib] - surface.positions[ia];
    QVector3D const edge_ac = surface.positions[ic] - surface.positions[ia];
    QVector3D const face_normal = QVector3D::crossProduct(edge_ab, edge_ac);
    normals[ia] += face_normal;
    normals[ib] += face_normal;
    normals[ic] += face_normal;
  }

  for (auto& normal : normals) {
    normal = safe_normalized(normal, QVector3D(0.0F, 1.0F, 0.0F));
  }
  return normals;
}

auto make_double_sided_cloth_mesh(const ClothSurface& surface)
    -> std::unique_ptr<Mesh> {
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;

  if (surface.positions.empty() || surface.positions.size() != surface.uvs.size() ||
      surface.indices.empty()) {
    return nullptr;
  }

  std::vector<QVector3D> const normals = build_normals(surface);
  vertices.reserve(surface.positions.size() * 2U);
  indices.reserve(surface.indices.size() * 2U);

  for (std::size_t i = 0; i < surface.positions.size(); ++i) {
    QVector3D const& pos = surface.positions[i];
    QVector3D const& normal = normals[i];
    QVector2D const& uv = surface.uvs[i];
    vertices.push_back({{pos.x(), pos.y(), pos.z()},
                        {normal.x(), normal.y(), normal.z()},
                        {uv.x(), uv.y()}});
  }

  indices.insert(indices.end(), surface.indices.begin(), surface.indices.end());

  auto const backface_base = static_cast<unsigned int>(surface.positions.size());
  for (std::size_t i = 0; i < surface.positions.size(); ++i) {
    QVector3D const& pos = surface.positions[i];
    QVector3D const back_normal = -normals[i];
    QVector2D const& uv = surface.uvs[i];
    vertices.push_back({{pos.x(), pos.y(), pos.z()},
                        {back_normal.x(), back_normal.y(), back_normal.z()},
                        {uv.x(), uv.y()}});
  }

  for (std::size_t i = 0; i + 2 < surface.indices.size(); i += 3) {
    indices.push_back(backface_base + surface.indices[i]);
    indices.push_back(backface_base + surface.indices[i + 2]);
    indices.push_back(backface_base + surface.indices[i + 1]);
  }

  return std::make_unique<Mesh>(vertices, indices);
}

auto make_cloak_back_surface() -> ClothSurface {
  ClothSurface surface;
  constexpr int k_width_segments = 12;
  constexpr int k_length_segments = 18;
  surface.positions.reserve(
      static_cast<std::size_t>((k_width_segments + 1) * (k_length_segments + 1)));
  surface.uvs.reserve(surface.positions.capacity());

  for (int y = 0; y <= k_length_segments; ++y) {
    float const v = static_cast<float>(y) / static_cast<float>(k_length_segments);
    float const hem_curve = 0.04F * v * v;
    float const half_width = 0.52F - 0.10F * std::pow(v, 1.10F);
    for (int x = 0; x <= k_width_segments; ++x) {
      float const u = static_cast<float>(x) / static_cast<float>(k_width_segments);
      float const x_norm = u * 2.0F - 1.0F;
      float const x_abs = std::abs(x_norm);
      float const edge_falloff = std::max(0.0F, 1.0F - std::pow(x_abs, 1.45F));
      float const fold_band = std::max(0.0F, std::cos(x_abs * k_pi * 3.0F - 0.35F));
      float const symmetric_folds =
          fold_band * edge_falloff * (0.008F + 0.018F * v * v);
      float const center_swell =
          std::max(0.0F, 1.0F - x_abs * 1.7F) * (0.010F + 0.020F * v);
      float const back_bulge =
          edge_falloff * (0.015F + 0.060F * std::sin(v * k_pi) + 0.050F * v);
      float const depth = back_bulge + symmetric_folds + center_swell;
      float const length = v - 0.5F + hem_curve * edge_falloff;
      surface.positions.emplace_back(x_norm * half_width, depth, length);
      surface.uvs.emplace_back(u, v);
    }
  }

  append_grid_indices(surface.indices, k_width_segments, k_length_segments);
  return surface;
}

auto make_cloak_shoulder_surface() -> ClothSurface {
  ClothSurface surface;
  constexpr int k_width_segments = 10;
  constexpr int k_depth_segments = 6;
  surface.positions.reserve(
      static_cast<std::size_t>((k_width_segments + 1) * (k_depth_segments + 1)));
  surface.uvs.reserve(surface.positions.capacity());

  for (int z = 0; z <= k_depth_segments; ++z) {
    float const v = static_cast<float>(z) / static_cast<float>(k_depth_segments);
    float const depth = v * 1.14F - 0.44F;
    for (int x = 0; x <= k_width_segments; ++x) {
      float const u = static_cast<float>(x) / static_cast<float>(k_width_segments);
      float const x_norm = u * 2.0F - 1.0F;
      float const x_abs = std::abs(x_norm);
      float const shoulder_drop = -(0.014F + 0.060F * v) * (1.0F - x_abs * 0.76F);
      float const back_drape =
          -std::max(0.0F, v - 0.28F) * (0.020F + 0.040F * (1.0F - x_abs));
      float const collar_rise =
          std::max(0.0F, 0.22F - v) * 0.020F * (1.0F - x_abs * 0.60F);
      surface.positions.emplace_back(
          x_norm * 0.5F, shoulder_drop + back_drape + collar_rise, depth);
      surface.uvs.emplace_back(u, v);
    }
  }

  append_grid_indices(surface.indices, k_width_segments, k_depth_segments);
  return surface;
}

auto shared_cloak_meshes() -> CloakMeshes {
  static std::unique_ptr<Mesh> const back_mesh =
      make_double_sided_cloth_mesh(make_cloak_back_surface());
  static std::unique_ptr<Mesh> const shoulder_mesh =
      make_double_sided_cloth_mesh(make_cloak_shoulder_surface());
  return {back_mesh.get(), shoulder_mesh.get()};
}

auto make_cloak_placement(const CloakConfig& config,
                          const TorsoLocalFrame& torso_local,
                          const AttachmentFrame& torso,
                          const AttachmentFrame& shoulder_l,
                          const AttachmentFrame& shoulder_r) -> CloakPlacement {
  QVector3D const up = safe_attachment_axis(torso.up, QVector3D(0.0F, 1.0F, 0.0F));
  QVector3D const forward =
      safe_attachment_axis(torso.forward, QVector3D(0.0F, 0.0F, 1.0F));
  QVector3D const back = -forward;

  float const torso_r = torso.radius;

  float shoulder_span = (shoulder_r.origin - shoulder_l.origin).length();
  if (shoulder_span < 1e-4F) {
    shoulder_span = torso_r * 3.0F;
  }
  QVector3D const shoulder_mid = (shoulder_l.origin + shoulder_r.origin) * 0.5F;

  CloakPlacement placement;

  float const shoulder_width = shoulder_span * 1.48F * config.width_scale;
  float const shoulder_depth = torso_r * 1.46F;
  QVector3D const shoulder_anchor = shoulder_mid +
                                    up * (torso_r * config.shoulder_anchor_up) +
                                    back * (torso_r * 0.16F);

  placement.shoulder_model.translate(torso_local.point(shoulder_anchor));
  placement.shoulder_model.scale(shoulder_width, 1.0F, shoulder_depth);

  float const drape_width = shoulder_span * 1.24F * config.width_scale;
  float const drape_length = torso_r * 4.2F * config.length_scale;
  QVector3D const drape_anchor = shoulder_mid +
                                 up * (torso_r * config.drape_anchor_up) +
                                 back * (torso_r * config.drape_anchor_back);

  placement.drape_model.translate(torso_local.point(drape_anchor));

  QMatrix4x4 drape_orient;
  drape_orient.setColumn(0, QVector4D(1.0F, 0.0F, 0.0F, 0.0F));
  drape_orient.setColumn(1, QVector4D(0.0F, 0.0F, -1.0F, 0.0F));
  drape_orient.setColumn(2, QVector4D(0.0F, -1.0F, 0.0F, 0.0F));
  drape_orient.setColumn(3, QVector4D(0.0F, 0.0F, 0.0F, 1.0F));
  placement.drape_model = placement.drape_model * drape_orient;
  placement.drape_model.translate(0.0F, 0.0F, drape_length * 0.5F);

  QMatrix4x4 flare;
  flare.setToIdentity();
  flare(0, 2) = 0.10F;
  placement.drape_model = placement.drape_model * flare;
  placement.drape_model.scale(drape_width, 1.0F, drape_length);

  if (config.show_clasp) {
    QVector3D const clasp_pos = shoulder_mid + up * (torso_r * config.clasp_anchor_up) +
                                forward * (torso_r * config.clasp_anchor_forward);
    placement.clasp_model =
        local_scale_model(torso_local.point(clasp_pos),
                          QVector3D(torso_r * 0.11F, torso_r * 0.11F, torso_r * 0.11F));
    placement.has_clasp = true;
  }

  return placement;
}

auto cloak_archetype(const CloakConfig& config,
                     const CloakMeshes& meshes,
                     const QMatrix4x4& shoulder_model,
                     const QMatrix4x4& drape_model,
                     const QMatrix4x4* clasp_model) -> const RenderArchetype& {
  struct CachedArchetype {
    std::string key;
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  std::string key = "cloak_";
  key += std::to_string(reinterpret_cast<std::uintptr_t>(meshes.shoulder));
  key.push_back('_');
  key += std::to_string(reinterpret_cast<std::uintptr_t>(meshes.back));
  key.push_back('_');
  append_quantized_key(key, config.back_material_id);
  append_quantized_key(key, config.shoulder_material_id);
  append_quantized_key(key, shoulder_model);
  append_quantized_key(key, drape_model);
  append_quantized_key(key, clasp_model != nullptr ? 1 : 0);
  if (clasp_model != nullptr) {
    append_quantized_key(key, *clasp_model);
  }

  for (const auto& entry : cache) {
    if (entry.key == key) {
      return entry.archetype;
    }
  }

  RenderArchetypeBuilder builder{key};
  builder.add_palette_mesh(meshes.shoulder,
                           shoulder_model,
                           k_cloak_cloth_slot,
                           nullptr,
                           1.0F,
                           config.shoulder_material_id);
  builder.add_palette_mesh(meshes.back,
                           drape_model,
                           k_cloak_cloth_slot,
                           nullptr,
                           1.0F,
                           config.back_material_id);
  if (clasp_model != nullptr) {
    builder.add_palette_mesh(
        get_unit_sphere(), *clasp_model, k_cloak_trim_slot, nullptr, 1.0F, 1);
  }

  cache.push_back({key, std::move(builder).build()});
  return cache.back().archetype;
}

} // namespace

CloakRenderer::CloakRenderer(const CloakConfig& config)
    : m_config(config) {
}

auto CloakRenderer::meshes() const noexcept -> CloakMeshes {
  return shared_cloak_meshes();
}

void CloakRenderer::set_config(const CloakConfig& config) {
  m_config = config;
}

void CloakRenderer::render(const DrawContext& ctx,
                           const BodyFrames& frames,
                           const HumanoidPalette& palette,
                           const HumanoidAnimationContext& anim,
                           EquipmentBatch& batch) {
  submit(m_config, shared_cloak_meshes(), ctx, frames, palette, anim, batch);
}

void CloakRenderer::submit(const CloakConfig& config,
                           const CloakMeshes& meshes,
                           const DrawContext& ctx,
                           const BodyFrames& frames,
                           const HumanoidPalette& palette,
                           const HumanoidAnimationContext& anim,
                           EquipmentBatch& batch) {
  (void)anim;

  const AttachmentFrame& torso = frames.torso;
  const AttachmentFrame& shoulder_l = frames.shoulder_l;
  const AttachmentFrame& shoulder_r = frames.shoulder_r;

  if (torso.radius <= 0.0F || meshes.back == nullptr || meshes.shoulder == nullptr) {
    return;
  }

  QVector3D const cloak_color = palette.cloth;
  QVector3D const trim_color = palette.metal;

  TorsoLocalFrame const torso_local = make_torso_local_frame(ctx.model, torso);
  CloakPlacement const placement =
      make_cloak_placement(config, torso_local, torso, shoulder_l, shoulder_r);

  std::array<QVector3D, 2> palette_slots{cloak_color, trim_color};

  if (placement.has_clasp) {
    append_equipment_archetype(batch,
                               cloak_archetype(config,
                                               meshes,
                                               placement.shoulder_model,
                                               placement.drape_model,
                                               &placement.clasp_model),
                               torso_local.world,
                               palette_slots);
    return;
  }

  append_equipment_archetype(
      batch,
      cloak_archetype(
          config, meshes, placement.shoulder_model, placement.drape_model, nullptr),
      torso_local.world,
      palette_slots);
}

auto cloak_fill_role_colors_with_primary(const QVector3D& primary_color,
                                         const HumanoidPalette& palette,
                                         QVector3D* out,
                                         std::size_t max) -> std::uint32_t {
  if (max < k_cloak_role_count) {
    return 0U;
  }
  out[0] = primary_color;
  out[1] = palette.metal;
  return k_cloak_role_count;
}

auto cloak_make_static_attachment(const CloakConfig& config,
                                  const CloakMeshes& meshes,
                                  std::uint16_t torso_socket_bone_index,
                                  std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec {
  const auto& bind_frames = Render::Humanoid::humanoid_bind_body_frames();
  const AttachmentFrame& torso = bind_frames.torso;
  const AttachmentFrame& shoulder_l = bind_frames.shoulder_l;
  const AttachmentFrame& shoulder_r = bind_frames.shoulder_r;

  TorsoLocalFrame const torso_local = make_torso_local_frame(QMatrix4x4{}, torso);
  CloakPlacement const placement =
      make_cloak_placement(config, torso_local, torso, shoulder_l, shoulder_r);
  const QMatrix4x4* clasp_ptr = nullptr;
  if (placement.has_clasp) {
    clasp_ptr = &placement.clasp_model;
  }

  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &cloak_archetype(
          config, meshes, placement.shoulder_model, placement.drape_model, clasp_ptr),
      .socket_bone_index = torso_socket_bone_index,
      .unit_local_pose_at_bind = torso_local.world,
  });
  spec.palette_role_remap[k_cloak_cloth_slot] =
      static_cast<std::uint8_t>(base_role_byte + 0U);
  spec.palette_role_remap[k_cloak_trim_slot] =
      static_cast<std::uint8_t>(base_role_byte + 1U);

  return spec;
}

} // namespace Render::GL
