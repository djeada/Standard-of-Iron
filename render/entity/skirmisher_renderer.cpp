#include "skirmisher_renderer.h"

#include <QQuaternion>

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
    c.shield_radius = 0.108F;
    c.shield_color = {0.42F, 0.065F, 0.045F};
    c.trim_color = {0.72F, 0.55F, 0.29F};
    c.metal_color = {0.68F, 0.69F, 0.66F};
    c.dome_depth = 0.18F;
    c.has_radial_decoration = true;
    return c;
  }();
  return config;
}

auto wolf_pelt() -> const RenderArchetype& {
  static const auto value = [] {
    std::vector<GeneratedEquipmentPrimitive> p{
        generated_ellipsoid({0.0F, 0.57F, -0.17F}, {1.04F, 0.67F, 1.12F}, Fur),
        generated_ellipsoid({0.0F, 0.96F, -0.24F}, {0.63F, 0.31F, 0.93F}, Dark),
        generated_ellipsoid({0.0F, 0.59F, 0.79F}, {0.58F, 0.30F, 0.70F}, Fur),
        generated_ellipsoid({0.0F, 0.43F, 1.15F}, {0.40F, 0.16F, 0.49F}, Linen),
        generated_ellipsoid({0.0F, 0.48F, 1.56F}, {0.27F, 0.17F, 0.16F}, Dark),
        generated_ellipsoid({0.0F, 0.30F, 1.09F}, {0.39F, 0.055F, 0.44F}, Dark),
        generated_ellipsoid({0.0F, -0.18F, -0.82F}, {1.02F, 0.86F, 0.43F}, Fur),
    };
    for (float side : {-1.0F, 1.0F}) {
      p.push_back(generated_cone(
          {side * 0.71F, 0.95F, -0.30F}, {side * 0.84F, 1.48F, -0.49F}, 0.27F, Fur));
      p.push_back(generated_cone(
          {side * 0.72F, 1.07F, -0.10F}, {side * 0.82F, 1.37F, -0.31F}, 0.13F, Dark));
      p.push_back(generated_ellipsoid(
          {side * 0.54F, 0.64F, 0.93F}, {0.18F, 0.105F, 0.10F}, Dark));
      p.push_back(generated_sphere({side * 0.55F, 0.65F, 1.01F}, 0.042F, Wood));
      p.push_back(generated_ellipsoid(
          {side * 0.59F, 0.79F, 0.83F}, {0.29F, 0.10F, 0.23F}, Fur));
      p.push_back(generated_ellipsoid(
          {side * 0.67F, 0.22F, 0.53F}, {0.32F, 0.28F, 0.44F}, Linen));
      p.push_back(generated_cone(
          {side * 0.30F, 0.30F, 1.26F}, {side * 0.28F, 0.10F, 1.29F}, 0.052F, Ivory));
      for (int i = 0; i < 3; ++i) {
        float const y = 0.17F - static_cast<float>(i) * 0.27F;
        p.push_back(generated_cone({side * 0.85F, y, -0.48F},
                                   {side * 1.10F, y - 0.36F, -0.63F},
                                   0.22F,
                                   i == 1 ? Linen : Fur));
      }
    }
    return build_generated_equipment_archetype("velites/wolf_pelt", p);
  }();
  return value;
}

auto slinger_headband() -> const RenderArchetype& {
  static const auto value = [] {
    std::vector<GeneratedEquipmentPrimitive> p{
        generated_ellipsoid({0.0F, 0.55F, 0.0F}, {0.87F, 0.66F, 0.94F}, Dark),
        generated_ellipsoid({0.0F, -0.03F, -0.63F}, {0.72F, 0.70F, 0.35F}, Dark),
    };

    for (int i = 0; i < 24; ++i) {
      float const a = static_cast<float>(i) * std::numbers::pi_v<float> / 12.0F;
      float const b = static_cast<float>(i + 1) * std::numbers::pi_v<float> / 12.0F;
      for (int row = 0; row < 2; ++row) {
        float const y = 0.51F + 0.12F * static_cast<float>(row);
        p.push_back(generated_cylinder({0.81F * std::sin(a), y, 0.90F * std::cos(a)},
                                       {0.81F * std::sin(b), y, 0.90F * std::cos(b)},
                                       0.067F,
                                       row == 0 ? Crimson : Linen));
      }
    }
    p.push_back(
        generated_ellipsoid({0.0F, 0.51F, -0.92F}, {0.17F, 0.14F, 0.12F}, Crimson));
    p.push_back(generated_cylinder(
        {-0.07F, 0.48F, -0.94F}, {-0.22F, -0.23F, -1.00F}, 0.055F, Crimson));
    p.push_back(generated_cylinder(
        {0.06F, 0.48F, -0.94F}, {0.19F, -0.05F, -1.04F}, 0.047F, Linen));
    return build_generated_equipment_archetype("slinger/headband", p);
  }();
  return value;
}

