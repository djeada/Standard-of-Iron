#include "work_apron_renderer.h"

#include <array>
#include <cmath>
#include <deque>
#include <numbers>
#include <string>

#include "../../humanoid/humanoid_spec.h"
#include "../attachment_builder.h"
#include "../generated_equipment.h"
#include "../humanoid_attachment_archetype.h"

namespace Render::GL {

namespace {

enum WorkApronBodyPaletteSlot : std::uint8_t {
  k_ring_0_slot = 0U,
  k_ring_1_slot = 1U,
  k_ring_2_slot = 2U,
  k_ring_3_slot = 3U,
  k_ring_4_slot = 4U,
  k_ring_5_slot = 5U,
  k_edge_slot = 6U,
};

enum WorkApronSinglePaletteSlot : std::uint8_t {
  k_single_slot = 0U,
};

constexpr int k_apron_rings = 6;

auto work_apron_body_palette(const WorkApronConfig& config)
    -> std::array<QVector3D, 7> {
  std::array<QVector3D, 7> palette{};
  for (int ring = 0; ring < k_apron_rings; ++ring) {
    float const t = static_cast<float>(ring) / 5.0F;
    palette[ring] = config.leather_color * (1.0F - t * 0.12F);
  }
  palette[k_edge_slot] = config.leather_color * 0.80F;
  return palette;
}

auto work_apron_body_archetype(const WorkApronConfig& config,
                               const AttachmentFrame& waist) -> const RenderArchetype& {
  struct CachedArchetype {
    int radius_key{0};
    int depth_key{0};
    int length_key{0};
    int width_key{0};
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  float const waist_r = waist.radius * config.apron_width;
  float const waist_d =
      (waist.depth > 0.0F) ? waist.depth * 0.85F : waist.radius * 0.75F;
  int const radius_key = std::lround(waist_r * 1000.0F);
  int const depth_key = std::lround(waist_d * 1000.0F);
  int const length_key = std::lround(config.apron_length * 1000.0F);
  int const width_key = std::lround(config.apron_width * 1000.0F);
  for (const auto& entry : cache) {
    if (entry.radius_key == radius_key && entry.depth_key == depth_key &&
        entry.length_key == length_key && entry.width_key == width_key) {
      return entry.archetype;
    }
  }

  std::vector<GeneratedEquipmentPrimitive> primitives;
  primitives.reserve(4);

  float const y_top = 0.03F;
  float const y_bottom = -config.apron_length;
  float const apron_height = y_top - y_bottom;
  float const mid_y = (y_top + y_bottom) * 0.5F;
  float const face_z = waist_d * 0.76F;

  primitives.push_back(
      generated_box(QVector3D(0.0F, mid_y, face_z),
                    QVector3D(waist_r * 2.28F, apron_height * 0.92F, 0.060F),
                    k_ring_2_slot));

  primitives.push_back(generated_box(QVector3D(0.0F, y_top - 0.022F, face_z + 0.006F),
                                     QVector3D(waist_r * 2.44F, 0.055F, 0.060F),
                                     k_ring_0_slot));

  primitives.push_back(generated_box(QVector3D(0.0F, mid_y, face_z + 0.004F),
                                     QVector3D(0.06F, apron_height * 0.80F, 0.022F),
                                     k_ring_5_slot));

  primitives.push_back(
      generated_box(QVector3D(0.0F, y_bottom + 0.022F, face_z + 0.006F),
                    QVector3D(waist_r * 2.44F, 0.045F, 0.055F),
                    k_edge_slot));

  cache.push_back({radius_key,
                   depth_key,
                   length_key,
                   width_key,
                   build_generated_equipment_archetype(
                       "work_apron_body_" + std::to_string(radius_key) + "_" +
                           std::to_string(depth_key) + "_" +
                           std::to_string(length_key) + "_" + std::to_string(width_key),
                       std::span<const GeneratedEquipmentPrimitive>(
                           primitives.data(), primitives.size()))});
  return cache.back().archetype;
}

auto work_apron_straps_archetype(const AttachmentFrame& torso)
    -> const RenderArchetype& {
  struct CachedArchetype {
    int radius_key{0};
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  int const radius_key = std::lround(torso.radius * 1000.0F);
  for (const auto& entry : cache) {
    if (entry.radius_key == radius_key) {
      return entry.archetype;
    }
  }

  std::array<GeneratedEquipmentPrimitive, 2> const primitives{{
      generated_box(QVector3D(torso.radius * 0.20F, 0.01F, -torso.radius * 0.56F),
                    QVector3D(0.040F, 0.24F, 0.034F),
                    k_single_slot),
      generated_box(QVector3D(-torso.radius * 0.20F, 0.01F, -torso.radius * 0.56F),
                    QVector3D(0.040F, 0.24F, 0.034F),
                    k_single_slot),
  }};

  cache.push_back({radius_key,
                   build_generated_equipment_archetype(
                       "work_apron_straps_" + std::to_string(radius_key), primitives)});
  return cache.back().archetype;
}

auto work_apron_pockets_archetype(const AttachmentFrame& waist)
    -> const RenderArchetype& {
  struct CachedArchetype {
    int radius_key{0};
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  int const radius_key = std::lround(waist.radius * 1000.0F);
  for (const auto& entry : cache) {
    if (entry.radius_key == radius_key) {
      return entry.archetype;
    }
  }

  std::vector<GeneratedEquipmentPrimitive> primitives;
  primitives.reserve(2);
  float const waist_d =
      (waist.depth > 0.0F) ? waist.depth * 0.85F : waist.radius * 0.75F;

  for (int side = -1; side <= 1; side += 2) {
    primitives.push_back(generated_box(
        QVector3D(
            static_cast<float>(side) * waist.radius * 0.26F, -0.13F, waist_d * 0.82F),
        QVector3D(0.10F, 0.12F, 0.05F),
        k_single_slot));
  }

  cache.push_back({radius_key,
                   build_generated_equipment_archetype(
                       "work_apron_pockets_" + std::to_string(radius_key),
                       std::span<const GeneratedEquipmentPrimitive>(
                           primitives.data(), primitives.size()))});
  return cache.back().archetype;
}

} // namespace

WorkApronRenderer::WorkApronRenderer(const WorkApronConfig& config)
    : m_config(config) {
}

void WorkApronRenderer::render(const DrawContext& ctx,
                               const BodyFrames& frames,
                               const HumanoidPalette& palette,
                               const HumanoidAnimationContext& anim,
                               EquipmentBatch& batch) {
  submit(m_config, ctx, frames, palette, anim, batch);
}

void WorkApronRenderer::submit(const WorkApronConfig& config,
                               const DrawContext& ctx,
                               const BodyFrames& frames,
                               const HumanoidPalette&,
                               const HumanoidAnimationContext&,
                               EquipmentBatch& batch) {
  render_apron_body(config, ctx, frames.torso, frames.waist, batch);

  if (config.include_straps) {
    render_straps(config, ctx, frames.torso, frames, batch);
  }

  render_pockets(config, ctx, frames.waist, batch);
}

void WorkApronRenderer::render_apron_body(const WorkApronConfig& config,
                                          const DrawContext& ctx,
                                          const AttachmentFrame& torso,
                                          const AttachmentFrame& waist,
                                          EquipmentBatch& batch) {
  if (torso.radius <= 0.0F || waist.radius <= 0.0F) {
    return;
  }

  auto const palette = work_apron_body_palette(config);
  append_humanoid_attachment_archetype_scaled(batch,
                                              ctx,
                                              waist,
                                              work_apron_body_archetype(config, waist),
                                              QVector3D(1.0F, 1.0F, 1.0F),
                                              palette);
}

void WorkApronRenderer::render_straps(const WorkApronConfig& config,
                                      const DrawContext& ctx,
                                      const AttachmentFrame& torso,
                                      const BodyFrames& frames,
                                      EquipmentBatch& batch) {
  if (torso.radius <= 0.0F) {
    return;
  }

  (void)frames;

  std::array<QVector3D, 1> const palette{config.strap_color};
  append_humanoid_attachment_archetype_scaled(batch,
                                              ctx,
                                              torso,
                                              work_apron_straps_archetype(torso),
                                              QVector3D(1.0F, 1.0F, 1.0F),
                                              palette);
}

void WorkApronRenderer::render_pockets(const WorkApronConfig& config,
                                       const DrawContext& ctx,
                                       const AttachmentFrame& waist,
                                       EquipmentBatch& batch) {
  if (waist.radius <= 0.0F) {
    return;
  }

  std::array<QVector3D, 1> const palette{config.leather_color * 0.88F};
  append_humanoid_attachment_archetype_scaled(batch,
                                              ctx,
                                              waist,
                                              work_apron_pockets_archetype(waist),
                                              QVector3D(1.0F, 1.0F, 1.0F),
                                              palette);
}

auto work_apron_fill_role_colors(const HumanoidPalette& palette,
                                 QVector3D* out,
                                 std::size_t max) -> std::uint32_t {
  (void)palette;
  if (max < k_work_apron_role_count) {
    return 0U;
  }
  constexpr WorkApronConfig cfg{};

  for (int i = 0; i < 7; ++i) {
    float const t = static_cast<float>(i) / 5.0F * 0.12F;
    out[i] = cfg.leather_color * (1.0F - t);
  }

  out[7] = cfg.leather_color * 0.88F;

  out[8] = cfg.strap_color;
  return k_work_apron_role_count;
}

auto work_apron_make_static_attachments(std::uint16_t waist_socket_bone_index,
                                        std::uint16_t chest_socket_bone_index,
                                        std::uint8_t base_role_byte)
    -> std::array<Render::Creature::StaticAttachmentSpec, 3> {
  const auto& bind_frames = Render::Humanoid::humanoid_bind_body_frames();
  const AttachmentFrame& waist = bind_frames.waist;
  const AttachmentFrame& torso = bind_frames.torso;

  QMatrix4x4 const waist_bind_mat = make_humanoid_attachment_transform_scaled(
      QMatrix4x4{}, waist, QVector3D(0.0F, 0.0F, 0.0F), QVector3D(1.0F, 1.0F, 1.0F));

  QMatrix4x4 const torso_bind_mat = make_humanoid_attachment_transform_scaled(
      QMatrix4x4{}, torso, QVector3D(0.0F, 0.0F, 0.0F), QVector3D(1.0F, 1.0F, 1.0F));

  WorkApronConfig const default_cfg{};

  auto body_spec = Render::Equipment::build_static_attachment({
      .archetype = &work_apron_body_archetype(default_cfg, waist),
      .socket_bone_index = waist_socket_bone_index,
      .unit_local_pose_at_bind = waist_bind_mat,
  });
  for (std::uint8_t i = 0; i < 7; ++i) {
    body_spec.palette_role_remap[i] = static_cast<std::uint8_t>(base_role_byte + i);
  }

  auto pockets_spec = Render::Equipment::build_static_attachment({
      .archetype = &work_apron_pockets_archetype(waist),
      .socket_bone_index = waist_socket_bone_index,
      .unit_local_pose_at_bind = waist_bind_mat,
  });
  pockets_spec.palette_role_remap[0] = static_cast<std::uint8_t>(base_role_byte + 7U);

  auto straps_spec = Render::Equipment::build_static_attachment({
      .archetype = &work_apron_straps_archetype(torso),
      .socket_bone_index = chest_socket_bone_index,
      .unit_local_pose_at_bind = torso_bind_mat,
  });
  straps_spec.palette_role_remap[0] = static_cast<std::uint8_t>(base_role_byte + 8U);

  return {body_spec, pockets_spec, straps_spec};
}

} // namespace Render::GL
