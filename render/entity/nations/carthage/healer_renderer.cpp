#include "healer_renderer.h"

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

#include "../../../../game/core/component.h"
#include "../../../../game/core/entity.h"
#include "../../../../game/systems/nation_id.h"
#include "../../../creature/archetype_registry.h"
#include "../../../creature/pipeline/creature_asset.h"
#include "../../../creature/pipeline/unit_visual_spec.h"
#include "../../../equipment/equipment_submit.h"
#include "../../../equipment/humanoid_equipment_archetype.h"
#include "../../../geom/transforms.h"
#include "../../../gl/backend.h"
#include "../../../gl/primitives.h"
#include "../../../gl/render_constants.h"
#include "../../../gl/shader.h"
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
#include "../../healer_renderer_common.h"
#include "../../registry.h"
#include "../../renderer_constants.h"
#include "../equipment_loadout_catalog.h"
#include "healer_style.h"
#include "math/math_utils.h"

using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;

namespace Render::GL::Carthage {

namespace {

constexpr auto k_profile =
    Render::GL::Humanoid::k_support_proportion_profile.with_offset({.x = 0.01F});

void apply_grave_priest_cast_pose_layer(
    const Render::Creature::Pipeline::HumanoidPoseLayerContext& context,
    HumanoidPose& io_pose) {
  if (context.animation == nullptr) {
    return;
  }

  auto const& anim = *context.animation;
  if (!anim.inputs.is_casting || anim.inputs.cast_kind != CastVisualKind::Fireball) {
    return;
  }

  float const phase = std::clamp(anim.attack_phase, 0.0F, 1.0F);
  float const intensity = std::sin(phase * std::numbers::pi_v<float>);
  if (intensity <= 0.0F) {
    return;
  }

  HumanoidPoseController controller(io_pose, anim);
  controller.tilt_torso(-0.08F * intensity, -0.12F * intensity);

  QVector3D const forward = anim.heading_forward();
  QVector3D const right = anim.heading_right();
  QVector3D const up = anim.heading_up();

  io_pose.hand_r += forward * (0.18F + 0.12F * intensity) +
                    up * (0.08F + 0.06F * intensity) - right * 0.03F;
  io_pose.elbow_r += forward * (0.04F + 0.05F * intensity) +
                     up * (0.05F + 0.04F * intensity) - right * 0.04F;
  io_pose.hand_l += -right * 0.05F + up * (0.03F + 0.03F * intensity) - forward * 0.03F;
  io_pose.elbow_l +=
      -right * 0.02F + up * (0.02F + 0.03F * intensity) - forward * 0.02F;
  io_pose.head_pos += up * (0.01F * intensity) + forward * (0.01F * intensity);
}

auto make_healer_spec(std::string_view renderer_key,
                      Render::Creature::Pipeline::CreatureAssetId creature_asset_id,
                      const Render::GL::Humanoid::ProportionProfile& profile)
    -> Render::Creature::Pipeline::UnitVisualSpec {
  using namespace Render::Creature::Pipeline;

  const auto loadout = Render::GL::Nation::resolve_equipment_loadout(renderer_key);
  const std::array<EquipmentHandle, 2> handles{loadout.armor_handle,
                                               loadout.cloak_handle};

  UnitVisualSpec out{};
  out.kind = CreatureKind::Humanoid;
  out.debug_name = renderer_key;
  out.scaling = profile.as_pipeline_scaling();
  out.owned_legacy_slots = LegacySlotMask::AllHumanoid;
  out.archetype_id = resolve_humanoid_equipment_archetype(
      renderer_key, Render::Creature::ArchetypeRegistry::k_humanoid_base, handles);
  out.creature_asset_id = creature_asset_id;
  if (renderer_key == "troops/iron_sepulcher/grave_priest") {
    out.animation_manifest.pose_layer = &apply_grave_priest_cast_pose_layer;
  }
  return out;
}

const Render::Creature::Pipeline::UnitVisualSpec& carthage_healer_visual_spec(
    std::string_view renderer_key,
    std::string_view,
    Render::Creature::Pipeline::CreatureAssetId creature_asset_id,
    const Render::GL::HealerStyleConfig&,
    const Render::GL::Humanoid::ProportionProfile& profile) {
  static const auto healer_spec =
      make_healer_spec("troops/carthage/healer",
                       Render::Creature::Pipeline::k_invalid_creature_asset,
                       profile);
  static const auto grave_priest_spec =
      make_healer_spec("troops/iron_sepulcher/grave_priest",
                       Render::Creature::Pipeline::k_skeleton_humanoid_asset,
                       profile);
  if (renderer_key == "troops/iron_sepulcher/grave_priest") {
    return grave_priest_spec;
  }
  (void)creature_asset_id;
  return healer_spec;
}

void decorate_carthage_healer_variant(const DrawContext&,
                                      std::uint32_t seed,
                                      const Render::GL::HealerStyleConfig& style,
                                      HumanoidVariant& v) {
  auto next_rand = [](uint32_t& s) -> float {
    s = s * 1664525U + 1013904223U;
    return float(s & 0x7FFFFFU) / float(0x7FFFFFU);
  };

  uint32_t beard_seed = seed ^ 0x0EA101U;
  bool wants_beard = style.force_beard;
  if (!wants_beard) {
    float const beard_roll = next_rand(beard_seed);
    wants_beard = (beard_roll < 0.85F);
  }

  if (wants_beard) {
    float const style_roll = next_rand(beard_seed);

    if (style_roll < 0.45F) {
      v.facial_hair.style = FacialHairStyle::ShortBeard;
      v.facial_hair.length = 0.8F + next_rand(beard_seed) * 0.4F;
    } else if (style_roll < 0.75F) {
      v.facial_hair.style = FacialHairStyle::FullBeard;
      v.facial_hair.length = 0.9F + next_rand(beard_seed) * 0.5F;
    } else if (style_roll < 0.90F) {
      v.facial_hair.style = FacialHairStyle::Goatee;
      v.facial_hair.length = 0.7F + next_rand(beard_seed) * 0.4F;
    } else {
      v.facial_hair.style = FacialHairStyle::MustacheAndBeard;
      v.facial_hair.length = 1.0F + next_rand(beard_seed) * 0.4F;
    }

    float const color_roll = next_rand(beard_seed);
    if (color_roll < 0.55F) {

      v.facial_hair.color = QVector3D(0.12F + next_rand(beard_seed) * 0.08F,
                                      0.10F + next_rand(beard_seed) * 0.06F,
                                      0.08F + next_rand(beard_seed) * 0.05F);
    } else if (color_roll < 0.80F) {

      v.facial_hair.color = QVector3D(0.22F + next_rand(beard_seed) * 0.10F,
                                      0.17F + next_rand(beard_seed) * 0.08F,
                                      0.12F + next_rand(beard_seed) * 0.06F);
    } else {

      v.facial_hair.color = QVector3D(0.35F + next_rand(beard_seed) * 0.15F,
                                      0.32F + next_rand(beard_seed) * 0.12F,
                                      0.30F + next_rand(beard_seed) * 0.10F);
      v.facial_hair.greyness = 0.3F + next_rand(beard_seed) * 0.4F;
    }

    v.facial_hair.thickness = 0.85F + next_rand(beard_seed) * 0.25F;
    v.facial_hair.coverage = 0.80F + next_rand(beard_seed) * 0.20F;
  }
}

const HealerRendererProfile k_healer_profile{
    .proportion_profile = k_profile,
    .visual_spec_factory = &carthage_healer_visual_spec,
    .variant_decorator = &decorate_carthage_healer_variant,
    .ensure_styles_registered = &register_carthage_healer_style,
};

const std::array<HealerRendererRegistration, 1> k_healer_renderers{{
    {"troops/carthage/healer", "carthage"},
}};

const std::array<HealerRendererRegistration, 1> k_grave_priest_renderers{{
    {"troops/iron_sepulcher/grave_priest",
     "iron_sepulcher",
     Render::Creature::Pipeline::k_skeleton_humanoid_asset},
}};

} // namespace

void register_healer_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_healer_renderer_profile(registry, k_healer_profile, k_healer_renderers);
}

void register_grave_priest_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_healer_renderer_profile(
      registry, k_healer_profile, k_grave_priest_renderers);
}

} // namespace Render::GL::Carthage
