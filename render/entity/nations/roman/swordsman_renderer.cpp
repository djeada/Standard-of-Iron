#include "swordsman_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/systems/nation_id.h"
#include "../../../creature/archetype_registry.h"
#include "../../../creature/pipeline/creature_asset.h"
#include "../../../creature/pipeline/unit_visual_spec.h"
#include "../../../equipment/armor/roman_armor.h"
#include "../../../equipment/armor/roman_greaves.h"
#include "../../../equipment/armor/roman_shoulder_cover.h"
#include "../../../equipment/armor/shoulder_cover_archetype.h"
#include "../../../equipment/equipment_registry.h"
#include "../../../equipment/equipment_submit.h"
#include "../../../equipment/helmets/roman_heavy_helmet.h"
#include "../../../equipment/weapons/roman_scutum.h"
#include "../../../equipment/weapons/sword_renderer.h"
#include "../../../geom/math_utils.h"
#include "../../../geom/transforms.h"
#include "../../../gl/backend.h"
#include "../../../gl/primitives.h"
#include "../../../gl/shader.h"
#include "../../../humanoid/humanoid_math.h"
#include "../../../humanoid/humanoid_renderer_base.h"
#include "../../../humanoid/humanoid_spec.h"
#include "../../../humanoid/humanoid_specs.h"
#include "../../../humanoid/skeleton.h"
#include "../../../humanoid/style_palette.h"
#include "../../../palette.h"
#include "../../../scene_renderer.h"
#include "../../../submitter.h"
#include "../../registry.h"
#include "../../renderer_constants.h"
#include "swordsman_style.h"
#include <numbers>
#include <qmatrix4x4.h>
#include <qstringliteral.h>
#include <qvectornd.h>
#include <unordered_map>

#include <QMatrix4x4>
#include <QString>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace Render::GL::Roman {

namespace {

constexpr std::string_view k_swordsman_default_style_key = "default";
constexpr float k_swordsman_team_mix_weight = 0.6F;
constexpr float k_swordsman_style_mix_weight = 0.4F;

auto swordsman_style_registry()
    -> std::unordered_map<std::string, KnightStyleConfig> & {
  static std::unordered_map<std::string, KnightStyleConfig> styles;
  return styles;
}

void ensure_swordsman_styles_registered() {
  static const bool registered = []() {
    register_roman_swordsman_style();
    return true;
  }();
  (void)registered;
}

} // namespace

void register_swordsman_style(const std::string &nation_id,
                              const KnightStyleConfig &style) {
  swordsman_style_registry()[nation_id] = style;
}

using Render::Geom::clamp01;
using Render::Geom::clamp_f;
using Render::Geom::cone_from_to;
using Render::Geom::cylinder_between;
using Render::Geom::ease_in_out_cubic;
using Render::Geom::lerp;
using Render::Geom::nlerp;
using Render::Geom::smoothstep;
using Render::Geom::sphere_at;
using Render::GL::Humanoid::mix_palette_color;
using Render::GL::Humanoid::saturate_color;

struct KnightExtras {
  QVector3D metal_color;
  QVector3D shield_color;
  QVector3D shield_trim_color;
  float sword_length = 0.80F;
  float sword_width = 0.065F;
  float shield_radius = 0.18F;
  float shield_aspect = 1.0F;

  float guard_half_width = 0.12F;
  float handle_radius = 0.016F;
  float pommel_radius = 0.045F;
  float blade_ricasso = 0.16F;
  float blade_taper_bias = 0.65F;
  bool shield_cross_decal = false;
  bool has_scabbard = true;
};

class KnightRenderer : public HumanoidRendererBase {
public:
  static constexpr float kLimbWidthScale = 1.00F;
  static constexpr float kTorsoWidthScale = 0.55F;
  static constexpr float kHeightScale = 0.78F;
  static constexpr float kDepthScale = 0.26F;

  auto get_proportion_scaling() const -> QVector3D override {
    return {1.00F, kHeightScale, kDepthScale};
  }

