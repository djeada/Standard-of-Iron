#include "quiver_renderer.h"

#include "../../entity/registry.h"
#include "../../geom/math_utils.h"
#include "../../gl/render_constants.h"
#include "../../humanoid/humanoid_math.h"
#include "../equipment_submit.h"
#include "../generated_equipment.h"
#include "../oriented_archetype_utils.h"
#include "arrow_archetype_utils.h"

#include <array>
#include <cmath>
#include <deque>
#include <string>

namespace Render::GL {

using Render::GL::HashXorShift::k_golden_ratio;

namespace {

enum QuiverPaletteSlot : std::uint8_t {
  k_leather_slot = 0U,
  k_wood_slot = 1U,
  k_fletching_slot = 2U,
};

auto waist_basis_point(const AttachmentFrame &waist,
                       const QVector3D &local) -> QVector3D {
  return waist.origin + waist.right * local.x() + waist.up * local.y() +
         waist.forward * local.z();
}

auto waist_basis_transform(const QMatrix4x4 &parent,
                           const AttachmentFrame &waist,
                           const QVector3D &local_origin) -> QMatrix4x4 {
  QMatrix4x4 local;
  local.setColumn(0, QVector4D(waist.right, 0.0F));
  local.setColumn(1, QVector4D(waist.up, 0.0F));
  local.setColumn(2, QVector4D(waist.forward, 0.0F));
  local.setColumn(3, QVector4D(waist_basis_point(waist, local_origin), 1.0F));
  return parent * local;
}

auto quiver_body_archetype(const QuiverRenderConfig &config)
    -> const RenderArchetype & {
  struct CachedArchetype {
    int radius_key{0};
    int material_id{0};
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  int const radius_key = std::lround(config.quiver_radius * 1000.0F);
  for (const auto &entry : cache) {
    if (entry.radius_key == radius_key &&
        entry.material_id == config.material_id) {
      return entry.archetype;
    }
  }

  std::array<GeneratedEquipmentPrimitive, 1> const primitives{{
      generated_cylinder(QVector3D(0.0F, -0.25F, 0.05F),
                         QVector3D(0.0F, 0.15F, -0.10F), config.quiver_radius,
                         k_leather_slot, 1.0F, config.material_id),
  }};
  cache.push_back(
      {radius_key, config.material_id,
       build_generated_equipment_archetype(
           "quiver_body_" + std::to_string(radius_key), primitives)});
  return cache.back().archetype;
}

auto quiver_palette(const QuiverRenderConfig &config,
                    const HumanoidPalette &palette)
    -> std::array<QVector3D, 3> {
  return {palette.leather, palette.wood, config.fletching_color};
}

void append_quiver_arrow(EquipmentBatch &batch, const DrawContext &ctx,
                         const AttachmentFrame &waist,
                         const QuiverRenderConfig &config,
                         std::span<const QVector3D> palette,
                         const QVector3D &q_top_local,
                         const QVector3D &shaft_local_end) {
  QVector3D const arrow_origin = waist_basis_point(waist, q_top_local);
  QVector3D const shaft_tip =
      waist_basis_point(waist, q_top_local + shaft_local_end);
  QVector3D const shaft_axis = shaft_tip - arrow_origin;

  append_equipment_archetype(
      batch, arrow_shaft_archetype(0.010F, config.material_id, "quiver_shaft"),
      oriented_segment_transform(ctx.model, arrow_origin, shaft_axis), palette);
  append_equipment_archetype(
      batch,
      arrow_fletching_archetype(0.025F, config.material_id, "quiver_fletching"),
      waist_basis_transform(ctx.model, waist, q_top_local + shaft_local_end) *
          [] {
            QMatrix4x4 scale;
            scale.setToIdentity();
            scale.scale(1.0F, 0.05F, 1.0F);
            return scale;
          }(),
      palette);
}

} // namespace

QuiverRenderer::QuiverRenderer(QuiverRenderConfig config)
    : m_config(std::move(config)) {}

void QuiverRenderer::render(const DrawContext &ctx, const BodyFrames &frames,
                            const HumanoidPalette &palette,
                            const HumanoidAnimationContext &anim,
                            EquipmentBatch &batch) {
  submit(m_config, ctx, frames, palette, anim, batch);
}

void QuiverRenderer::submit(const QuiverRenderConfig &config,
                            const DrawContext &ctx, const BodyFrames &frames,
                            const HumanoidPalette &palette,
                            const HumanoidAnimationContext &,
                            EquipmentBatch &batch) {
  const AttachmentFrame &waist = frames.waist;
  if (waist.radius <= 0.0F) {
    return;
  }

  QVector3D const quiver_pos_local(waist.radius * 0.9F + 0.15F, -0.10F, 0.0F);
  QVector3D const q_top_local =
      quiver_pos_local + QVector3D(0.0F, 0.15F, -0.10F);
  auto const palette_slots = quiver_palette(config, palette);

  append_equipment_archetype(
      batch, quiver_body_archetype(config),
      waist_basis_transform(ctx.model, waist, quiver_pos_local), palette_slots);

  uint32_t seed = 0U;
  if (ctx.entity != nullptr) {
    seed = uint32_t(reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFU);
  }

  float const j = (hash_01(seed) - 0.5F) * 0.04F;
  float const k = (hash_01(seed ^ k_golden_ratio) - 0.5F) * 0.04F;

  append_quiver_arrow(batch, ctx, waist, config, palette_slots, q_top_local,
                      QVector3D(j, 0.08F, k));

  if (config.num_arrows >= 2) {
    append_quiver_arrow(batch, ctx, waist, config, palette_slots, q_top_local,
                        QVector3D(0.02F - j, 0.07F, 0.02F - k));
  }
}

} // namespace Render::GL
