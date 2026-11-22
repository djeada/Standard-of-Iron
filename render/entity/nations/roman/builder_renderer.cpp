#include "render/entity/nations/roman/builder_renderer.h"

#include <QMatrix4x4>
#include <QString>
#include <QVector3D>
#include <qmatrix4x4.h>
#include <qstringliteral.h>
#include <qvectornd.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "animation/rig/humanoid_proportions.h"
#include "game/core/component_core.h"
#include "game/core/entity.h"
#include "game/systems/nation_id.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/humanoid/asset/bind_skeleton.h"
#include "render/creature/humanoid/asset/humanoid_spec.h"
#include "render/creature/humanoid/runtime/humanoid_math.h"
#include "render/creature/humanoid/runtime/humanoid_renderer.h"
#include "render/creature/humanoid/runtime/skeleton_evaluator.h"
#include "render/creature/humanoid/runtime/style_palette.h"
#include "render/creature/humanoid/schema/humanoid_proportion_profiles.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/entity/civilian_actor.h"
#include "render/entity/nations/builder_tool_palette.h"
#include "render/entity/nations/equipment_loadout_catalog.h"
#include "render/entity/nations/roman/builder_parts.h"
#include "render/entity/nations/roman/builder_style.h"
#include "render/entity/registry.h"
#include "render/equipment/armor/arm_guards_renderer.h"
#include "render/equipment/armor/tool_belt_renderer.h"
#include "render/equipment/armor/torso_local_archetype_utils.h"
#include "render/equipment/armor/work_apron_renderer.h"
#include "render/equipment/attachment_builder.h"
#include "render/equipment/generated_equipment.h"
#include "render/equipment/helmets/roman_light_helmet.h"
#include "render/equipment/humanoid_equipment_archetype.h"
#include "render/equipment/render_archetype_registry.h"
#include "render/equipment/renderer_constants.h"
#include "render/frame/frame_submitter.h"
#include "render/frame/palette.h"
#include "render/frame/submitter.h"
#include "render/geom/garment_shell.h"
#include "render/geom/primitives.h"
#include "render/geom/transforms.h"
#include "render/math/math_utils.h"
#include "render/math/render_constants.h"

using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;