  auto get_torso_scale() const -> float override { return kTorsoWidthScale; }

private:
public:
  auto visual_spec() const
      -> const Render::Creature::Pipeline::UnitVisualSpec & override {
    using namespace Render::Creature::Pipeline;
    static auto &reg = Render::GL::EquipmentRegistry::instance();
    (void)reg;

    ensure_swordsman_styles_registered();
    static const KnightStyleConfig captured_style = []() {
      auto &styles = swordsman_style_registry();
      auto it = styles.find("roman_republic");
      if (it != styles.end()) {
        return it->second;
      }
      auto it_default = styles.find(std::string(k_swordsman_default_style_key));
      if (it_default != styles.end()) {
        return it_default->second;
      }
      return KnightStyleConfig{};
    }();

    static const UnitVisualSpec spec = []() {
      static const auto k_head_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Head);
      static const auto k_foot_l_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::FootL);
      static const auto k_foot_r_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::FootR);
      static const auto k_shoulder_l_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::ShoulderL);
      static const auto k_shoulder_r_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::ShoulderR);
      static const auto k_helmet_base_role_byte = static_cast<std::uint8_t>(
          Render::Humanoid::k_humanoid_role_count + 1U);
      static const auto k_greaves_base_role_byte = static_cast<std::uint8_t>(
          k_helmet_base_role_byte + Render::GL::kRomanHeavyHelmetRoleCount);
      static const auto k_shoulder_base_role_byte = static_cast<std::uint8_t>(
          k_greaves_base_role_byte + Render::GL::kRomanGreavesRoleCount);
      static const auto k_shield_base_role_byte = static_cast<std::uint8_t>(
          k_shoulder_base_role_byte + Render::GL::kRomanShoulderCoverRoleCount);
      static const auto k_armor_base_role_byte = static_cast<std::uint8_t>(
          k_shield_base_role_byte + Render::GL::k_roman_scutum_role_count);
      static const auto k_sword_base_role_byte = static_cast<std::uint8_t>(
          k_armor_base_role_byte + Render::GL::kRomanHeavyArmorRoleCount);
      static const auto k_scabbard_base_role_byte = static_cast<std::uint8_t>(
          k_sword_base_role_byte + Render::GL::kSwordRoleCount);
      static const auto k_hand_l_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::HandL);
      static const auto k_chest_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest);
      static const auto k_head_bind_matrix =
          Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(
              Render::Humanoid::HumanoidBone::Head)];
      auto frame_to_matrix =
          [](const Render::GL::AttachmentFrame &f) -> QMatrix4x4 {
        QMatrix4x4 m;
        m.setColumn(0, QVector4D(f.right, 0.0F));
        m.setColumn(1, QVector4D(f.up, 0.0F));
        m.setColumn(2, QVector4D(f.forward, 0.0F));
        m.setColumn(3, QVector4D(f.origin, 1.0F));
        return m;
      };
      const auto &bind_frames = Render::Humanoid::humanoid_bind_body_frames();
      static const auto k_shin_l_bind_matrix =
          frame_to_matrix(bind_frames.shin_l);
      static const auto k_shin_r_bind_matrix =
          frame_to_matrix(bind_frames.shin_r);
      static const auto k_shoulder_l_bind_matrix =
          Render::GL::make_shoulder_cover_transform(
              QMatrix4x4{}, bind_frames.shoulder_l.origin,
              -bind_frames.torso.right, bind_frames.torso.up);
      static const auto k_shoulder_r_bind_matrix =
          Render::GL::make_shoulder_cover_transform(
              QMatrix4x4{}, bind_frames.shoulder_r.origin,
              bind_frames.torso.right, bind_frames.torso.up);
      static const auto k_hand_l_bind_matrix =
          Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(
              Render::Humanoid::HumanoidBone::HandL)];
      static const auto k_hand_r_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::HandR);
      static const auto k_hip_l_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::HipL);
      static const auto k_hand_r_bind_matrix =
          Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(
              Render::Humanoid::HumanoidBone::HandR)];
      static const SwordRenderConfig k_canonical_sword_cfg = []() {
        SwordRenderConfig cfg;
        cfg.sword_length = 0.80F;
        cfg.sword_width = 0.060F;
        cfg.guard_half_width = 0.120F;
        cfg.handle_radius = 0.016F;
        cfg.pommel_radius = 0.045F;
        cfg.blade_ricasso = 0.14F;
        cfg.material_id = 3;
        return cfg;
      }();
      constexpr float k_canonical_sheath_r = 0.060F * 0.85F;
      static const Render::Creature::StaticAttachmentSpec k_helmet_spec =
          Render::GL::roman_heavy_helmet_make_static_attachment(
              k_head_bone, k_helmet_base_role_byte, k_head_bind_matrix);
      static const Render::Creature::StaticAttachmentSpec k_greaves_l_spec =
          Render::GL::roman_greaves_make_static_attachment(
              k_foot_l_bone, k_greaves_base_role_byte, k_shin_l_bind_matrix);
      static const Render::Creature::StaticAttachmentSpec k_greaves_r_spec =
          Render::GL::roman_greaves_make_static_attachment(
              k_foot_r_bone, k_greaves_base_role_byte, k_shin_r_bind_matrix);
      static const Render::Creature::StaticAttachmentSpec k_shoulder_l_spec =
          Render::GL::roman_shoulder_cover_make_static_attachment(
              k_shoulder_l_bone, k_shoulder_base_role_byte,
              k_shoulder_l_bind_matrix);
      static const Render::Creature::StaticAttachmentSpec k_shoulder_r_spec =
          Render::GL::roman_shoulder_cover_make_static_attachment(
              k_shoulder_r_bone, k_shoulder_base_role_byte,
              k_shoulder_r_bind_matrix);
      static const Render::Creature::StaticAttachmentSpec k_shield_spec =
          Render::GL::roman_scutum_make_static_attachment(
              k_shield_base_role_byte);
      static const Render::Creature::StaticAttachmentSpec k_armor_spec =
          Render::GL::roman_heavy_armor_make_static_attachment(
              k_chest_bone, k_armor_base_role_byte);
      static const Render::Creature::StaticAttachmentSpec k_sword_spec =
          Render::GL::sword_make_static_attachment(k_canonical_sword_cfg,
                                                   k_sword_base_role_byte);
      static const Render::Creature::StaticAttachmentSpec k_scabbard_spec =
          Render::GL::scabbard_make_static_attachment(
              k_canonical_sheath_r, k_hip_l_bone, k_scabbard_base_role_byte);
      static const std::array<Render::Creature::StaticAttachmentSpec, 9>
          k_attachments{k_helmet_spec,     k_greaves_l_spec,  k_greaves_r_spec,
                        k_shoulder_l_spec, k_shoulder_r_spec, k_shield_spec,
                        k_armor_spec,      k_sword_spec,      k_scabbard_spec};
      static const auto k_archetype =
          Render::Creature::ArchetypeRegistry::instance()
              .register_unit_archetype(
                  "troops/roman/swordsman", CreatureKind::Humanoid,
                  std::span<const Render::Creature::StaticAttachmentSpec>(
                      k_attachments.data(), k_attachments.size()),
                  +[](const void *variant_void, QVector3D *out,
                      std::uint32_t base_count,
                      std::size_t max_count) -> std::uint32_t {
                    if (variant_void == nullptr || max_count <= base_count) {
                      return base_count;
                    }
                    const auto &v =
                        *static_cast<const HumanoidVariant *>(variant_void);
                    auto count = base_count;
                    count += Render::GL::roman_heavy_helmet_fill_role_colors(
                        v.palette, out + count, max_count - count);
                    if (max_count <= count) {
                      return count;
                    }
                    count += Render::GL::roman_greaves_fill_role_colors(
                        v.palette, out + count, max_count - count);
                    if (max_count <= count) {
                      return count;
                    }
                    count += Render::GL::roman_shoulder_cover_fill_role_colors(
                        v.palette, out + count, max_count - count);
                    if (max_count <= count) {
                      return count;
                    }
                    count += Render::GL::roman_scutum_fill_role_colors(
                        v.palette, out + count, max_count - count);
                    if (max_count <= count) {
                      return count;
                    }
                    count += Render::GL::roman_heavy_armor_fill_role_colors(
                        v.palette, out + count, max_count - count);
                    if (max_count <= count) {
                      return count;
                    }
                    count += Render::GL::sword_fill_role_colors(
                        v.palette, k_canonical_sword_cfg, out + count,
                        max_count - count);
                    if (max_count <= count) {
                      return count;
                    }
                    count += Render::GL::scabbard_fill_role_colors(
                        v.palette, out + count, max_count - count);
                    return count;
                  });

      UnitVisualSpec s{};
      s.kind = CreatureKind::Humanoid;
      s.debug_name = "troops/roman/swordsman";
      s.scaling = ProportionScaling{1.00F, 0.78F, 0.26F};
      s.owned_legacy_slots = LegacySlotMask::AllHumanoid;
      s.archetype_id = k_archetype;
      s.creature_asset_id = Render::Creature::Pipeline::kHumanoidSwordAsset;
      return s;
    }();
    return spec;
  }

  void get_variant(const DrawContext &ctx, uint32_t seed,
                   HumanoidVariant &v) const override {
    QVector3D const team_tint = resolve_team_tint(ctx);
    v.palette = make_humanoid_palette(team_tint, seed);
    auto const &style = resolve_style(ctx);
    apply_palette_overrides(style, team_tint, v);
  }

