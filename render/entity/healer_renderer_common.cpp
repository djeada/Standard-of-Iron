#include "healer_renderer_common.h"

#include <array>
#include <memory>
#include <string>
#include <unordered_map>

#include "../../game/core/component.h"
#include "../../game/systems/nation_id.h"
#include "../creature/archetype_registry.h"
#include "../creature/pipeline/unit_visual_spec.h"
#include "../equipment/equipment_registry.h"
#include "../equipment/humanoid_equipment_archetype.h"
#include "../humanoid/humanoid_renderer_base.h"
#include "../humanoid/style_palette.h"
#include "../palette.h"
#include "nations/equipment_loadout_catalog.h"

namespace Render::GL {
namespace {

constexpr std::string_view k_default_style_key = "default";
constexpr float k_team_mix_weight = 0.65F;
constexpr float k_style_mix_weight = 0.35F;

using Render::GL::Humanoid::mix_palette_color;

using StyleRegistry = std::unordered_map<std::string, HealerStyleConfig>;

auto style_registry() -> StyleRegistry& {
  static StyleRegistry styles;
  return styles;
}

auto resolve_healer_style(std::string_view style_key) -> const HealerStyleConfig& {
  auto& styles = style_registry();
  if (auto it = styles.find(std::string(style_key)); it != styles.end()) {
    return it->second;
  }
  if (auto fallback = styles.find(std::string(k_default_style_key));
      fallback != styles.end()) {
    return fallback->second;
  }
  static const HealerStyleConfig default_style{};
  return default_style;
}

auto resolve_healer_style(const DrawContext& ctx,
                          std::string_view style_key) -> const HealerStyleConfig& {
  auto& styles = style_registry();
  std::string nation_id;
  if (ctx.entity != nullptr) {
    if (auto* unit = ctx.entity->get_component<Engine::Core::UnitComponent>()) {
      nation_id = Game::Systems::nation_id_to_string(unit->nation_id);
    }
  }
  if (!nation_id.empty()) {
    if (auto it = styles.find(nation_id); it != styles.end()) {
      return it->second;
    }
  }
  return resolve_healer_style(style_key);
}

class HealerRenderer final : public HumanoidRendererBase {
public:
  HealerRenderer(const HealerRendererProfile& profile,
                 std::string_view renderer_key,
                 std::string_view style_key,
                 Render::Creature::Pipeline::CreatureAssetId creature_asset_id)
      : m_profile(profile)
      , m_renderer_key(renderer_key)
      , m_style_key(style_key)
      , m_creature_asset_id(creature_asset_id) {}

  auto get_proportion_scaling() const -> QVector3D override {
    return m_profile.proportion_profile.as_vector();
  }

  auto get_hold_kneel_depth() const -> float override {
    return m_profile.kneel_depth_multiplier;
  }

  auto
  visual_spec() const -> const Render::Creature::Pipeline::UnitVisualSpec& override {
    using namespace Render::Creature::Pipeline;

    if (m_visual_spec_baked) {
      return m_visual_spec_cache;
    }

    const auto& style = resolve_healer_style(m_style_key);
    if (m_profile.visual_spec_factory != nullptr) {
      return m_profile.visual_spec_factory(m_renderer_key,
                                           m_style_key,
                                           m_creature_asset_id,
                                           style,
                                           m_profile.proportion_profile);
    }

    const auto loadout = Render::GL::Nation::resolve_equipment_loadout(m_renderer_key);
    const std::array<EquipmentHandle, 2> handles{loadout.armor_handle,
                                                 loadout.cloak_handle};

    UnitVisualSpec spec{};
    spec.kind = CreatureKind::Humanoid;
    spec.debug_name = m_renderer_key;
    spec.scaling = m_profile.proportion_profile.as_pipeline_scaling();
    spec.owned_legacy_slots = LegacySlotMask::AllHumanoid;
    spec.archetype_id = resolve_humanoid_equipment_archetype(
        m_renderer_key, Render::Creature::ArchetypeRegistry::k_humanoid_base, handles);
    spec.creature_asset_id = m_creature_asset_id;
    m_visual_spec_cache = spec;
    m_visual_spec_baked = true;
    return m_visual_spec_cache;
  }

  void get_variant(const DrawContext& ctx,
                   uint32_t seed,
                   HumanoidVariant& variant) const override {
    QVector3D const team_tint = resolve_team_tint(ctx);
    variant.palette = make_humanoid_palette(team_tint, seed);
    const auto& style = resolve_healer_style(ctx, m_style_key);
    apply_palette_overrides(style, team_tint, variant);
    if (m_profile.variant_decorator != nullptr) {
      m_profile.variant_decorator(ctx, seed, style, variant);
    }
  }

private:
  const HealerRendererProfile& m_profile;
  std::string_view m_renderer_key;
  std::string_view m_style_key;
  Render::Creature::Pipeline::CreatureAssetId m_creature_asset_id;
  mutable Render::Creature::Pipeline::UnitVisualSpec m_visual_spec_cache{};
  mutable bool m_visual_spec_baked{false};

  void apply_palette_overrides(const HealerStyleConfig& style,
                               const QVector3D& team_tint,
                               HumanoidVariant& variant) const {
    auto apply_color = [&](const std::optional<QVector3D>& override_color,
                           QVector3D& target,
                           float team_weight = k_team_mix_weight,
                           float style_weight = k_style_mix_weight) {
      target = mix_palette_color(
          target, override_color, team_tint, team_weight, style_weight);
    };

    apply_color(style.skin_color, variant.palette.skin, 0.0F, 1.0F);
    apply_color(style.cloth_color, variant.palette.cloth);
    apply_color(style.leather_color, variant.palette.leather);
    apply_color(style.leather_dark_color, variant.palette.leather_dark);
    apply_color(style.metal_color, variant.palette.metal);
    apply_color(style.wood_color, variant.palette.wood);
  }
};

} // namespace

void register_healer_style(std::string_view style_key, const HealerStyleConfig& style) {
  style_registry()[std::string(style_key)] = style;
}

void register_healer_styles(std::span<const HealerStyleRegistration> styles) {
  for (const auto& style : styles) {
    register_healer_style(style.key, style.style);
  }
}

void register_healer_renderer_profile(
    EntityRendererRegistry& registry,
    const HealerRendererProfile& profile,
    std::span<const HealerRendererRegistration> renderers) {
  if (profile.ensure_styles_registered != nullptr) {
    profile.ensure_styles_registered();
  }

  for (const auto& renderer : renderers) {
    auto renderer_instance = std::make_shared<HealerRenderer>(
        profile, renderer.renderer_key, renderer.style_key, renderer.creature_asset_id);
    registry.register_renderer(
        std::string(renderer.renderer_key),
        [renderer_instance](const DrawContext& ctx, ISubmitter& out) {
          renderer_instance->render(ctx, out);
        });
  }
}

} // namespace Render::GL
