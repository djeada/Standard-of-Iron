#include "archer_renderer.h"

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

#include "../../../creature/archetype_registry.h"
#include "../../../creature/humanoid_clip_ids.h"
#include "../../../creature/pipeline/creature_render_graph.h"
#include "../../../creature/pipeline/preparation_common.h"
#include "../../../creature/pipeline/unit_visual_spec.h"
#include "../../../equipment/armor/cloak_renderer.h"
#include "../../../equipment/armor/roman_armor.h"
#include "../../../equipment/armor/roman_greaves.h"
#include "../../../equipment/equipment_registry.h"
#include "../../../equipment/helmets/roman_light_helmet.h"
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
#include "../../../humanoid/humanoid_proportion_profiles.h"
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

namespace Render::GL::Roman {

namespace {

constexpr std::string_view k_default_style_key = "default";
constexpr std::string_view k_attachment_headwrap = "carthage_headwrap";

constexpr float k_kneel_depth_multiplier = 1.125F;
constexpr float k_lean_amount_multiplier = 0.83F;
constexpr auto k_profile =
    Render::GL::Humanoid::k_ranged_infantry_proportion_profile.with_offset(
        {.x = -0.04F, .z = -0.01F});
constexpr float k_height_scale_multiplier = 1.02F;
constexpr float k_bulk_scale_multiplier = 0.95F;
constexpr float k_stance_width_multiplier = 0.94F;
constexpr float k_arm_swing_multiplier = 0.96F;

auto style_registry() -> std::unordered_map<std::string, ArcherStyleConfig>& {
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

auto canonical_bow_config() -> BowRenderConfig {
  BowRenderConfig cfg;
  cfg.bow_top_y = HumanProportions::SHOULDER_Y + 0.55F;
  cfg.bow_bot_y = HumanProportions::WAIST_Y - 0.25F;
  cfg.bow_x = 0.0F;
  return cfg;
}

} // namespace

void register_archer_style(const std::string& nation_id,
                           const ArcherStyleConfig& style) {
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
  explicit ArcherRenderer(std::string_view renderer_key = "troops/roman/archer",
                          std::string_view style_key = "roman_republic")
      : m_renderer_key(renderer_key)
      , m_style_key(style_key) {}

  auto get_proportion_scaling() const -> QVector3D override {
    return k_profile.as_vector();
  }

  void adjust_variation(const DrawContext&,
                        uint32_t,
                        VariationParams& variation) const override {
    variation.height_scale *= k_height_scale_multiplier;
    variation.bulk_scale *= k_bulk_scale_multiplier;
    variation.stance_width *= k_stance_width_multiplier;
    variation.arm_swing_amp *= k_arm_swing_multiplier;
  }

  auto get_hold_kneel_depth() const -> float override {
    return k_kneel_depth_multiplier;
  }

  auto
  visual_spec() const -> const Render::Creature::Pipeline::UnitVisualSpec& override {
    using namespace Render::Creature::Pipeline;

    if (m_visual_spec_baked) {
      return m_visual_spec_cache;
    }

    static const auto k_archer_base_archetype = []() {
      using Render::Creature::AnimationStateId;
      using Render::Creature::ArchetypeDescriptor;
      using Render::Creature::ArchetypeRegistry;

      auto& registry = ArchetypeRegistry::instance();
      auto const* base_desc = registry.get(ArchetypeRegistry::k_humanoid_base);
      if (base_desc == nullptr) {
        return Render::Creature::k_invalid_archetype;
      }

      ArchetypeDescriptor desc = *base_desc;
      desc.debug_name = "troops/roman/archer/base";
      auto const attack_bow_clip =
          desc.bpat_clip[static_cast<std::size_t>(AnimationStateId::AttackBow)];
      desc.bpat_clip[static_cast<std::size_t>(AnimationStateId::Idle)] =
          attack_bow_clip;
      desc.bpat_clip[static_cast<std::size_t>(AnimationStateId::Hold)] =
          Render::Creature::k_humanoid_hold_bow_clip;
      desc.bpat_clip[static_cast<std::size_t>(AnimationStateId::AttackRanged)] =
          Render::Creature::k_humanoid_hold_bow_clip;
      return registry.register_archetype(desc);
    }();
    const auto loadout = Render::GL::Nation::resolve_equipment_loadout(m_renderer_key);
    const std::array<EquipmentHandle, 6> handles{loadout.helmet_handle,
                                                 loadout.greaves_handle,
                                                 loadout.quiver_handle,
                                                 loadout.armor_handle,
                                                 loadout.bow_handle,
                                                 loadout.cloak_handle};

    UnitVisualSpec s{};
    s.kind = CreatureKind::Humanoid;
    s.debug_name = m_renderer_key;
    s.scaling = k_profile.as_pipeline_scaling();
    s.owned_legacy_slots = LegacySlotMask::AllHumanoid;
    s.archetype_id = resolve_humanoid_equipment_archetype(
        m_renderer_key, k_archer_base_archetype, handles);
    s.creature_asset_id = Render::Creature::Pipeline::k_humanoid_asset;
    m_visual_spec_cache = s;
    m_visual_spec_baked = true;
    return m_visual_spec_cache;
  }

  void get_variant(const DrawContext& ctx,
                   uint32_t seed,
                   HumanoidVariant& v) const override {
    QVector3D const team_tint = resolve_team_tint(ctx);
    v.palette = make_humanoid_palette(team_tint, seed);
    auto const& style = resolve_style(ctx);
    apply_palette_overrides(style, team_tint, v);
  }

  void append_companion_preparation(
      const DrawContext& ctx,
      const HumanoidVariant& variant,
      const HumanoidPose&,
      const HumanoidAnimationContext& anim_ctx,
      std::uint32_t,
      Render::Creature::CreatureLOD lod,
      Render::Creature::Pipeline::CreaturePreparationResult& out) const override {
    (void)ctx;
    (void)variant;
    (void)anim_ctx;
    (void)lod;
    (void)out;
  }

private:
  std::string_view m_renderer_key;
  std::string_view m_style_key;
  mutable Render::Creature::Pipeline::UnitVisualSpec m_visual_spec_cache{};
  mutable bool m_visual_spec_baked = false;

  auto resolve_style(const DrawContext&) const -> const ArcherStyleConfig& {
    ensure_archer_styles_registered();
    auto& styles = style_registry();
    auto it = styles.find(std::string(m_style_key));
    if (it != styles.end()) {
      return it->second;
    }
    auto fallback = styles.find(std::string(k_default_style_key));
    if (fallback != styles.end()) {
      return fallback->second;
    }
    static const ArcherStyleConfig default_style{};
    return default_style;
  }

  void apply_palette_overrides(const ArcherStyleConfig& style,
                               const QVector3D& team_tint,
                               HumanoidVariant& variant) const {
    auto apply_color = [&](const std::optional<QVector3D>& override_color,
                           QVector3D& target) {
      target = mix_palette_color(
          target, override_color, team_tint, k_team_mix_weight, k_style_mix_weight);
    };

    apply_color(style.cloth_color, variant.palette.cloth);
    apply_color(style.leather_color, variant.palette.leather);
    apply_color(style.leather_dark_color, variant.palette.leather_dark);
    apply_color(style.metal_color, variant.palette.metal);
    apply_color(style.wood_color, variant.palette.wood);
  }
};

void register_archer_renderer(Render::GL::EntityRendererRegistry& registry) {
  ensure_archer_styles_registered();
  registry.register_renderer("troops/roman/archer",
                             [](const DrawContext& ctx, ISubmitter& out) {
                               static ArcherRenderer const static_renderer;
                               static_renderer.render(ctx, out);
                             });
  registry.register_renderer("troops/roman/commanders/marcellus",
                             [](const DrawContext& ctx, ISubmitter& out) {
                               static ArcherRenderer const static_renderer{
                                   "troops/roman/commanders/marcellus"};
                               static_renderer.render(ctx, out);
                             });
}

} // namespace Render::GL::Roman
