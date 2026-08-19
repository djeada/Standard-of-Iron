#include "healer_renderer_common.h"

#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <unordered_map>

#include "game/core/component.h"
#include "game/systems/nation_id.h"
#include "nations/equipment_loadout_catalog.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/equipment/equipment_registry.h"
#include "render/equipment/humanoid_equipment_archetype.h"
#include "render/humanoid/humanoid_renderer_base.h"
#include "render/humanoid/humanoid_spec.h"
#include "render/humanoid/pose_controller.h"
#include "render/humanoid/skeleton.h"
#include "render/humanoid/style_palette.h"
#include "render/palette.h"

namespace Render::GL {
namespace {

constexpr std::string_view k_default_style_key = "default";
constexpr float k_team_mix_weight = 0.65F;
constexpr float k_style_mix_weight = 0.35F;

constexpr float k_leather_team_mix_weight = 0.15F;
constexpr float k_cloth_team_mix_weight = 0.09F;
constexpr float k_cloth_style_mix_weight = 0.91F;

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
    apply_color(style.cloth_color,
                variant.palette.cloth,
                k_cloth_team_mix_weight,
                k_cloth_style_mix_weight);
    apply_color(
        style.leather_color, variant.palette.leather, k_leather_team_mix_weight);
    apply_color(style.leather_dark_color,
                variant.palette.leather_dark,
                k_leather_team_mix_weight);
    apply_color(style.metal_color, variant.palette.metal);
    apply_color(style.wood_color, variant.palette.wood);
  }
};

auto channel_pose_weight(const Render::GL::HumanoidAnimationContext& anim) -> float {
  const auto& inputs = anim.inputs;
  if (inputs.is_dying || inputs.is_dead || inputs.is_attacking ||
      inputs.is_hit_reacting || inputs.is_routing) {
    return 0.0F;
  }
  if (inputs.is_healing || inputs.is_casting) {
    return 1.0F;
  }
  switch (inputs.movement_state) {
  case Render::Creature::MovementAnimationState::Idle:
    return 1.0F;
  case Render::Creature::MovementAnimationState::Walk:
    return 0.55F;
  default:
    return 0.22F;
  }
}

} // namespace

void apply_healer_channel_pose_layer(
    const Render::Creature::Pipeline::HumanoidPoseLayerContext& context,
    HumanoidPose& io_pose) {
  Render::Humanoid::apply_skeleton_proportion_pose_layer(context, io_pose);
  if (context.animation == nullptr) {
    return;
  }
  const auto& anim = *context.animation;
  float const weight = channel_pose_weight(anim);
  if (weight <= 0.0F) {
    return;
  }

  QVector3D const forward = anim.heading_forward();
  QVector3D const right = anim.heading_right();
  QVector3D const up = anim.heading_up();
  float const breathe = 0.014F * std::sin(anim.inputs.time * 1.6F);

  HumanoidPoseController controller(io_pose, anim);
  controller.tilt_torso(0.0F, -0.07F * weight);

  io_pose.hand_l +=
      (forward * 0.30F + up * (0.20F + breathe) + right * 0.045F) * weight;
  io_pose.hand_r +=
      (forward * 0.30F + up * (0.20F - breathe) - right * 0.045F) * weight;
  io_pose.elbow_l += (forward * 0.09F + up * 0.04F - right * 0.10F) * weight;
  io_pose.elbow_r += (forward * 0.09F + up * 0.04F + right * 0.10F) * weight;
  io_pose.head_pos += forward * (0.012F * weight);
}

void apply_healer_staff_pose_layer(
    const Render::Creature::Pipeline::HumanoidPoseLayerContext& context,
    HumanoidPose& io_pose) {
  Render::Humanoid::apply_skeleton_proportion_pose_layer(context, io_pose);
  if (context.animation == nullptr) {
    return;
  }
  const auto& anim = *context.animation;

  QVector3D const forward = anim.heading_forward();
  QVector3D const right = anim.heading_right();
  QVector3D const up = anim.heading_up();

  io_pose.hand_r += forward * 0.20F + right * 0.13F - up * 0.02F;
  io_pose.elbow_r += forward * 0.05F + right * 0.11F - up * 0.04F;

  float const weight = channel_pose_weight(anim);
  if (weight <= 0.0F) {
    return;
  }
  float const breathe = 0.016F * std::sin(anim.inputs.time * 1.4F);

  HumanoidPoseController controller(io_pose, anim);
  controller.tilt_torso(0.0F, -0.06F * weight);
  io_pose.hand_l += (forward * 0.34F + up * (0.26F + breathe) + right * 0.02F) * weight;
  io_pose.elbow_l += (forward * 0.10F + up * 0.06F - right * 0.09F) * weight;
  io_pose.head_pos += forward * (0.010F * weight);
}

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
    register_humanoid_renderer(
        registry, std::string(renderer.renderer_key), renderer_instance);
  }
}

} // namespace Render::GL
