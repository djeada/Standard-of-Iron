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

#include "animation/rig/humanoid_proportions.h"
#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/systems/nation_id.h"
#include "healer_style.h"
#include "math/math_utils.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/pipeline/creature_asset.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/entity/healer_renderer_common.h"
#include "render/entity/nations/equipment_loadout_catalog.h"
#include "render/entity/registry.h"
#include "render/entity/renderer_constants.h"
#include "render/equipment/armor/torso_local_archetype_utils.h"
#include "render/equipment/attachment_builder.h"
#include "render/equipment/equipment_submit.h"
#include "render/equipment/generated_equipment.h"
#include "render/equipment/humanoid_attachment_archetype.h"
#include "render/equipment/humanoid_equipment_archetype.h"
#include "render/geom/transforms.h"
#include "render/gl/backend.h"
#include "render/gl/primitives.h"
#include "render/gl/render_constants.h"
#include "render/gl/shader.h"
#include "render/humanoid/asset/bind_skeleton.h"
#include "render/humanoid/asset/humanoid_spec.h"
#include "render/humanoid/runtime/humanoid_math.h"
#include "render/humanoid/runtime/humanoid_renderer.h"
#include "render/humanoid/runtime/pose_controller.h"
#include "render/humanoid/runtime/skeleton_evaluator.h"
#include "render/humanoid/runtime/style_palette.h"
#include "render/humanoid/schema/humanoid_proportion_profiles.h"
#include "render/palette.h"
#include "render/scene_renderer.h"
#include "render/submitter.h"

using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;

namespace Render::GL::Carthage {

namespace {

constexpr auto k_profile =
    Render::GL::Humanoid::k_support_proportion_profile.with_offset({.x = 0.01F});

void apply_grave_priest_cast_pose(const Render::GL::HumanoidAnimationContext& anim,
                                  HumanoidPose& io_pose) {
  Render::Humanoid::apply_skeleton_proportion_pose(io_pose);

  if (!anim.inputs.is_casting || anim.inputs.cast_kind != CastVisualKind::Fireball) {
    return;
  }

  float const phase = std::clamp(anim.attack_phase, 0.0F, 1.0F);
  float const intensity = std::sin(phase * std::numbers::pi_v<float>);
  if (intensity <= 0.0F) {
    return;
  }

  HumanoidPoseController controller(io_pose, anim);
  controller.tilt_torso(-0.13F * intensity, -0.18F * intensity);

  QVector3D const forward = anim.heading_forward();
  QVector3D const right = anim.heading_right();
  QVector3D const up = anim.heading_up();

  io_pose.hand_r += forward * (0.23F + 0.16F * intensity) +
                    up * (0.10F + 0.11F * intensity) - right * 0.09F;
  io_pose.elbow_r += forward * (0.05F + 0.07F * intensity) +
                     up * (0.06F + 0.06F * intensity) + right * 0.05F;
  io_pose.hand_l += forward * (0.12F + 0.10F * intensity) +
                    up * (0.09F + 0.09F * intensity) + right * 0.10F;
  io_pose.elbow_l += forward * (0.01F + 0.04F * intensity) +
                     up * (0.04F + 0.05F * intensity) - right * 0.07F;
  io_pose.head_pos += up * (0.015F * intensity) + forward * (0.025F * intensity);
}

constexpr std::uint32_t k_dark_mage_role_count = 6;

constexpr auto k_dark_mage_base_role =
    static_cast<std::uint8_t>(Render::Humanoid::k_humanoid_role_count + 1U);

enum DarkMagePaletteSlot : std::uint8_t {
  k_mage_robe_slot = 0U,
  k_mage_robe_lit_slot = 1U,
  k_mage_glyph_slot = 2U,
  k_mage_metal_slot = 3U,
  k_mage_bone_slot = 4U,
  k_mage_shadow_slot = 5U,
};

auto dark_mage_fill_role_colors(const HumanoidPalette& palette,
                                QVector3D* out,
                                std::size_t max) -> std::uint32_t {
  if (max < k_dark_mage_role_count) {
    return 0U;
  }
  QVector3D const robe = Render::GL::Humanoid::saturate_color(
      palette.cloth * 0.42F + QVector3D(0.055F, 0.048F, 0.088F));
  out[k_mage_robe_slot] = robe;
  out[k_mage_robe_lit_slot] = Render::GL::Humanoid::saturate_color(
      robe * 2.30F + QVector3D(0.035F, 0.028F, 0.070F));

  out[k_mage_glyph_slot] = Render::GL::Humanoid::saturate_color(
      QVector3D(0.30F, 0.86F, 0.62F) * 0.76F + palette.cloth * 0.24F);
  out[k_mage_metal_slot] = Render::GL::Humanoid::saturate_color(
      palette.metal * 0.55F + QVector3D(0.10F, 0.07F, 0.02F));
  out[k_mage_bone_slot] = QVector3D(0.78F, 0.75F, 0.66F);
  out[k_mage_shadow_slot] = QVector3D(0.022F, 0.020F, 0.034F);
  return k_dark_mage_role_count;
}

auto dark_mage_extra_role_colors(const void* variant_void,
                                 QVector3D* out,
                                 std::uint32_t base_count,
                                 std::size_t max_count) -> std::uint32_t {
  if (variant_void == nullptr || max_count <= base_count) {
    return base_count;
  }
  const auto& variant = *static_cast<const HumanoidVariant*>(variant_void);
  return base_count + dark_mage_fill_role_colors(
                          variant.palette, out + base_count, max_count - base_count);
}

auto dark_mage_robe_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    const auto& bind_frames = Render::Humanoid::humanoid_bind_body_frames();
    const AttachmentFrame& torso = bind_frames.torso;
    const AttachmentFrame& waist = bind_frames.waist;
    const TorsoLocalFrame torso_local = make_torso_local_frame(QMatrix4x4{}, torso);