namespace Render::GL::Roman {

using namespace builder_parts_detail;

namespace {

constexpr std::string_view k_default_style_key = "default";

auto style_registry() -> std::unordered_map<std::string, BuilderStyleConfig>& {
  static std::unordered_map<std::string, BuilderStyleConfig> styles;
  return styles;
}

void ensure_builder_styles_registered() {
  static const bool registered = []() {
    register_roman_builder_style();
    return true;
  }();
  (void)registered;
}

auto roman_builder_base_archetype() -> Render::Creature::ArchetypeId {
  static const auto archetype = []() {
    auto& registry = Render::Creature::ArchetypeRegistry::instance();
    const auto* base_desc =
        registry.get(Render::Creature::ArchetypeRegistry::k_humanoid_base);
    if (base_desc == nullptr) {
      return Render::Creature::k_invalid_archetype;
    }

    auto desc = *base_desc;
    desc.debug_name = "troops/roman/builder/base";
    desc.bake_attachments[desc.bake_attachment_count++] =
        builder_work_tunic_make_static_attachment(
            static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest),
            desc.role_count);
    desc.role_count =
        static_cast<std::uint8_t>(desc.role_count + k_builder_work_tunic_role_count);
    desc.append_extra_role_colors_fn(&builder_work_tunic_extra_role_colors);
    return registry.register_archetype(desc);
  }();
  return archetype;
}

auto roman_builder_idle_archetype() -> Render::Creature::ArchetypeId {
  static const auto archetype = []() {
    const auto loadout =
        Render::GL::Nation::resolve_equipment_loadout("troops/roman/builder");
    const std::array<EquipmentHandle, 4> handles{loadout.helmet_handle,
                                                 loadout.tool_belt_handle,
                                                 loadout.work_apron_handle,
                                                 loadout.arm_guards_handle};
    return resolve_humanoid_equipment_archetype(
        "troops/roman/builder", roman_builder_base_archetype(), handles);
  }();
  return archetype;
}

void ensure_roman_civilian_equipment_contributions_registered() {
  static const bool registered = []() {
    const auto loadout =
        Render::GL::Nation::resolve_equipment_loadout("troops/roman/civilian");
    if (loadout.armor_handle != k_invalid_equipment_handle) {
      register_humanoid_equipment_contribution(
          loadout.armor_handle,
          {.build_attachments = &builder_work_tunic_contribution_attachments,
           .append_role_colors = &builder_work_tunic_extra_role_colors,
           .role_count = static_cast<std::uint8_t>(k_builder_work_tunic_role_count)});
    }
    if (loadout.cloak_handle != k_invalid_equipment_handle) {
      register_humanoid_equipment_contribution(
          loadout.cloak_handle,
          {.build_attachments = &roman_civilian_mantle_contribution_attachments,
           .append_role_colors = &roman_civilian_mantle_extra_role_colors,
           .role_count =
               static_cast<std::uint8_t>(k_roman_civilian_mantle_role_count)});
    }
    if (loadout.work_apron_handle != k_invalid_equipment_handle) {
      register_humanoid_equipment_contribution(
          loadout.work_apron_handle,
          {.build_attachments = &roman_civilian_pack_contribution_attachments,
           .append_role_colors = &civilian_pack_extra_role_colors,
           .role_count = static_cast<std::uint8_t>(k_civilian_pack_role_count)});
    }
    return true;
  }();
  (void)registered;
}

auto roman_builder_hammer_unit_archetype() -> Render::Creature::ArchetypeId {
  static constexpr std::array<std::uint8_t, 1> k_slots{
      k_builder_hammer_metal_dark_slot};
  static const auto k_tool_spec = [] {
    auto spec = builder_tool_make_static_attachment(
        builder_hammer_archetype(),
        Render::Creature::ArchetypeRegistry::instance()
            .get(roman_builder_idle_archetype())
            ->role_count,
        k_slots);
    spec.palette_role_remap[k_builder_hammer_wood_slot] =
        Render::Humanoid::k_humanoid_wood_role;
    spec.palette_role_remap[k_builder_hammer_metal_slot] =
        Render::Humanoid::k_humanoid_metal_role;
    return spec;
  }();
  static const auto k_archetype = register_builder_tool_variant_archetype(
      "troops/roman/builder/construction_hammer",
      roman_builder_idle_archetype(),
      k_tool_spec,
      +[](const void* variant_void,
          QVector3D* out,
          std::uint32_t base_count,
          std::size_t max_count) -> std::uint32_t {
        if (variant_void == nullptr || max_count <= base_count) {
          return base_count;
        }
        const auto& v = *static_cast<const HumanoidVariant*>(variant_void);
        return base_count + builder_hammer_fill_role_colors(
                                v.palette, out + base_count, max_count - base_count);
      },
      static_cast<std::uint8_t>(k_builder_hammer_role_count));
  return k_archetype;
}

auto roman_builder_saw_unit_archetype() -> Render::Creature::ArchetypeId {
  static constexpr std::array<std::uint8_t, 1> k_slots{k_saw_metal_dark_slot};
  static const auto k_tool_spec = [] {
    auto spec = builder_tool_make_static_attachment(
        builder_saw_archetype(),
        Render::Creature::ArchetypeRegistry::instance()
            .get(roman_builder_idle_archetype())
            ->role_count,
        k_slots);
    spec.palette_role_remap[k_saw_wood_slot] = Render::Humanoid::k_humanoid_wood_role;
    spec.palette_role_remap[k_saw_metal_slot] = Render::Humanoid::k_humanoid_metal_role;
    spec.palette_role_remap[k_saw_leather_slot] =
        Render::Humanoid::k_humanoid_leather_dark_role;
    return spec;
  }();
  static const auto k_archetype = register_builder_tool_variant_archetype(
      "troops/roman/builder/construction_saw",
      roman_builder_idle_archetype(),
      k_tool_spec,
      +[](const void* variant_void,
          QVector3D* out,
          std::uint32_t base_count,
          std::size_t max_count) -> std::uint32_t {
        if (variant_void == nullptr || max_count <= base_count) {
          return base_count;
        }
        const auto& v = *static_cast<const HumanoidVariant*>(variant_void);
        return base_count + builder_saw_fill_role_colors(
                                v.palette, out + base_count, max_count - base_count);
      },
      static_cast<std::uint8_t>(k_builder_saw_role_count));
  return k_archetype;
}

auto roman_builder_chisel_unit_archetype() -> Render::Creature::ArchetypeId {
  static constexpr std::array<std::uint8_t, 1> k_slots{k_chisel_metal_slot};
  static const auto k_tool_spec = [] {
    auto spec = builder_tool_make_static_attachment(
        builder_chisel_archetype(),
        Render::Creature::ArchetypeRegistry::instance()
            .get(roman_builder_idle_archetype())
            ->role_count,
        k_slots);
    spec.palette_role_remap[k_chisel_wood_slot] =
        Render::Humanoid::k_humanoid_wood_role;
    return spec;
  }();
  static const auto k_archetype = register_builder_tool_variant_archetype(
      "troops/roman/builder/construction_chisel",
      roman_builder_idle_archetype(),
      k_tool_spec,
      +[](const void* variant_void,
          QVector3D* out,
          std::uint32_t base_count,
          std::size_t max_count) -> std::uint32_t {
        if (variant_void == nullptr || max_count <= base_count) {
          return base_count;
        }
        const auto& v = *static_cast<const HumanoidVariant*>(variant_void);
        return base_count + builder_chisel_fill_role_colors(
                                v.palette, out + base_count, max_count - base_count);
      },
      static_cast<std::uint8_t>(k_builder_chisel_role_count));
  return k_archetype;
}

auto roman_builder_sickle_unit_archetype() -> Render::Creature::ArchetypeId {
  static constexpr std::array<std::uint8_t, 1> k_slots{k_builder_sickle_metal_slot};
  static const auto k_tool_spec = [] {
    auto spec = builder_tool_make_static_attachment(
        builder_sickle_archetype(),
        Render::Creature::ArchetypeRegistry::instance()
            .get(roman_builder_idle_archetype())
            ->role_count,
        k_slots);
    spec.palette_role_remap[k_builder_sickle_wood_slot] =
        Render::Humanoid::k_humanoid_wood_role;
    return spec;
  }();
  static const auto k_archetype = register_builder_tool_variant_archetype(
      "troops/roman/builder/construction_sickle",
      roman_builder_idle_archetype(),
      k_tool_spec,
      +[](const void* variant_void,
          QVector3D* out,
          std::uint32_t base_count,
          std::size_t max_count) -> std::uint32_t {
        if (variant_void == nullptr || max_count <= base_count) {
          return base_count;
        }
        const auto& v = *static_cast<const HumanoidVariant*>(variant_void);
        return base_count + builder_sickle_fill_role_colors(
                                v.palette, out + base_count, max_count - base_count);
      },
      static_cast<std::uint8_t>(k_builder_sickle_role_count));
  return k_archetype;
}

auto roman_builder_variant_table() -> const Render::Creature::ArchetypeVariantTable& {
  static const Render::Creature::ArchetypeVariantTable k_table = []() {
    Render::Creature::ArchetypeVariantTable t{};
    t.variant_trigger_pose = Render::Creature::PoseIntent::Construct;
    t.variant_stride = 5;

    t.state_for_pose[static_cast<std::size_t>(
        Render::Creature::PoseIntent::AttackMelee)] =
        Render::Creature::AnimationStateId::AttackMelee;
    t.variant_is_seed_based = true;
    t.seed_variant_limit = 4;

    t.archetype_for_variant[0] = roman_builder_hammer_unit_archetype();
    t.state_for_variant[0] = Render::Creature::AnimationStateId::AttackSword;

    t.archetype_for_variant[1] = roman_builder_saw_unit_archetype();
    t.state_for_variant[1] = Render::Creature::AnimationStateId::AttackSword;

    t.archetype_for_variant[2] = roman_builder_chisel_unit_archetype();
    t.state_for_variant[2] = Render::Creature::AnimationStateId::AttackSword;

    t.archetype_for_variant[3] = roman_builder_chisel_unit_archetype();
    t.state_for_variant[3] = Render::Creature::AnimationStateId::AttackSword;

    t.archetype_for_variant[4] = roman_builder_sickle_unit_archetype();
    t.state_for_variant[4] = Render::Creature::AnimationStateId::AttackSword;
    return t;
  }();
  return k_table;
}

auto roman_civilian_idle_archetype() -> Render::Creature::ArchetypeId {
  static const auto archetype = []() {
    ensure_roman_civilian_equipment_contributions_registered();
    const auto loadout =
        Render::GL::Nation::resolve_equipment_loadout("troops/roman/civilian");
    const std::array<EquipmentHandle, 3> handles{
        loadout.armor_handle, loadout.cloak_handle, loadout.work_apron_handle};
    return resolve_humanoid_equipment_archetype(
        "troops/roman/civilian",
        Render::Creature::ArchetypeRegistry::k_humanoid_base,
        handles);
  }();
  return archetype;
}

auto roman_civilian_cudgel_unit_archetype() -> Render::Creature::ArchetypeId {
  static constexpr std::array<std::uint8_t, 2> k_slots{k_civilian_cudgel_wood_slot,
                                                       k_civilian_cudgel_band_slot};
  static const auto k_tool_spec = builder_tool_make_static_attachment(
      civilian_cudgel_archetype(),
      Render::Creature::ArchetypeRegistry::instance()
          .get(roman_civilian_idle_archetype())
          ->role_count,
      k_slots);
  static const auto k_archetype = register_builder_tool_variant_archetype(
      "troops/roman/civilian/cudgel",
      roman_civilian_idle_archetype(),
      k_tool_spec,
      +[](const void* variant_void,
          QVector3D* out,
          std::uint32_t base_count,
          std::size_t max_count) -> std::uint32_t {
        if (variant_void == nullptr || max_count <= base_count) {
          return base_count;
        }
        const auto& v = *static_cast<const HumanoidVariant*>(variant_void);
        return base_count + civilian_cudgel_fill_role_colors(
                                v.palette, out + base_count, max_count - base_count);
      },
      static_cast<std::uint8_t>(k_civilian_cudgel_role_count));
  return k_archetype;
}

auto roman_civilian_variant_table() -> const Render::Creature::ArchetypeVariantTable& {
  static const Render::Creature::ArchetypeVariantTable k_table = []() {
    Render::Creature::ArchetypeVariantTable t{};
    t.archetype_for_pose[static_cast<std::size_t>(
        Render::Creature::PoseIntent::AttackMelee)] =
        roman_civilian_cudgel_unit_archetype();
    t.state_for_pose[static_cast<std::size_t>(
        Render::Creature::PoseIntent::AttackMelee)] =
        Render::Creature::AnimationStateId::AttackSword;
    return t;
  }();
  return k_table;
}

} // namespace

