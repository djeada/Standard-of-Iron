#include "swordsman_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/systems/nation_id.h"
#include "../../../creature/pipeline/equipment_registry.h"
#include "../../../creature/pipeline/unit_visual_spec.h"
#include "../../../equipment/equipment_registry.h"
#include "../../../equipment/weapons/shield_renderer.h"
#include "../../../equipment/weapons/sword_renderer.h"
#include "../../../geom/math_utils.h"
#include "../../../geom/transforms.h"
#include "../../../gl/backend.h"
#include "../../../gl/primitives.h"
#include "../../../gl/shader.h"
#include "../../../humanoid/humanoid_math.h"
#include "../../../humanoid/humanoid_renderer_base.h"
#include "../../../humanoid/humanoid_specs.h"
#include "../../../humanoid/pose_controller.h"
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
    static auto helmet =
        reg.get(Render::GL::EquipmentCategory::Helmet, "carthage_heavy");
    static auto armor =
        reg.get(Render::GL::EquipmentCategory::Armor, "armor_heavy_carthage");
    static auto shoulder = reg.get(Render::GL::EquipmentCategory::Armor,
                                   "carthage_shoulder_cover");
    static auto shield_ptr =
        reg.get(Render::GL::EquipmentCategory::Weapon, "shield_carthage");

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

    static const std::array<EquipmentRecord, 7> records{
        make_legacy_equipment_record(*helmet),
        make_legacy_equipment_record(*armor),
        make_legacy_equipment_record(*shoulder),

        make_payload_record<SwordRenderer>([](uint32_t seed) {
          return compute_carthage_swordsman_sword_config(seed,
                                                         KnightStyleConfig{});
        }),

        make_legacy_equipment_record(*shield_ptr),

        EquipmentRecord{
            .dispatch =
                [](const EquipmentSubmitContext &sub,
                   Render::GL::EquipmentBatch &batch) {
                  if (sub.ctx == nullptr || sub.frames == nullptr ||
                      sub.palette == nullptr || sub.anim == nullptr) {
                    return;
                  }

                  bool const has_scabbard =
                      (Render::GL::hash_01(sub.seed ^ 0x5CABU) > 0.15F);
                  bool const is_attacking = sub.anim->inputs.is_attacking &&
                                            sub.anim->inputs.is_melee;
                  if (is_attacking || !has_scabbard) {
                    return;
                  }

                  using HP = HumanProportions;
                  QVector3D const hip(0.10F, HP::WAIST_Y - 0.04F, -0.02F);
                  QVector3D const tip = hip + QVector3D(-0.05F, -0.22F, -0.12F);
                  float const sword_width =
                      0.060F +
                      (Render::GL::hash_01(sub.seed ^ 0x7777U) - 0.5F) * 0.010F;
                  float const sheath_r = sword_width * 0.85F;
                  QVector3D const metal_color(0.72F, 0.73F, 0.78F);

                  batch.meshes.push_back(
                      {get_unit_cylinder(), nullptr,
                       cylinder_between(sub.ctx->model, hip, tip, sheath_r),
                       sub.palette->leather * 0.9F, nullptr, 1.0F, 0});
                  batch.meshes.push_back(
                      {get_unit_cone(), nullptr,
                       cone_from_to(sub.ctx->model, tip,
                                    tip + QVector3D(-0.02F, -0.02F, -0.02F),
                                    sheath_r),
                       metal_color, nullptr, 1.0F, 0});
                },
        },
    };

    static const UnitVisualSpec spec = []() {
      UnitVisualSpec s{};
      s.kind = CreatureKind::Humanoid;
      s.debug_name = "troops/carthage/swordsman";
      s.scaling = ProportionScaling{0.90F, 1.03F, 0.46F};
      s.equipment =
          std::span<const EquipmentRecord>{records.data(), records.size()};
      s.owned_legacy_slots = LegacySlotMask::Helmet | LegacySlotMask::Armor |
                             LegacySlotMask::Attachments;
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