    float const tr = torso.radius;
    constexpr float pi = std::numbers::pi_v<float>;

    float const y_top = 0.010F;
    float const y_waist = torso_local.point(waist.origin).y();
    float const y_hem = y_waist - 0.66F;

    float const chest_w = tr * 0.98F;
    float const chest_d = tr * 0.70F;
    float const waist_w = tr * 1.06F;
    float const waist_d = tr * 0.77F;
    float const skirt_mid_w = tr * 1.58F;
    float const skirt_mid_d = tr * 1.18F;
    float const hem_w = tr * 2.02F;
    float const hem_d = tr * 1.56F;

    RenderArchetypeBuilder builder{"carthage_dark_mage_robe"};

    auto add_shell = [&](float y_bottom,
                         float y_top_edge,
                         float bottom_w,
                         float bottom_d,
                         float top_w,
                         std::uint8_t slot) {
      QMatrix4x4 model;
      model.translate(0.0F, (y_bottom + y_top_edge) * 0.5F, 0.0F);
      model.scale(bottom_w, y_top_edge - y_bottom, bottom_d);
      builder.add_palette_mesh(
          get_unit_tapered_cylinder(1.0F, top_w / bottom_w, 18), model, slot);
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

    add_shell(y_waist, y_top, waist_w, waist_d, chest_w, k_mage_robe_slot);
    add_shell(y_waist - 0.30F,
              y_waist + 0.01F,
              skirt_mid_w,
              skirt_mid_d,
              waist_w,
              k_mage_robe_slot);
    add_shell(y_hem, y_waist - 0.29F, hem_w, hem_d, skirt_mid_w, k_mage_robe_slot);

    constexpr int k_folds = 14;
    for (int i = 0; i < k_folds; ++i) {
      float const a = (static_cast<float>(i) / k_folds) * 2.0F * pi;
      float const sa = std::sin(a);
      float const ca = std::cos(a);
      builder.add_palette_mesh(
          get_unit_cylinder(),
          cylinder_between(
              QVector3D(sa * hem_w * 0.99F, y_hem + 0.03F, ca * hem_d * 0.99F),
              QVector3D(
                  sa * skirt_mid_w * 0.99F, y_waist - 0.29F, ca * skirt_mid_d * 0.99F),
              0.012F),
          (i % 2 == 0) ? k_mage_robe_lit_slot : k_mage_robe_slot);
    }

    add_band(
        y_hem + 0.020F, 0.038F, hem_w * 1.015F, hem_d * 1.015F, k_mage_shadow_slot);
    add_band(y_hem + 0.048F, 0.010F, hem_w * 1.008F, hem_d * 1.008F, k_mage_glyph_slot);

    {
      float const mantle_top = y_top + 0.062F;
      float const mantle_bottom = y_top - 0.21F;
      QMatrix4x4 model;
      model.translate(0.0F, (mantle_bottom + mantle_top) * 0.5F, 0.0F);
      model.scale(tr * 1.40F, mantle_top - mantle_bottom, tr * 1.00F);
      builder.add_palette_mesh(
          get_unit_tapered_cylinder(1.0F, 0.74F, 18), model, k_mage_robe_slot);
      add_band(
          mantle_bottom + 0.013F, 0.022F, tr * 1.385F, tr * 0.99F, k_mage_metal_slot);
      constexpr int k_collar = 10;
      for (int i = 0; i < k_collar; ++i) {
        float const a = (static_cast<float>(i) / k_collar) * 2.0F * pi;
        builder.add_palette_mesh(get_unit_sphere(),
                                 local_scale_model(QVector3D(std::sin(a) * tr * 0.94F,
                                                             mantle_top - 0.008F,
                                                             std::cos(a) * tr * 0.70F),
                                                   QVector3D(0.028F, 0.026F, 0.028F)),
                                 k_mage_robe_slot);
      }
    }

    QVector3D const shoulder_l = torso_local.point(bind_frames.shoulder_l.origin);
    float const shoulder_x =
        std::abs(shoulder_l.x()) > 0.01F ? std::abs(shoulder_l.x()) : chest_w;

    for (int side = -1; side <= 1; side += 2) {
      float const sx = static_cast<float>(side) * shoulder_x;
      builder.add_palette_mesh(
          get_unit_sphere(),
          local_scale_model(QVector3D(sx * 0.94F, y_top - 0.008F, 0.0F),
                            QVector3D(0.100F, 0.090F, 0.092F)),
          k_mage_robe_slot);
      for (int step = 0; step < 6; ++step) {
        float const t = static_cast<float>(step) / 5.0F;
        builder.add_palette_mesh(
            get_unit_sphere(),
            local_scale_model(
                QVector3D(
                    sx * (0.98F + 0.09F * t), y_top - 0.085F - t * 0.20F, -0.004F),
                QVector3D(0.084F - 0.020F * t, 0.052F, 0.076F - 0.018F * t)),
            (step == 5) ? k_mage_robe_lit_slot : k_mage_robe_slot);
      }
    }

    {
      QVector3D const base(0.0F, y_top - 0.300F, chest_d * 1.24F);
      float const arm = tr * 0.30F;
      builder.add_palette_mesh(
          get_unit_cone(),
          Render::Geom::cone_from_to(base, base + QVector3D(0.0F, 0.098F, 0.0F), arm),
          k_mage_glyph_slot);
      builder.add_palette_mesh(
          get_unit_cylinder(),
          cylinder_between(base + QVector3D(-arm * 1.22F, 0.110F, 0.003F),
                           base + QVector3D(arm * 1.22F, 0.110F, 0.003F),
                           0.010F),
          k_mage_glyph_slot);
      builder.add_palette_mesh(get_unit_sphere(),
                               local_scale_model(base + QVector3D(0.0F, 0.150F, 0.003F),
                                                 QVector3D(0.028F, 0.028F, 0.013F)),
                               k_mage_glyph_slot);
      for (int side = -1; side <= 1; side += 2) {
        float const sx = static_cast<float>(side);
        builder.add_palette_mesh(
            get_unit_cylinder(),
            cylinder_between(base + QVector3D(sx * arm * 1.16F, 0.110F, 0.003F),
                             base + QVector3D(sx * arm * 1.38F, 0.150F, 0.003F),
                             0.008F),
            k_mage_glyph_slot);
      }
    }

    add_band(
        y_waist + 0.015F, 0.030F, waist_w * 1.04F, waist_d * 1.04F, k_mage_shadow_slot);
    builder.add_palette_mesh(
        get_unit_sphere(),
        local_scale_model(QVector3D(0.0F, y_waist + 0.015F, waist_d * 1.03F),
                          QVector3D(0.030F, 0.024F, 0.018F)),
        k_mage_metal_slot);
    for (int i = -1; i <= 1; i += 2) {
      float const bx = static_cast<float>(i) * waist_w * 0.62F;
      builder.add_palette_mesh(
          get_unit_cylinder(),
          cylinder_between(QVector3D(bx, y_waist - 0.005F, waist_d * 0.86F),
                           QVector3D(bx, y_waist - 0.090F, waist_d * 0.90F),
                           0.006F),
          k_mage_shadow_slot);
      builder.add_palette_mesh(
          get_unit_sphere(),
          local_scale_model(QVector3D(bx, y_waist - 0.108F, waist_d * 0.90F),
                            QVector3D(0.020F, 0.028F, 0.016F)),
          k_mage_bone_slot);
    }

    return std::move(builder).build();
  }();
  return archetype;
}

auto dark_mage_hood_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    constexpr float pi = std::numbers::pi_v<float>;
    constexpr float k_head_r = 0.168F;