void ensure_civilian_equipment_registered() {
  ensure_roman_civilian_equipment_contributions_registered();
}

void register_builder_style(const std::string& nation_id,
                            const BuilderStyleConfig& style) {
  style_registry()[nation_id] = style;
}

using Render::GL::Humanoid::mix_palette_color;

auto resolve_builder_style(const DrawContext& ctx) -> const BuilderStyleConfig& {
  ensure_builder_styles_registered();
  auto& styles = style_registry();
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
  auto fallback = styles.find(std::string(k_default_style_key));
  if (fallback != styles.end()) {
    return fallback->second;
  }
  static const BuilderStyleConfig default_style{};
  return default_style;
}

void apply_builder_palette_overrides(const BuilderStyleConfig& style,
                                     const QVector3D& team_tint,
                                     HumanoidVariant& variant) {
  Humanoid::apply_uniform_palette(variant.palette, style, team_tint);
}

void apply_roman_civilian_palette(const QVector3D& team_tint,
                                  std::uint32_t seed,
                                  HumanoidVariant& variant) {
  QVector3D const wood_color(0.52F, 0.42F, 0.28F);
  QVector3D const metal_color(0.72F, 0.55F, 0.35F);
  float const cloth_roll = hash_01(seed ^ 0x2A15U);
  float const sash_roll = hash_01(seed ^ 0x6D31U);

  QVector3D tunic_color = cloth_roll < 0.33F   ? QVector3D(0.80F, 0.72F, 0.60F)
                          : cloth_roll < 0.66F ? QVector3D(0.70F, 0.62F, 0.52F)
                                               : QVector3D(0.66F, 0.58F, 0.48F);
  QVector3D sash_color = sash_roll < 0.5F ? QVector3D(0.58F, 0.24F, 0.18F)
                                          : QVector3D(0.45F, 0.29F, 0.16F);

  variant.palette.cloth =
      mix_palette_color(variant.palette.cloth, tunic_color, team_tint, 0.10F, 0.90F);
  variant.palette.leather =
      mix_palette_color(variant.palette.leather, sash_color, team_tint, 0.08F, 0.92F);
  variant.palette.leather_dark = variant.palette.leather * 0.74F;
  variant.palette.wood =
      mix_palette_color(variant.palette.wood, wood_color, team_tint, 0.10F, 0.90F);
  variant.palette.metal =
      mix_palette_color(variant.palette.metal, metal_color, team_tint, 0.08F, 0.92F);
}

