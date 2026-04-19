

#include "humanoid_renderer_base.h"

#include "../../game/units/troop_config.h"
#include "../../game/visuals/team_colors.h"
#include "../geom/math_utils.h"
#include "../geom/parts.h"
#include "../geom/transforms.h"
#include "../gl/backend.h"
#include "../gl/camera.h"
#include "../gl/humanoid/humanoid_constants.h"
#include "../gl/primitives.h"
#include "../gl/render_constants.h"
#include "../gl/resources.h"
#include "../graphics_settings.h"
#include "../palette.h"
#include "../submitter.h"
#include "humanoid_math.h"
#include "humanoid_spec.h"
#include "humanoid_specs.h"
#include "rig_stats_shim.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>
#include <QtMath>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <span>
#include <vector>

namespace Render::GL {

using namespace Render::GL::Geometry;
using Render::Geom::cone_from_to;
using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;

namespace {
struct BeardPrim {
  Render::GL::Mesh *mesh{nullptr};
  QMatrix4x4 model{};
  QVector3D color{};
  float alpha{1.0F};
  int material_id{0};
};
} // namespace

void HumanoidRendererBase::draw_armor_overlay(
    const DrawContext &, const HumanoidVariant &, const HumanoidPose &, float,
    float, float, float, const QVector3D &, ISubmitter &) const {}

void HumanoidRendererBase::draw_shoulder_decorations(
    const DrawContext &, const HumanoidVariant &, const HumanoidPose &, float,
    float, const QVector3D &, ISubmitter &) const {}

void HumanoidRendererBase::draw_facial_hair(const DrawContext &ctx,
                                            const HumanoidVariant &v,
                                            const HumanoidPose &pose,
                                            ISubmitter &out) const {
  const FacialHairParams &fh = v.facial_hair;

  if (fh.style == FacialHairStyle::None || fh.coverage < 0.01F) {
    return;
  }

  const auto &gfx_settings = Render::GraphicsSettings::instance();
  if (!gfx_settings.features().enable_facial_hair) {
    return;
  }

  constexpr float k_facial_hair_max_distance = 12.0F;
  if (ctx.camera != nullptr) {
    QVector3D const soldier_world_pos =
        ctx.model.map(QVector3D(0.0F, 0.0F, 0.0F));
    float const distance =
        (soldier_world_pos - ctx.camera->get_position()).length();
    if (distance > k_facial_hair_max_distance) {
      detail::increment_facial_hair_skipped_distance();
      return;
    }
  }

  const AttachmentFrame &frame = pose.body_frames.head;
  float const head_r = frame.radius;
  if (head_r <= 0.0F) {
    return;
  }

  auto saturate = [](const QVector3D &c) -> QVector3D {
    return {std::clamp(c.x(), 0.0F, 1.0F), std::clamp(c.y(), 0.0F, 1.0F),
            std::clamp(c.z(), 0.0F, 1.0F)};
  };

  QVector3D hair_color = fh.color * (1.0F - fh.greyness) +
                         QVector3D(0.75F, 0.75F, 0.75F) * fh.greyness;
  QVector3D hair_dark = hair_color * 0.80F;
  QVector3D const hair_root = hair_dark * 0.95F;
  QVector3D const hair_tip = hair_color * 1.08F;

  float const chin_y = -head_r * 0.95F;
  float const mouth_y = -head_r * 0.18F;
  float const jaw_z = head_r * 0.68F;

  float const chin_norm = chin_y / head_r;
  float const mouth_norm = mouth_y / head_r;
  float const jaw_forward_norm = jaw_z / head_r;

  uint32_t rand_state = 0x9E3779B9U;
  if (ctx.entity != nullptr) {
    uintptr_t ptr = reinterpret_cast<uintptr_t>(ctx.entity);
    rand_state ^= static_cast<uint32_t>(ptr & 0xFFFFFFFFU);
    rand_state ^= static_cast<uint32_t>((ptr >> 32) & 0xFFFFFFFFU);
  }
  rand_state ^= static_cast<uint32_t>(fh.length * 9973.0F);
  rand_state ^= static_cast<uint32_t>(fh.thickness * 6151.0F);
  rand_state ^= static_cast<uint32_t>(fh.coverage * 4099.0F);

  auto random01 = [&]() -> float {
    rand_state = rand_state * 1664525U + 1013904223U;
    return hash_01(rand_state);
  };

  auto jitter = [&](float amplitude) -> float {
    return (random01() - 0.5F) * 2.0F * amplitude;
  };

  float const beard_forward_tilt_norm = 0.32F;

  constexpr int k_max_facial_hair_strands = 24;
  int total_strands_emitted = 0;

  std::vector<BeardPrim> beard_prims;
  beard_prims.reserve(static_cast<std::size_t>(k_max_facial_hair_strands) * 3U);

  auto place_strands = [&](int rows, int segments, float jaw_span,
                           float row_spacing_norm, float base_length_norm,
                           float length_variation, float forward_bias_norm,
                           float base_radius_norm) {
    if (rows <= 0 || segments <= 0) {
      return;
    }

    const float phi_half_range = std::max(0.35F, jaw_span * 0.5F);
    const float base_y_norm = chin_norm + 0.10F;
    for (int row = 0; row < rows; ++row) {
      float const row_t = (rows > 1) ? float(row) / float(rows - 1) : 0.0F;
      float const target_y_norm =
          std::clamp(base_y_norm + row_t * row_spacing_norm, -0.92F, 0.10F);
      float const plane_radius =
          std::sqrt(std::max(0.02F, 1.0F - target_y_norm * target_y_norm));
      for (int seg = 0; seg < segments; ++seg) {
        float const seg_t =
            (segments > 1) ? float(seg) / float(segments - 1) : 0.5F;
        float const base_phi = (seg_t - 0.5F) * jaw_span;
        float const phi = std::clamp(base_phi + jitter(0.25F), -phi_half_range,
                                     phi_half_range);
        float const coverage_falloff =
            1.0F - std::abs(phi) / std::max(0.001F, phi_half_range);
        float const keep_prob = std::clamp(
            fh.coverage * (0.75F + coverage_falloff * 0.35F), 0.05F, 1.0F);
        if (random01() > keep_prob) {
          continue;
        }
        if (total_strands_emitted >= k_max_facial_hair_strands) {
          return;
        }

        float const wrap_scale = 0.80F + (1.0F - row_t) * 0.20F;
        float lateral_norm = plane_radius * std::sin(phi) * wrap_scale;
        float forward_norm = plane_radius * std::cos(phi);
        lateral_norm += jitter(0.06F);
        forward_norm += jitter(0.08F);
        float const y_norm = target_y_norm + jitter(0.05F);

        QVector3D surface_dir(lateral_norm, y_norm,
                              forward_norm *
                                      (0.75F + forward_bias_norm * 0.45F) +
                                  (forward_bias_norm - 0.05F));
        float const dir_len = surface_dir.length();
        if (dir_len < 1e-4F) {
          continue;
        }
        surface_dir /= dir_len;

        float const shell = 1.02F + jitter(0.03F);
        QVector3D const root = frame_local_position(frame, surface_dir * shell);

        QVector3D local_dir(jitter(0.15F),
                            -(0.55F + row_t * 0.30F) + jitter(0.10F),
                            forward_bias_norm + beard_forward_tilt_norm +
                                row_t * 0.20F + jitter(0.12F));
        QVector3D strand_dir =
            frame.right * local_dir.x() + frame.up * local_dir.y() +
            frame.forward * local_dir.z() - surface_dir * 0.25F;
        if (strand_dir.lengthSquared() < 1e-6F) {
          continue;
        }
        strand_dir.normalize();

        float const strand_length = base_length_norm * fh.length *
                                    (1.0F + length_variation * jitter(0.5F)) *
                                    (1.0F + row_t * 0.25F);
        if (strand_length < 0.05F) {
          continue;
        }

        QVector3D const tip = root + strand_dir * (head_r * strand_length);

        float const base_radius =
            std::max(head_r * base_radius_norm * fh.thickness *
                         (0.7F + coverage_falloff * 0.35F),
                     head_r * 0.010F);
        float const mid_radius = base_radius * 0.55F;

        float const color_jitter = 0.85F + random01() * 0.30F;
        QVector3D const root_color = saturate(hair_root * color_jitter);
        QVector3D const tip_color = saturate(hair_tip * color_jitter);

        QMatrix4x4 base_blob = sphere_at(ctx.model, root, base_radius * 0.95F);
        beard_prims.push_back(
            {get_unit_sphere(), base_blob, root_color, 1.0F, 0});

        QVector3D const mid = root + (tip - root) * 0.40F;
        beard_prims.push_back(
            {get_unit_cylinder(),
             cylinder_between(ctx.model, root, mid, base_radius), root_color,
             1.0F, 0});

        beard_prims.push_back({get_unit_cone(),
                               cone_from_to(ctx.model, mid, tip, mid_radius),
                               tip_color, 1.0F, 0});
        ++total_strands_emitted;
      }
    }
  };

  auto place_mustache = [&](int segments, float base_length_norm,
                            float upward_bias_norm) {
    if (segments <= 0) {
      return;
    }

    float const mustache_y_norm = mouth_norm + upward_bias_norm - 0.04F;
    float const phi_half_range = 0.55F;
    for (int side = -1; side <= 1; side += 2) {
      for (int seg = 0; seg < segments; ++seg) {
        float const t =
            (segments > 1) ? float(seg) / float(segments - 1) : 0.5F;
        float const base_phi = (t - 0.5F) * (phi_half_range * 2.0F);
        float const phi = std::clamp(base_phi + jitter(0.18F), -phi_half_range,
                                     phi_half_range);
        float const plane_radius = std::sqrt(
            std::max(0.02F, 1.0F - mustache_y_norm * mustache_y_norm));
        float lateral_norm = plane_radius * std::sin(phi);
        float forward_norm = plane_radius * std::cos(phi);
        lateral_norm += jitter(0.04F);
        forward_norm += jitter(0.05F);
        if (random01() > fh.coverage) {
          continue;
        }
        if (total_strands_emitted >= k_max_facial_hair_strands) {
          return;
        }
        QVector3D surface_dir(lateral_norm, mustache_y_norm + jitter(0.03F),
                              forward_norm * 0.85F + 0.20F);
        float const dir_len = surface_dir.length();
        if (dir_len < 1e-4F) {
          continue;
        }
        surface_dir /= dir_len;
        QVector3D const root =
            frame_local_position(frame, surface_dir * (1.01F + jitter(0.02F)));

        QVector3D const dir_local(side * (0.55F + jitter(0.12F)), jitter(0.06F),
                                  0.34F + jitter(0.08F));
        QVector3D strand_dir =
            frame.right * dir_local.x() + frame.up * dir_local.y() +
            frame.forward * dir_local.z() - surface_dir * 0.20F;
        if (strand_dir.lengthSquared() < 1e-6F) {
          continue;
        }
        strand_dir.normalize();

        float const strand_length =
            base_length_norm * fh.length * (1.0F + jitter(0.35F));
        QVector3D const tip = root + strand_dir * (head_r * strand_length);

        float const base_radius =
            std::max(head_r * 0.028F * fh.thickness, head_r * 0.0065F);
        float const mid_radius = base_radius * 0.45F;
        float const color_jitter = 0.92F + random01() * 0.18F;
        QVector3D const root_color =
            saturate(hair_root * (color_jitter * 0.95F));
        QVector3D const tip_color = saturate(hair_tip * (color_jitter * 1.02F));

        beard_prims.push_back({get_unit_sphere(),
                               sphere_at(ctx.model, root, base_radius * 0.7F),
                               root_color, 1.0F, 0});

        QVector3D const mid = root + (tip - root) * 0.5F;
        beard_prims.push_back(
            {get_unit_cylinder(),
             cylinder_between(ctx.model, root, mid, base_radius * 0.85F),
             root_color, 1.0F, 0});
        beard_prims.push_back({get_unit_cone(),
                               cone_from_to(ctx.model, mid, tip, mid_radius),
                               tip_color, 1.0F, 0});
        ++total_strands_emitted;
      }
    }
  };

  switch (fh.style) {
  case FacialHairStyle::Stubble: {
    place_strands(1, 11, 2.0F, 0.12F, 0.28F, 0.30F, 0.08F, 0.035F);
    break;
  }

  case FacialHairStyle::ShortBeard: {
    place_strands(3, 14, 2.6F, 0.18F, 0.58F, 0.38F, 0.12F, 0.055F);
    break;
  }

  case FacialHairStyle::FullBeard:
  case FacialHairStyle::LongBeard: {
    bool const is_long = (fh.style == FacialHairStyle::LongBeard);
    if (is_long) {
      place_strands(5, 20, 3.0F, 0.24F, 1.00F, 0.48F, 0.18F, 0.060F);
    } else {
      place_strands(4, 18, 2.8F, 0.22F, 0.85F, 0.42F, 0.16F, 0.058F);
    }
    break;
  }

  case FacialHairStyle::Goatee: {
    place_strands(2, 8, 1.8F, 0.16F, 0.70F, 0.34F, 0.14F, 0.055F);
    break;
  }

  case FacialHairStyle::Mustache: {
    place_mustache(5, 0.32F, 0.05F);
    break;
  }

  case FacialHairStyle::MustacheAndBeard: {
    FacialHairParams mustache_only = fh;
    mustache_only.style = FacialHairStyle::Mustache;
    FacialHairParams beard_only = fh;
    beard_only.style = FacialHairStyle::ShortBeard;

    HumanoidVariant v_mustache = v;
    v_mustache.facial_hair = mustache_only;
    draw_facial_hair(ctx, v_mustache, pose, out);

    HumanoidVariant v_beard = v;
    v_beard.facial_hair = beard_only;
    draw_facial_hair(ctx, v_beard, pose, out);
    break;
  }

  case FacialHairStyle::None:
  default:
    break;
  }

  for (std::size_t i = 0; i < beard_prims.size(); ++i) {
    const auto &p = beard_prims[i];
    if (p.mesh != nullptr) {
      out.mesh(p.mesh, p.model, p.color, nullptr, p.alpha, p.material_id);
    }
  }
}

} // namespace Render::GL
