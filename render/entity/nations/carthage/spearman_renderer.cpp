#include "spearman_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/systems/nation_id.h"
#include "../../../creature/pipeline/equipment_registry.h"
#include "../../../creature/pipeline/unit_visual_spec.h"
#include "../../../equipment/equipment_registry.h"
#include "../../../equipment/equipment_submit.h"
#include "../../../equipment/weapons/spear_renderer.h"
#include "../../../geom/math_utils.h"
#include "../../../geom/transforms.h"
#include "../../../gl/primitives.h"
#include "../../../humanoid/humanoid_math.h"
#include "../../../humanoid/humanoid_renderer_base.h"
#include "../../../humanoid/humanoid_specs.h"
#include "../../../humanoid/pose_controller.h"
#include "../../../humanoid/spear_pose_utils.h"
#include "../../../humanoid/style_palette.h"
#include "../../../palette.h"
#include "../../../submitter.h"
#include "../../registry.h"
#include "../../renderer_constants.h"
#include "spearman_style.h"

#include <QMatrix4x4>
#include <QString>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <qstringliteral.h>
#include <qvectornd.h>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Render::GL::Carthage {

namespace {

constexpr std::string_view k_spearman_default_style_key = "default";
constexpr float k_spearman_team_mix_weight = 0.6F;
constexpr float k_spearman_style_mix_weight = 0.4F;

constexpr float k_kneel_depth_multiplier = 0.875F;
constexpr float k_lean_amount_multiplier = 0.67F;

auto spearman_style_registry()
    -> std::unordered_map<std::string, SpearmanStyleConfig> & {
  static std::unordered_map<std::string, SpearmanStyleConfig> styles;
  return styles;
}

void ensure_spearman_styles_registered() {
  static const bool registered = []() {
    register_carthage_spearman_style();
    return true;
  }();
  (void)registered;
}

auto resolve_spearman_style_fn(const Render::GL::DrawContext &ctx)
    -> const SpearmanStyleConfig & {
  ensure_spearman_styles_registered();
  auto &styles = spearman_style_registry();
  std::string nation_id;
  if (ctx.entity != nullptr) {
    if (auto *unit = ctx.entity->get_component<Engine::Core::UnitComponent>()) {
      nation_id = Game::Systems::nation_id_to_string(unit->nation_id);
    }
  }
  if (!nation_id.empty()) {
    auto it = styles.find(nation_id);
    if (it != styles.end()) {
      return it->second;
    }
  }
  auto it_default = styles.find(std::string(k_spearman_default_style_key));
  if (it_default != styles.end()) {
    return it_default->second;
  }
  static const SpearmanStyleConfig k_empty{};
  return k_empty;
}

} // namespace

void register_spearman_style(const std::string &nation_id,
                             const SpearmanStyleConfig &style) {
  spearman_style_registry()[nation_id] = style;
}

using Render::Geom::clamp01;
using Render::Geom::cone_from_to;
using Render::Geom::cylinder_between;
using Render::Geom::ease_in_out_cubic;
using Render::Geom::lerp;
using Render::Geom::smoothstep;
using Render::Geom::sphere_at;
using Render::GL::Humanoid::mix_palette_color;
using Render::GL::Humanoid::saturate_color;

struct SpearmanExtras {
  QVector3D spear_shaft_color;
  QVector3D spearhead_color;
  float spear_length = 1.20F;
  float spear_shaft_radius = 0.020F;
  float spearheadLength = 0.18F;
};

class SpearmanRenderer : public HumanoidRendererBase {
public:
  auto get_proportion_scaling() const -> QVector3D override {
    return {0.72F, 1.02F, 0.74F};
  }