    RenderArchetypeBuilder builder{"carthage_dark_mage_hood"};

    constexpr int k_lobes = 14;
    for (int i = 0; i < k_lobes; ++i) {
      float const t = static_cast<float>(i) / (k_lobes - 1);
      float const a = (0.26F + t * 1.48F) * pi;
      float const sa = std::sin(a);
      float const ca = std::cos(a);
      builder.add_palette_mesh(
          get_unit_sphere(),
          local_scale_model(
              QVector3D(sa * k_head_r * 1.10F, 0.006F, ca * k_head_r * 1.10F),
              QVector3D(0.042F, 0.086F, 0.042F)),
          k_mage_robe_slot);
    }

    builder.add_palette_mesh(
        get_unit_sphere(),
        local_scale_model(
            QVector3D(0.0F, 0.058F, -0.010F),
            QVector3D(k_head_r * 1.10F, k_head_r * 0.80F, k_head_r * 1.12F)),
        k_mage_robe_slot);

    builder.add_palette_mesh(
        get_unit_cone(),
        Render::Geom::cone_from_to(QVector3D(0.0F, 0.086F, -0.006F),
                                   QVector3D(0.0F, 0.232F, -0.030F),
                                   k_head_r * 0.74F),
        k_mage_robe_slot);
    builder.add_palette_mesh(get_unit_sphere(),
                             local_scale_model(QVector3D(0.0F, 0.236F, -0.030F),
                                               QVector3D(0.026F, 0.026F, 0.026F)),
                             k_mage_metal_slot);