private:
  static auto compute_knight_extras(uint32_t seed,
                                    const HumanoidVariant &v) -> KnightExtras {
    KnightExtras e;

    e.metal_color = QVector3D(0.72F, 0.73F, 0.78F);

    float const shield_hue = hash_01(seed ^ 0x12345U);
    if (shield_hue < 0.45F) {
      e.shield_color = v.palette.cloth * 1.10F;
    } else if (shield_hue < 0.90F) {
      e.shield_color = v.palette.leather * 1.25F;
    } else {

      e.shield_color = e.metal_color * 0.95F;
    }

    e.sword_length = 0.80F + (hash_01(seed ^ 0xABCDU) - 0.5F) * 0.16F;
    e.sword_width = 0.060F + (hash_01(seed ^ 0x7777U) - 0.5F) * 0.010F;
    e.shield_radius = 0.16F + (hash_01(seed ^ 0xDEF0U) - 0.5F) * 0.04F;

    e.guard_half_width = 0.120F + (hash_01(seed ^ 0x3456U) - 0.5F) * 0.020F;
    e.handle_radius = 0.016F + (hash_01(seed ^ 0x88AAU) - 0.5F) * 0.003F;
    e.pommel_radius = 0.045F + (hash_01(seed ^ 0x19C3U) - 0.5F) * 0.006F;

    e.blade_ricasso =
        clamp_f(0.14F + (hash_01(seed ^ 0xBEEFU) - 0.5F) * 0.04F, 0.10F, 0.20F);
    e.blade_taper_bias =
        clamp01(0.6F + (hash_01(seed ^ 0xFACEU) - 0.5F) * 0.2F);

    e.shield_cross_decal = (hash_01(seed ^ 0xA11CU) > 0.55F);
    e.has_scabbard = (hash_01(seed ^ 0x5CABU) > 0.15F);
    e.shield_trim_color = e.metal_color * 0.95F;
    e.shield_aspect = 1.0F;
    return e;
  }

  auto
  resolve_style(const DrawContext &ctx) const -> const KnightStyleConfig & {
    ensure_swordsman_styles_registered();
    auto &styles = swordsman_style_registry();
    std::string nation_id;
    if (ctx.entity != nullptr) {
      if (auto *unit =
              ctx.entity->get_component<Engine::Core::UnitComponent>()) {
        nation_id = Game::Systems::nation_id_to_string(unit->nation_id);
      }
    }
    if (!nation_id.empty()) {
      auto it = styles.find(nation_id);
      if (it != styles.end()) {
        return it->second;
      }
    }
    auto it_default = styles.find(std::string(k_swordsman_default_style_key));
    if (it_default != styles.end()) {
      return it_default->second;
    }
    static const KnightStyleConfig k_empty{};
    return k_empty;
  }

