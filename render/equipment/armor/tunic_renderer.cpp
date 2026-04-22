#include "tunic_renderer.h"
#include "torso_local_archetype_utils.h"

#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../humanoid/style_palette.h"
#include "../equipment_submit.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>

namespace Render::GL {

using Render::GL::Humanoid::saturate_color;

namespace {

struct PaletteDrawSpec {
  Mesh *mesh{nullptr};
  QMatrix4x4 model{};
  std::uint8_t palette_slot{0U};
  int material_id{0};
};

auto cached_palette_archetype(std::string_view prefix,
                              const std::vector<PaletteDrawSpec> &draws)
    -> const RenderArchetype & {
  struct CachedArchetype {
    std::string key;
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;

  std::string key(prefix);
  key.push_back('_');
  for (const auto &draw : draws) {
    append_quantized_key(key, static_cast<int>(draw.palette_slot));
    append_quantized_key(key, draw.material_id);
    append_quantized_key(key, draw.model);
  }

  for (const auto &entry : cache) {
    if (entry.key == key) {
      return entry.archetype;
    }
  }

  RenderArchetypeBuilder builder{key};
  for (const auto &draw : draws) {
    builder.add_palette_mesh(draw.mesh, draw.model, draw.palette_slot, nullptr,
                             1.0F, draw.material_id);
  }

  cache.push_back({key, std::move(builder).build()});
  return cache.back().archetype;
}

enum TunicBodyPaletteSlot : std::uint8_t {
  k_body_top_slot = 0U,
  k_body_mid_slot = 1U,
  k_body_lower_slot = 2U,
  k_body_waist_slot = 3U,
  k_body_connector_slot = 4U,
};

enum TunicPauldronPaletteSlot : std::uint8_t {
  k_pauldron_top_slot = 0U,
  k_pauldron_mid_slot = 1U,
  k_pauldron_lower_slot = 2U,
  k_pauldron_bottom_slot = 3U,
  k_pauldron_rivet_slot = 4U,
};

enum TunicTrimPaletteSlot : std::uint8_t {
  k_trim_steel_slot = 0U,
  k_trim_brass_slot = 1U,
};

} // namespace

TunicRenderer::TunicRenderer(const TunicConfig &config) : m_config(config) {}

void TunicRenderer::render(const DrawContext &ctx, const BodyFrames &frames,
                           const HumanoidPalette &palette,
                           const HumanoidAnimationContext &anim,
                           EquipmentBatch &batch) {
  (void)anim;

  const AttachmentFrame &torso = frames.torso;
  const AttachmentFrame &waist = frames.waist;

  if (torso.radius <= 0.0F) {
    return;
  }

  QVector3D const steel_color =
      saturate_color(palette.metal * QVector3D(0.95F, 0.96F, 1.0F));
  QVector3D const brass_color =
      saturate_color(palette.metal * QVector3D(1.3F, 1.1F, 0.7F));

  using HP = HumanProportions;

  TorsoLocalFrame const torso_local = make_torso_local_frame(ctx.model, torso);
  QVector3D const origin = torso.origin;
  QVector3D const right =
      safe_attachment_axis(torso.right, QVector3D(1.0F, 0.0F, 0.0F));
  QVector3D const up =
      safe_attachment_axis(torso.up, QVector3D(0.0F, 1.0F, 0.0F));
  QVector3D const forward =
      safe_attachment_axis(torso.forward, QVector3D(0.0F, 0.0F, 1.0F));

  float const torso_r = torso.radius * m_config.torso_scale;
  float const torso_depth = (torso.depth > 0.0F)
                                ? torso.depth * m_config.chest_depth_scale
                                : torso.radius * m_config.chest_depth_scale;

  auto map_torso_y = [&](float spec_y) {
    float const delta = spec_y - HP::SHOULDER_Y;
    return origin.y() + delta;
  };

  float const y_top = map_torso_y(HP::SHOULDER_Y + 0.02F);
  float const y_mid_chest = map_torso_y((HP::SHOULDER_Y + HP::CHEST_Y) * 0.5F);
  float const y_bottom_chest = map_torso_y(HP::CHEST_Y);
  float const y_waist = map_torso_y(HP::WAIST_Y + 0.06F);

  float const shoulder_width = torso_r * m_config.shoulder_width_scale;
  float const chest_width = torso_r * 1.15F;
  float const waist_width = torso_r * m_config.waist_taper;

  float const chest_depth_front = std::max(0.04F, torso_depth * 1.05F);
  float const chest_depth_back = std::max(0.03F, torso_depth * 0.75F);

  constexpr int k_segments = 16;
  constexpr float k_pi = std::numbers::pi_v<float>;

  std::vector<PaletteDrawSpec> body_draws;
  body_draws.reserve(88);

  auto create_torso_segment = [&](float y_pos, float width_scale,
                                  float depth_front, float depth_back,
                                  std::uint8_t palette_slot) {
    for (int i = 0; i < k_segments; ++i) {
      float const angle1 = (static_cast<float>(i) / k_segments) * 2.0F * k_pi;
      float const angle2 =
          (static_cast<float>(i + 1) / k_segments) * 2.0F * k_pi;

      auto radius_at_angle = [&](float angle_rad) -> float {
        float const cos_a = std::cos(angle_rad);
        float const abs_cos = std::abs(cos_a);
        float const depth = cos_a > 0.0F ? depth_front : depth_back;
        float const shoulder_bias =
            1.0F + 0.15F * std::abs(std::sin(angle_rad));
        return width_scale * shoulder_bias * (abs_cos * 0.3F + 0.7F * depth);
      };

      float const sin1 = std::sin(angle1);
      float const cos1 = std::cos(angle1);
      float const sin2 = std::sin(angle2);
      float const cos2 = std::cos(angle2);
      float const r1 = radius_at_angle(angle1);
      float const r2 = radius_at_angle(angle2);

      QVector3D const p1 = origin + right * (r1 * sin1) +
                           forward * (r1 * cos1) + up * (y_pos - origin.y());
      QVector3D const p2 = origin + right * (r2 * sin2) +
                           forward * (r2 * cos2) + up * (y_pos - origin.y());
      float const seg_r = (r1 + r2) * 0.5F * 0.08F;

      body_draws.push_back(
          {get_unit_cylinder(),
           Render::Geom::cylinder_between(torso_local.point(p1),
                                          torso_local.point(p2), seg_r),
           palette_slot});
    }
  };

  create_torso_segment(y_top, shoulder_width, chest_depth_front,
                       chest_depth_back, k_body_top_slot);
  create_torso_segment(y_mid_chest, chest_width, chest_depth_front,
                       chest_depth_back, k_body_mid_slot);
  create_torso_segment(y_bottom_chest, chest_width * 0.98F,
                       chest_depth_front * 0.95F, chest_depth_back * 0.95F,
                       k_body_lower_slot);
  create_torso_segment(y_waist, waist_width, chest_depth_front * 0.90F,
                       chest_depth_back * 0.90F, k_body_waist_slot);

  auto connect_segments = [&](float y1, float y2, float width1, float width2) {
    for (int i = 0; i < k_segments / 2; ++i) {
      float const angle =
          (static_cast<float>(i) / (k_segments / 2)) * 2.0F * k_pi;
      float const sin_a = std::sin(angle);
      float const cos_a = std::cos(angle);
      float const depth1 = cos_a > 0.0F ? chest_depth_front : chest_depth_back;
      float const depth2 =
          cos_a > 0.0F ? chest_depth_front * 0.95F : chest_depth_back * 0.95F;
      float const r1 = width1 * depth1;
      float const r2 = width2 * depth2;

      QVector3D const top = origin + right * (r1 * sin_a) +
                            forward * (r1 * cos_a) + up * (y1 - origin.y());
      QVector3D const bot = origin + right * (r2 * sin_a) +
                            forward * (r2 * cos_a) + up * (y2 - origin.y());
      body_draws.push_back(
          {get_unit_cylinder(),
           Render::Geom::cylinder_between(
               torso_local.point(top), torso_local.point(bot), torso_r * 0.06F),
           k_body_connector_slot});
    }
  };

  connect_segments(y_top, y_mid_chest, shoulder_width, chest_width);
  connect_segments(y_mid_chest, y_bottom_chest, chest_width,
                   chest_width * 0.98F);
  connect_segments(y_bottom_chest, y_waist, chest_width * 0.98F, waist_width);

  std::array<QVector3D, 5> const body_palette{
      steel_color,         steel_color * 0.99F, steel_color * 0.98F,
      steel_color * 0.97F, steel_color * 0.96F,
  };
  append_equipment_archetype(batch,
                             cached_palette_archetype("tunic_body", body_draws),
                             torso_local.world, body_palette);

  std::vector<PaletteDrawSpec> rivet_draws;
  rivet_draws.reserve(8);
  constexpr float k_rivet_position_scale = 0.92F;
  for (int i = 0; i < 8; ++i) {
    float const angle = (static_cast<float>(i) / 8.0F) * 2.0F * k_pi;
    float const x = chest_width * std::sin(angle) * chest_depth_front *
                    k_rivet_position_scale;
    float const z = chest_width * std::cos(angle) * chest_depth_front *
                    k_rivet_position_scale;
    QVector3D const pos = origin + right * x + forward * z +
                          up * (y_mid_chest + 0.08F - origin.y());
    rivet_draws.push_back({get_unit_sphere(),
                           local_scale_model(torso_local.point(pos),
                                             QVector3D(0.012F, 0.012F, 0.012F)),
                           0U});
  }
  std::array<QVector3D, 1> const rivet_palette{brass_color};
  append_equipment_archetype(
      batch, cached_palette_archetype("tunic_rivets", rivet_draws),
      torso_local.world, rivet_palette);

  if (m_config.include_pauldrons) {
    std::vector<PaletteDrawSpec> pauldron_draws;
    pauldron_draws.reserve(14);
    auto draw_pauldron = [&](const QVector3D &shoulder,
                             const QVector3D &outward) {
      float const upper_arm_r = HP::UPPER_ARM_R;
      for (int i = 0; i < 4; ++i) {
        float const seg_y =
            shoulder.y() + 0.04F - static_cast<float>(i) * 0.045F;
        float const seg_r =
            upper_arm_r * (2.5F - static_cast<float>(i) * 0.12F);
        QVector3D seg_pos =
            shoulder + outward * (0.02F + static_cast<float>(i) * 0.008F);
        seg_pos.setY(seg_y);

        pauldron_draws.push_back(
            {get_unit_sphere(),
             local_scale_model(torso_local.point(seg_pos),
                               QVector3D(seg_r, seg_r, seg_r)),
             static_cast<std::uint8_t>(std::min(i, 3))});

        if (i < 3) {
          QVector3D const rivet_pos = seg_pos + QVector3D(0.0F, 0.015F, 0.03F);
          pauldron_draws.push_back(
              {get_unit_sphere(),
               local_scale_model(torso_local.point(rivet_pos),
                                 QVector3D(0.012F, 0.012F, 0.012F)),
               k_pauldron_rivet_slot});
        }
      }
    };

    draw_pauldron(frames.shoulder_l.origin, -frames.torso.right);
    draw_pauldron(frames.shoulder_r.origin, frames.torso.right);

    std::array<QVector3D, 5> const pauldron_palette{
        steel_color * 1.05F, steel_color * 0.97F, steel_color * 0.94F,
        steel_color * 0.91F, brass_color,
    };
    append_equipment_archetype(
        batch, cached_palette_archetype("tunic_pauldrons", pauldron_draws),
        torso_local.world, pauldron_palette);
  }

  if (m_config.include_gorget) {
    std::vector<PaletteDrawSpec> gorget_draws;
    gorget_draws.reserve(2);

    QVector3D const gorget_top(origin.x(), y_top + 0.025F, origin.z());
    QVector3D const gorget_bot(origin.x(), y_top - 0.012F, origin.z());
    gorget_draws.push_back(
        {get_unit_cylinder(),
         Render::Geom::cylinder_between(torso_local.point(gorget_bot),
                                        torso_local.point(gorget_top),
                                        HP::NECK_RADIUS * 2.6F),
         k_trim_steel_slot});

    QVector3D const trim_a = gorget_top + QVector3D(0.0F, 0.005F, 0.0F);
    QVector3D const trim_b = gorget_top - QVector3D(0.0F, 0.005F, 0.0F);
    gorget_draws.push_back(
        {get_unit_cylinder(),
         Render::Geom::cylinder_between(torso_local.point(trim_b),
                                        torso_local.point(trim_a),
                                        HP::NECK_RADIUS * 2.62F),
         k_trim_brass_slot});

    std::array<QVector3D, 2> const gorget_palette{steel_color * 1.08F,
                                                  brass_color};
    append_equipment_archetype(
        batch, cached_palette_archetype("tunic_gorget", gorget_draws),
        torso_local.world, gorget_palette);
  }

  if (m_config.include_belt) {
    std::vector<PaletteDrawSpec> belt_draws;
    belt_draws.reserve(2);

    float const waist_r = waist.radius * m_config.waist_taper;
    auto waist_y = [&](float spec_y) {
      float const delta = spec_y - HP::WAIST_Y;
      return waist.origin.y() + delta;
    };

    float const y_center = waist_y(HP::WAIST_Y + 0.02F);
    QVector3D const belt_top(waist.origin.x(), y_center + 0.02F,
                             waist.origin.z());
    QVector3D const belt_bot(waist.origin.x(), y_center - 0.02F,
                             waist.origin.z());
    belt_draws.push_back({get_unit_cylinder(),
                          Render::Geom::cylinder_between(
                              torso_local.point(belt_bot),
                              torso_local.point(belt_top), waist_r * 1.08F),
                          k_trim_steel_slot});

    QVector3D const trim_top = belt_top + QVector3D(0.0F, 0.005F, 0.0F);
    QVector3D const trim_bot = belt_bot - QVector3D(0.0F, 0.005F, 0.0F);
    belt_draws.push_back({get_unit_cylinder(),
                          Render::Geom::cylinder_between(
                              torso_local.point(trim_bot),
                              torso_local.point(trim_top), waist_r * 1.12F),
                          k_trim_brass_slot});

    std::array<QVector3D, 2> const belt_palette{steel_color * 0.94F,
                                                brass_color * 0.95F};
    append_equipment_archetype(
        batch, cached_palette_archetype("tunic_belt", belt_draws),
        torso_local.world, belt_palette);
  }
}

} // namespace Render::GL