    constexpr int k_diadem = 12;
    for (int i = 0; i < k_diadem; ++i) {
      float const a = (static_cast<float>(i) / k_diadem) * 2.0F * pi;
      builder.add_palette_mesh(
          get_unit_sphere(),
          local_scale_model(QVector3D(std::sin(a) * k_head_r * 1.09F,
                                      0.086F,
                                      std::cos(a) * k_head_r * 1.11F),
                            QVector3D(0.021F, 0.016F, 0.021F)),
          k_mage_metal_slot);
    }

    for (int step = 0; step < 5; ++step) {
      float const t = static_cast<float>(step) / 4.0F;
      builder.add_palette_mesh(
          get_unit_sphere(),
          local_scale_model(
              QVector3D(0.0F, 0.020F - t * 0.072F, -(k_head_r * 1.06F + t * 0.014F)),
              QVector3D(0.074F - 0.008F * t, 0.048F, 0.032F)),
          (step % 2 == 0) ? k_mage_robe_slot : k_mage_robe_lit_slot);
    }

    for (int side = -1; side <= 1; side += 2) {
      float const sx = static_cast<float>(side);
      for (int step = 0; step < 4; ++step) {
        float const t = static_cast<float>(step) / 3.0F;
        builder.add_palette_mesh(
            get_unit_sphere(),
            local_scale_model(QVector3D(sx * k_head_r * (1.02F + 0.06F * t),
                                        -0.010F - t * 0.070F,
                                        -k_head_r * (0.30F + 0.16F * t)),
                              QVector3D(0.034F, 0.046F, 0.030F)),
            k_mage_robe_slot);
      }
    }

    builder.add_palette_mesh(
        get_unit_sphere(),
        local_scale_model(QVector3D(0.0F, -0.030F, k_head_r * 0.44F),
                          QVector3D(0.100F, 0.086F, 0.062F)),
        k_mage_shadow_slot);