private:
  void apply_palette_overrides(const KnightStyleConfig &style,
                               const QVector3D &team_tint,
                               HumanoidVariant &variant) const {
    auto apply_color = [&](const std::optional<QVector3D> &override_color,
                           QVector3D &target) {
      target = mix_palette_color(target, override_color, team_tint,
                                 k_swordsman_team_mix_weight,
                                 k_swordsman_style_mix_weight);
    };

    apply_color(style.cloth_color, variant.palette.cloth);
    apply_color(style.leather_color, variant.palette.leather);
    apply_color(style.leather_dark_color, variant.palette.leather_dark);
    apply_color(style.metal_color, variant.palette.metal);
  }

  void apply_extras_overrides(const KnightStyleConfig &style,
                              const QVector3D &team_tint,
                              const HumanoidVariant &variant,
                              KnightExtras &extras) const {
    extras.metal_color = saturate_color(variant.palette.metal);
    extras.shield_color = saturate_color(extras.shield_color);
    extras.shield_trim_color = saturate_color(extras.shield_trim_color);

    auto apply_shield_color =
        [&](const std::optional<QVector3D> &override_color, QVector3D &target) {
          target = mix_palette_color(target, override_color, team_tint,
                                     k_swordsman_team_mix_weight,
                                     k_swordsman_style_mix_weight);
        };

    apply_shield_color(style.shield_color, extras.shield_color);
    apply_shield_color(style.shield_trim_color, extras.shield_trim_color);

    if (style.shield_radius_scale) {
      extras.shield_radius =
          std::max(0.10F, extras.shield_radius * *style.shield_radius_scale);
    }
    if (style.shield_aspect_ratio) {
      extras.shield_aspect = std::max(0.40F, *style.shield_aspect_ratio);
    }
    if (style.has_scabbard) {
      extras.has_scabbard = *style.has_scabbard;
    }
    if (style.shield_cross_decal) {
      extras.shield_cross_decal = *style.shield_cross_decal;
    }
  }
};

void register_knight_renderer(Render::GL::EntityRendererRegistry &registry) {
  ensure_swordsman_styles_registered();
  static KnightRenderer const renderer;
  registry.register_renderer("troops/roman/swordsman",
                             [](const DrawContext &ctx, ISubmitter &out) {
                               static KnightRenderer const static_renderer;
                               static_renderer.render(ctx, out);
                             });
}

} // namespace Render::GL::Roman
