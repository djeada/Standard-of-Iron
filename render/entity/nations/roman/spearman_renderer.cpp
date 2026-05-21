#include "spearman_renderer.h"

#include <QMatrix4x4>
#include <QString>
#include <QVector3D>
#include <qstringliteral.h>
#include <qvectornd.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

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
#include "../../../equipment/humanoid_equipment_archetype.h"
#include "../../../equipment/weapons/spear_renderer.h"
#include "../../../geom/math_utils.h"
#include "../../../geom/transforms.h"
#include "../../../gl/primitives.h"
#include "../../../humanoid/humanoid_math.h"
#include "../../../humanoid/humanoid_proportion_profiles.h"
#include "../../../humanoid/humanoid_renderer_base.h"
#include "../../../humanoid/humanoid_spec.h"
#include "../../../humanoid/humanoid_specs.h"
#include "../../../humanoid/skeleton.h"
#include "../../../humanoid/spear_pose_utils.h"
#include "../../../humanoid/style_palette.h"
#include "../../../palette.h"
#include "../../../submitter.h"
#include "../../registry.h"
#include "../../renderer_constants.h"
#include "../equipment_loadout_catalog.h"
#include "spearman_style.h"
namespace Render::GL::Roman {

namespace {

constexpr std::string_view k_spearman_default_style_key = "default";
constexpr float k_spearman_team_mix_weight = 0.6F;
constexpr float k_spearman_style_mix_weight = 0.4F;

constexpr float k_kneel_depth_multiplier = 0.875F;
constexpr float k_lean_amount_multiplier = 0.67F;
constexpr auto k_profile =
    Render::GL::Humanoid::k_polearm_infantry_proportion_profile.with_offset(
        {.x = 0.02F, .y = -0.01F, .torso_scale = -0.03F});
constexpr float k_bulk_scale_multiplier = 0.94F;
constexpr float k_stance_width_multiplier = 0.95F;

auto spearman_style_registry()
    -> std::unordered_map<std::string, SpearmanStyleConfig>& {
  static std::unordered_map<std::string, SpearmanStyleConfig> styles;
  return styles;
}

void ensure_spearman_styles_registered() {
  static const bool registered = []() {
    register_roman_spearman_style();
    return true;
  }();
  (void)registered;
}

} // namespace

void register_spearman_style(const std::string& nation_id,
                             const SpearmanStyleConfig& style) {
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
  float spearhead_length = 0.18F;
};

class SpearmanRenderer : public HumanoidRendererBase {
public:
  explicit SpearmanRenderer(std::string_view renderer_key = "troops/roman/spearman")
      : m_renderer_key(renderer_key) {}

  auto get_proportion_scaling() const -> QVector3D override {
    return k_profile.as_vector();
  }

  auto get_torso_scale() const -> float override { return k_profile.torso_scale; }

  void adjust_variation(const DrawContext&,
                        uint32_t,
                        VariationParams& variation) const override {
    variation.bulk_scale *= k_bulk_scale_multiplier;
    variation.stance_width *= k_stance_width_multiplier;
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

    const auto loadout = Render::GL::Nation::resolve_equipment_loadout(m_renderer_key);
    const std::array<EquipmentHandle, 5> handles{loadout.helmet_handle,
                                                 loadout.greaves_handle,
                                                 loadout.shoulder_handle,
                                                 loadout.armor_handle,
                                                 loadout.spear_handle};

    UnitVisualSpec s{};
    s.kind = CreatureKind::Humanoid;
    s.debug_name = m_renderer_key;
    s.creature_asset_id = Render::Creature::Pipeline::k_humanoid_spear_asset;
    s.scaling = k_profile.as_pipeline_scaling();
    s.owned_legacy_slots = LegacySlotMask::AllHumanoid;
    s.archetype_id = resolve_humanoid_equipment_archetype(
        m_renderer_key, Render::Creature::ArchetypeRegistry::k_humanoid_base, handles);
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

private:
  std::string_view m_renderer_key;
  mutable Render::Creature::Pipeline::UnitVisualSpec m_visual_spec_cache{};
  mutable bool m_visual_spec_baked = false;

  static auto compute_spearman_extras(uint32_t seed,
                                      const HumanoidVariant& v) -> SpearmanExtras {
    SpearmanExtras e;

    e.spear_shaft_color = v.palette.leather * QVector3D(0.85F, 0.75F, 0.65F);
    e.spearhead_color = QVector3D(0.75F, 0.76F, 0.80F);

    e.spear_length = 1.15F + (hash_01(seed ^ 0xABCDU) - 0.5F) * 0.10F;
    e.spear_shaft_radius = 0.018F + (hash_01(seed ^ 0x7777U) - 0.5F) * 0.003F;
    e.spearhead_length = 0.16F + (hash_01(seed ^ 0xBEEFU) - 0.5F) * 0.04F;

    return e;
  }

  auto resolve_style(const DrawContext& ctx) const -> const SpearmanStyleConfig& {
    ensure_spearman_styles_registered();
    auto& styles = spearman_style_registry();
    std::string nation_id;
    if (ctx.entity != nullptr) {
      if (auto* unit = ctx.entity->get_component<Engine::Core::UnitComponent>()) {
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

  void apply_palette_overrides(const SpearmanStyleConfig& style,
                               const QVector3D& team_tint,
                               HumanoidVariant& variant) const {
    auto apply_color = [&](const std::optional<QVector3D>& override_color,
                           QVector3D& target) {
      target = mix_palette_color(target,
                                 override_color,
                                 team_tint,
                                 k_spearman_team_mix_weight,
                                 k_spearman_style_mix_weight);
    };

    apply_color(style.cloth_color, variant.palette.cloth);
    apply_color(style.leather_color, variant.palette.leather);
    apply_color(style.leather_dark_color, variant.palette.leather_dark);
    apply_color(style.metal_color, variant.palette.metal);
  }

  void apply_extras_overrides(const SpearmanStyleConfig& style,
                              const QVector3D& team_tint,
                              [[maybe_unused]] const HumanoidVariant& variant,
                              SpearmanExtras& extras) const {
    extras.spear_shaft_color = saturate_color(extras.spear_shaft_color);
    extras.spearhead_color = saturate_color(extras.spearhead_color);

    auto apply_color = [&](const std::optional<QVector3D>& override_color,
                           QVector3D& target) {
      target = mix_palette_color(target,
                                 override_color,
                                 team_tint,
                                 k_spearman_team_mix_weight,
                                 k_spearman_style_mix_weight);
    };

    apply_color(style.spear_shaft_color, extras.spear_shaft_color);
    apply_color(style.spearhead_color, extras.spearhead_color);

    if (style.spear_length_scale) {
      extras.spear_length =
          std::max(0.80F, extras.spear_length * *style.spear_length_scale);
    }
  }
};

void register_spearman_renderer(Render::GL::EntityRendererRegistry& registry) {
  ensure_spearman_styles_registered();
  registry.register_renderer("troops/roman/spearman",
                             [](const DrawContext& ctx, ISubmitter& out) {
                               static SpearmanRenderer const static_renderer;
                               static_renderer.render(ctx, out);
                             });
  registry.register_renderer("troops/roman/commanders/fabius_maximus",
                             [](const DrawContext& ctx, ISubmitter& out) {
                               static SpearmanRenderer const static_renderer{
                                   "troops/roman/commanders/fabius_maximus"};
                               static_renderer.render(ctx, out);
                             });
}

} // namespace Render::GL::Roman