class BuilderRenderer : public HumanoidRendererBase {
public:
  BuilderRenderer()
      : HumanoidRendererBase(make_visual_spec()) {}

  friend void register_builder_renderer(Render::GL::EntityRendererRegistry& registry);

  static auto make_visual_spec() -> Render::Creature::Pipeline::UnitVisualSpec {
    using namespace Render::Creature::Pipeline;
    UnitVisualSpec s{};
    s.kind = CreatureKind::Humanoid;
    s.debug_name = "troops/roman/builder";
    s.scaling = k_builder_profile.as_pipeline_scaling();
    s.archetype_id = roman_builder_idle_archetype();
    s.animation_manifest.variant_table = &roman_builder_variant_table();
    return s;
  }

  static void
  fill_crew_variant(const DrawContext& ctx, std::uint32_t seed, HumanoidVariant& v) {
    QVector3D const team_tint = resolve_team_tint(ctx);
    v.palette = make_humanoid_palette(team_tint, seed);
    auto const& style = resolve_builder_style(ctx);
    apply_builder_palette_overrides(style, team_tint, v);
    seed_missing_humanoid_wear(v, seed);
  }

  void get_variant(const DrawContext& ctx,
                   uint32_t seed,
                   HumanoidVariant& v) const override {
    QVector3D const team_tint = resolve_team_tint(ctx);
    v.palette = make_humanoid_palette(team_tint, seed);
    apply_builder_palette_overrides(resolve_builder_style(ctx), team_tint, v);
  }
};

