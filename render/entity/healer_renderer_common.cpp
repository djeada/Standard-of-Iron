#include "render/entity/healer_renderer_common.h"

#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <unordered_map>

#include "game/core/component_core.h"
#include "game/systems/nation_id.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/humanoid/asset/humanoid_spec.h"
#include "render/creature/humanoid/runtime/humanoid_renderer.h"
#include "render/creature/humanoid/runtime/pose_controller.h"
#include "render/creature/humanoid/runtime/style_palette.h"
#include "render/creature/humanoid/schema/skeleton_schema.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/entity/civilian_actor.h"
#include "render/entity/nations/equipment_loadout_catalog.h"
#include "render/equipment/equipment_registry.h"
#include "render/equipment/humanoid_equipment_archetype.h"
#include "render/frame/palette.h"

namespace Render::GL {
namespace {

constexpr std::string_view k_default_style_key = "default";

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
      , m_creature_asset_id(creature_asset_id) {
    set_visual_spec(build_visual_spec());
  }

  auto get_hold_kneel_depth() const -> float override {
    return m_profile.kneel_depth_multiplier;
  }

  auto build_visual_spec() const -> Render::Creature::Pipeline::UnitVisualSpec {
    using namespace Render::Creature::Pipeline;

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
    spec.archetype_id = resolve_humanoid_equipment_archetype(
        m_renderer_key, Render::Creature::ArchetypeRegistry::k_humanoid_base, handles);
    spec.creature_asset_id = m_creature_asset_id;
    return spec;
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
    if (m_facial_hair.has_value()) {
      variant.facial_hair = *m_facial_hair;
    }
  }

private:
  const HealerRendererProfile& m_profile;
  std::string_view m_renderer_key;
  std::optional<FacialHairParams> m_facial_hair{
      Render::GL::Nation::resolve_equipment_loadout(m_renderer_key).ids.facial_hair};
  std::string_view m_style_key;
  Render::Creature::Pipeline::CreatureAssetId m_creature_asset_id;

  void apply_palette_overrides(const HealerStyleConfig& style,
                               const QVector3D& team_tint,
                               HumanoidVariant& variant) const {
    if (style.skin_color) {
      variant.palette.skin = Humanoid::saturate_color(*style.skin_color);
    }
    Humanoid::apply_uniform_palette(variant.palette, style, team_tint, {0.09F, 0.91F});
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

namespace {

std::array<std::shared_ptr<const HealerRenderer>, 2> g_priest_renderers{};

template <std::size_t Nation>
void fill_priest_variant(const DrawContext& ctx,
                         std::uint32_t seed,
                         HumanoidVariant& v) {
  if (const auto& renderer = g_priest_renderers[Nation]; renderer != nullptr) {
    renderer->get_variant(ctx, seed, v);
  }
}

void register_priest_rig(std::string_view renderer_key,
                         const std::shared_ptr<const HealerRenderer>& renderer) {
  const bool carthage = renderer_key.find("carthage") != std::string_view::npos;
  if (!carthage && renderer_key.find("roman") == std::string_view::npos) {
    return;
  }
  g_priest_renderers[carthage ? 1U : 0U] = renderer;
  NationCivilianRig rig{};
  rig.spec = renderer->build_visual_spec();
  rig.idle = rig.spec.archetype_id;
  rig.working = rig.spec.archetype_id;
  rig.fill_variant = carthage ? &fill_priest_variant<1> : &fill_priest_variant<0>;
  register_nation_priest_rig(carthage, rig);
}

} // namespace

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
    register_humanoid_renderer(
        registry, std::string(renderer.renderer_key), renderer_instance);
    register_priest_rig(renderer.renderer_key, renderer_instance);
  }
}

} // namespace Render::GL
