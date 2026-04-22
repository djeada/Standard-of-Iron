#include "archer_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/core/entity.h"
#include "../../../../game/systems/nation_id.h"
#include "../../../creature/pipeline/equipment_registry.h"
#include "../../../creature/pipeline/unit_visual_spec.h"
#include "../../../equipment/armor/cloak_renderer.h"
#include "../../../equipment/equipment_registry.h"
#include "../../../equipment/equipment_submit.h"
#include "../../../equipment/weapons/bow_renderer.h"
#include "../../../equipment/weapons/quiver_renderer.h"
#include "../../../geom/math_utils.h"
#include "../../../geom/transforms.h"
#include "../../../gl/backend.h"
#include "../../../gl/primitives.h"
#include "../../../gl/render_constants.h"
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
#include "archer_style.h"

#include <QMatrix4x4>
#include <QString>
#include <QVector3D>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <qmatrix4x4.h>
#include <qstringliteral.h>
#include <qvectornd.h>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Render::GL::Roman {

namespace {

constexpr std::string_view k_default_style_key = "default";
constexpr std::string_view k_attachment_headwrap = "carthage_headwrap";

constexpr float k_kneel_depth_multiplier = 1.125F;
constexpr float k_lean_amount_multiplier = 0.83F;

auto style_registry() -> std::unordered_map<std::string, ArcherStyleConfig> & {
  static std::unordered_map<std::string, ArcherStyleConfig> styles;
  return styles;
}

void ensure_archer_styles_registered() {
  static const bool registered = []() {
    register_roman_archer_style();
    return true;
  }();
  (void)registered;
}

constexpr float k_team_mix_weight = 0.65F;
constexpr float k_style_mix_weight = 0.35F;

} // namespace

void register_archer_style(const std::string &nation_id,
                           const ArcherStyleConfig &style) {
  style_registry()[nation_id] = style;
}

using Render::Geom::clamp01;
using Render::Geom::clamp_f;
using Render::Geom::cone_from_to;
using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;
using Render::GL::Humanoid::mix_palette_color;
using Render::GL::Humanoid::saturate_color;

class ArcherRenderer : public HumanoidRendererBase {
public:
  auto get_proportion_scaling() const -> QVector3D override {
    return {0.78F, 1.01F, 0.96F};
  }