    return std::move(builder).build();
  }();
  return archetype;
}

auto dark_mage_stave_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    constexpr float pi = std::numbers::pi_v<float>;
    RenderArchetypeBuilder builder{"carthage_dark_mage_stave"};

    QVector3D const foot(0.012F, -0.66F, 0.010F);
    QVector3D const grip(0.0F, -0.04F, 0.0F);
    QVector3D const neck(-0.012F, 0.42F, -0.008F);
    QVector3D const crown(0.004F, 0.70F, 0.002F);

    builder.add_palette_mesh(
        get_unit_cylinder(), cylinder_between(foot, grip, 0.016F), k_mage_bone_slot);
    builder.add_palette_mesh(
        get_unit_cylinder(), cylinder_between(grip, neck, 0.018F), k_mage_bone_slot);
    builder.add_palette_mesh(
        get_unit_cylinder(), cylinder_between(neck, crown, 0.015F), k_mage_bone_slot);

    for (float y : {-0.34F, -0.02F, 0.34F}) {
      builder.add_palette_mesh(get_unit_sphere(),
                               local_scale_model(QVector3D(0.0F, y, 0.0F),
                                                 QVector3D(0.026F, 0.013F, 0.026F)),
                               k_mage_metal_slot);
    }

    constexpr int k_crescent = 9;
    for (int i = 0; i < k_crescent; ++i) {
      float const t = static_cast<float>(i) / (k_crescent - 1);
      float const a = (-0.42F + t * 0.84F) * pi;
      builder.add_palette_mesh(
          get_unit_sphere(),
          local_scale_model(crown + QVector3D(std::sin(a) * 0.070F,
                                              0.058F + std::cos(a) * 0.070F,
                                              0.0F),
                            QVector3D(0.017F, 0.017F, 0.014F)),
          k_mage_metal_slot);
    }
    builder.add_palette_mesh(get_unit_sphere(),
                             local_scale_model(crown + QVector3D(0.0F, 0.058F, 0.0F),
                                               QVector3D(0.030F, 0.030F, 0.022F)),
                             k_mage_glyph_slot);

    return std::move(builder).build();
  }();
  return archetype;
}

auto dark_mage_stave_make_static_attachment()
    -> Render::Creature::StaticAttachmentSpec {
  constexpr auto k_socket = Render::Humanoid::HumanoidSocket::GripR;
  constexpr auto k_bone = Render::Humanoid::HumanoidBone::HandR;
  QMatrix4x4 const bind_bone =
      Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(k_bone)];
  QMatrix4x4 const bind_socket = Render::Humanoid::bind_socket_transform(k_socket);
  auto spec = Render::Equipment::build_socket_static_attachment({
      .archetype = &dark_mage_stave_archetype(),
      .socket_bone_index = static_cast<std::uint16_t>(k_bone),
      .bind_bone_transform = bind_bone,
      .bind_socket_transform = bind_socket,
      .mesh_from_socket = QMatrix4x4{},
  });
  for (std::uint8_t i = 0; i < static_cast<std::uint8_t>(k_dark_mage_role_count); ++i) {
    spec.palette_role_remap[i] = static_cast<std::uint8_t>(k_dark_mage_base_role + i);
  }
  return spec;
}

auto dark_mage_make_static_attachment(const RenderArchetype& archetype,
                                      std::uint16_t bone_index,
                                      const QMatrix4x4& bind_pose)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &archetype,
      .socket_bone_index = bone_index,
      .unit_local_pose_at_bind = bind_pose,
  });
  for (std::uint8_t i = 0; i < static_cast<std::uint8_t>(k_dark_mage_role_count); ++i) {
    spec.palette_role_remap[i] = static_cast<std::uint8_t>(k_dark_mage_base_role + i);
  }
  return spec;
}

