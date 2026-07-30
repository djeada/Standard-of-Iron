#include "healer_renderer.h"

#include <QMatrix4x4>
#include <QString>
#include <QVector3D>
#include <qmatrix4x4.h>
#include <qstringliteral.h>
#include <qvectornd.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "../../../../game/core/component.h"
#include "../../../../game/core/entity.h"
#include "../../../../game/systems/nation_id.h"
#include "../../../creature/archetype_registry.h"
#include "../../../creature/pipeline/unit_visual_spec.h"
#include "../../../equipment/armor/torso_local_archetype_utils.h"
#include "../../../equipment/attachment_builder.h"
#include "../../../equipment/generated_equipment.h"
#include "../../../equipment/humanoid_attachment_archetype.h"
#include "../../../equipment/humanoid_equipment_archetype.h"
#include "../../../geom/transforms.h"
#include "../../../gl/backend.h"
#include "../../../gl/primitives.h"
#include "../../../gl/render_constants.h"
#include "../../../gl/shader.h"
#include "../../../humanoid/humanoid_math.h"
#include "../../../humanoid/humanoid_proportion_profiles.h"
#include "../../../humanoid/humanoid_renderer_base.h"
#include "../../../humanoid/humanoid_spec.h"
#include "../../../humanoid/skeleton.h"
#include "../../../humanoid/style_palette.h"
#include "../../../palette.h"
#include "../../../scene_renderer.h"
#include "../../../submitter.h"
#include "../../healer_renderer_common.h"
#include "../../registry.h"
#include "../../renderer_constants.h"
#include "../equipment_loadout_catalog.h"
#include "animation/rig/humanoid_proportions.h"
#include "healer_style.h"
#include "math/math_utils.h"

using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;

namespace Render::GL::Roman {

namespace {

constexpr auto k_profile =
    Render::GL::Humanoid::k_support_proportion_profile.with_offset({.x = -0.01F});

constexpr std::uint32_t k_senator_role_count = 6;

constexpr auto k_senator_base_role =
    static_cast<std::uint8_t>(Render::Humanoid::k_humanoid_role_count + 1U);

enum SenatorPaletteSlot : std::uint8_t {
  k_toga_cloth_slot = 0U,
  k_toga_shade_slot = 1U,
  k_toga_clavus_slot = 2U,
  k_toga_gold_slot = 3U,
  k_toga_leather_slot = 4U,
  k_toga_laurel_slot = 5U,
};

auto senator_fill_role_colors(const HumanoidPalette& palette,
                              QVector3D* out,
                              std::size_t max) -> std::uint32_t {
  if (max < k_senator_role_count) {
    return 0U;
  }
  out[k_toga_cloth_slot] = QVector3D(0.94F, 0.92F, 0.86F);
  out[k_toga_shade_slot] = QVector3D(0.79F, 0.76F, 0.69F);
  out[k_toga_clavus_slot] = Render::GL::Humanoid::saturate_color(
      palette.cloth * 0.62F + QVector3D(0.15F, 0.01F, 0.17F));
  out[k_toga_gold_slot] = Render::GL::Humanoid::saturate_color(
      palette.metal * 0.45F + QVector3D(0.46F, 0.34F, 0.10F));
  out[k_toga_leather_slot] = palette.leather;
  out[k_toga_laurel_slot] = QVector3D(0.27F, 0.40F, 0.20F);
  return k_senator_role_count;
}

auto senator_extra_role_colors(const void* variant_void,
                               QVector3D* out,
                               std::uint32_t base_count,
                               std::size_t max_count) -> std::uint32_t {
  if (variant_void == nullptr || max_count <= base_count) {
    return base_count;
  }
  const auto& variant = *static_cast<const HumanoidVariant*>(variant_void);
  return base_count + senator_fill_role_colors(
                          variant.palette, out + base_count, max_count - base_count);
}

auto senator_toga_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    const auto& bind_frames = Render::Humanoid::humanoid_bind_body_frames();
    const AttachmentFrame& torso = bind_frames.torso;
    const AttachmentFrame& waist = bind_frames.waist;
    const TorsoLocalFrame torso_local = make_torso_local_frame(QMatrix4x4{}, torso);