auto hand_weapon(bool velites) -> const RenderArchetype& {
  static const auto sling = [] {
    std::array p{
        generated_cylinder(
            {-0.010F, 0.0F, 0.0F}, {-0.040F, 0.46F, 0.0F}, 0.007F, Linen),
        generated_cylinder({0.010F, 0.0F, 0.0F}, {0.040F, 0.46F, 0.0F}, 0.007F, Linen),
        generated_ellipsoid({0.0F, 0.48F, 0.0F}, {0.052F, 0.079F, 0.022F}, Leather),
        generated_ellipsoid({0.0F, 0.48F, 0.018F}, {0.029F, 0.038F, 0.022F}, Ivory),
    };
    return build_generated_equipment_archetype("slinger/sling", p);
  }();
  static const auto javelin = [] {
    std::array p{
        generated_cylinder({0.0F, -0.55F, 0.0F}, {0.0F, 0.57F, 0.0F}, 0.012F, Wood),
        generated_cylinder({0.0F, -0.07F, 0.0F}, {0.0F, 0.07F, 0.0F}, 0.018F, Leather),
        generated_cylinder({0.0F, -0.55F, 0.0F}, {0.0F, -0.48F, 0.0F}, 0.014F, Metal),
        generated_cylinder({0.0F, 0.51F, 0.0F}, {0.0F, 0.57F, 0.0F}, 0.017F, Linen),
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
        generated_ellipsoid(waist + QVector3D(0.23F, -0.075F, 0.06F),
                            {0.100F, 0.120F, 0.065F},
                            Leather),
        generated_ellipsoid(
            waist + QVector3D(0.23F, -0.018F, 0.105F), {0.095F, 0.051F, 0.028F}, Dark),
        generated_box(waist + QVector3D(0.23F, -0.063F, 0.128F),
                      {0.014F, 0.040F, 0.008F},
                      Leather),
        generated_sphere(
            waist + QVector3D(0.23F, -0.073F, 0.140F), 0.012F, Metal, 1.0F, 3),
    };

    for (int i = 0; i < 5; ++i) {
      float const x = 0.17F + static_cast<float>(i) * 0.029F;
      p.push_back(generated_cylinder(waist + QVector3D(x, -0.031F, 0.132F),
                                     waist + QVector3D(x + 0.005F, -0.044F, 0.132F),
                                     0.0025F,
                                     Linen));
    }
    if (roman) {

      p.push_back(generated_ellipsoid(
          chest + QVector3D(0.0F, -0.06F, -0.14F), {0.23F, 0.25F, 0.080F}, Fur));
      p.push_back(generated_ellipsoid(
          chest + QVector3D(0.0F, -0.035F, -0.20F), {0.105F, 0.23F, 0.026F}, Dark));
      for (float side : {-1.0F, 1.0F}) {
        p.push_back(generated_ellipsoid(chest + QVector3D(side * 0.17F, 0.08F, -0.015F),
                                        {0.14F, 0.080F, 0.17F},
                                        Fur));
        p.push_back(generated_ellipsoid(chest + QVector3D(side * 0.16F, -0.025F, 0.13F),
                                        {0.068F, 0.135F, 0.032F},
                                        Linen));
        p.push_back(generated_ellipsoid(chest + QVector3D(side * 0.17F, -0.16F, 0.13F),
                                        {0.060F, 0.045F, 0.036F},
                                        Fur));
        for (int i = 0; i < 5; ++i) {
          float const x = side * (0.035F + 0.041F * static_cast<float>(i));
          float const y = -0.23F + 0.025F * static_cast<float>(i);
          p.push_back(generated_cone(chest + QVector3D(x, y + 0.06F, -0.17F),
                                     chest + QVector3D(x * 1.13F, y - 0.055F, -0.19F),
                                     0.033F,
                                     i % 2 == 0 ? Fur : Linen));
        }
      }
      p.push_back(generated_cylinder(chest + QVector3D(-0.13F, 0.01F, 0.15F),
                                     chest + QVector3D(0.13F, 0.01F, 0.15F),
                                     0.010F,
                                     Leather));
      p.push_back(generated_sphere(
          chest + QVector3D(0.0F, 0.01F, 0.16F), 0.020F, Metal, 1.0F, 3));

      for (int i = 0; i < 3; ++i) {
        float const x = -0.08F + 0.040F * static_cast<float>(i);
        QVector3D const a = waist + QVector3D(x + 0.12F, -0.15F, -0.25F);
        QVector3D const b =
            chest +
            QVector3D(x - 0.12F, 0.40F + 0.035F * static_cast<float>(i), -0.26F);
        p.push_back(generated_cylinder(a, b, 0.011F, Wood));
        p.push_back(generated_cone(
            b, b + (b - a).normalized() * 0.15F, 0.025F, Metal, 1.0F, 3));
      }
      for (float y : {-0.08F, 0.10F}) {
        p.push_back(generated_cylinder(chest + QVector3D(-0.16F, y, -0.275F),
                                       chest + QVector3D(-0.035F, y, -0.275F),
                                       0.015F,
                                       Leather));
      }
    } else {
      for (int i = 0; i < 3; ++i) {
        p.push_back(generated_ellipsoid(
            waist + QVector3D(0.185F + 0.039F * static_cast<float>(i), 0.018F, 0.072F),
            {0.024F, 0.020F, 0.022F},
            Ivory));
      }

      QVector3D const c = waist + QVector3D(-0.16F, -0.11F, 0.10F);
      for (int i = 0; i < 12; ++i) {
        float const a = static_cast<float>(i) * std::numbers::pi_v<float> / 6.0F;
        float const b = static_cast<float>(i + 1) * std::numbers::pi_v<float> / 6.0F;
        p.push_back(generated_cylinder(
            c + QVector3D(0.045F * std::sin(a), 0.10F * std::cos(a), 0.0F),
            c + QVector3D(0.045F * std::sin(b), 0.10F * std::cos(b), 0.0F),
            0.008F,
            Linen));
      }
      p.push_back(generated_ellipsoid(
          waist + QVector3D(-0.16F, -0.21F, 0.10F), {0.032F, 0.046F, 0.018F}, Leather));
    }
    RenderArchetypeBuilder builder{roman ? "velites/field_gear" : "slinger/field_gear"};
    for (auto const& primitive : p) {
      add_generated_equipment_primitive(builder, primitive);
    }

    auto strap = [&](const QVector3D& a, const QVector3D& b, float half_width) {
      QMatrix4x4 model;
      model.translate((a + b) * 0.5F);
      model.rotate(
          QQuaternion::rotationTo(QVector3D(0.0F, 1.0F, 0.0F), (b - a).normalized()));
      model.scale(half_width, (b - a).length() * 0.5F, 0.006F);
      builder.add_palette_mesh(get_unit_cube(), model, Leather, nullptr, 1.0F, 1);
    };
    strap(chest + QVector3D(-0.17F, 0.08F, 0.155F),
          waist + QVector3D(0.19F, -0.015F, 0.155F),
          0.024F);
    strap(chest + QVector3D(-0.17F, 0.08F, -0.17F),
          waist + QVector3D(0.19F, -0.015F, -0.17F),
          0.024F);
    strap(chest + QVector3D(-0.17F, 0.08F, -0.17F),
          chest + QVector3D(-0.17F, 0.08F, 0.155F),
          0.024F);
    return std::move(builder).build();
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

template <bool Velites>
auto fill_gear_colors(const void* raw,
                      QVector3D* out,
                      std::uint32_t count,
                      std::size_t max) -> std::uint32_t {
  if (raw == nullptr || count > max || max - count < 18U) {
    return count;
  }
  auto const& p = static_cast<const HumanoidVariant*>(raw)->palette;
  std::array<QVector3D, 8> const colors{
      {{0.66F, 0.62F, 0.51F},
       p.leather,
       {0.34F, 0.36F, 0.33F},
       {0.12F, 0.105F, 0.085F},
       {0.88F, 0.84F, 0.68F},
       {0.72F, 0.73F, 0.70F},
       Velites ? QVector3D(0.60F, 0.12F, 0.09F) : QVector3D(0.46F, 0.23F, 0.12F),
       {0.48F, 0.30F, 0.15F}}};
  for (auto const& color : colors) {
    out[count++] = color;
  }
  count += shield_fill_role_colors(p, parma_config(), out + count, max - count);
  auto const tunic_start = count;
  count += iberian_tunic_fill_role_colors(p, out + count, max - count);
  out[tunic_start + 1U] = colors[Crimson];
  return count;
}

auto make_spec(bool velites) -> Render::Creature::Pipeline::UnitVisualSpec {
  using namespace Render::Creature;
  using namespace Render::Creature::Pipeline;
  auto const& bind = Render::Humanoid::humanoid_bind_body_frames();
  QMatrix4x4 head = attachment_frame_transform(bind.head);

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
      spec.debug_name,
      spec.kind,
      attachments,
      velites ? fill_gear_colors<true> : fill_gear_colors<false>);
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
        (m_velites ? QVector3D(0.83F, 0.78F, 0.65F) : QVector3D(0.72F, 0.60F, 0.40F)) *
        variation;
    v.palette.leather = {0.30F, 0.18F, 0.09F};
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
