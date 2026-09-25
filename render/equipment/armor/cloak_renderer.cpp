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

#include "render/equipment/attachment_builder.h"
#include "render/equipment/equipment_submit.h"
#include "render/gl/primitives.h"
#include "render/gl/shared_geometry_cache.h"
#include "render/humanoid/asset/humanoid_spec.h"
#include "render/humanoid/schema/skeleton_schema.h"
#include "render/static_attachment_spec.h"
#include "sheet_mesh.h"
#include "torso_local_archetype_utils.h"

namespace Render::GL {

namespace {

enum CloakPaletteSlot : std::uint8_t {
  k_cloak_cloth_slot = 0U,
  k_cloak_trim_slot = 1U,
};

constexpr float k_pi = std::numbers::pi_v<float>;

struct CloakPlacement {
  QMatrix4x4 cloak_model;
  std::array<QMatrix4x4, 2> clasp_models{};
  bool has_clasp = false;
};

auto safe_normalized(const QVector3D& value, const QVector3D& fallback) -> QVector3D {
  if (value.lengthSquared() <= 1e-8F) {
    return fallback;
  }
  return value.normalized();
}

// The cloak is authored against the reference body the static attachment is
// baked on (humanoid_bind_body_frames): shoulders 0.252 m either side of the
// torso origin, torso radius 0.1867 m. Coordinates below are metres in the
// torso-local frame (x right, y up, z forward) with the origin at the middle
// of the shoulder line. The placement rescales x by the actual shoulder span
// and y/z by the actual torso radius, so other bodies still get a fitted
// cloak.
constexpr float k_reference_shoulder_half_span = 0.252F;
constexpr float k_reference_torso_radius = 0.1867F;
constexpr float k_cloth_thickness = 0.0055F;
constexpr float k_brooch_radius = 0.0165F;

// One horizontal cross-section of the cloth. The section is an elliptical arc
// wrapped round the back of the body: `back_z` is where it crosses the spine,
// (`side_x`, `side_z`) where its front edge sits, `half_arc` how far round the
// body it reaches, and the cloth is `back_y` high on the spine and `side_y`
// high at the edge.
struct CloakSection {
  float back_y;
  float side_y;
  float back_z;
  float side_x;
  float side_z;
  float half_arc;
};

auto lerp(float a, float b, float t) -> float {
  return a + (b - a) * t;
}

auto catmull_rom(float p0, float p1, float p2, float p3, float t) -> float {
  float const t2 = t * t;
  float const t3 = t2 * t;
  return 0.5F *
         ((2.0F * p1) + (-p0 + p2) * t + (2.0F * p0 - 5.0F * p1 + 4.0F * p2 - p3) * t2 +
          (-p0 + 3.0F * p1 - 3.0F * p2 + p3) * t3);
}

auto blend_sections(const CloakSection& a,
                    const CloakSection& b,
                    const CloakSection& c,
                    const CloakSection& d,
                    float t) -> CloakSection {
  return {catmull_rom(a.back_y, b.back_y, c.back_y, d.back_y, t),
          catmull_rom(a.side_y, b.side_y, c.side_y, d.side_y, t),
          catmull_rom(a.back_z, b.back_z, c.back_z, d.back_z, t),
          catmull_rom(a.side_x, b.side_x, c.side_x, d.side_x, t),
          catmull_rom(a.side_z, b.side_z, c.side_z, d.side_z, t),
          catmull_rom(a.half_arc, b.half_arc, c.half_arc, d.half_arc, t)};
}

struct CloakShape {
  std::array<CloakSection, 6> keys{};
  // Rows generated between consecutive keys; the shoulder keys are close
  // together and curve hard, the drape keys are far apart and nearly straight.
  std::array<int, 5> rows_between{};
  int drape_key = 3;
};

auto make_cloak_shape(const CloakConfig& config) -> CloakShape {
  float const r = k_reference_torso_radius;
  float const collar_lift = (config.shoulder_anchor_up - 0.12F) * 0.25F * r;
  float const drape_lift = (config.drape_anchor_up - 0.12F) * 0.50F * r;
  float const hang_back = std::max(0.0F, config.drape_anchor_back - 0.54F) * 0.80F * r;
  float const width = std::clamp(config.width_scale, 0.6F, 1.4F);
  float const length = std::max(0.3F, config.length_scale);

  float const hem_y = 0.035F - 0.795F * length + drape_lift;
  float const mid_y = lerp(-0.040F, hem_y, 0.30F);

  CloakShape shape;
  // The cloak is gathered narrow where it is pinned, tucked under the
  // shoulder guards, and falls in a widening cone to the hem. Its back slopes
  // away from the body on the way down, as heavy wool does when it hangs
  // from the shoulder blades rather than lying on the spine.
  shape.keys = {{
      // Collar: lies round the base of the neck and comes forward over the
      // trapezius to the brooches at the front of each shoulder.
      {0.094F + collar_lift, 0.068F + collar_lift, -0.086F, 0.078F, 0.044F, 1.95F},
      // Over the trapezius, following the slope out from the neck.
      {0.070F + collar_lift, 0.050F + collar_lift, -0.128F, 0.160F, 0.018F, 1.80F},
      // Across the top of the shoulder blades, inside the shoulder guards.
      {0.024F, 0.004F, -0.162F, 0.212F, -0.040F, 1.52F},
      // Below the shoulder blades: from here the cloth only hangs.
      {-0.070F + drape_lift, -0.100F + drape_lift, -0.180F, 0.246F, -0.105F, 1.30F},
      {mid_y,
       mid_y - 0.030F,
       -0.205F - 0.35F * hang_back,
       0.300F * lerp(1.0F, width, 0.5F),
       -0.130F - 0.30F * hang_back,
       1.22F},
      {hem_y,
       hem_y + 0.040F,
       -0.250F - hang_back,
       0.385F * width,
       -0.150F - 0.80F * hang_back,
       1.16F},
  }};
  shape.rows_between = {2, 3, 3, 5, 9};
  return shape;
}

auto make_cloak_grid(const CloakConfig& config) -> SheetGrid {
  CloakShape const shape = make_cloak_shape(config);
  constexpr int k_columns = 25;

  std::vector<CloakSection> sections;
  std::vector<float> drape_t;
  auto const key_count = static_cast<int>(shape.keys.size());
  float const drape_top = shape.keys[static_cast<std::size_t>(shape.drape_key)].back_y;
  float const hem = shape.keys.back().back_y;
  for (int k = 0; k + 1 < key_count; ++k) {
    auto key = [&](int i) -> const CloakSection& {
      return shape.keys[static_cast<std::size_t>(std::clamp(i, 0, key_count - 1))];
    };
    int const steps = shape.rows_between[static_cast<std::size_t>(k)];
    for (int s = 0; s < steps; ++s) {
      float const t = static_cast<float>(s) / static_cast<float>(steps);
      sections.push_back(blend_sections(key(k - 1), key(k), key(k + 1), key(k + 2), t));
    }
  }
  sections.push_back(shape.keys.back());
  for (const auto& section : sections) {
    float const t = (drape_top - section.back_y) / std::max(1e-4F, drape_top - hem);
    drape_t.push_back(std::clamp(t, 0.0F, 1.0F));
  }

  SheetGrid grid;
  grid.columns = k_columns;
  grid.rows = static_cast<int>(sections.size());
  grid.positions.reserve(static_cast<std::size_t>(grid.columns * grid.rows));
  grid.uvs.reserve(grid.positions.capacity());

  float accumulated_v = 0.0F;
  float const total_drop = std::max(1e-4F, sections.front().back_y - hem);
  for (int row = 0; row < grid.rows; ++row) {
    const CloakSection& sec = sections[static_cast<std::size_t>(row)];
    float const t = drape_t[static_cast<std::size_t>(row)];
    float const cos_max = std::cos(sec.half_arc);
    float const sin_max = std::sin(sec.half_arc);
    float const b = (sec.side_z - sec.back_z) / std::max(1e-4F, 1.0F - cos_max);
    float const zc = sec.back_z + b;
    float const a = sec.side_x / std::max(1e-4F, sin_max);
    accumulated_v = (sections.front().back_y - sec.back_y) / total_drop;

    // Folds start as a faint rumple where the cloth is gathered at the
    // shoulders and open into deep pleats towards the hem. Because the pleats
    // are spaced across the cloak's width they fan out as it widens, so they
    // radiate from the pins the way cloth does.
    float const fold_depth = 0.0035F + 0.0440F * std::pow(t, 1.3F);
    float const hem_wave = 0.020F * t * t * t;

    for (int col = 0; col < grid.columns; ++col) {
      float const u = static_cast<float>(col) / static_cast<float>(grid.columns - 1);
      float const across = u * 2.0F - 1.0F;
      float const theta = across * sec.half_arc;
      float const side_weight = 1.0F - std::cos(theta);
      float const side_norm = std::max(1e-4F, 1.0F - cos_max);

      float x = a * std::sin(theta);
      float z = zc - b * std::cos(theta);
      float y = lerp(sec.back_y, sec.side_y, side_weight / side_norm);

      // Outward normal of the section ellipse, used to push folds out of the
      // surface without changing where the cloth rests on the body.
      QVector3D outward(
          x / std::max(1e-4F, a * a), 0.0F, (z - zc) / std::max(1e-4F, b * b));
      outward = safe_normalized(outward, QVector3D(0.0F, 0.0F, -1.0F));

      // Three broad pleats with a ridge down the spine, and a finer ripple
      // riding on them so the folds do not read as a regular corrugation.
      float const pleat = 0.74F * std::cos(across * k_pi * 3.0F) +
                          0.26F * std::cos(across * k_pi * 7.0F + 0.6F * across);
      float const edge_fade = 1.0F - std::pow(std::abs(across), 5.0F) * 0.55F;
      float const fold = fold_depth * pleat * edge_fade;
      x += outward.x() * fold;
      z += outward.z() * fold;
      // The hem scallops with the pleats and curves up towards the front
      // edges, where the cloth wraps round the body.
      y += hem_wave * (pleat - 1.0F) * 0.5F;
      y += 0.060F * t * t * across * across;

      grid.positions.emplace_back(x, y, z);
      grid.uvs.emplace_back(u, accumulated_v);
    }
  }
  return grid;
}

auto cloak_mesh_key(const CloakConfig& config) -> std::uint64_t {
  auto q = [](float v) {
    return static_cast<std::uint64_t>(std::lround(v * 200.0F) & 0x3FF);
  };
  std::uint64_t key = q(config.length_scale);
  key = (key << 10U) | q(config.width_scale);
  key = (key << 10U) | q(config.shoulder_anchor_up + 1.0F);
  key = (key << 10U) | q(config.drape_anchor_up + 1.0F);
  key = (key << 10U) | q(config.drape_anchor_back);
  return key;
}

auto cloak_mesh_for(const CloakConfig& config) -> Mesh* {
  return SharedGeometryCache::instance().get_or_build(
      geometry_key("equipment/cloak/fitted", cloak_mesh_key(config)), [config] {
        // Columns run left to right round the back and rows run down, so
        // cross(du, dv) already points away from the body.
        return make_thick_sheet_mesh(make_cloak_grid(config), false, k_cloth_thickness);
      });
}

auto make_cloak_placement(const CloakConfig& config,
                          const TorsoLocalFrame& torso_local,
                          const AttachmentFrame& torso,
                          const AttachmentFrame& shoulder_l,
                          const AttachmentFrame& shoulder_r) -> CloakPlacement {
  QVector3D const up = safe_attachment_axis(torso.up, QVector3D(0.0F, 1.0F, 0.0F));
  QVector3D const forward =
      safe_attachment_axis(torso.forward, QVector3D(0.0F, 0.0F, 1.0F));

  float const torso_r = torso.radius;
  float shoulder_half_span = 0.5F * (shoulder_r.origin - shoulder_l.origin).length();
  if (shoulder_half_span < 1e-4F) {
    shoulder_half_span =
        torso_r * (k_reference_shoulder_half_span / k_reference_torso_radius);
  }
  QVector3D const shoulder_mid = (shoulder_l.origin + shoulder_r.origin) * 0.5F;
  QVector3D const anchor = torso_local.point(shoulder_mid);

  float const sx = shoulder_half_span / k_reference_shoulder_half_span;
  float const syz = torso_r / k_reference_torso_radius;

  CloakPlacement placement;
  placement.cloak_model.translate(anchor);
  placement.cloak_model.scale(sx, syz, syz);

  if (config.show_clasp) {
    // Brooches pin the cloak at the front of the collar on both shoulders.
    CloakSection const collar = make_cloak_shape(config).keys.front();
    for (int side = 0; side < 2; ++side) {
      float const s = side == 0 ? -1.0F : 1.0F;
      QVector3D local(s * collar.side_x, collar.side_y, collar.side_z);
      local +=
          QVector3D(0.0F,
                    (config.clasp_anchor_up - 0.14F) * k_reference_torso_radius,
                    (config.clasp_anchor_forward - 0.14F) * k_reference_torso_radius +
                        k_cloth_thickness);
      QVector3D const scaled(anchor.x() + local.x() * sx,
                             anchor.y() + local.y() * syz,
                             anchor.z() + local.z() * syz);
      (void)up;
      (void)forward;
      placement.clasp_models[static_cast<std::size_t>(side)] =
          local_scale_model(scaled,
                            QVector3D(k_brooch_radius * syz,
                                      k_brooch_radius * syz,
                                      k_brooch_radius * 0.55F * syz));
    }
    placement.has_clasp = true;
  }

  return placement;
}

auto cloak_archetype(const CloakConfig& config,
                     const CloakMeshes& meshes,
                     const CloakPlacement& placement) -> const RenderArchetype& {
  struct CachedArchetype {
    std::string key;
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  std::string key = "cloak_";
  key += std::to_string(reinterpret_cast<std::uintptr_t>(meshes.cloak));
  key.push_back('_');
  append_quantized_key(key, config.back_material_id);
  append_quantized_key(key, placement.cloak_model);
  append_quantized_key(key, placement.has_clasp ? 1 : 0);
  if (placement.has_clasp) {
    for (const auto& clasp : placement.clasp_models) {
      append_quantized_key(key, clasp);
    }
  }

  for (const auto& entry : cache) {
    if (entry.key == key) {
      return entry.archetype;
    }
  }

  RenderArchetypeBuilder builder{key};
  builder.add_palette_mesh(meshes.cloak,
                           placement.cloak_model,
                           k_cloak_cloth_slot,
                           nullptr,
                           1.0F,
                           config.back_material_id);
  if (placement.has_clasp) {
    for (const auto& clasp : placement.clasp_models) {
      builder.add_palette_mesh(
          get_unit_sphere(), clasp, k_cloak_trim_slot, nullptr, 1.0F, 1);
    }
  }

  cache.push_back({key, std::move(builder).build()});
  return cache.back().archetype;
}

} // namespace

auto shared_cloak_meshes(const CloakConfig& config) -> CloakMeshes {
  return {cloak_mesh_for(config)};
}

CloakRenderer::CloakRenderer(const CloakConfig& config)
    : m_config(config) {
}

auto CloakRenderer::meshes() const noexcept -> CloakMeshes {
  return shared_cloak_meshes(m_config);
}

void CloakRenderer::set_config(const CloakConfig& config) {
  m_config = config;
}

void CloakRenderer::render(const DrawContext& ctx,
                           const BodyFrames& frames,
                           const HumanoidPalette& palette,
                           const HumanoidAnimationContext& anim,
                           EquipmentBatch& batch) {
  submit(m_config, shared_cloak_meshes(m_config), ctx, frames, palette, anim, batch);
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
  if (torso.radius <= 0.0F || meshes.cloak == nullptr) {
    return;
  }

  TorsoLocalFrame const torso_local = make_torso_local_frame(ctx.model, torso);
  CloakPlacement const placement = make_cloak_placement(
      config, torso_local, torso, frames.shoulder_l, frames.shoulder_r);

  std::array<QVector3D, 2> palette_slots{palette.cloth, palette.metal};
  append_equipment_archetype(batch,
                             cloak_archetype(config, meshes, placement),
                             torso_local.world,
                             palette_slots);
}

auto cloak_mantle_color(const CloakConfig& config,
                        const HumanoidPalette& palette) -> QVector3D {
  float const blend = std::clamp(config.team_blend, 0.0F, 1.0F);
  if (blend <= 0.0F) {
    return config.primary_color;
  }

  QVector3D const team_cloth = palette.cloth * std::max(0.0F, config.team_shade);
  QVector3D const mixed =
      (config.primary_color * (1.0F - blend)) + (team_cloth * blend);
  return {std::clamp(mixed.x(), 0.0F, 1.0F),
          std::clamp(mixed.y(), 0.0F, 1.0F),
          std::clamp(mixed.z(), 0.0F, 1.0F)};
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

  TorsoLocalFrame const torso_local = make_torso_local_frame(QMatrix4x4{}, torso);
  CloakPlacement const placement = make_cloak_placement(
      config, torso_local, torso, bind_frames.shoulder_l, bind_frames.shoulder_r);

  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &cloak_archetype(config, meshes, placement),
      .socket_bone_index = torso_socket_bone_index,
      .unit_local_pose_at_bind = torso_local.world,
  });
  // Below the shoulder blades the cloth hands its weight from the chest to
  // the hips and thighs, so the lower cloak hangs instead of pitching with
  // every lean and twist of the torso.
  using Render::Humanoid::HumanoidBone;
  float const shoulder_y =
      0.5F * (bind_frames.shoulder_l.origin.y() + bind_frames.shoulder_r.origin.y());
  spec.drape.enabled = true;
  spec.drape.pelvis_bone = static_cast<std::uint16_t>(HumanoidBone::Pelvis);
  spec.drape.leg_l_bone = static_cast<std::uint16_t>(HumanoidBone::HipL);
  spec.drape.leg_r_bone = static_cast<std::uint16_t>(HumanoidBone::HipR);
  spec.drape.top_y = shoulder_y - torso.radius * 1.10F;
  spec.drape.bottom_y = bind_frames.waist.origin.y() - torso.radius * 1.10F;
  spec.drape.leg_share = 0.35F;
  spec.drape.leg_crossfade_half_width = 0.16F;
  spec.palette_role_remap[k_cloak_cloth_slot] =
      static_cast<std::uint8_t>(base_role_byte + 0U);
  spec.palette_role_remap[k_cloak_trim_slot] =
      static_cast<std::uint8_t>(base_role_byte + 1U);

  return spec;
}

} // namespace Render::GL