    float const tr = torso.radius;

    float const y_top = 0.010F;
    float const y_waist = torso_local.point(waist.origin).y();
    float const y_knee = y_waist - 0.52F;

    float const chest_w = tr * 0.99F;
    float const chest_d = tr * 0.70F;
    float const waist_w = tr * 1.04F;
    float const waist_d = tr * 0.74F;
    float const skirt_mid_w = tr * 1.56F;
    float const skirt_mid_d = tr * 1.08F;
    float const hem_w = tr * 2.05F;
    float const hem_d = tr * 1.46F;

    RenderArchetypeBuilder builder{"roman_senator_toga"};

    auto add_shell = [&](float y_bottom,
                         float y_top_edge,
                         float bottom_w,
                         float bottom_d,
                         float top_w,
                         float top_d,
                         std::uint8_t slot) {
      float const height = y_top_edge - y_bottom;
      QMatrix4x4 model;
      model.translate(0.0F, (y_bottom + y_top_edge) * 0.5F, 0.0F);
      model.scale(bottom_w, height, bottom_d);
      builder.add_palette_mesh(
          get_unit_tapered_cylinder(1.0F, top_w / bottom_w, 18), model, slot);
      (void)top_d;
    };

    auto add_band = [&](float y_center,
                        float thickness,
                        float half_w,
                        float half_d,
                        std::uint8_t slot) {
      QMatrix4x4 model;
      model.translate(0.0F, y_center, 0.0F);
      model.scale(half_w, thickness, half_d);
      builder.add_palette_mesh(get_unit_tapered_cylinder(1.0F, 1.0F, 18), model, slot);
    };

    add_shell(y_waist, y_top, waist_w, waist_d, chest_w, chest_d, k_toga_cloth_slot);
    add_shell(y_waist - 0.26F,
              y_waist + 0.01F,
              skirt_mid_w,
              skirt_mid_d,
              waist_w,
              waist_d,
              k_toga_cloth_slot);
    add_shell(y_knee,
              y_waist - 0.25F,
              hem_w,
              hem_d,
              skirt_mid_w,
              skirt_mid_d,
              k_toga_cloth_slot);

    constexpr int k_folds = 12;
    constexpr float pi = std::numbers::pi_v<float>;
    for (int i = 0; i < k_folds; ++i) {
      float const a = (static_cast<float>(i) / k_folds) * 2.0F * pi;
      float const s = std::sin(a);
      float const c = std::cos(a);
      builder.add_palette_mesh(
          get_unit_cylinder(),
          cylinder_between(
              QVector3D(s * hem_w * 0.99F, y_knee + 0.03F, c * hem_d * 0.99F),
              QVector3D(
                  s * skirt_mid_w * 0.99F, y_waist - 0.25F, c * skirt_mid_d * 0.99F),
              0.011F),
          (i % 2 == 0) ? k_toga_shade_slot : k_toga_cloth_slot);
    }

    add_band(y_knee + 0.024F, 0.048F, hem_w * 1.02F, hem_d * 1.02F, k_toga_clavus_slot);
    add_band(y_knee + 0.056F, 0.011F, hem_w * 1.01F, hem_d * 1.01F, k_toga_gold_slot);

    for (int side = -1; side <= 1; side += 2) {
      float const x_top = static_cast<float>(side) * chest_w * 0.40F;
      float const x_bot = static_cast<float>(side) * waist_w * 0.42F;
      builder.add_palette_mesh(
          get_unit_cylinder(),
          cylinder_between(QVector3D(x_top, y_top - 0.02F, chest_d * 0.90F),
                           QVector3D(x_bot, y_waist + 0.03F, waist_d * 0.90F),
                           0.017F),
          k_toga_clavus_slot);
    }

