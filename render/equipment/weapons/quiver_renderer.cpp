#include "quiver_renderer.h"

#include "../../entity/registry.h"
#include "../../geom/math_utils.h"
#include "../../gl/render_constants.h"
#include "../../humanoid/humanoid_math.h"
#include "../equipment_submit.h"
#include "../generated_equipment.h"
#include "../oriented_archetype_utils.h"
#include "arrow_archetype_utils.h"

#include "../../humanoid/humanoid_spec.h"
#include "../attachment_builder.h"

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

auto quiver_fill_role_colors(const HumanoidPalette &palette,
                             const QuiverRenderConfig &config, QVector3D *out,
                             std::size_t max) -> std::uint32_t {
  if (max < kQuiverRoleCount) {
    return 0;
  }
  out[k_leather_slot] = palette.leather;
  out[k_wood_slot] = palette.wood;
  out[k_fletching_slot] = config.fletching_color;
  return kQuiverRoleCount;
}

auto quiver_make_static_attachments(const QuiverRenderConfig &config,
                                    std::uint16_t socket_bone_index,
                                    std::uint8_t base_role_byte)
    -> std::array<Render::Creature::StaticAttachmentSpec, 5> {
  const AttachmentFrame &bw =
      Render::Humanoid::humanoid_bind_body_frames().waist;
  QVector3D const quiver_pos_local(bw.radius * 0.9F + 0.15F, -0.10F, 0.0F);
  QVector3D const q_top_local =
      quiver_pos_local + QVector3D(0.0F, 0.15F, -0.10F);

  auto make_spec =
      [&](const RenderArchetype &arch, const QMatrix4x4 &unit_local_transform,
          std::uint8_t slot0_role,
          std::uint8_t slot2_role) -> Render::Creature::StaticAttachmentSpec {
    auto spec = Render::Equipment::build_static_attachment({
        .archetype = &arch,
        .socket_bone_index = socket_bone_index,
        .unit_local_pose_at_bind = unit_local_transform,
    });
    spec.palette_role_remap[0] = slot0_role;
    spec.palette_role_remap[2] = slot2_role;
    return spec;
  };

  const std::uint8_t leather_role = base_role_byte;
  const std::uint8_t fletching_role =
      static_cast<std::uint8_t>(base_role_byte + 2U);

  QMatrix4x4 const body_transform =
      waist_basis_transform(QMatrix4x4{}, bw, quiver_pos_local);

  QVector3D const shaft1_end_local(0.0F, 0.08F, 0.0F);
  QVector3D const arrow1_origin_UL = waist_basis_point(bw, q_top_local);
  QVector3D const arrow1_tip_UL =
      waist_basis_point(bw, q_top_local + shaft1_end_local);
  QMatrix4x4 const shaft1_transform = oriented_segment_transform(
      QMatrix4x4{}, arrow1_origin_UL, arrow1_tip_UL - arrow1_origin_UL);
  QMatrix4x4 scale1;
  scale1.scale(1.0F, 0.05F, 1.0F);
  QMatrix4x4 const fletching1_transform =
      waist_basis_transform(QMatrix4x4{}, bw, q_top_local + shaft1_end_local) *
      scale1;

  QVector3D const shaft2_end_local(0.02F, 0.07F, 0.02F);
  QVector3D const arrow2_origin_UL = waist_basis_point(bw, q_top_local);
  QVector3D const arrow2_tip_UL =
      waist_basis_point(bw, q_top_local + shaft2_end_local);
  QMatrix4x4 const shaft2_transform = oriented_segment_transform(
      QMatrix4x4{}, arrow2_origin_UL, arrow2_tip_UL - arrow2_origin_UL);
  QMatrix4x4 scale2;
  scale2.scale(1.0F, 0.05F, 1.0F);
  QMatrix4x4 const fletching2_transform =
      waist_basis_transform(QMatrix4x4{}, bw, q_top_local + shaft2_end_local) *
      scale2;

  return {
      make_spec(quiver_body_archetype(config), body_transform, leather_role,
                leather_role),
      make_spec(
          arrow_shaft_archetype(0.010F, config.material_id, "quiver_shaft"),
          shaft1_transform, leather_role, fletching_role),
      make_spec(arrow_fletching_archetype(0.025F, config.material_id,
                                          "quiver_fletching"),
                fletching1_transform, leather_role, fletching_role),
      make_spec(
          arrow_shaft_archetype(0.010F, config.material_id, "quiver_shaft"),
          shaft2_transform, leather_role, fletching_role),
      make_spec(arrow_fletching_archetype(0.025F, config.material_id,
                                          "quiver_fletching"),
                fletching2_transform, leather_role, fletching_role),
  };
}

} // namespace Render::GL