  auto visual_spec() const
      -> const Render::Creature::Pipeline::UnitVisualSpec & override {
    using namespace Render::Creature::Pipeline;
    static auto &reg = Render::GL::EquipmentRegistry::instance();
    static auto helmet =
        reg.get(Render::GL::EquipmentCategory::Helmet, "roman_light");
    static auto armor_body =
        reg.get(Render::GL::EquipmentCategory::Armor, "roman_light_armor");
    static auto greaves =
        reg.get(Render::GL::EquipmentCategory::Armor, "roman_greaves");
    static auto cloak_inst =
        reg.get(Render::GL::EquipmentCategory::Armor, "cloak_carthage");
    static auto quiver =
        reg.get(Render::GL::EquipmentCategory::Weapon, "quiver");

    static const CloakMeshes cloak_meshes = []() -> CloakMeshes {
      if (cloak_inst) {
        if (auto *cr = dynamic_cast<CloakRenderer *>(cloak_inst.get())) {
          return cr->meshes();
        }
      }
      return CloakMeshes{};
    }();

    static const std::array<EquipmentRecord, 6> records{
        make_legacy_equipment_record(*helmet),
        make_legacy_equipment_record(*armor_body),
        make_legacy_equipment_record(*greaves),

        EquipmentRecord{
            .dispatch =
                [](const EquipmentSubmitContext &sub,
                   Render::GL::EquipmentBatch &batch) {
                  if (sub.ctx == nullptr || sub.frames == nullptr ||
                      sub.palette == nullptr || sub.anim == nullptr) {
                    return;
                  }
                  CloakConfig cfg;
                  cfg.primary_color = QVector3D(0.70F, 0.15F, 0.18F);
                  cfg.trim_color = sub.palette->metal;
                  CloakRenderer::submit(cfg, cloak_meshes, *sub.ctx,
                                        *sub.frames, *sub.palette, *sub.anim,
                                        batch);
                },
        },

        make_legacy_equipment_record(*quiver),

        EquipmentRecord{
            .dispatch =
                [](const EquipmentSubmitContext &sub,
                   Render::GL::EquipmentBatch &batch) {
                  if (sub.ctx == nullptr || sub.frames == nullptr ||
                      sub.palette == nullptr || sub.anim == nullptr) {
                    return;
                  }
                  BowRenderConfig cfg;
                  cfg.string_color =
                      saturate_color(QVector3D(0.28F, 0.28F, 0.32F));
                  cfg.metal_color =
                      Render::Geom::clamp_vec_01(sub.palette->metal * 1.15F);
                  cfg.fletching_color =
                      saturate_color(QVector3D(0.93F, 0.83F, 0.33F));
                  cfg.bow_top_y = HumanProportions::SHOULDER_Y + 0.55F;
                  cfg.bow_bot_y = HumanProportions::WAIST_Y - 0.25F;
                  cfg.bow_x = 0.0F;
                  cfg.arrow_visibility = ArrowVisibility::IdleAndAttackCycle;
                  BowRenderer::submit(cfg, *sub.ctx, *sub.frames, *sub.palette,
                                      *sub.anim, batch);
                },
        },
    };

    static const UnitVisualSpec spec = []() {
      UnitVisualSpec s{};
      s.kind = CreatureKind::Humanoid;
      s.debug_name = "troops/roman/archer";
      s.scaling = ProportionScaling{0.78F, 1.01F, 0.96F};
      s.equipment =
          std::span<const EquipmentRecord>{records.data(), records.size()};
      s.owned_legacy_slots = LegacySlotMask::AllHumanoid;
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

    float const bow_x = 0.0F;

    if (anim.is_in_hold_mode || anim.is_exiting_hold) {
      float const t =
          anim.is_in_hold_mode ? 1.0F : (1.0F - anim.hold_exit_progress);

      controller.kneel(t * k_kneel_depth_multiplier);
      controller.lean(QVector3D(0.0F, 0.0F, 1.0F),
                      t * k_lean_amount_multiplier);

      QVector3D const hold_hand_r(
          bow_x + 0.03F, controller.get_shoulder_y(false) + 0.30F, 0.55F);
      QVector3D const hold_hand_l(
          bow_x - 0.02F, controller.get_shoulder_y(true) + 0.12F, 0.55F);
      QVector3D const normal_hand_r(bow_x + 0.03F - arm_asymmetry,
                                    HP::SHOULDER_Y + 0.05F + arm_height_jitter,
                                    0.55F);
      QVector3D const normal_hand_l(
          bow_x - 0.02F + arm_asymmetry * 0.5F,
          HP::SHOULDER_Y + 0.12F + arm_height_jitter * 0.8F, 0.50F);

      QVector3D const blended_hand_r =
          normal_hand_r * (1.0F - t) + hold_hand_r * t;
      QVector3D const blended_hand_l =
          normal_hand_l * (1.0F - t) + hold_hand_l * t;

      controller.place_hand_at(false, blended_hand_r);
      controller.place_hand_at(true, blended_hand_l);
    } else {
      QVector3D const idle_hand_r(bow_x + 0.03F - arm_asymmetry,
                                  HP::SHOULDER_Y + 0.05F + arm_height_jitter,
                                  0.55F);
      QVector3D const idle_hand_l(
          bow_x - 0.05F + arm_asymmetry * 0.5F,
          HP::SHOULDER_Y + 0.14F + arm_height_jitter * 0.8F, 0.48F);

      controller.place_hand_at(false, idle_hand_r);
      controller.place_hand_at(true, idle_hand_l);
    }

    if (anim.is_attacking && !anim.is_in_hold_mode) {
      float const attack_phase =
          std::fmod(anim_ctx.attack_phase * ARCHER_INV_ATTACK_CYCLE_TIME, 1.0F);

      if (anim.is_melee) {
        controller.melee_strike(attack_phase);
      } else {
        controller.aim_bow(attack_phase);
      }
    }
  }

private:
  auto
  resolve_style(const DrawContext &ctx) const -> const ArcherStyleConfig & {
    ensure_archer_styles_registered();
    auto &styles = style_registry();
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
    auto fallback = styles.find(std::string(k_default_style_key));
    if (fallback != styles.end()) {
      return fallback->second;
    }
    static const ArcherStyleConfig default_style{};
    return default_style;
  }

private:
  void apply_palette_overrides(const ArcherStyleConfig &style,
                               const QVector3D &team_tint,
                               HumanoidVariant &variant) const {
    auto apply_color = [&](const std::optional<QVector3D> &override_color,
                           QVector3D &target) {
      target = mix_palette_color(target, override_color, team_tint,
                                 k_team_mix_weight, k_style_mix_weight);
    };

    apply_color(style.cloth_color, variant.palette.cloth);
    apply_color(style.leather_color, variant.palette.leather);
    apply_color(style.leather_dark_color, variant.palette.leather_dark);
    apply_color(style.metal_color, variant.palette.metal);
    apply_color(style.wood_color, variant.palette.wood);
  }
};

void register_archer_renderer(Render::GL::EntityRendererRegistry &registry) {
  ensure_archer_styles_registered();
  static ArcherRenderer const renderer;
  registry.register_renderer("troops/roman/archer",
                             [](const DrawContext &ctx, ISubmitter &out) {
                               static ArcherRenderer const static_renderer;
                               static_renderer.render(ctx, out);
                             });
}

} // namespace Render::GL::Roman