    QVector3D const shoulder_l = torso_local.point(bind_frames.shoulder_l.origin);
    QVector3D const shoulder_r = torso_local.point(bind_frames.shoulder_r.origin);
    float const shoulder_x =
        std::abs(shoulder_l.x()) > 0.01F ? std::abs(shoulder_l.x()) : chest_w;

    for (int side = -1; side <= 1; side += 2) {
      float const sx = static_cast<float>(side) * shoulder_x;
      builder.add_palette_mesh(
          get_unit_sphere(),
          local_scale_model(QVector3D(sx * 0.96F, y_top - 0.012F, 0.0F),
                            QVector3D(0.098F, 0.086F, 0.090F)),
          k_toga_cloth_slot);
      builder.add_palette_mesh(
          get_unit_sphere(),
          local_scale_model(QVector3D(sx * 1.04F, y_top - 0.104F, 0.004F),
                            QVector3D(0.084F, 0.062F, 0.078F)),
          k_toga_cloth_slot);
      builder.add_palette_mesh(
          get_unit_sphere(),
          local_scale_model(QVector3D(sx * 1.06F, y_top - 0.152F, 0.006F),
                            QVector3D(0.076F, 0.030F, 0.070F)),
          k_toga_shade_slot);
    }

    QVector3D const drape_top(-shoulder_x * 0.72F, y_top + 0.030F, chest_d * 0.62F);
    QVector3D const drape_mid(-chest_w * 0.14F, y_top - 0.20F, chest_d * 1.02F);
    QVector3D const drape_low(waist_w * 0.94F, y_waist + 0.01F, waist_d * 0.74F);
    constexpr int k_drape_steps = 16;
    for (int step = 0; step <= k_drape_steps; ++step) {
      float const t = static_cast<float>(step) / k_drape_steps;
      float const inv = 1.0F - t;
      QVector3D const point =
          drape_top * (inv * inv) + drape_mid * (2.0F * inv * t) + drape_low * (t * t);
      builder.add_palette_mesh(
          get_unit_sphere(),
          local_scale_model(point, QVector3D(0.046F, 0.046F, 0.020F)),
          k_toga_clavus_slot);
      builder.add_palette_mesh(
          get_unit_sphere(),
          local_scale_model(point + QVector3D(0.030F, -0.036F, 0.005F),
                            QVector3D(0.013F, 0.013F, 0.011F)),
          k_toga_gold_slot);
    }

    builder.add_palette_mesh(
        get_unit_sphere(),
        local_scale_model(QVector3D(-shoulder_x * 0.86F, y_top + 0.004F, 0.0F),
                          QVector3D(0.062F, 0.046F, chest_d * 1.02F)),
        k_toga_cloth_slot);
    builder.add_palette_mesh(
        get_unit_sphere(),
        local_scale_model(
            QVector3D(-shoulder_x * 0.70F, y_top - 0.070F, -chest_d * 0.72F),
            QVector3D(0.054F, 0.052F, 0.030F)),
        k_toga_shade_slot);
    constexpr int k_tail_steps = 7;
    for (int step = 0; step < k_tail_steps; ++step) {
      float const t = static_cast<float>(step) / (k_tail_steps - 1);
      builder.add_palette_mesh(
          get_unit_sphere(),
          local_scale_model(QVector3D(-shoulder_x * (0.72F - 0.10F * t),
                                      y_top - 0.115F - t * 0.30F,
                                      -(chest_d * 1.06F + t * 0.014F)),
                            QVector3D(0.048F - 0.005F * t, 0.044F, 0.024F)),
          t > 0.78F ? k_toga_clavus_slot : k_toga_shade_slot);
    }

    builder.add_palette_mesh(
        get_unit_sphere(),
        local_scale_model(
            QVector3D(-shoulder_x * 0.74F, y_top - 0.016F, chest_d * 0.66F),
            QVector3D(0.024F, 0.024F, 0.018F)),
        k_toga_gold_slot);