  void adjust_variation(const DrawContext &, uint32_t,
                        VariationParams &variation) const override {
    variation.bulk_scale *= 0.90F;
    variation.stance_width *= 0.92F;
  }

public:
  auto visual_spec() const
      -> const Render::Creature::Pipeline::UnitVisualSpec & override {
    using namespace Render::Creature::Pipeline;
    static auto &reg = Render::GL::EquipmentRegistry::instance();
    static auto helmet =
        reg.get(Render::GL::EquipmentCategory::Helmet, "carthage_heavy");
    static auto shoulder = reg.get(Render::GL::EquipmentCategory::Armor,
                                   "carthage_shoulder_cover");

    ensure_spearman_styles_registered();
    static const std::optional<float> spear_length_scale_opt =
        []() -> std::optional<float> {
      auto &styles = spearman_style_registry();
      auto it = styles.find("carthage");
      if (it != styles.end() && it->second.spear_length_scale) {
        return it->second.spear_length_scale;
      }
      auto it_default = styles.find(std::string(k_spearman_default_style_key));
      if (it_default != styles.end() && it_default->second.spear_length_scale) {
        return it_default->second.spear_length_scale;
      }
      return std::nullopt;
    }();

    static const std::array<EquipmentRecord, 4> records{
        make_legacy_equipment_record(*helmet),
        make_legacy_equipment_record(*shoulder),

        EquipmentRecord{
            .dispatch =
                [](const EquipmentSubmitContext &sub,
                   Render::GL::EquipmentBatch &batch) {
                  if (sub.ctx == nullptr || sub.frames == nullptr ||
                      sub.palette == nullptr || sub.anim == nullptr) {
                    return;
                  }
                  const SpearmanStyleConfig &style =
                      resolve_spearman_style_fn(*sub.ctx);
                  std::string const armor_key = style.armor_id.empty()
                                                    ? "armor_light_carthage"
                                                    : style.armor_id;
                  auto armor = EquipmentRegistry::instance().get(
                      EquipmentCategory::Armor, armor_key);
                  if (armor) {
                    armor->render(*sub.ctx, *sub.frames, *sub.palette,
                                  *sub.anim, batch);
                  }
                },
        },

        make_payload_record<SpearRenderer>([](uint32_t seed) {
          SpearRenderConfig cfg;
          cfg.shaft_color =
              QVector3D(0.5F, 0.3F, 0.2F) * QVector3D(0.85F, 0.75F, 0.65F);
          cfg.spearhead_color = QVector3D(0.75F, 0.76F, 0.80F);
          cfg.spear_length =
              1.15F + (Render::GL::hash_01(seed ^ 0xABCDU) - 0.5F) * 0.10F;
          cfg.shaft_radius =
              0.018F + (Render::GL::hash_01(seed ^ 0x7777U) - 0.5F) * 0.003F;
          cfg.spearhead_length =
              0.16F + (Render::GL::hash_01(seed ^ 0xBEEFU) - 0.5F) * 0.04F;
          if (spear_length_scale_opt) {
            cfg.spear_length =
                std::max(0.80F, cfg.spear_length * *spear_length_scale_opt);
          }
          return cfg;
        }),
    };

    static const UnitVisualSpec spec = []() {
      UnitVisualSpec s{};
      s.kind = CreatureKind::Humanoid;
      s.debug_name = "troops/carthage/spearman";
      s.scaling = ProportionScaling{0.72F, 1.02F, 0.74F};
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

    auto nextRand = [](uint32_t &s) -> float {
      s = s * 1664525U + 1013904223U;
      return float(s & 0x7FFFFFU) / float(0x7FFFFFU);
    };

    uint32_t beard_seed = seed ^ 0xBEEFFAU;
    bool wants_beard = style.force_beard;
    if (!wants_beard) {
      float const beard_roll = nextRand(beard_seed);
      wants_beard = (beard_roll < 0.90F);
    }

    if (wants_beard) {
      float const style_roll = nextRand(beard_seed);

      if (style_roll < 0.55F) {
        v.facial_hair.style = FacialHairStyle::FullBeard;
        v.facial_hair.length = 1.0F + nextRand(beard_seed) * 0.7F;
      } else if (style_roll < 0.80F) {
        v.facial_hair.style = FacialHairStyle::LongBeard;
        v.facial_hair.length = 1.3F + nextRand(beard_seed) * 0.9F;
      } else {
        v.facial_hair.style = FacialHairStyle::ShortBeard;
        v.facial_hair.length = 0.9F + nextRand(beard_seed) * 0.5F;
      }

      float const color_roll = nextRand(beard_seed);
      if (color_roll < 0.60F) {
        v.facial_hair.color = QVector3D(0.18F + nextRand(beard_seed) * 0.10F,
                                        0.14F + nextRand(beard_seed) * 0.08F,
                                        0.10F + nextRand(beard_seed) * 0.06F);
      } else if (color_roll < 0.85F) {
        v.facial_hair.color = QVector3D(0.30F + nextRand(beard_seed) * 0.12F,
                                        0.24F + nextRand(beard_seed) * 0.10F,
                                        0.16F + nextRand(beard_seed) * 0.08F);
      } else {
        v.facial_hair.color = QVector3D(0.35F + nextRand(beard_seed) * 0.10F,
                                        0.20F + nextRand(beard_seed) * 0.08F,
                                        0.12F + nextRand(beard_seed) * 0.06F);
      }

      v.facial_hair.thickness = 0.95F + nextRand(beard_seed) * 0.30F;
      v.facial_hair.coverage = 0.80F + nextRand(beard_seed) * 0.20F;

      if (nextRand(beard_seed) < 0.12F) {
        v.facial_hair.greyness = 0.12F + nextRand(beard_seed) * 0.30F;
      } else {
        v.facial_hair.greyness = 0.0F;
      }
    } else {
      v.facial_hair.style = FacialHairStyle::None;
    }
  }

  void customize_pose(const DrawContext &,
                      const HumanoidAnimationContext &anim_ctx, uint32_t seed,
                      HumanoidPose &pose) const override {
    using HP = HumanProportions;

    const AnimationInputs &anim = anim_ctx.inputs;
    HumanoidPoseController controller(pose, anim_ctx);

    float const arm_height_jitter = (hash_01(seed ^ 0xABCDU) - 0.5F) * 0.03F;
    float const arm_asymmetry = (hash_01(seed ^ 0xDEF0U) - 0.5F) * 0.04F;

    if (anim.is_in_hold_mode || anim.is_exiting_hold) {
      float const hold_t =
          anim.is_in_hold_mode ? 1.0F : (1.0F - anim.hold_exit_progress);

      if (anim.is_exiting_hold) {
        controller.kneel_transition(anim.hold_exit_progress, true);
      } else {
        controller.kneel(hold_t * k_kneel_depth_multiplier);
      }
      controller.lean(QVector3D(0.0F, 0.0F, 1.0F),
                      hold_t * k_lean_amount_multiplier);

      if (anim.is_attacking && anim.is_melee && anim.is_in_hold_mode) {
        float const attack_phase = std::fmod(
            anim_ctx.attack_phase * SPEARMAN_INV_ATTACK_CYCLE_TIME, 1.0F);
        controller.spear_thrust_from_hold(attack_phase,
                                          hold_t * k_kneel_depth_multiplier);
      } else {

        float const lowered_shoulder_y = controller.get_shoulder_y(true);
        float const pelvis_y = controller.get_pelvis_y();

        QVector3D const hand_r_pos(0.18F * (1.0F - hold_t) + 0.22F * hold_t,
                                   lowered_shoulder_y * (1.0F - hold_t) +
                                       (pelvis_y + 0.05F) * hold_t,
                                   0.15F * (1.0F - hold_t) + 0.20F * hold_t);

        float const offhand_along = lerp(-0.06F, -0.02F, hold_t);
        float const offhand_drop = 0.10F + 0.02F * hold_t;
        QVector3D const hand_l_pos =
            compute_offhand_spear_grip(pose, anim_ctx, hand_r_pos, false,
                                       offhand_along, offhand_drop, -0.08F);

        controller.place_hand_at(false, hand_r_pos);
        controller.place_hand_at(true, hand_l_pos);
      }

    } else if (anim.is_attacking && anim.is_melee && !anim.is_in_hold_mode) {
      float const attack_phase = std::fmod(
          anim_ctx.attack_phase * SPEARMAN_INV_ATTACK_CYCLE_TIME, 1.0F);
      controller.spear_thrust_variant(attack_phase, anim.attack_variant);
    } else {
      QVector3D const idle_hand_r(0.28F + arm_asymmetry,
                                  HP::SHOULDER_Y - 0.02F + arm_height_jitter,
                                  0.30F);
      QVector3D const idle_hand_l = compute_offhand_spear_grip(
          pose, anim_ctx, idle_hand_r, false, -0.04F, 0.10F, -0.08F);

      controller.place_hand_at(false, idle_hand_r);
      controller.place_hand_at(true, idle_hand_l);
    }
  }

private:
  auto
  resolve_style(const DrawContext &ctx) const -> const SpearmanStyleConfig & {
    ensure_spearman_styles_registered();
    auto &styles = spearman_style_registry();
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
    auto it_default = styles.find(std::string(k_spearman_default_style_key));
    if (it_default != styles.end()) {
      return it_default->second;
    }
    static const SpearmanStyleConfig k_empty{};
    return k_empty;
  }

private:
  void apply_palette_overrides(const SpearmanStyleConfig &style,
                               const QVector3D &team_tint,
                               HumanoidVariant &variant) const {
    auto apply_color = [&](const std::optional<QVector3D> &override_color,
                           QVector3D &target) {
      target = mix_palette_color(target, override_color, team_tint,
                                 k_spearman_team_mix_weight,
                                 k_spearman_style_mix_weight);
    };

    apply_color(style.cloth_color, variant.palette.cloth);
    apply_color(style.leather_color, variant.palette.leather);
    apply_color(style.leather_dark_color, variant.palette.leather_dark);
    apply_color(style.metal_color, variant.palette.metal);
  }
};

void register_spearman_renderer(Render::GL::EntityRendererRegistry &registry) {
  ensure_spearman_styles_registered();
  static SpearmanRenderer const renderer;
  registry.register_renderer("troops/carthage/spearman",
                             [](const DrawContext &ctx, ISubmitter &out) {
                               static SpearmanRenderer const static_renderer;
                               static_renderer.render(ctx, out);
                             });
}

} // namespace Render::GL::Carthage
