#include "skirmisher_renderer.h"

#include <array>
#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

#include "animation/clip_manifest.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/pipeline/creature_asset.h"
#include "render/equipment/armor/allied_garments.h"
#include "render/equipment/attachment_builder.h"
#include "render/equipment/generated_equipment.h"
#include "render/equipment/weapons/shield_anchor.h"
#include "render/equipment/weapons/shield_renderer.h"
#include "render/humanoid/asset/humanoid_spec.h"
#include "render/humanoid/runtime/humanoid_renderer.h"
#include "render/humanoid/runtime/skeleton_evaluator.h"
#include "render/humanoid/schema/humanoid_proportion_profiles.h"

namespace Render::GL {
namespace {
using Render::Humanoid::HumanoidBone;
constexpr std::uint8_t k_gear_roles = 9U;
constexpr std::uint8_t k_shield_roles = 17U;
constexpr std::uint8_t k_tunic_roles = 23U;
enum GearColor : std::uint8_t {
  Linen,
  Leather,
  Fur,
  Dark,
  Ivory,
  Metal,
  Crimson,
  Wood
};

auto parma_config() -> const ShieldRenderConfig& {
  static const ShieldRenderConfig config = [] {
    ShieldRenderConfig c;
    c.shield_radius = 0.095F;
    c.shield_color = {0.52F, 0.13F, 0.10F};
    c.trim_color = {0.82F, 0.70F, 0.43F};
    c.metal_color = {0.68F, 0.69F, 0.66F};
    c.dome_depth = 0.18F;
    c.has_radial_decoration = true;
    return c;
  }();
  return config;
}

auto wolf_pelt() -> const RenderArchetype& {
  static const auto value = [] {
    // Open jaw sits above the brow; the human face remains visible below it.
    std::vector<GeneratedEquipmentPrimitive> p{
        generated_ellipsoid({0.0F, 0.60F, -0.10F}, {1.10F, 0.72F, 1.16F}, Fur),
        generated_ellipsoid({0.0F, 0.56F, 0.91F}, {0.63F, 0.33F, 0.72F}, Fur),
        generated_ellipsoid({0.0F, 0.48F, 1.52F}, {0.30F, 0.21F, 0.19F}, Dark),
        generated_ellipsoid({0.0F, 0.42F, 1.01F}, {0.48F, 0.08F, 0.57F}, Dark),
        generated_ellipsoid({0.0F, 0.34F, 0.95F}, {0.49F, 0.06F, 0.49F}, Ivory),
        generated_ellipsoid({0.0F, -0.17F, -0.85F}, {1.06F, 0.86F, 0.38F}, Fur),
    };
    for (float side : {-1.0F, 1.0F}) {
      p.push_back(generated_cone(
          {side * 0.73F, 0.99F, -0.26F}, {side * 0.85F, 1.69F, -0.37F}, 0.30F, Fur));
      p.push_back(generated_cone(
          {side * 0.73F, 1.12F, -0.01F}, {side * 0.84F, 1.55F, -0.20F}, 0.15F, Dark));
      p.push_back(generated_ellipsoid(
          {side * 0.57F, 0.73F, 0.95F}, {0.15F, 0.10F, 0.09F}, Dark));
      p.push_back(generated_sphere({side * 0.57F, 0.73F, 1.03F}, 0.043F, Ivory));
      p.push_back(generated_cone(
          {side * 0.37F, 0.34F, 1.24F}, {side * 0.34F, 0.10F, 1.27F}, 0.067F, Ivory));
      for (int i = 0; i < 4; ++i) {
        float const y = 0.22F - static_cast<float>(i) * 0.25F;
        p.push_back(generated_cone(
            {side * 0.91F, y, -0.50F}, {side * 1.17F, y - 0.38F, -0.57F}, 0.24F, Fur));
      }
    }
    return build_generated_equipment_archetype("velites/wolf_pelt", p);
  }();
  return value;
}

auto slinger_headband() -> const RenderArchetype& {
  static const auto value = [] {
    std::vector<GeneratedEquipmentPrimitive> p;
    for (int i = 0; i < 20; ++i) {
      float const a = static_cast<float>(i) * std::numbers::pi_v<float> / 10.0F;
      float const b = static_cast<float>(i + 1) * std::numbers::pi_v<float> / 10.0F;
      p.push_back(generated_cylinder({0.80F * std::sin(a), 0.72F, 0.88F * std::cos(a)},
                                     {0.80F * std::sin(b), 0.72F, 0.88F * std::cos(b)},
                                     0.085F,
                                     Crimson));
    }
    p.push_back(
        generated_ellipsoid({0.0F, 0.68F, -0.90F}, {0.19F, 0.15F, 0.13F}, Crimson));
    p.push_back(generated_cylinder(
        {-0.07F, 0.63F, -0.93F}, {-0.22F, -0.10F, -1.05F}, 0.07F, Crimson));
    p.push_back(generated_cylinder(
        {0.06F, 0.63F, -0.93F}, {0.21F, 0.05F, -1.05F}, 0.07F, Crimson));
    return build_generated_equipment_archetype("slinger/headband", p);
  }();
  return value;
}

auto hand_weapon(bool velites) -> const RenderArchetype& {
  static const auto sling = [] {
    std::array p{
        generated_cylinder(
            {-0.010F, 0.0F, 0.0F}, {-0.035F, 0.40F, 0.0F}, 0.005F, Linen),
        generated_cylinder({0.010F, 0.0F, 0.0F}, {0.035F, 0.40F, 0.0F}, 0.005F, Linen),
        generated_ellipsoid({0.0F, 0.42F, 0.0F}, {0.047F, 0.075F, 0.018F}, Leather),
        generated_ellipsoid({0.0F, 0.42F, 0.014F}, {0.024F, 0.033F, 0.018F}, Ivory),
    };
    return build_generated_equipment_archetype("slinger/sling", p);
  }();
  static const auto javelin = [] {
    std::array p{
        generated_cylinder({0.0F, -0.55F, 0.0F}, {0.0F, 0.57F, 0.0F}, 0.012F, Wood),
        generated_cylinder({0.0F, -0.07F, 0.0F}, {0.0F, 0.07F, 0.0F}, 0.018F, Leather),
        generated_cylinder(
            {0.0F, 0.54F, 0.0F}, {0.0F, 0.72F, 0.0F}, 0.010F, Metal, 1.0F, 3),
        generated_cone(
            {0.0F, 0.70F, 0.0F}, {0.0F, 0.88F, 0.0F}, 0.029F, Metal, 1.0F, 3),
    };
    return build_generated_equipment_archetype("velites/javelin", p);
  }();
  return velites ? javelin : sling;
}

auto field_gear(bool velites) -> const RenderArchetype& {
  auto build = [](bool roman) {
    auto const& bind = Render::Humanoid::humanoid_bind_body_frames();
    QVector3D const chest = bind.torso.origin;
    QVector3D const waist = bind.waist.origin;
    std::vector<GeneratedEquipmentPrimitive> p{
        generated_cylinder(chest + QVector3D(-0.15F, 0.08F, 0.13F),
                           waist + QVector3D(0.16F, 0.01F, 0.13F),
                           0.018F,
                           Leather),
        generated_ellipsoid(
            waist + QVector3D(0.20F, -0.06F, 0.02F), {0.085F, 0.105F, 0.055F}, Leather),
        generated_ellipsoid(
            waist + QVector3D(0.20F, -0.005F, 0.052F), {0.081F, 0.044F, 0.018F}, Dark),
        generated_sphere(
            waist + QVector3D(0.20F, -0.024F, 0.075F), 0.014F, Metal, 1.0F, 3),
    };
    if (roman) {
      for (float side : {-1.0F, 1.0F}) {
        p.push_back(generated_ellipsoid(chest + QVector3D(side * 0.16F, 0.09F, -0.035F),
                                        {0.13F, 0.07F, 0.16F},
                                        Fur));
        for (int i = 0; i < 4; ++i) {
          float const x = side * (0.04F + 0.052F * static_cast<float>(i));
          p.push_back(generated_cone(chest + QVector3D(x, 0.05F, -0.13F),
                                     chest + QVector3D(x * 1.2F, -0.20F, -0.16F),
                                     0.045F,
                                     Fur));
        }
      }
      // Spare shafts project above the left shoulder and stay clear of the shield.
      for (int i = 0; i < 3; ++i) {
        float const x = -0.08F + 0.035F * static_cast<float>(i);
        QVector3D const a = waist + QVector3D(x + 0.12F, -0.15F, -0.16F);
        QVector3D const b = chest + QVector3D(x - 0.12F, 0.40F, -0.17F);
        p.push_back(generated_cylinder(a, b, 0.011F, Wood));
        p.push_back(generated_cone(
            b, b + (b - a).normalized() * 0.15F, 0.025F, Metal, 1.0F, 3));
      }
    } else {
      // A spare sling loops visibly from the belt beside the stone pouch.
      QVector3D const c = waist + QVector3D(-0.16F, -0.11F, 0.10F);
      for (int i = 0; i < 12; ++i) {
        float const a = static_cast<float>(i) * std::numbers::pi_v<float> / 6.0F;
        float const b = static_cast<float>(i + 1) * std::numbers::pi_v<float> / 6.0F;
        p.push_back(generated_cylinder(
            c + QVector3D(0.045F * std::sin(a), 0.10F * std::cos(a), 0.0F),
            c + QVector3D(0.045F * std::sin(b), 0.10F * std::cos(b), 0.0F),
            0.006F,
            Linen));
      }
    }
    return build_generated_equipment_archetype(
        roman ? "velites/field_gear" : "slinger/field_gear", p);
  };
  static const auto roman = build(true);
  static const auto balearic = build(false);
  return velites ? roman : balearic;
}

auto gear_attachment(const RenderArchetype& archetype,
                     HumanoidBone bone,
                     const QMatrix4x4& pose = {})
    -> Render::Creature::StaticAttachmentSpec {
  auto spec = Render::Equipment::build_static_attachment(
      {.archetype = &archetype,
       .socket_bone_index = static_cast<std::uint16_t>(bone),
       .unit_local_pose_at_bind = pose});
  for (std::uint8_t i = 0; i < 8U; ++i) {
    spec.palette_role_remap[i] = k_gear_roles + i;
  }
  return spec;
}

auto fill_gear_colors(const void* raw,
                      QVector3D* out,
                      std::uint32_t count,
                      std::size_t max) -> std::uint32_t {
  if (raw == nullptr || max - count < 18U) {
    return count;
  }
  auto const& p = static_cast<const HumanoidVariant*>(raw)->palette;
  std::array<QVector3D, 8> const colors{{{0.83F, 0.77F, 0.60F},
                                         p.leather,
                                         {0.43F, 0.42F, 0.37F},
                                         {0.14F, 0.13F, 0.11F},
                                         {0.88F, 0.84F, 0.68F},
                                         {0.72F, 0.73F, 0.70F},
                                         {0.60F, 0.12F, 0.09F},
                                         {0.48F, 0.30F, 0.15F}}};
  for (auto const& color : colors) {
    out[count++] = color;
  }
  count += shield_fill_role_colors(p, parma_config(), out + count, max - count);
  count += iberian_tunic_fill_role_colors(p, out + count, max - count);
  return count;
}

auto make_spec(bool velites) -> Render::Creature::Pipeline::UnitVisualSpec {
  using namespace Render::Creature;
  using namespace Render::Creature::Pipeline;
  auto const& bind = Render::Humanoid::humanoid_bind_body_frames();
  QMatrix4x4 head = attachment_frame_transform(bind.head);
  // Head gear is authored against the rendered skull, not the rig head radius.
  head.scale(0.168F);
  std::vector<StaticAttachmentSpec> attachments{
      gear_attachment(
          velites ? wolf_pelt() : slinger_headband(), HumanoidBone::Head, head),
      gear_attachment(hand_weapon(velites),
                      HumanoidBone::HandR,
                      attachment_frame_transform(bind.grip_r)),
      gear_attachment(field_gear(velites), HumanoidBone::Chest),
  };
  attachments.push_back(iberian_tunic_make_static_attachment(k_tunic_roles));
  attachments.push_back(
      iberian_hem_band_make_static_attachment(true, k_tunic_roles + 1U));
  attachments.push_back(
      iberian_hem_band_make_static_attachment(false, k_tunic_roles + 1U));
  if (velites) {
    attachments.push_back(
        shield_make_static_attachment(parma_config(), k_shield_roles));
  }
  UnitVisualSpec spec;
  spec.debug_name = velites ? "troops/roman/velites" : "troops/carthage/slinger";
  spec.kind = CreatureKind::Humanoid;
  spec.creature_asset_id = k_humanoid_asset;
  spec.scaling = Humanoid::k_ranged_infantry_proportion_profile.as_pipeline_scaling();
  spec.archetype_id = ArchetypeRegistry::instance().register_unit_archetype(
      spec.debug_name, spec.kind, attachments, fill_gear_colors);
  spec.animation_manifest.ranged_clip_override =
      velites ? Animation::k_humanoid_javelin_throw_clip
              : Animation::k_humanoid_sling_throw_clip;
  spec.animation_manifest.melee_clip_override = Animation::k_humanoid_archer_melee_clip;
  return spec;
}

class SkirmisherRenderer final : public HumanoidRendererBase {
public:
  explicit SkirmisherRenderer(bool velites)
      : HumanoidRendererBase(make_spec(velites))
      , m_velites(velites) {}
  void get_variant(const DrawContext& ctx,
                   std::uint32_t seed,
                   HumanoidVariant& v) const override {
    HumanoidRendererBase::get_variant(ctx, seed, v);
    float const variation = 0.92F + static_cast<float>(seed % 17U) * 0.008F;
    v.palette.cloth =
        (m_velites ? QVector3D(0.86F, 0.82F, 0.70F) : QVector3D(0.65F, 0.52F, 0.31F)) *
        variation;
    v.palette.leather = {0.36F, 0.23F, 0.12F};
    v.palette.leather_dark = {0.20F, 0.13F, 0.08F};
    v.facial_hair.style = m_velites ? FacialHairStyle::None : FacialHairStyle::Stubble;
  }
  void adjust_variation(const DrawContext&,
                        std::uint32_t,
                        VariationParams& v) const override {
    v.bulk_scale *= m_velites ? 0.96F : 0.90F;
    v.stance_width *= 1.06F;
  }

private:
  bool m_velites;
};
} // namespace

void register_skirmisher_renderers(EntityRendererRegistry& registry) {
  register_humanoid_renderer(
      registry, "troops/carthage/slinger", std::make_shared<SkirmisherRenderer>(false));
  register_humanoid_renderer(
      registry, "troops/roman/velites", std::make_shared<SkirmisherRenderer>(true));
}
} // namespace Render::GL
