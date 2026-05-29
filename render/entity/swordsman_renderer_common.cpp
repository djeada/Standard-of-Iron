#include "swordsman_renderer_common.h"

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
constexpr float k_team_mix_weight = 0.6F;
constexpr float k_style_mix_weight = 0.4F;

using Render::GL::Humanoid::mix_palette_color;

using StyleRegistry = std::unordered_map<std::string, SwordsmanStyleConfig>;

auto style_registry() -> StyleRegistry& {
  static StyleRegistry styles;
  return styles;
}

auto resolve_style(const DrawContext& ctx) -> const SwordsmanStyleConfig& {
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
  if (auto it_default = styles.find(std::string(k_default_style_key));
      it_default != styles.end()) {
    return it_default->second;
  }
  static const SwordsmanStyleConfig empty{};
  return empty;
}

auto loadout_handle_for_slot(const Nation::ResolvedEquipmentLoadout& loadout,
                             SwordsmanLoadoutSlot slot) -> EquipmentHandle {
  switch (slot) {
  case SwordsmanLoadoutSlot::Helmet:
    return loadout.helmet_handle;
  case SwordsmanLoadoutSlot::Greaves:
    return loadout.greaves_handle;
  case SwordsmanLoadoutSlot::Shoulder:
    return loadout.shoulder_handle;
  case SwordsmanLoadoutSlot::Shield:
    return loadout.shield_handle;
  case SwordsmanLoadoutSlot::Armor:
    return loadout.armor_handle;
  case SwordsmanLoadoutSlot::Sword:
    return loadout.sword_handle;
  case SwordsmanLoadoutSlot::Cloak:
    return loadout.cloak_handle;
  }
  return k_invalid_equipment_handle;
}

class SwordsmanRenderer final : public HumanoidRendererBase {
public:
  SwordsmanRenderer(const SwordsmanRendererProfile& profile,
                    std::string_view renderer_key,
                    Render::Creature::Pipeline::CreatureAssetId creature_asset_id)
      : m_profile(profile)
      , m_renderer_key(renderer_key)
      , m_creature_asset_id(creature_asset_id) {}

  auto get_proportion_scaling() const -> QVector3D override {
    return m_profile.proportion_profile.as_vector();
  }

  auto get_torso_scale() const -> float override {
    return m_profile.proportion_profile.torso_scale;
  }

  void adjust_variation(const DrawContext&,
                        uint32_t,
                        VariationParams& variation) const override {
    variation.bulk_scale *= m_profile.variation.bulk_scale;
    variation.stance_width *= m_profile.variation.stance_width;
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

    const auto loadout = Render::GL::Nation::resolve_equipment_loadout(m_renderer_key);
    std::array<EquipmentHandle, 7> handles{};
    for (std::size_t i = 0; i < m_profile.loadout_slot_count; ++i) {
      handles[i] = loadout_handle_for_slot(loadout, m_profile.loadout_slots[i]);
    }

    UnitVisualSpec spec{};
    spec.kind = CreatureKind::Humanoid;
    spec.debug_name = m_renderer_key;
    spec.scaling = m_profile.proportion_profile.as_pipeline_scaling();
    spec.owned_legacy_slots = LegacySlotMask::AllHumanoid;
    spec.archetype_id = resolve_humanoid_equipment_archetype(
        m_renderer_key,
        Render::Creature::ArchetypeRegistry::k_humanoid_base,
        std::span<const EquipmentHandle>(handles.data(), m_profile.loadout_slot_count));
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
    apply_palette_overrides(resolve_style(ctx), team_tint, variant);
  }

private:
  const SwordsmanRendererProfile& m_profile;
  std::string_view m_renderer_key;
  Render::Creature::Pipeline::CreatureAssetId m_creature_asset_id;

  void apply_palette_overrides(const SwordsmanStyleConfig& style,
                               const QVector3D& team_tint,
                               HumanoidVariant& variant) const {
    auto apply_color = [&](const std::optional<QVector3D>& override_color,
                           QVector3D& target,
                           float team_weight = k_team_mix_weight,
                           float style_weight = k_style_mix_weight) {
      target = mix_palette_color(
          target, override_color, team_tint, team_weight, style_weight);
    };

    if (m_profile.apply_skin_override) {
      apply_color(style.skin_color, variant.palette.skin, 0.0F, 1.0F);
    }
    apply_color(style.cloth_color, variant.palette.cloth);
    apply_color(style.leather_color, variant.palette.leather);
    apply_color(style.leather_dark_color, variant.palette.leather_dark);
    apply_color(style.metal_color, variant.palette.metal);
  }
};

} // namespace

void register_swordsman_style(std::string_view style_key,
                              const SwordsmanStyleConfig& style) {
  style_registry()[std::string(style_key)] = style;
}

void register_swordsman_styles(std::span<const SwordsmanStyleRegistration> styles) {
  for (const auto& style : styles) {
    register_swordsman_style(style.key, style.style);
  }
}

void register_swordsman_renderer_profile(
    EntityRendererRegistry& registry,
    const SwordsmanRendererProfile& profile,
    std::span<const SwordsmanRendererRegistration> renderers) {
  if (profile.ensure_styles_registered != nullptr) {
    profile.ensure_styles_registered();
  }

  for (const auto& renderer : renderers) {
    auto renderer_instance = std::make_shared<SwordsmanRenderer>(
        profile, renderer.renderer_key, renderer.creature_asset_id);
    registry.register_renderer(
        std::string(renderer.renderer_key),
        [renderer_instance](const DrawContext& ctx, ISubmitter& out) {
          renderer_instance->render(ctx, out);
        });
  }
}

} // namespace Render::GL
