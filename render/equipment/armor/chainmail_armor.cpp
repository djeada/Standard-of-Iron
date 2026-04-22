#include "chainmail_armor.h"
#include "torso_local_archetype_utils.h"

#include "../../geom/parts.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/mesh_helpers.h"
#include "../equipment_submit.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <deque>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>

namespace Render::GL {

using Render::Geom::oriented_cylinder;

namespace {

struct FixedDrawSpec {
  Mesh *mesh{nullptr};
  QMatrix4x4 model{};
  QVector3D color{1.0F, 1.0F, 1.0F};
  float alpha{1.0F};
  int material_id{0};
};

auto cached_fixed_archetype(std::string_view prefix,
                            const std::vector<FixedDrawSpec> &draws)
    -> const RenderArchetype & {
  struct CachedArchetype {
    std::string key;
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;

  std::string key(prefix);
  key.push_back('_');
  for (const auto &draw : draws) {
    append_quantized_key(key, draw.model);
    append_quantized_key(key, draw.color);
    append_quantized_key(key, draw.alpha);
    append_quantized_key(key, draw.material_id);
  }

  for (const auto &entry : cache) {
    if (entry.key == key) {
      return entry.archetype;
    }
  }

  RenderArchetypeBuilder builder{key};
  for (const auto &draw : draws) {
    builder.add_mesh(draw.mesh, draw.model, draw.color, nullptr, draw.alpha,
                     draw.material_id);
  }

  cache.push_back({key, std::move(builder).build()});
  return cache.back().archetype;
}

} // namespace

auto ChainmailArmorRenderer::calculate_ring_color(
    const ChainmailArmorConfig &config, float x, float y,
    float z) -> QVector3D {
  float rust_noise =
      std::sin(x * 127.3F) * std::cos(y * 97.1F) * std::sin(z * 83.7F);
  rust_noise = (rust_noise + 1.0F) * 0.5F;

  float gravity_rust = std::clamp(1.0F - y * 0.8F, 0.0F, 1.0F);
  float total_rust =
      (rust_noise * 0.6F + gravity_rust * 0.4F) * config.rust_amount;

  return config.metal_color * (1.0F - total_rust) +
         config.rust_tint * total_rust;
}

void ChainmailArmorRenderer::render(const DrawContext &ctx,
                                    const BodyFrames &frames,
                                    const HumanoidPalette &palette,
                                    const HumanoidAnimationContext &anim,
                                    EquipmentBatch &batch) {
  submit(m_config, ctx, frames, palette, anim, batch);
}

void ChainmailArmorRenderer::submit(const ChainmailArmorConfig &config,
                                    const DrawContext &ctx,
                                    const BodyFrames &frames,
                                    const HumanoidPalette &palette,
                                    const HumanoidAnimationContext &anim,
                                    EquipmentBatch &batch) {
  (void)anim;
  (void)palette;

  const AttachmentFrame &torso = frames.torso;
  const AttachmentFrame &waist = frames.waist;

  if (torso.radius <= 0.0F) {
    return;
  }

  TorsoLocalFrame const torso_local = make_torso_local_frame(ctx.model, torso);
  std::vector<FixedDrawSpec> draws;
  draws.reserve(24);

  float const torso_r = torso.radius;
  float const torso_depth =
      (torso.depth > 0.0F) ? torso.depth : torso.radius * 0.75F;
  QVector3D const top = torso.origin + torso.up * (torso_r * 0.20F);
  QVector3D const bottom = waist.origin - waist.up * (torso_r * 0.35F);

  QMatrix4x4 mail_transform =
      oriented_cylinder(torso_local.point(top), torso_local.point(bottom),
                        torso_local.direction(safe_attachment_axis(
                            torso.right, QVector3D(1.0F, 0.0F, 0.0F))),
                        torso_r * 1.12F, std::max(0.08F, torso_depth * 1.10F));
  align_torso_mesh_forward(mail_transform);

  Mesh *torso_mesh = torso_mesh_without_bottom_cap();
  if (torso_mesh == nullptr) {
    torso_mesh = get_unit_torso();
  }
  draws.push_back({torso_mesh, mail_transform, QVector3D(0.65F, 0.67F, 0.70F)});

  if (config.has_shoulder_guards) {
    const AttachmentFrame &shoulder_l = frames.shoulder_l;
    const AttachmentFrame &shoulder_r = frames.shoulder_r;

    QVector3D const left_base = shoulder_l.origin;
    QVector3D const left_tip =
        left_base + torso.up * 0.08F + torso.right * (-0.05F);
    QVector3D const right_base = shoulder_r.origin;
    QVector3D const right_tip =
        right_base + torso.up * 0.08F + torso.right * 0.05F;
    float const shoulder_radius = 0.08F;

    QVector3D const left_color = calculate_ring_color(
        config, left_base.x(), left_base.y(), left_base.z());
    QVector3D const right_color = calculate_ring_color(
        config, right_base.x(), right_base.y(), right_base.z());

    draws.push_back({get_unit_cylinder(),
                     Render::Geom::cylinder_between(
                         torso_local.point(left_base),
                         torso_local.point(left_tip), shoulder_radius),
                     left_color, 0.8F});
    draws.push_back({get_unit_cylinder(),
                     Render::Geom::cylinder_between(
                         torso_local.point(right_base),
                         torso_local.point(right_tip), shoulder_radius),
                     right_color, 0.8F});

    if (config.detail_level >= 1) {
      for (int layer = 0; layer < 3; ++layer) {
        float const layer_offset = static_cast<float>(layer) * 0.025F;
        QVector3D const left_layer = left_base + torso.up * (-layer_offset);
        QVector3D const right_layer = right_base + torso.up * (-layer_offset);
        QVector3D const layer_scale(shoulder_radius * 1.3F,
                                    shoulder_radius * 1.3F,
                                    shoulder_radius * 1.3F);

        draws.push_back(
            {get_unit_sphere(),
             local_scale_model(torso_local.point(left_layer), layer_scale),
             left_color * (1.0F - layer_offset), 0.75F});
        draws.push_back(
            {get_unit_sphere(),
             local_scale_model(torso_local.point(right_layer), layer_scale),
             right_color * (1.0F - layer_offset), 0.75F});
      }
    }
  }

  if (config.has_arm_coverage) {
    auto append_arm = [&](const QVector3D &shoulder, const QVector3D &hand) {
      QVector3D const elbow = (shoulder + hand) * 0.5F;
      int const arm_segments = config.detail_level >= 2 ? 6 : 3;

      for (int i = 0; i < arm_segments; ++i) {
        float const t0 =
            static_cast<float>(i) / static_cast<float>(arm_segments);
        float const t1 =
            static_cast<float>(i + 1) / static_cast<float>(arm_segments);
        QVector3D const pos0 = shoulder * (1.0F - t0) + elbow * t0;
        QVector3D const pos1 = shoulder * (1.0F - t1) + elbow * t1;
        float const radius = 0.05F * (1.0F - t0 * 0.2F);
        QVector3D const color =
            calculate_ring_color(config, pos0.x(), pos0.y(), pos0.z());

        draws.push_back(
            {get_unit_cylinder(),
             Render::Geom::cylinder_between(torso_local.point(pos0),
                                            torso_local.point(pos1), radius),
             color, 0.75F});
      }
    };

    append_arm(frames.shoulder_l.origin, frames.hand_l.origin);
    append_arm(frames.shoulder_r.origin, frames.hand_r.origin);
  }

  append_equipment_archetype(batch,
                             cached_fixed_archetype("chainmail_armor", draws),
                             torso_local.world);
}

} // namespace Render::GL
