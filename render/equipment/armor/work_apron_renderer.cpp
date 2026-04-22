#include "work_apron_renderer.h"

#include "../generated_equipment.h"
#include "../humanoid_attachment_archetype.h"

#include <array>
#include <cmath>
#include <deque>
#include <numbers>
#include <string>

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
constexpr int k_apron_ring_segments = 14;

auto work_apron_body_palette(const WorkApronConfig &config)
    -> std::array<QVector3D, 7> {
  std::array<QVector3D, 7> palette{};
  for (int ring = 0; ring < k_apron_rings; ++ring) {
    float const t = static_cast<float>(ring) / 5.0F;
    palette[ring] = config.leather_color * (1.0F - t * 0.12F);
  }
  palette[k_edge_slot] = config.leather_color * 0.80F;
  return palette;
}

auto work_apron_body_archetype(const WorkApronConfig &config,
                               const AttachmentFrame &waist)
    -> const RenderArchetype & {
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
  for (const auto &entry : cache) {
    if (entry.radius_key == radius_key && entry.depth_key == depth_key &&
        entry.length_key == length_key && entry.width_key == width_key) {
      return entry.archetype;
    }
  }

  constexpr float pi = std::numbers::pi_v<float>;
  std::vector<GeneratedEquipmentPrimitive> primitives;
  primitives.reserve(78);

  float const y_top = 0.05F;
  float const y_bottom = -config.apron_length;

  for (int ring = 0; ring < k_apron_rings; ++ring) {
    float const t = static_cast<float>(ring) / 5.0F;
    float const y = y_top - t * (y_top - y_bottom);
    float const flare = 1.0F + t * 0.15F;
    float const w = waist_r * flare;
    float const d = waist_d * flare;
    float const thickness = 0.018F + t * 0.004F;

    for (int i = 0; i < k_apron_ring_segments; ++i) {
      float const angle_start =
          (static_cast<float>(i) / k_apron_ring_segments - 0.25F) * pi;
      float const angle_end =
          (static_cast<float>(i + 1) / k_apron_ring_segments - 0.25F) * pi;

      if (angle_start < -pi * 0.5F || angle_start > pi * 0.5F) {
        continue;
      }

      QVector3D const p1(w * std::sin(angle_start), y,
                         d * std::cos(angle_start));
      QVector3D const p2(w * std::sin(angle_end), y, d * std::cos(angle_end));
      primitives.push_back(generated_cylinder(
          p1, p2, thickness, static_cast<std::uint8_t>(k_ring_0_slot + ring)));
    }
  }

  for (int edge = 0; edge < 2; ++edge) {
    float const side_angle = (edge == 0) ? -pi * 0.25F : pi * 0.25F;

    for (int i = 0; i < k_apron_rings; ++i) {
      float const t = static_cast<float>(i) / 5.0F;
      float const y = y_top - t * (y_top - y_bottom);
      float const flare = 1.0F + t * 0.15F;
      float const w = waist_r * flare;
      float const d = waist_d * flare;

      primitives.push_back(generated_sphere(
          QVector3D(w * std::sin(side_angle), y, d * std::cos(side_angle)),
          0.020F, k_edge_slot));
    }
  }

  cache.push_back(
      {radius_key, depth_key, length_key, width_key,
       build_generated_equipment_archetype(
           "work_apron_body_" + std::to_string(radius_key) + "_" +
               std::to_string(depth_key) + "_" + std::to_string(length_key) +
               "_" + std::to_string(width_key),
           std::span<const GeneratedEquipmentPrimitive>(primitives.data(),
                                                        primitives.size()))});
  return cache.back().archetype;
}

