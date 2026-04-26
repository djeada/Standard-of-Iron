#include "arm_guards_renderer.h"

#include "../../entity/registry.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_spec.h"
#include "../../static_attachment_spec.h"
#include "../attachment_builder.h"
#include "../equipment_submit.h"

#include "../../render_archetype.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <string>

namespace Render::GL {

namespace {

enum ArmGuardsPaletteSlot : std::uint8_t {
  k_guard_slot = 0U,
  k_strap_slot = 1U,
};

auto arm_guard_transform(const QMatrix4x4 &parent, const QVector3D &elbow,
                         const QVector3D &wrist) -> QMatrix4x4 {
  QVector3D const arm_dir = (wrist - elbow).normalized();
  QVector3D side_axis =
      QVector3D::crossProduct(QVector3D(0.0F, 0.0F, 1.0F), arm_dir);
  if (side_axis.lengthSquared() < 1e-5F) {
    side_axis = QVector3D::crossProduct(QVector3D(1.0F, 0.0F, 0.0F), arm_dir);
  }
  side_axis.normalize();
  QVector3D normal_axis =
      QVector3D::crossProduct(side_axis, arm_dir).normalized();

  QMatrix4x4 local;
  local.setColumn(0, QVector4D(side_axis, 0.0F));
  local.setColumn(1, QVector4D(arm_dir, 0.0F));
  local.setColumn(2, QVector4D(normal_axis, 0.0F));
  local.setColumn(3, QVector4D(elbow, 1.0F));
  return parent * local;
}

auto arm_guards_archetype(const ArmGuardsConfig &config,
                          float arm_length) -> const RenderArchetype & {
  struct CachedArchetype {
    int arm_length_key{0};
    int guard_length_key{0};
    bool include_straps{false};
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  int const arm_length_key = std::lround(arm_length * 1000.0F);
  int const guard_length_key = std::lround(config.guard_length * 1000.0F);
  for (const auto &entry : cache) {
    if (entry.arm_length_key == arm_length_key &&
        entry.guard_length_key == guard_length_key &&
        entry.include_straps == config.include_straps) {
      return entry.archetype;
    }
  }

  float const guard_start = arm_length * 0.15F;
  float const guard_end =
      std::min(arm_length * (0.15F + config.guard_length), arm_length * 0.85F);

  RenderArchetypeBuilder builder{
      "arm_guards_" + std::to_string(arm_length_key) + "_" +
      std::to_string(guard_length_key) +
      (config.include_straps ? "_straps" : "_plain")};

  constexpr int segments = 5;
  for (int i = 0; i < segments; ++i) {
    float const t = static_cast<float>(i) / static_cast<float>(segments - 1);
    float const y = guard_start * (1.0F - t) + guard_end * t;
    float const radius = 0.026F - t * 0.004F;
    builder.add_palette_mesh(
        get_unit_sphere(),
        Render::Geom::sphere_at(QVector3D(0.0F, y, 0.0F), radius),
        k_guard_slot);
  }

  if (config.include_straps) {
    std::array<float, 3> const strap_positions{
        guard_start + 0.02F,
        guard_start + (guard_end - guard_start) * 0.5F,
        guard_end - 0.02F,
    };
    for (float y : strap_positions) {
      builder.add_palette_mesh(
          get_unit_sphere(),
          Render::Geom::sphere_at(QVector3D(0.0F, y, 0.0F), 0.010F),
          k_strap_slot);
    }
  }

  cache.push_back({arm_length_key, guard_length_key, config.include_straps,
                   std::move(builder).build()});
  return cache.back().archetype;
}

} // namespace

ArmGuardsRenderer::ArmGuardsRenderer(const ArmGuardsConfig &config)
    : m_config(config) {}

void ArmGuardsRenderer::render(const DrawContext &ctx, const BodyFrames &frames,
                               const HumanoidPalette &palette,
                               const HumanoidAnimationContext &anim,
                               EquipmentBatch &batch) {
  submit(m_config, ctx, frames, palette, anim, batch);
}

void ArmGuardsRenderer::submit(const ArmGuardsConfig &config,
                               const DrawContext &ctx, const BodyFrames &frames,
                               const HumanoidPalette &,
                               const HumanoidAnimationContext &,
                               EquipmentBatch &batch) {
  std::array<QVector3D, 2> const palette{config.leather_color,
                                         config.strap_color};
  QVector3D elbow_l = frames.shoulder_l.origin +
                      (frames.hand_l.origin - frames.shoulder_l.origin) * 0.55F;
  QVector3D elbow_r = frames.shoulder_r.origin +
                      (frames.hand_r.origin - frames.shoulder_r.origin) * 0.55F;

  renderArmGuard(config, ctx, elbow_l, frames.hand_l.origin, palette, batch);
  renderArmGuard(config, ctx, elbow_r, frames.hand_r.origin, palette, batch);
}

void ArmGuardsRenderer::renderArmGuard(const ArmGuardsConfig &config,
                                       const DrawContext &ctx,
                                       const QVector3D &elbow,
                                       const QVector3D &wrist,
                                       std::span<const QVector3D> palette,
                                       EquipmentBatch &batch) {
  float const arm_length = (wrist - elbow).length();

  if (arm_length < 0.01F) {
    return;
  }

  append_equipment_archetype(batch, arm_guards_archetype(config, arm_length),
                             arm_guard_transform(ctx.model, elbow, wrist),
                             palette);
}

auto arm_guards_fill_role_colors(const HumanoidPalette &palette, QVector3D *out,
                                 std::size_t max) -> std::uint32_t {
  (void)palette;
  if (max < kArmGuardsRoleCount) {
    return 0U;
  }
  constexpr ArmGuardsConfig cfg{};
  out[0] = cfg.leather_color;
  out[1] = cfg.strap_color;
  return kArmGuardsRoleCount;
}

auto arm_guards_make_static_attachments(std::uint16_t shoulder_l_bone_index,
                                        std::uint16_t shoulder_r_bone_index,
                                        std::uint8_t base_role_byte)
    -> std::array<Render::Creature::StaticAttachmentSpec, 2> {
  const auto &bind_frames = Render::Humanoid::humanoid_bind_body_frames();
  const AttachmentFrame &shoulder_l = bind_frames.shoulder_l;
  const AttachmentFrame &shoulder_r = bind_frames.shoulder_r;
  const AttachmentFrame &hand_l = bind_frames.hand_l;
  const AttachmentFrame &hand_r = bind_frames.hand_r;

  QVector3D const elbow_l =
      shoulder_l.origin + (hand_l.origin - shoulder_l.origin) * 0.55F;
  QVector3D const elbow_r =
      shoulder_r.origin + (hand_r.origin - shoulder_r.origin) * 0.55F;

  float const arm_length_l = (hand_l.origin - elbow_l).length();
  float const arm_length_r = (hand_r.origin - elbow_r).length();

  constexpr ArmGuardsConfig default_cfg{};

  QMatrix4x4 const bind_mat_l =
      arm_guard_transform(QMatrix4x4{}, elbow_l, hand_l.origin);

  auto spec_l = Render::Equipment::build_static_attachment({
      .archetype = &arm_guards_archetype(default_cfg, arm_length_l),
      .socket_bone_index = shoulder_l_bone_index,
      .unit_local_pose_at_bind = bind_mat_l,
  });
  spec_l.palette_role_remap[k_guard_slot] =
      static_cast<std::uint8_t>(base_role_byte + 0U);
  spec_l.palette_role_remap[k_strap_slot] =
      static_cast<std::uint8_t>(base_role_byte + 1U);

  QMatrix4x4 const bind_mat_r =
      arm_guard_transform(QMatrix4x4{}, elbow_r, hand_r.origin);

  auto spec_r = Render::Equipment::build_static_attachment({
      .archetype = &arm_guards_archetype(default_cfg, arm_length_r),
      .socket_bone_index = shoulder_r_bone_index,
      .unit_local_pose_at_bind = bind_mat_r,
  });
  spec_r.palette_role_remap[k_guard_slot] =
      static_cast<std::uint8_t>(base_role_byte + 0U);
  spec_r.palette_role_remap[k_strap_slot] =
      static_cast<std::uint8_t>(base_role_byte + 1U);

  return {spec_l, spec_r};
}

} // namespace Render::GL