    add_band(
        y_waist + 0.020F, 0.032F, waist_w * 1.03F, waist_d * 1.03F, k_toga_clavus_slot);
    builder.add_palette_mesh(
        get_unit_sphere(),
        local_scale_model(QVector3D(0.0F, y_waist + 0.020F, waist_d * 1.02F),
                          QVector3D(0.028F, 0.022F, 0.018F)),
        k_toga_gold_slot);

    QVector3D const capsa(waist_w * 1.02F, y_waist - 0.085F, waist_d * 0.20F);
    builder.add_palette_mesh(get_unit_cylinder(),
                             cylinder_between(capsa + QVector3D(0.0F, 0.042F, 0.0F),
                                              capsa - QVector3D(0.0F, 0.042F, 0.0F),
                                              0.032F),
                             k_toga_leather_slot);
    builder.add_palette_mesh(get_unit_sphere(),
                             local_scale_model(capsa + QVector3D(0.0F, 0.046F, 0.0F),
                                               QVector3D(0.034F, 0.010F, 0.034F)),
                             k_toga_gold_slot);
    builder.add_palette_mesh(
        get_unit_cylinder(),
        cylinder_between(QVector3D(shoulder_r.x() * 0.45F, y_top - 0.030F, 0.0F),
                         capsa + QVector3D(0.0F, 0.036F, 0.0F),
                         0.009F),
        k_toga_leather_slot);

    return std::move(builder).build();
  }();
  return archetype;
}

auto senator_laurel_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    constexpr float pi = std::numbers::pi_v<float>;
    constexpr float k_head_silhouette_r = 0.168F;

    RenderArchetypeBuilder builder{"roman_senator_laurel"};

    constexpr float k_ring_y = 0.132F;
    float const ring_r =
        std::sqrt(std::max(
            0.0F, k_head_silhouette_r * k_head_silhouette_r - k_ring_y * k_ring_y)) *
        1.03F;

    constexpr int k_leaves = 16;
    for (int i = 0; i < k_leaves; ++i) {
      float const a = (static_cast<float>(i) / k_leaves) * 2.0F * pi;
      float const s = std::sin(a);
      float const c = std::cos(a);
      QVector3D const base(s * ring_r, k_ring_y, c * ring_r);
      builder.add_palette_mesh(
          get_unit_sphere(),
          local_scale_model(base, QVector3D(0.026F, 0.015F, 0.026F)),
          k_toga_laurel_slot);
      builder.add_palette_mesh(
          get_unit_sphere(),
          local_scale_model(base + QVector3D(s * 0.008F, 0.026F, c * 0.008F),
                            QVector3D(0.020F, 0.026F, 0.020F)),
          k_toga_laurel_slot);
    }

    builder.add_palette_mesh(
        get_unit_sphere(),
        local_scale_model(QVector3D(0.0F, k_ring_y + 0.004F, -ring_r * 1.04F),
                          QVector3D(0.030F, 0.022F, 0.020F)),
        k_toga_gold_slot);
    for (int i = 0; i < 3; ++i) {
      float const t = static_cast<float>(i);
      builder.add_palette_mesh(
          get_unit_sphere(),
          local_scale_model(QVector3D(0.022F * (i % 2 == 0 ? 1.0F : -1.0F),
                                      k_ring_y - 0.030F - t * 0.044F,
                                      -k_head_silhouette_r * 0.94F),
                            QVector3D(0.015F, 0.023F, 0.013F)),
          k_toga_gold_slot);
    }

    return std::move(builder).build();
  }();
  return archetype;
}

auto senator_toga_make_static_attachment(std::uint16_t chest_bone_index)
    -> Render::Creature::StaticAttachmentSpec {
  const auto& bind_frames = Render::Humanoid::humanoid_bind_body_frames();
  const TorsoLocalFrame torso_local =
      make_torso_local_frame(QMatrix4x4{}, bind_frames.torso);
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &senator_toga_archetype(),
      .socket_bone_index = chest_bone_index,
      .unit_local_pose_at_bind = torso_local.world,
  });
  for (std::uint8_t i = 0; i < static_cast<std::uint8_t>(k_senator_role_count); ++i) {
    spec.palette_role_remap[i] = static_cast<std::uint8_t>(k_senator_base_role + i);
  }
  return spec;
}