auto work_apron_straps_archetype(const AttachmentFrame &torso)
    -> const RenderArchetype & {
  struct CachedArchetype {
    int radius_key{0};
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  int const radius_key = std::lround(torso.radius * 1000.0F);
  for (const auto &entry : cache) {
    if (entry.radius_key == radius_key) {
      return entry.archetype;
    }
  }

  QVector3D const chest_l(torso.radius * 0.30F, 0.08F, torso.radius * 0.80F);
  QVector3D const chest_r(-torso.radius * 0.30F, 0.08F, torso.radius * 0.80F);
  QVector3D const back_l(torso.radius * 0.20F, -0.02F, -torso.radius * 0.65F);
  QVector3D const back_r(-torso.radius * 0.20F, -0.02F, -torso.radius * 0.65F);

  std::array<GeneratedEquipmentPrimitive, 3> const primitives{{
      generated_cylinder(chest_l, back_l, 0.020F, k_single_slot),
      generated_cylinder(chest_r, back_r, 0.020F, k_single_slot),
      generated_cylinder(back_l, back_r, 0.018F, k_single_slot),
  }};

  cache.push_back(
      {radius_key,
       build_generated_equipment_archetype(
           "work_apron_straps_" + std::to_string(radius_key), primitives)});
  return cache.back().archetype;
}

auto work_apron_pockets_archetype(const AttachmentFrame &waist)
    -> const RenderArchetype & {
  struct CachedArchetype {
    int radius_key{0};
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  int const radius_key = std::lround(waist.radius * 1000.0F);
  for (const auto &entry : cache) {
    if (entry.radius_key == radius_key) {
      return entry.archetype;
    }
  }

  constexpr float pi = std::numbers::pi_v<float>;
  std::vector<GeneratedEquipmentPrimitive> primitives;
  primitives.reserve(18);

  for (int side = -1; side <= 1; side += 2) {
    float const pocket_angle = static_cast<float>(side) * 0.12F * pi;
    float const pocket_x = waist.radius * 0.55F * std::sin(pocket_angle);
    float const pocket_z = waist.radius * 0.45F * std::cos(pocket_angle);
    QVector3D const pocket_center(pocket_x, -0.12F, pocket_z);

    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        float const x_off = (static_cast<float>(i) - 1.0F) * 0.018F;
        float const y_off = (static_cast<float>(j) - 1.0F) * 0.022F;
        float const radius = 0.012F - static_cast<float>(i + j) * 0.0005F;

        primitives.push_back(generated_sphere(
            pocket_center +
                QVector3D(x_off * static_cast<float>(side), y_off, 0.0F),
            radius, k_single_slot));
      }
    }
  }

  cache.push_back(
      {radius_key, build_generated_equipment_archetype(
                       "work_apron_pockets_" + std::to_string(radius_key),
                       std::span<const GeneratedEquipmentPrimitive>(
                           primitives.data(), primitives.size()))});
  return cache.back().archetype;
}

} // namespace

WorkApronRenderer::WorkApronRenderer(const WorkApronConfig &config)
    : m_config(config) {}

void WorkApronRenderer::render(const DrawContext &ctx, const BodyFrames &frames,
                               const HumanoidPalette &palette,
                               const HumanoidAnimationContext &anim,
                               EquipmentBatch &batch) {
  submit(m_config, ctx, frames, palette, anim, batch);
}

void WorkApronRenderer::submit(const WorkApronConfig &config,
                               const DrawContext &ctx, const BodyFrames &frames,
                               const HumanoidPalette &,
                               const HumanoidAnimationContext &,
                               EquipmentBatch &batch) {
  renderApronBody(config, ctx, frames.torso, frames.waist, batch);

  if (config.include_straps) {
    renderStraps(config, ctx, frames.torso, frames, batch);
  }

  if (config.include_pockets) {
    renderPockets(config, ctx, frames.waist, batch);
  }
}

void WorkApronRenderer::renderApronBody(const WorkApronConfig &config,
                                        const DrawContext &ctx,
                                        const AttachmentFrame &torso,
                                        const AttachmentFrame &waist,
                                        EquipmentBatch &batch) {
  if (torso.radius <= 0.0F || waist.radius <= 0.0F) {
    return;
  }

  auto const palette = work_apron_body_palette(config);
  append_humanoid_attachment_archetype_scaled(
      batch, ctx, waist, work_apron_body_archetype(config, waist),
      QVector3D(1.0F, 1.0F, 1.0F), palette);
}

void WorkApronRenderer::renderStraps(const WorkApronConfig &config,
                                     const DrawContext &ctx,
                                     const AttachmentFrame &torso,
                                     const BodyFrames &frames,
                                     EquipmentBatch &batch) {
  if (torso.radius <= 0.0F) {
    return;
  }

  (void)frames;

  std::array<QVector3D, 1> const palette{config.strap_color};
  append_humanoid_attachment_archetype_scaled(
      batch, ctx, torso, work_apron_straps_archetype(torso),
      QVector3D(1.0F, 1.0F, 1.0F), palette);
}

void WorkApronRenderer::renderPockets(const WorkApronConfig &config,
                                      const DrawContext &ctx,
                                      const AttachmentFrame &waist,
                                      EquipmentBatch &batch) {
  if (waist.radius <= 0.0F) {
    return;
  }

  std::array<QVector3D, 1> const palette{config.leather_color * 0.88F};
  append_humanoid_attachment_archetype_scaled(
      batch, ctx, waist, work_apron_pockets_archetype(waist),
      QVector3D(1.0F, 1.0F, 1.0F), palette);
}

} // namespace Render::GL