auto register_carthage_dark_mage_archetype(
    std::string_view debug_name, bool carry_stave) -> Render::Creature::ArchetypeId {
  auto& registry = Render::Creature::ArchetypeRegistry::instance();
  const auto* base_desc =
      registry.get(Render::Creature::ArchetypeRegistry::k_humanoid_base);
  if (base_desc == nullptr) {
    return Render::Creature::k_invalid_archetype;
  }

  const auto& bind_frames = Render::Humanoid::humanoid_bind_body_frames();
  const TorsoLocalFrame torso_local =
      make_torso_local_frame(QMatrix4x4{}, bind_frames.torso);
  QMatrix4x4 const head_bind =
      make_humanoid_attachment_transform_scaled(QMatrix4x4{},
                                                bind_frames.head,
                                                QVector3D(0.0F, 0.0F, 0.0F),
                                                QVector3D(1.0F, 1.0F, 1.0F));

  Render::Creature::ArchetypeDescriptor desc = *base_desc;
  desc.debug_name = std::string(debug_name);
  desc.bake_attachments[desc.bake_attachment_count++] =
      dark_mage_make_static_attachment(
          dark_mage_robe_archetype(),
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest),
          torso_local.world);
  desc.bake_attachments[desc.bake_attachment_count++] =
      dark_mage_make_static_attachment(
          dark_mage_hood_archetype(),
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Head),
          head_bind);
  if (carry_stave) {
    desc.bake_attachments[desc.bake_attachment_count++] =
        dark_mage_stave_make_static_attachment();
  }
  desc.role_count =
      static_cast<std::uint8_t>(k_dark_mage_base_role + k_dark_mage_role_count);
  desc.append_extra_role_colors_fn(&dark_mage_extra_role_colors);
  return registry.register_archetype(desc);
}

auto carthage_dark_mage_archetype() -> Render::Creature::ArchetypeId {
  static const auto archetype =
      register_carthage_dark_mage_archetype("troops/carthage/healer", true);
  return archetype;
}

auto carthage_dark_mage_unarmed_archetype() -> Render::Creature::ArchetypeId {
  static const auto archetype =
      register_carthage_dark_mage_archetype("troops/carthage/healer/unarmed", false);
  return archetype;
}

auto carthage_healer_variant_table() -> const Animation::ArchetypeVariantTable& {
  static const Animation::ArchetypeVariantTable table = [] {
    Animation::ArchetypeVariantTable value;
    value.archetype_for_pose[static_cast<std::size_t>(
        Animation::PoseIntent::AttackMelee)] = carthage_dark_mage_unarmed_archetype();
    return value;
  }();
  return table;
}

auto make_healer_spec(std::string_view renderer_key,
                      Render::Creature::Pipeline::CreatureAssetId creature_asset_id,
                      const Render::GL::Humanoid::ProportionProfile& profile)
    -> Render::Creature::Pipeline::UnitVisualSpec {
  using namespace Render::Creature::Pipeline;

  const auto loadout = Render::GL::Nation::resolve_equipment_loadout(renderer_key);
  const std::array<EquipmentHandle, 3> handles{
      loadout.helmet_handle, loadout.armor_handle, loadout.cloak_handle};

  UnitVisualSpec out{};
  out.kind = CreatureKind::Humanoid;
  out.debug_name = renderer_key;
  out.scaling = profile.as_pipeline_scaling();
  out.archetype_id = resolve_humanoid_equipment_archetype(
      renderer_key, Render::Creature::ArchetypeRegistry::k_humanoid_base, handles);
  out.creature_asset_id = creature_asset_id;
  if (renderer_key == "troops/iron_sepulcher/grave_priest") {
    out.animation_manifest.pose_policy =
        Render::Humanoid::HumanoidPosePolicy::GravePriestCast;
  }
  return out;
}

const Render::Creature::Pipeline::UnitVisualSpec& carthage_healer_visual_spec(
    std::string_view renderer_key,
    std::string_view,
    Render::Creature::Pipeline::CreatureAssetId creature_asset_id,
    const Render::GL::HealerStyleConfig&,
    const Render::GL::Humanoid::ProportionProfile& profile) {
  using namespace Render::Creature::Pipeline;

  static const auto healer_spec = [&]() {
    UnitVisualSpec out{};
    out.kind = CreatureKind::Humanoid;
    out.debug_name = "troops/carthage/healer";
    out.scaling = profile.as_pipeline_scaling();
    out.archetype_id = carthage_dark_mage_archetype();
    out.creature_asset_id = Render::Creature::Pipeline::k_stave_caster_humanoid_asset;
    out.animation_manifest.variant_table = &carthage_healer_variant_table();
    return out;
  }();
  static const auto grave_priest_spec =
      make_healer_spec("troops/iron_sepulcher/grave_priest",
                       Render::Creature::Pipeline::k_skeleton_humanoid_asset,
                       profile);
  if (renderer_key == "troops/iron_sepulcher/grave_priest") {
    return grave_priest_spec;
  }
  (void)creature_asset_id;
  return healer_spec;
}