class CivilianRenderer : public HumanoidRendererBase {
public:
  CivilianRenderer()
      : HumanoidRendererBase(make_visual_spec()) {}

  static auto make_visual_spec() -> Render::Creature::Pipeline::UnitVisualSpec {
    using namespace Render::Creature::Pipeline;
    UnitVisualSpec s{};
    s.kind = CreatureKind::Humanoid;
    s.debug_name = "troops/roman/civilian";
    s.scaling = k_civilian_profile.as_pipeline_scaling();
    s.archetype_id = roman_civilian_idle_archetype();
    s.animation_manifest.variant_table = &roman_civilian_variant_table();
    return s;
  }

  void get_variant(const DrawContext& ctx,
                   std::uint32_t seed,
                   HumanoidVariant& v) const override {
    QVector3D const team_tint = resolve_team_tint(ctx);
    v.palette = make_humanoid_palette(team_tint, seed);
    apply_roman_civilian_palette(team_tint, seed, v);
  }

  static void
  fill_ambient_variant(const DrawContext& ctx, std::uint32_t seed, HumanoidVariant& v) {
    QVector3D const team_tint = resolve_team_tint(ctx);
    v.palette = make_humanoid_palette(team_tint, seed);
    apply_roman_civilian_palette(team_tint, seed, v);
    seed_missing_humanoid_wear(v, seed);
  }
};

void register_crew_rig_for_nation() {
  NationCivilianRig rig{};
  rig.spec = BuilderRenderer::make_visual_spec();
  rig.idle = roman_builder_idle_archetype();
  rig.working = roman_builder_hammer_unit_archetype();
  rig.fill_variant = &BuilderRenderer::fill_crew_variant;
  register_nation_crew_rig(false, rig);
}

void register_civilian_rig_for_nation() {

  NationCivilianRig rig{};
  rig.spec = CivilianRenderer::make_visual_spec();
  rig.idle = roman_civilian_idle_archetype();
  rig.working = roman_builder_sickle_unit_archetype();
  rig.armed = roman_civilian_cudgel_unit_archetype();
  rig.fill_variant = &CivilianRenderer::fill_ambient_variant;
  register_nation_civilian_rig(false, rig);
}

void register_builder_renderer(Render::GL::EntityRendererRegistry& registry) {
  ensure_builder_styles_registered();
  register_crew_rig_for_nation();
  register_humanoid_renderer(
      registry, "troops/roman/builder", std::make_shared<BuilderRenderer const>());

  auto& ar = Render::GL::RenderArchetypeRegistry::instance();
  ar.register_archetype("roman_builder_work_tunic",
                        [] { (void)builder_work_tunic_archetype(); });
  ar.register_archetype("roman_civilian_mantle",
                        [] { (void)roman_civilian_mantle_archetype(); });
  ar.register_archetype("roman_civilian_pack",
                        [] { (void)roman_civilian_pack_archetype(); });
  ar.register_archetype("roman_builder_hammer",
                        [] { (void)builder_hammer_archetype(); });
  ar.register_archetype("roman_builder_saw", [] { (void)builder_saw_archetype(); });
  ar.register_archetype("roman_builder_chisel",
                        [] { (void)builder_chisel_archetype(); });
  ar.register_archetype("roman_builder_sickle",
                        [] { (void)builder_sickle_archetype(); });
}

void register_civilian_renderer(Render::GL::EntityRendererRegistry& registry) {
  ensure_builder_styles_registered();
  register_civilian_rig_for_nation();
  register_humanoid_renderer(
      registry, "troops/roman/civilian", std::make_shared<CivilianRenderer const>());

  auto& ar = Render::GL::RenderArchetypeRegistry::instance();
  ar.register_archetype("roman_civilian_cudgel",
                        [] { (void)civilian_cudgel_archetype(); });
}

} // namespace Render::GL::Roman