auto senator_laurel_make_static_attachment(std::uint16_t head_bone_index)
    -> Render::Creature::StaticAttachmentSpec {
  const auto& bind_frames = Render::Humanoid::humanoid_bind_body_frames();
  QMatrix4x4 const head_bind =
      make_humanoid_attachment_transform_scaled(QMatrix4x4{},
                                                bind_frames.head,
                                                QVector3D(0.0F, 0.0F, 0.0F),
                                                QVector3D(1.0F, 1.0F, 1.0F));
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &senator_laurel_archetype(),
      .socket_bone_index = head_bone_index,
      .unit_local_pose_at_bind = head_bind,
  });
  for (std::uint8_t i = 0; i < static_cast<std::uint8_t>(k_senator_role_count); ++i) {
    spec.palette_role_remap[i] = static_cast<std::uint8_t>(k_senator_base_role + i);
  }
  return spec;
}

const Render::Creature::Pipeline::UnitVisualSpec&
roman_healer_visual_spec(std::string_view,
                         std::string_view,
                         Render::Creature::Pipeline::CreatureAssetId,
                         const Render::GL::HealerStyleConfig& style,
                         const Render::GL::Humanoid::ProportionProfile& profile) {
  using namespace Render::Creature::Pipeline;

  static const UnitVisualSpec spec = [&]() {
    static const auto k_healer_base_archetype = []() {
      auto& registry = Render::Creature::ArchetypeRegistry::instance();
      const auto* base_desc =
          registry.get(Render::Creature::ArchetypeRegistry::k_humanoid_base);
      if (base_desc == nullptr) {
        return Render::Creature::k_invalid_archetype;
      }

      Render::Creature::ArchetypeDescriptor desc = *base_desc;
      desc.debug_name = "troops/roman/healer";
      desc.bake_attachments[desc.bake_attachment_count++] =
          senator_toga_make_static_attachment(
              static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest));
      desc.bake_attachments[desc.bake_attachment_count++] =
          senator_laurel_make_static_attachment(
              static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Head));
      desc.role_count =
          static_cast<std::uint8_t>(k_senator_base_role + k_senator_role_count);
      desc.append_extra_role_colors_fn(&senator_extra_role_colors);
      return registry.register_archetype(desc);
    }();
    const auto loadout =
        Render::GL::Nation::resolve_equipment_loadout("troops/roman/healer");
    const std::array<EquipmentHandle, 2> handles{
        style.show_helmet ? loadout.helmet_handle : k_invalid_equipment_handle,
        style.show_armor ? loadout.armor_handle : k_invalid_equipment_handle};

    UnitVisualSpec out{};
    out.kind = CreatureKind::Humanoid;
    out.debug_name = "troops/roman/healer";
    out.scaling = profile.as_pipeline_scaling();
    out.owned_legacy_slots = LegacySlotMask::AllHumanoid;
    out.archetype_id = resolve_humanoid_equipment_archetype(
        "troops/roman/healer", k_healer_base_archetype, handles);
    out.creature_asset_id = Render::Creature::Pipeline::k_caster_humanoid_asset;
    return out;
  }();
  return spec;
}

const HealerRendererProfile k_healer_profile{
    .proportion_profile = k_profile,
    .visual_spec_factory = &roman_healer_visual_spec,
    .ensure_styles_registered = &register_roman_healer_style,
};

const std::array<HealerRendererRegistration, 1> k_healer_renderers{{
    {"troops/roman/healer",
     "roman_republic",
     Render::Creature::Pipeline::k_caster_humanoid_asset},
}};

} // namespace

void register_healer_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_healer_renderer_profile(registry, k_healer_profile, k_healer_renderers);
}

} // namespace Render::GL::Roman
