#include "swordsman_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/systems/nation_id.h"
#include "../../../creature/archetype_registry.h"
#include "../../../creature/pipeline/unit_visual_spec.h"
#include "../../../equipment/armor/armor_heavy_carthage.h"
#include "../../../equipment/armor/carthage_shoulder_cover.h"
#include "../../../equipment/armor/shoulder_cover_archetype.h"
#include "../../../equipment/equipment_registry.h"
#include "../../../equipment/helmets/carthage_heavy_helmet.h"
#include "../../../equipment/weapons/shield_renderer.h"
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
#include "../../../humanoid/pose_controller.h"
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

#include "../../../equipment/equipment_submit.h"
namespace Render::GL::Carthage {

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
    register_carthage_swordsman_style();
    return true;
  }();
  (void)registered;
}

static auto compute_carthage_swordsman_sword_config(
    uint32_t seed, const KnightStyleConfig &style) -> SwordRenderConfig {
  SwordRenderConfig cfg;
  cfg.metal_color = QVector3D(0.72F, 0.73F, 0.78F);
  cfg.sword_length =
      0.80F + (Render::GL::hash_01(seed ^ 0xABCDU) - 0.5F) * 0.16F;
  cfg.sword_width =
      0.060F + (Render::GL::hash_01(seed ^ 0x7777U) - 0.5F) * 0.010F;
  cfg.guard_half_width =
      0.120F + (Render::GL::hash_01(seed ^ 0x3456U) - 0.5F) * 0.020F;
  cfg.handle_radius =
      0.016F + (Render::GL::hash_01(seed ^ 0x88AAU) - 0.5F) * 0.003F;
  cfg.pommel_radius =
      0.045F + (Render::GL::hash_01(seed ^ 0x19C3U) - 0.5F) * 0.006F;
  cfg.blade_ricasso = Render::Geom::clamp_f(
      0.14F + (Render::GL::hash_01(seed ^ 0xBEEFU) - 0.5F) * 0.04F, 0.10F,
      0.20F);
  cfg.blade_taper_bias = Render::Geom::clamp01(
      0.6F + (Render::GL::hash_01(seed ^ 0xFACEU) - 0.5F) * 0.2F);
  cfg.has_scabbard = (Render::GL::hash_01(seed ^ 0x5CABU) > 0.15F);
  return cfg;
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
  float swordWidth = 0.065F;
  float shieldRadius = 0.18F;
  float shield_aspect = 1.0F;

  float guard_half_width = 0.12F;
  float handleRadius = 0.016F;
  float pommel_radius = 0.045F;
  float blade_ricasso = 0.16F;
  float blade_taper_bias = 0.65F;
  bool shieldCrossDecal = false;
  bool has_scabbard = true;
};

class KnightRenderer : public HumanoidRendererBase {
public:
  static constexpr float kLimbWidthScale = 0.90F;
  static constexpr float kTorsoWidthScale = 0.75F;
  static constexpr float kHeightScale = 1.03F;
  static constexpr float kDepthScale = 0.46F;

  auto get_proportion_scaling() const -> QVector3D override {
    return {kLimbWidthScale, kHeightScale, kDepthScale};
  }

