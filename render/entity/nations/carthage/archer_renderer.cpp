#include "archer_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/core/entity.h"
#include "../../../../game/systems/nation_id.h"
#include "../../../creature/archetype_registry.h"
#include "../../../creature/humanoid_clip_ids.h"
#include "../../../creature/pipeline/creature_render_graph.h"
#include "../../../creature/pipeline/preparation_common.h"
#include "../../../creature/pipeline/unit_visual_spec.h"
#include "../../../equipment/armor/armor_light_carthage.h"
#include "../../../equipment/armor/cloak_renderer.h"
#include "../../../equipment/equipment_registry.h"
#include "../../../equipment/helmets/carthage_light_helmet.h"
#include "../../../equipment/helmets/headwrap.h"
#include "../../../equipment/humanoid_equipment_archetype.h"
#include "../../../equipment/weapons/bow_renderer.h"
#include "../../../equipment/weapons/quiver_renderer.h"
#include "../../../geom/math_utils.h"
#include "../../../geom/transforms.h"
#include "../../../gl/backend.h"
#include "../../../gl/primitives.h"
#include "../../../gl/render_constants.h"
#include "../../../gl/shader.h"
#include "../../../humanoid/humanoid_full_builder.h"
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
#include "../equipment_loadout_catalog.h"
#include "archer_style.h"

#include <QMatrix4x4>
#include <QString>
#include <QVector3D>
#include <algorithm>
#include <array>
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

namespace Render::GL::Carthage {

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
    register_carthage_archer_style();
    return true;
  }();
  (void)registered;
}

constexpr float k_team_mix_weight = 0.65F;
constexpr float k_style_mix_weight = 0.35F;

auto canonical_bow_config() -> BowRenderConfig {
  BowRenderConfig cfg;
  cfg.bow_top_y = HumanProportions::SHOULDER_Y + 0.55F;
  cfg.bow_bot_y = HumanProportions::WAIST_Y - 0.25F;
  cfg.bow_x = 0.0F;
  return cfg;
}

auto resolve_archer_style_fn(const Render::GL::DrawContext &ctx)
    -> const ArcherStyleConfig & {
  ensure_archer_styles_registered();
  auto &styles = style_registry();
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
  auto it_default = styles.find(std::string(k_default_style_key));
  if (it_default != styles.end()) {
    return it_default->second;
  }
  static const ArcherStyleConfig k_empty{};
  return k_empty;
}

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

    return {1.03F, 1.08F, 0.98F};
  }

  void adjust_variation(const DrawContext &, uint32_t,
                        VariationParams &variation) const override {
    variation.height_scale *= 1.06F;
    variation.bulk_scale *= 0.90F;
    variation.stance_width *= 0.90F;
    variation.arm_swing_amp *= 0.92F;
  }

  auto get_hold_kneel_depth() const -> float override {
    return k_kneel_depth_multiplier;
  }

  auto visual_spec() const
      -> const Render::Creature::Pipeline::UnitVisualSpec & override {
    using namespace Render::Creature::Pipeline;

    static const UnitVisualSpec spec = []() {
      static const auto k_archer_base_archetype = []() {
        using Render::Creature::AnimationStateId;
        using Render::Creature::ArchetypeDescriptor;
        using Render::Creature::ArchetypeRegistry;

        auto &registry = ArchetypeRegistry::instance();
        auto const *base_desc =
            registry.get(ArchetypeRegistry::k_humanoid_base);
        if (base_desc == nullptr) {
          return Render::Creature::k_invalid_archetype;
        }

        ArchetypeDescriptor desc = *base_desc;
        desc.debug_name = "troops/carthage/archer/base";
        auto const attack_bow_clip = desc.bpat_clip[static_cast<std::size_t>(
            AnimationStateId::AttackBow)];
        desc.bpat_clip[static_cast<std::size_t>(AnimationStateId::Idle)] =
            attack_bow_clip;
        desc.bpat_clip[static_cast<std::size_t>(AnimationStateId::Hold)] =
            Render::Creature::k_humanoid_hold_bow_clip;
        return registry.register_archetype(desc);
      }();
      const auto loadout = Render::GL::Nation::resolve_equipment_loadout(
          "troops/carthage/archer");
      const std::array<EquipmentHandle, 5> handles{
          loadout.helmet_handle, loadout.armor_handle, loadout.quiver_handle,
          loadout.bow_handle, loadout.cloak_handle};

      UnitVisualSpec s{};
      s.kind = CreatureKind::Humanoid;
      s.debug_name = "troops/carthage/archer";
      s.scaling = ProportionScaling{1.03F, 1.08F, 0.98F};
      s.owned_legacy_slots = LegacySlotMask::AllHumanoid;
      s.archetype_id = resolve_humanoid_equipment_archetype(
          "troops/carthage/archer", k_archer_base_archetype, handles);
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

    auto next_rand = [](uint32_t &s) -> float {
      s = s * 1664525U + 1013904223U;
      return float(s & 0x7FFFFFU) / float(0x7FFFFFU);
    };

    uint32_t beard_seed = seed ^ 0xBEAD01U;

    v.facial_hair.style = FacialHairStyle::None;

    v.muscularity = 0.95F + next_rand(beard_seed) * 0.25F;
    v.scarring = next_rand(beard_seed) * 0.30F;
    v.weathering = 0.40F + next_rand(beard_seed) * 0.40F;
  }

  void append_companion_preparation(
      const DrawContext &ctx, const HumanoidVariant &variant,
      const HumanoidPose &, const HumanoidAnimationContext &anim_ctx,
      std::uint32_t, Render::Creature::CreatureLOD lod,
      Render::Creature::Pipeline::CreaturePreparationResult &out)
      const override {
    (void)ctx;
    (void)variant;
    (void)anim_ctx;
    (void)lod;
    (void)out;
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
  registry.register_renderer("troops/carthage/archer",
                             [](const DrawContext &ctx, ISubmitter &out) {
                               static ArcherRenderer const static_renderer;
                               static_renderer.render(ctx, out);
                             });
}
} // namespace Render::GL::Carthage
