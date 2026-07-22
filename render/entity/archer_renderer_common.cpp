#include "archer_renderer_common.h"

#include <array>
#include <memory>
#include <string>
#include <unordered_map>

#include "../creature/archetype_registry.h"
#include "../creature/humanoid_clip_ids.h"
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

using StyleRegistry = std::unordered_map<std::string, ArcherStyleConfig>;

auto style_registry() -> StyleRegistry& {
  static StyleRegistry styles;
  return styles;
}

auto resolve_style(std::string_view style_key) -> const ArcherStyleConfig& {
  auto& styles = style_registry();
  if (auto it = styles.find(std::string(style_key)); it != styles.end()) {
    return it->second;
  }
  if (auto fallback = styles.find(std::string(k_default_style_key));
      fallback != styles.end()) {
    return fallback->second;
  }
  static const ArcherStyleConfig default_style{};
  return default_style;
}

auto archer_base_archetype(const ArcherRendererProfile& profile)
    -> Render::Creature::ArchetypeId {
  using Render::Creature::AnimationStateId;
  using Render::Creature::ArchetypeDescriptor;
  using Render::Creature::ArchetypeRegistry;

  static std::unordered_map<std::string, Render::Creature::ArchetypeId> cache;
  const std::string debug_name(profile.base_debug_name);
  if (auto it = cache.find(debug_name); it != cache.end()) {
    return it->second;
  }

  auto& registry = ArchetypeRegistry::instance();
  auto const* base_desc = registry.get(ArchetypeRegistry::k_humanoid_base);
  if (base_desc == nullptr) {
    return Render::Creature::k_invalid_archetype;
  }

  ArchetypeDescriptor desc = *base_desc;
  desc.debug_name = debug_name;
  auto const attack_bow_clip =
      desc.bpat_clip[static_cast<std::size_t>(AnimationStateId::AttackBow)];
  desc.bpat_clip[static_cast<std::size_t>(AnimationStateId::Idle)] = attack_bow_clip;
  desc.bpat_clip[static_cast<std::size_t>(AnimationStateId::Hold)] =
      Render::Creature::k_humanoid_hold_bow_clip;
  desc.bpat_clip[static_cast<std::size_t>(AnimationStateId::AttackRanged)] =
      Render::Creature::k_humanoid_hold_bow_clip;
  const auto archetype_id = registry.register_archetype(desc);
  cache.emplace(debug_name, archetype_id);
  return archetype_id;
}

auto loadout_handle_for_slot(const Nation::ResolvedEquipmentLoadout& loadout,
                             ArcherLoadoutSlot slot) -> EquipmentHandle {
  switch (slot) {
  case ArcherLoadoutSlot::Helmet:
    return loadout.helmet_handle;
  case ArcherLoadoutSlot::Greaves:
    return loadout.greaves_handle;
  case ArcherLoadoutSlot::Quiver:
    return loadout.quiver_handle;
  case ArcherLoadoutSlot::Armor:
    return loadout.armor_handle;
  case ArcherLoadoutSlot::Bow:
    return loadout.bow_handle;
  case ArcherLoadoutSlot::Cloak:
    return loadout.cloak_handle;
  }
  return k_invalid_equipment_handle;
}

class ArcherRenderer final : public HumanoidRendererBase {
public:
  ArcherRenderer(const ArcherRendererProfile& profile,
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

  void adjust_variation(const DrawContext&,
                        uint32_t,
                        VariationParams& variation) const override {
    variation.height_scale *= m_profile.variation.height_scale;
    variation.bulk_scale *= m_profile.variation.bulk_scale;
    variation.stance_width *= m_profile.variation.stance_width;
    variation.arm_swing_amp *= m_profile.variation.arm_swing_amp;
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

    if (m_base_archetype == Render::Creature::k_invalid_archetype) {
      m_base_archetype = archer_base_archetype(m_profile);
    }

    const auto loadout = Render::GL::Nation::resolve_equipment_loadout(m_renderer_key);
    std::array<EquipmentHandle, 6> handles{};
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
        m_base_archetype,
        std::span<const EquipmentHandle>(handles.data(), m_profile.loadout_slot_count));
    spec.creature_asset_id = m_creature_asset_id;
    spec.animation_manifest.melee_clip_override =
        Render::Creature::k_humanoid_archer_melee_clip;
    m_visual_spec_cache = spec;
    m_visual_spec_baked = true;
    return m_visual_spec_cache;
  }

  void get_variant(const DrawContext& ctx,
                   uint32_t seed,
                   HumanoidVariant& variant) const override {
    QVector3D const team_tint = resolve_team_tint(ctx);
    variant.palette = make_humanoid_palette(team_tint, seed);
    apply_palette_overrides(resolve_style(m_style_key), team_tint, variant);

    if (m_profile.apply_carthage_variant_traits) {
      auto next_rand = [](uint32_t& s) -> float {
        s = s * 1664525U + 1013904223U;
        return float(s & 0x7FFFFFU) / float(0x7FFFFFU);
      };

      uint32_t beard_seed = seed ^ 0xBEAD01U;
      variant.facial_hair.style = FacialHairStyle::None;
      variant.muscularity = 0.95F + next_rand(beard_seed) * 0.25F;
      variant.scarring = next_rand(beard_seed) * 0.30F;
      variant.weathering = 0.40F + next_rand(beard_seed) * 0.40F;
    }
  }

  void append_companion_preparation(
      const DrawContext&,
      const HumanoidVariant&,
      const HumanoidPose&,
      const HumanoidAnimationContext&,
      std::uint32_t,
      Render::Creature::CreatureLOD,
      Render::Creature::Pipeline::CreaturePreparationResult&) const override {}

private:
  const ArcherRendererProfile& m_profile;
  std::string_view m_renderer_key;
  std::string_view m_style_key;
  Render::Creature::Pipeline::CreatureAssetId m_creature_asset_id;
  mutable Render::Creature::ArchetypeId m_base_archetype{
      Render::Creature::k_invalid_archetype};

  void apply_palette_overrides(const ArcherStyleConfig& style,
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
    apply_color(style.wood_color, variant.palette.wood);
  }
};

} // namespace

void register_archer_style(std::string_view style_key, const ArcherStyleConfig& style) {
  style_registry()[std::string(style_key)] = style;
}

void register_archer_styles(std::span<const ArcherStyleRegistration> styles) {
  for (const auto& style : styles) {
    register_archer_style(style.key, style.style);
  }
}

void register_archer_renderer_profile(
    EntityRendererRegistry& registry,
    const ArcherRendererProfile& profile,
    std::span<const ArcherRendererRegistration> renderers) {
  if (profile.ensure_styles_registered != nullptr) {
    profile.ensure_styles_registered();
  }

  for (const auto& renderer : renderers) {
    auto renderer_instance = std::make_shared<ArcherRenderer>(
        profile, renderer.renderer_key, renderer.style_key, renderer.creature_asset_id);
    registry.register_renderer(
        std::string(renderer.renderer_key),
        [renderer_instance](const DrawContext& ctx, ISubmitter& out) {
          renderer_instance->render(ctx, out);
        });
  }
}

} // namespace Render::GL