void decorate_carthage_healer_variant(const DrawContext&,
                                      std::uint32_t seed,
                                      const Render::GL::HealerStyleConfig& style,
                                      HumanoidVariant& v) {
  auto next_rand = [](uint32_t& s) -> float {
    s = s * 1664525U + 1013904223U;
    return float(s & 0x7FFFFFU) / float(0x7FFFFFU);
  };

  uint32_t beard_seed = seed ^ 0x0EA101U;
  bool wants_beard = style.force_beard;
  if (!wants_beard) {
    float const beard_roll = next_rand(beard_seed);
    wants_beard = (beard_roll < 0.85F);
  }

  if (wants_beard) {
    float const style_roll = next_rand(beard_seed);

    if (style_roll < 0.45F) {
      v.facial_hair.style = FacialHairStyle::ShortBeard;
      v.facial_hair.length = 0.8F + next_rand(beard_seed) * 0.4F;
    } else if (style_roll < 0.75F) {
      v.facial_hair.style = FacialHairStyle::FullBeard;
      v.facial_hair.length = 0.9F + next_rand(beard_seed) * 0.5F;
    } else if (style_roll < 0.90F) {
      v.facial_hair.style = FacialHairStyle::Goatee;
      v.facial_hair.length = 0.7F + next_rand(beard_seed) * 0.4F;
    } else {
      v.facial_hair.style = FacialHairStyle::MustacheAndBeard;
      v.facial_hair.length = 1.0F + next_rand(beard_seed) * 0.4F;
    }

    float const color_roll = next_rand(beard_seed);
    if (color_roll < 0.55F) {

      v.facial_hair.color = QVector3D(0.12F + next_rand(beard_seed) * 0.08F,
                                      0.10F + next_rand(beard_seed) * 0.06F,
                                      0.08F + next_rand(beard_seed) * 0.05F);
    } else if (color_roll < 0.80F) {

      v.facial_hair.color = QVector3D(0.22F + next_rand(beard_seed) * 0.10F,
                                      0.17F + next_rand(beard_seed) * 0.08F,
                                      0.12F + next_rand(beard_seed) * 0.06F);
    } else {

      v.facial_hair.color = QVector3D(0.35F + next_rand(beard_seed) * 0.15F,
                                      0.32F + next_rand(beard_seed) * 0.12F,
                                      0.30F + next_rand(beard_seed) * 0.10F);
      v.facial_hair.greyness = 0.3F + next_rand(beard_seed) * 0.4F;
    }

    v.facial_hair.thickness = 0.85F + next_rand(beard_seed) * 0.25F;
    v.facial_hair.coverage = 0.80F + next_rand(beard_seed) * 0.20F;
  }
}

const HealerRendererProfile k_healer_profile{
    .proportion_profile = k_profile,
    .visual_spec_factory = &carthage_healer_visual_spec,
    .variant_decorator = &decorate_carthage_healer_variant,
    .ensure_styles_registered = &register_carthage_healer_style,
};

const std::array<HealerRendererRegistration, 1> k_healer_renderers{{
    {"troops/carthage/healer",
     "carthage",
     Render::Creature::Pipeline::k_stave_caster_humanoid_asset},
}};

const std::array<HealerRendererRegistration, 1> k_grave_priest_renderers{{
    {"troops/iron_sepulcher/grave_priest",
     "iron_sepulcher",
     Render::Creature::Pipeline::k_skeleton_humanoid_asset},
}};

} // namespace

void register_healer_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_healer_renderer_profile(registry, k_healer_profile, k_healer_renderers);
}

void register_grave_priest_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_healer_renderer_profile(
      registry, k_healer_profile, k_grave_priest_renderers);
}

} // namespace Render::GL::Carthage