  auto get_torso_scale() const -> float override { return kTorsoWidthScale; }

public:
  auto visual_spec() const
      -> const Render::Creature::Pipeline::UnitVisualSpec & override {
    using namespace Render::Creature::Pipeline;
    static auto &reg = Render::GL::EquipmentRegistry::instance();
    (void)reg;

    ensure_swordsman_styles_registered();
    static const KnightStyleConfig style = []() {
      auto &styles = swordsman_style_registry();
      auto it = styles.find("carthage");
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
      static const auto k_helmet_base_role_byte =
          static_cast<std::uint8_t>(Render::Humanoid::kHumanoidRoleCount + 1U);
      static const auto k_shield_base_role_byte = static_cast<std::uint8_t>(
          k_helmet_base_role_byte + Render::GL::kCarthageHeavyHelmetRoleCount);
      static const auto k_shoulder_base_role_byte = static_cast<std::uint8_t>(
          k_shield_base_role_byte + Render::GL::kShieldRoleCount);
      static const auto k_armor_base_role_byte = static_cast<std::uint8_t>(
          k_shoulder_base_role_byte +
          Render::GL::kCarthageShoulderCoverRoleCount);
      static const auto k_sword_base_role_byte = static_cast<std::uint8_t>(
          k_armor_base_role_byte + Render::GL::kArmorHeavyCarthageRoleCount);
      static const auto k_scabbard_base_role_byte = static_cast<std::uint8_t>(
          k_sword_base_role_byte + Render::GL::kSwordRoleCount);
      static const auto k_chest_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest);
      static const auto k_hand_l_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::HandL);
      static const auto k_shoulder_l_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::ShoulderL);
      static const auto k_shoulder_r_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::ShoulderR);
      static const auto k_head_bind_matrix =
          Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(
              Render::Humanoid::HumanoidBone::Head)];
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
      static const ShieldRenderConfig k_canonical_shield_cfg = []() {
        ShieldRenderConfig cfg;
        cfg.shield_color = QVector3D(0.20F, 0.46F, 0.62F);
        cfg.trim_color = QVector3D(0.76F, 0.68F, 0.42F);
        cfg.metal_color = QVector3D(0.70F, 0.68F, 0.52F);
        cfg.shield_radius = 0.18F * 0.9F;
        cfg.shield_aspect = 1.0F;
        cfg.has_cross_decal = false;
        return cfg;
      }();
      static const Render::Creature::StaticAttachmentSpec k_shield_spec =
          Render::GL::shield_make_static_attachment(
              k_canonical_shield_cfg, k_hand_l_bone, k_shield_base_role_byte,
              k_hand_l_bind_matrix);
      const auto &bind_frames = Render::Humanoid::humanoid_bind_body_frames();
      static const auto k_shoulder_l_bind_matrix =
          Render::GL::make_shoulder_cover_transform(
              QMatrix4x4{}, bind_frames.shoulder_l.origin,
              -bind_frames.torso.right, bind_frames.torso.up);
      static const auto k_shoulder_r_bind_matrix =
          Render::GL::make_shoulder_cover_transform(
              QMatrix4x4{}, bind_frames.shoulder_r.origin,
              bind_frames.torso.right, bind_frames.torso.up);
      static const Render::Creature::StaticAttachmentSpec k_shoulder_l_spec =
          Render::GL::carthage_shoulder_cover_make_static_attachment(
              k_shoulder_l_bone, k_shoulder_base_role_byte,
              k_shoulder_l_bind_matrix);
      static const Render::Creature::StaticAttachmentSpec k_shoulder_r_spec =
          Render::GL::carthage_shoulder_cover_make_static_attachment(
              k_shoulder_r_bone, k_shoulder_base_role_byte,
              k_shoulder_r_bind_matrix);
      static const Render::Creature::StaticAttachmentSpec k_armor_spec =
          Render::GL::armor_heavy_carthage_make_static_attachment(
              k_chest_bone, k_armor_base_role_byte);
      static const Render::Creature::StaticAttachmentSpec k_sword_spec =
          Render::GL::sword_make_static_attachment(
              k_canonical_sword_cfg, k_hand_r_bone, k_sword_base_role_byte,
              k_hand_r_bind_matrix);
      static const Render::Creature::StaticAttachmentSpec k_scabbard_spec =
          Render::GL::scabbard_make_static_attachment(
              k_canonical_sheath_r, k_hip_l_bone, k_scabbard_base_role_byte);
      static const std::array<Render::Creature::StaticAttachmentSpec, 12>
          k_attachments{
              Render::GL::carthage_heavy_helmet_make_static_attachment(
                  Render::GL::carthage_heavy_helmet_shell_archetype(),
                  k_head_bone, k_helmet_base_role_byte, k_head_bind_matrix),
              Render::GL::carthage_heavy_helmet_make_static_attachment(
                  Render::GL::carthage_heavy_helmet_neck_guard_archetype(),
                  k_head_bone, k_helmet_base_role_byte, k_head_bind_matrix),
              Render::GL::carthage_heavy_helmet_make_static_attachment(
                  Render::GL::carthage_heavy_helmet_cheek_guards_archetype(),
                  k_head_bone, k_helmet_base_role_byte, k_head_bind_matrix),
              Render::GL::carthage_heavy_helmet_make_static_attachment(
                  Render::GL::carthage_heavy_helmet_face_plate_archetype(),
                  k_head_bone, k_helmet_base_role_byte, k_head_bind_matrix),
              Render::GL::carthage_heavy_helmet_make_static_attachment(
                  Render::GL::carthage_heavy_helmet_crest_archetype(),
                  k_head_bone, k_helmet_base_role_byte, k_head_bind_matrix),
              Render::GL::carthage_heavy_helmet_make_static_attachment(
                  Render::GL::carthage_heavy_helmet_rivets_archetype(),
                  k_head_bone, k_helmet_base_role_byte, k_head_bind_matrix),
              k_shield_spec,
              k_shoulder_l_spec,
              k_shoulder_r_spec,
              k_armor_spec,
              k_sword_spec,
              k_scabbard_spec,
          };
      static const auto k_archetype =
          Render::Creature::ArchetypeRegistry::instance()
              .register_unit_archetype(
                  "troops/carthage/swordsman", CreatureKind::Humanoid,
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
                    count += Render::GL::carthage_heavy_helmet_fill_role_colors(
                        v.palette, out + count, max_count - count);
                    if (max_count <= count) {
                      return count;
                    }
                    static const ShieldRenderConfig k_shield_cfg = []() {
                      ShieldRenderConfig cfg;
                      cfg.shield_color = QVector3D(0.20F, 0.46F, 0.62F);
                      cfg.trim_color = QVector3D(0.76F, 0.68F, 0.42F);
                      cfg.metal_color = QVector3D(0.70F, 0.68F, 0.52F);
                      cfg.shield_radius = 0.18F * 0.9F;
                      cfg.shield_aspect = 1.0F;
                      cfg.has_cross_decal = false;
                      return cfg;
                    }();
                    count += Render::GL::shield_fill_role_colors(
                        v.palette, k_shield_cfg, out + count,
                        max_count - count);
                    if (max_count <= count) {
                      return count;
                    }
                    count +=
                        Render::GL::carthage_shoulder_cover_fill_role_colors(
                            v.palette, out + count, max_count - count);
                    if (max_count <= count) {
                      return count;
                    }
                    count += Render::GL::armor_heavy_carthage_fill_role_colors(
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
      s.debug_name = "troops/carthage/swordsman";
      s.scaling = ProportionScaling{0.90F, 1.03F, 0.46F};
      s.owned_legacy_slots = LegacySlotMask::AllHumanoid;
      s.archetype_id = k_archetype;
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

  void customize_pose(const DrawContext &,
                      const HumanoidAnimationContext &anim_ctx, uint32_t seed,
                      HumanoidPose &pose) const override {
    using HP = HumanProportions;

    const AnimationInputs &anim = anim_ctx.inputs;
    HumanoidPoseController controller(pose, anim_ctx);

    float const arm_height_jitter = (hash_01(seed ^ 0xABCDU) - 0.5F) * 0.03F;
    float const arm_asymmetry = (hash_01(seed ^ 0xDEF0U) - 0.5F) * 0.04F;

    if (anim.is_attacking && anim.is_melee) {
      float const attack_phase =
          std::fmod(anim_ctx.attack_phase * KNIGHT_INV_ATTACK_CYCLE_TIME, 1.0F);
      controller.sword_slash_variant(attack_phase, anim.attack_variant);
    } else {
      QVector3D const idle_hand_r(0.30F + arm_asymmetry,
                                  HP::SHOULDER_Y - 0.02F + arm_height_jitter,
                                  0.35F);
      QVector3D const idle_hand_l(-0.22F - 0.5F * arm_asymmetry,
                                  HP::SHOULDER_Y + 0.5F * arm_height_jitter,
                                  0.18F);

      controller.place_hand_at(false, idle_hand_r);
      controller.place_hand_at(true, idle_hand_l);
    }
  }

private:
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
};

void register_knight_renderer(Render::GL::EntityRendererRegistry &registry) {
  ensure_swordsman_styles_registered();
  static KnightRenderer const renderer;
  registry.register_renderer("troops/carthage/swordsman",
                             [](const DrawContext &ctx, ISubmitter &out) {
                               static KnightRenderer const static_renderer;
                               static_renderer.render(ctx, out);
                             });
}

} // namespace Render::GL::Carthage
