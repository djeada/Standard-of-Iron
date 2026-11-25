#include "render/entity/nations/carthage/builder_renderer.h"

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

#include "animation/clip_manifest.h"
#include "animation/rig/humanoid_proportions.h"
#include "game/core/component_core.h"
#include "game/core/entity.h"
#include "game/systems/nation_id.h"
#include "render/cache/static_attachment_spec.h"
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
#include "render/entity/nations/carthage/builder_parts.h"
#include "render/entity/nations/carthage/builder_style.h"
#include "render/entity/nations/equipment_loadout_catalog.h"
#include "render/entity/nations/worker_tools.h"
#include "render/entity/registry.h"
#include "render/equipment/armor/arm_guards_renderer.h"
#include "render/equipment/armor/tool_belt_renderer.h"
#include "render/equipment/armor/torso_local_archetype_utils.h"
#include "render/equipment/armor/work_apron_renderer.h"
#include "render/equipment/attachment_builder.h"
#include "render/equipment/equipment_registry.h"
#include "render/equipment/equipment_submit.h"
#include "render/equipment/generated_equipment.h"
#include "render/equipment/humanoid_attachment_archetype.h"
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

namespace Render::GL::Carthage {

using namespace builder_parts_detail;

namespace {

constexpr std::string_view k_default_style_key = "default";

auto style_registry() -> std::unordered_map<std::string, BuilderStyleConfig>& {
  static std::unordered_map<std::string, BuilderStyleConfig> styles;
  return styles;
}

void ensure_builder_styles_registered() {
  static const bool registered = []() {
    register_carthage_builder_style();
    return true;
  }();
  (void)registered;
}

constexpr auto k_builder_profile =
    Render::GL::Humanoid::k_laborer_proportion_profile.with_offset(
        {.x = -0.02F, .y = 0.01F, .z = -0.02F});
constexpr auto k_civilian_profile =
    Render::GL::Humanoid::k_civilian_proportion_profile.with_offset(
        {.x = -0.01F, .y = 0.01F, .z = -0.02F});

} // namespace

namespace {

auto register_builder_tool_variant_archetype(
    std::string_view debug_name,
    Render::Creature::ArchetypeId base_archetype_id,
    const Render::Creature::StaticAttachmentSpec& tool_spec,
    Render::Creature::ArchetypeDescriptor::ExtraRoleColorsFn tool_role_colors,
    std::uint8_t tool_role_count) -> Render::Creature::ArchetypeId {
  auto& registry = Render::Creature::ArchetypeRegistry::instance();
  const auto* base_desc = registry.get(base_archetype_id);
  if (base_desc == nullptr) {
    return Render::Creature::k_invalid_archetype;
  }

  auto desc = *base_desc;
  desc.debug_name = debug_name;
  desc.bake_attachments[desc.bake_attachment_count++] = tool_spec;
  desc.role_count = static_cast<std::uint8_t>(desc.role_count + tool_role_count);
  desc.append_extra_role_colors_fn(tool_role_colors);
  return registry.register_archetype(desc);
}

auto carthage_builder_base_archetype() -> Render::Creature::ArchetypeId {
  static const auto archetype = []() {
    auto& registry = Render::Creature::ArchetypeRegistry::instance();
    const auto* base_desc =
        registry.get(Render::Creature::ArchetypeRegistry::k_humanoid_base);
    if (base_desc == nullptr) {
      return Render::Creature::k_invalid_archetype;
    }

    auto desc = *base_desc;
    desc.debug_name = "troops/carthage/builder/base";
    desc.bake_attachments[desc.bake_attachment_count++] =
        carthage_robes_make_static_attachment(
            static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest),
            desc.role_count);
    desc.role_count =
        static_cast<std::uint8_t>(desc.role_count + k_carthage_robes_role_count);
    desc.append_extra_role_colors_fn(&carthage_robes_extra_role_colors);
    return registry.register_archetype(desc);
  }();
  return archetype;
}

auto carthage_builder_kit_archetype() -> Render::Creature::ArchetypeId {
  static const auto archetype = []() {
    const auto loadout =
        Render::GL::Nation::resolve_equipment_loadout("troops/carthage/builder");
    const std::array<EquipmentHandle, 4> handles{loadout.helmet_handle,
                                                 loadout.tool_belt_handle,
                                                 loadout.work_apron_handle,
                                                 loadout.arm_guards_handle};
    return resolve_humanoid_equipment_archetype(
        "troops/carthage/builder", carthage_builder_base_archetype(), handles);
  }();
  return archetype;
}

auto carthage_builder_idle_archetype() -> Render::Creature::ArchetypeId {
  static const auto archetype = []() {
    return WorkerTools::with_attachment(
        "troops/carthage/builder/slung_mallet",
        carthage_builder_kit_archetype(),
        WorkerTools::back_attachment(WorkerTools::Kind::LongMallet,
                                     Render::Humanoid::k_humanoid_wood_role),
        false);
  }();
  return archetype;
}

auto carthage_builder_hand_tool_archetype(std::string_view debug_name,
                                          WorkerTools::Kind kind)
    -> Render::Creature::ArchetypeId {
  auto const idle = carthage_builder_idle_archetype();
  const auto* idle_desc = Render::Creature::ArchetypeRegistry::instance().get(idle);
  if (idle_desc == nullptr) {
    return idle;
  }
  return WorkerTools::with_attachment(
      debug_name,
      idle,
      WorkerTools::hand_attachment(kind, idle_desc->role_count),
      true);
}

auto carthage_builder_axe_unit_archetype() -> Render::Creature::ArchetypeId {
  static const auto archetype = carthage_builder_hand_tool_archetype(
      "troops/carthage/builder/felling_axe", WorkerTools::Kind::FellingAxe);
  return archetype;
}

auto carthage_builder_pick_unit_archetype() -> Render::Creature::ArchetypeId {
  static const auto archetype = carthage_builder_hand_tool_archetype(
      "troops/carthage/builder/quarry_pick", WorkerTools::Kind::QuarryPick);
  return archetype;
}

auto carthage_civilian_sash_fill_role_colors(const HumanoidPalette& palette,
                                             QVector3D* out,
                                             std::size_t max) -> std::uint32_t {
  if (max < k_carthage_civilian_sash_role_count) {
    return 0U;
  }
  out[0] = palette.leather;
  out[1] = palette.leather_dark;
  return k_carthage_civilian_sash_role_count;
}

auto carthage_civilian_sash_extra_role_colors(const void* variant_void,
                                              QVector3D* out,
                                              std::uint32_t base_count,
                                              std::size_t max_count) -> std::uint32_t {
  if (variant_void == nullptr || max_count <= base_count) {
    return base_count;
  }
  const auto& variant = *static_cast<const HumanoidVariant*>(variant_void);
  return base_count + carthage_civilian_sash_fill_role_colors(
                          variant.palette, out + base_count, max_count - base_count);
}

void ensure_carthage_civilian_equipment_contributions_registered() {
  static const bool registered = []() {
    const auto loadout =
        Render::GL::Nation::resolve_equipment_loadout("troops/carthage/civilian");
    if (loadout.helmet_handle != k_invalid_equipment_handle) {
      register_humanoid_equipment_contribution(
          loadout.helmet_handle,
          {.build_attachments = &carthage_headwrap_contribution_attachments,
           .append_role_colors = &carthage_headwrap_extra_role_colors,
           .role_count = static_cast<std::uint8_t>(k_carthage_headwrap_role_count)});
    }
    if (loadout.armor_handle != k_invalid_equipment_handle) {
      register_humanoid_equipment_contribution(
          loadout.armor_handle,
          {.build_attachments = &carthage_robes_contribution_attachments,
           .append_role_colors = &carthage_robes_extra_role_colors,
           .role_count = static_cast<std::uint8_t>(k_carthage_robes_role_count)});
    }
    if (loadout.cloak_handle != k_invalid_equipment_handle) {
      register_humanoid_equipment_contribution(
          loadout.cloak_handle,
          {.build_attachments = &carthage_civilian_sash_contribution_attachments,
           .append_role_colors = &carthage_civilian_sash_extra_role_colors,
           .role_count =
               static_cast<std::uint8_t>(k_carthage_civilian_sash_role_count)});
    }
    if (loadout.work_apron_handle != k_invalid_equipment_handle) {
      register_humanoid_equipment_contribution(
          loadout.work_apron_handle,
          {.build_attachments = &carthage_civilian_pack_contribution_attachments,
           .append_role_colors = &civilian_pack_extra_role_colors,
           .role_count = static_cast<std::uint8_t>(k_civilian_pack_role_count)});
    }
    return true;
  }();
  (void)registered;
}

auto carthage_builder_hammer_unit_archetype() -> Render::Creature::ArchetypeId {
  static constexpr std::array<std::uint8_t, 2> k_slots{0U, 1U};
  static const auto k_tool_spec = carthage_tool_make_static_attachment(
      carthage_hammer_archetype(),
      Render::Creature::ArchetypeRegistry::instance()
          .get(carthage_builder_idle_archetype())
          ->role_count,
      k_slots);
  static const auto k_archetype = register_builder_tool_variant_archetype(
      "troops/carthage/builder/construction_hammer",
      carthage_builder_idle_archetype(),
      k_tool_spec,
      +[](const void* variant_void,
          QVector3D* out,
          std::uint32_t base_count,
          std::size_t max_count) -> std::uint32_t {
        if (variant_void == nullptr || max_count <= base_count) {
          return base_count;
        }
        const auto& v = *static_cast<const HumanoidVariant*>(variant_void);
        return base_count + carthage_hammer_fill_role_colors(
                                v.palette, out + base_count, max_count - base_count);
      },
      static_cast<std::uint8_t>(k_carthage_hammer_role_count));
  return k_archetype;
}

auto carthage_builder_saw_unit_archetype() -> Render::Creature::ArchetypeId {
  static constexpr std::array<std::uint8_t, 4> k_slots{
      k_saw_wood_slot, k_saw_metal_slot, k_saw_metal_dark_slot, k_saw_leather_slot};
  static const auto k_tool_spec = carthage_tool_make_static_attachment(
      carthage_saw_archetype(),
      Render::Creature::ArchetypeRegistry::instance()
          .get(carthage_builder_idle_archetype())
          ->role_count,
      k_slots);
  static const auto k_archetype = register_builder_tool_variant_archetype(
      "troops/carthage/builder/construction_saw",
      carthage_builder_idle_archetype(),
      k_tool_spec,
      +[](const void* variant_void,
          QVector3D* out,
          std::uint32_t base_count,
          std::size_t max_count) -> std::uint32_t {
        if (variant_void == nullptr || max_count <= base_count) {
          return base_count;
        }
        const auto& v = *static_cast<const HumanoidVariant*>(variant_void);
        return base_count + carthage_saw_fill_role_colors(
                                v.palette, out + base_count, max_count - base_count);
      },
      static_cast<std::uint8_t>(k_carthage_saw_role_count));
  return k_archetype;
}

auto carthage_builder_chisel_unit_archetype() -> Render::Creature::ArchetypeId {
  static constexpr std::array<std::uint8_t, 2> k_slots{k_chisel_wood_slot,
                                                       k_chisel_metal_slot};
  static const auto k_tool_spec = carthage_tool_make_static_attachment(
      carthage_chisel_archetype(),
      Render::Creature::ArchetypeRegistry::instance()
          .get(carthage_builder_idle_archetype())
          ->role_count,
      k_slots);
  static const auto k_archetype = register_builder_tool_variant_archetype(
      "troops/carthage/builder/construction_chisel",
      carthage_builder_idle_archetype(),
      k_tool_spec,
      +[](const void* variant_void,
          QVector3D* out,
          std::uint32_t base_count,
          std::size_t max_count) -> std::uint32_t {
        if (variant_void == nullptr || max_count <= base_count) {
          return base_count;
        }
        const auto& v = *static_cast<const HumanoidVariant*>(variant_void);
        return base_count + carthage_chisel_fill_role_colors(
                                v.palette, out + base_count, max_count - base_count);
      },
      static_cast<std::uint8_t>(k_carthage_chisel_role_count));
  return k_archetype;
}

auto carthage_builder_sickle_unit_archetype() -> Render::Creature::ArchetypeId {
  static constexpr std::array<std::uint8_t, 2> k_slots{k_sickle_wood_slot,
                                                       k_sickle_metal_slot};
  static const auto k_tool_spec = carthage_tool_make_static_attachment(
      carthage_sickle_archetype(),
      Render::Creature::ArchetypeRegistry::instance()
          .get(carthage_builder_idle_archetype())
          ->role_count,
      k_slots);
  static const auto k_archetype = register_builder_tool_variant_archetype(
      "troops/carthage/builder/construction_sickle",
      carthage_builder_idle_archetype(),
      k_tool_spec,
      +[](const void* variant_void,
          QVector3D* out,
          std::uint32_t base_count,
          std::size_t max_count) -> std::uint32_t {
        if (variant_void == nullptr || max_count <= base_count) {
          return base_count;
        }
        const auto& v = *static_cast<const HumanoidVariant*>(variant_void);
        return base_count + carthage_sickle_fill_role_colors(
                                v.palette, out + base_count, max_count - base_count);
      },
      static_cast<std::uint8_t>(k_carthage_sickle_role_count));
  return k_archetype;
}

auto carthage_builder_variant_table()
    -> const Render::Creature::ArchetypeVariantTable& {
  static const Render::Creature::ArchetypeVariantTable k_table = []() {
    Render::Creature::ArchetypeVariantTable t{};
    t.variant_trigger_pose = Render::Creature::PoseIntent::Construct;
    t.variant_stride = 7;

    t.state_for_pose[static_cast<std::size_t>(
        Render::Creature::PoseIntent::AttackMelee)] =
        Render::Creature::AnimationStateId::AttackMelee;
    t.variant_is_seed_based = true;
    t.seed_variant_limit = 4;

    t.archetype_for_variant[0] = carthage_builder_hammer_unit_archetype();
    t.state_for_variant[0] = Render::Creature::AnimationStateId::AttackSword;

    t.archetype_for_variant[1] = carthage_builder_saw_unit_archetype();
    t.state_for_variant[1] = Render::Creature::AnimationStateId::AttackSword;

    t.archetype_for_variant[2] = carthage_builder_chisel_unit_archetype();
    t.state_for_variant[2] = Render::Creature::AnimationStateId::AttackSword;

    t.archetype_for_variant[3] = carthage_builder_chisel_unit_archetype();
    t.state_for_variant[3] = Render::Creature::AnimationStateId::AttackSword;

    t.archetype_for_variant[4] = carthage_builder_sickle_unit_archetype();
    t.state_for_variant[4] = Render::Creature::AnimationStateId::AttackSword;

    t.archetype_for_variant[Animation::k_humanoid_felling_axe_variant] =
        carthage_builder_axe_unit_archetype();
    t.state_for_variant[Animation::k_humanoid_felling_axe_variant] =
        Render::Creature::AnimationStateId::AttackSword;

    t.archetype_for_variant[Animation::k_humanoid_quarry_pick_variant] =
        carthage_builder_pick_unit_archetype();
    t.state_for_variant[Animation::k_humanoid_quarry_pick_variant] =
        Render::Creature::AnimationStateId::AttackSword;
    return t;
  }();
  return k_table;
}

auto carthage_civilian_idle_archetype() -> Render::Creature::ArchetypeId {
  static const auto archetype = []() {
    ensure_carthage_civilian_equipment_contributions_registered();
    const auto loadout =
        Render::GL::Nation::resolve_equipment_loadout("troops/carthage/civilian");
    const std::array<EquipmentHandle, 4> handles{loadout.helmet_handle,
                                                 loadout.armor_handle,
                                                 loadout.cloak_handle,
                                                 loadout.work_apron_handle};
    return resolve_humanoid_equipment_archetype(
        "troops/carthage/civilian",
        Render::Creature::ArchetypeRegistry::k_humanoid_base,
        handles);
  }();
  return archetype;
}

auto carthage_civilian_cudgel_unit_archetype() -> Render::Creature::ArchetypeId {
  static constexpr std::array<std::uint8_t, 2> k_slots{k_civilian_cudgel_wood_slot,
                                                       k_civilian_cudgel_band_slot};
  static const auto k_tool_spec = carthage_tool_make_static_attachment(
      civilian_cudgel_archetype(),
      Render::Creature::ArchetypeRegistry::instance()
          .get(carthage_civilian_idle_archetype())
          ->role_count,
      k_slots);
  static const auto k_archetype = register_builder_tool_variant_archetype(
      "troops/carthage/civilian/cudgel",
      carthage_civilian_idle_archetype(),
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

auto carthage_civilian_variant_table()
    -> const Render::Creature::ArchetypeVariantTable& {
  static const Render::Creature::ArchetypeVariantTable k_table = []() {
    Render::Creature::ArchetypeVariantTable t{};
    t.archetype_for_pose[static_cast<std::size_t>(
        Render::Creature::PoseIntent::AttackMelee)] =
        carthage_civilian_cudgel_unit_archetype();
    t.state_for_pose[static_cast<std::size_t>(
        Render::Creature::PoseIntent::AttackMelee)] =
        Render::Creature::AnimationStateId::AttackSword;
    return t;
  }();
  return k_table;
}

} // namespace

void ensure_civilian_equipment_registered() {
  ensure_carthage_civilian_equipment_contributions_registered();
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

constexpr float k_punic_skin_depth = 0.45F;

void apply_builder_palette_overrides(const BuilderStyleConfig& style,
                                     const QVector3D& team_tint,
                                     std::uint32_t seed,
                                     HumanoidVariant& variant) {
  variant.palette.skin = style.skin_color ? Humanoid::saturate_color(*style.skin_color)
                                          : make_skin_tone(seed, k_punic_skin_depth);
  Humanoid::apply_uniform_palette(variant.palette, style, team_tint);
}

void apply_carthage_beard(std::uint32_t seed,
                          float chance,
                          bool force_beard,
                          HumanoidVariant& variant) {
  (void)seed;
  (void)chance;
  if (!force_beard) {
    return;
  }

  variant.facial_hair.style = FacialHairStyle::FullBeard;
  variant.facial_hair.length = 0.92F;
  variant.facial_hair.color = QVector3D(0.18F, 0.14F, 0.11F);
  variant.facial_hair.thickness = 0.88F;
}

void apply_carthage_civilian_palette(const QVector3D& team_tint,
                                     std::uint32_t seed,
                                     HumanoidVariant& variant) {
  QVector3D const wood_color(0.45F, 0.35F, 0.22F);
  QVector3D const metal_color(0.70F, 0.52F, 0.32F);
  float const robe_roll = hash_01(seed ^ 0xCA77U);
  float const sash_roll = hash_01(seed ^ 0xB10EU);

  QVector3D robe_color = robe_roll < 0.33F   ? QVector3D(0.84F, 0.76F, 0.63F)
                         : robe_roll < 0.66F ? QVector3D(0.74F, 0.67F, 0.56F)
                                             : QVector3D(0.68F, 0.61F, 0.52F);
  QVector3D sash_color = sash_roll < 0.5F ? QVector3D(0.16F, 0.38F, 0.42F)
                                          : QVector3D(0.42F, 0.29F, 0.16F);

  variant.palette.cloth =
      mix_palette_color(variant.palette.cloth, robe_color, team_tint, 0.08F, 0.92F);
  variant.palette.leather =
      mix_palette_color(variant.palette.leather, sash_color, team_tint, 0.06F, 0.94F);
  variant.palette.leather_dark = variant.palette.leather * 0.74F;
  variant.palette.wood =
      mix_palette_color(variant.palette.wood, wood_color, team_tint, 0.08F, 0.92F);
  variant.palette.metal =
      mix_palette_color(variant.palette.metal, metal_color, team_tint, 0.06F, 0.94F);
  variant.palette.skin = make_skin_tone(seed, k_punic_skin_depth);
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
    s.debug_name = "troops/carthage/builder";
    s.scaling = k_builder_profile.as_pipeline_scaling();
    s.archetype_id = carthage_builder_idle_archetype();
    s.animation_manifest.variant_table = &carthage_builder_variant_table();
    return s;
  }

  static void
  fill_crew_variant(const DrawContext& ctx, std::uint32_t seed, HumanoidVariant& v) {
    QVector3D const team_tint = resolve_team_tint(ctx);
    v.palette = make_humanoid_palette(team_tint, seed);
    auto const& style = resolve_builder_style(ctx);
    apply_builder_palette_overrides(style, team_tint, seed, v);
    apply_carthage_beard(seed, 0.75F, style.force_beard, v);
    seed_missing_humanoid_wear(v, seed);
  }

  void get_variant(const DrawContext& ctx,
                   uint32_t seed,
                   HumanoidVariant& v) const override {
    QVector3D const team_tint = resolve_team_tint(ctx);
    v.palette = make_humanoid_palette(team_tint, seed);
    auto const& style = resolve_builder_style(ctx);
    apply_builder_palette_overrides(style, team_tint, seed, v);
    apply_carthage_beard(seed, 0.75F, style.force_beard, v);
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
    s.debug_name = "troops/carthage/civilian";
    s.scaling = k_civilian_profile.as_pipeline_scaling();
    s.archetype_id = carthage_civilian_idle_archetype();
    s.animation_manifest.variant_table = &carthage_civilian_variant_table();
    return s;
  }

  void get_variant(const DrawContext& ctx,
                   std::uint32_t seed,
                   HumanoidVariant& v) const override {
    QVector3D const team_tint = resolve_team_tint(ctx);
    v.palette = make_humanoid_palette(team_tint, seed);
    apply_carthage_civilian_palette(team_tint, seed, v);
    apply_carthage_beard(seed, 0.35F, false, v);
  }

  static void
  fill_ambient_variant(const DrawContext& ctx, std::uint32_t seed, HumanoidVariant& v) {
    QVector3D const team_tint = resolve_team_tint(ctx);
    v.palette = make_humanoid_palette(team_tint, seed);
    apply_carthage_civilian_palette(team_tint, seed, v);
    apply_carthage_beard(seed, 0.35F, false, v);
    seed_missing_humanoid_wear(v, seed);
  }
};

void register_crew_rig_for_nation() {
  NationCivilianRig rig{};
  rig.spec = BuilderRenderer::make_visual_spec();
  rig.idle = carthage_builder_idle_archetype();
  rig.working = carthage_builder_hammer_unit_archetype();
  rig.fill_variant = &BuilderRenderer::fill_crew_variant;
  register_nation_crew_rig(true, rig);
}

void register_civilian_rig_for_nation() {

  NationCivilianRig rig{};
  rig.spec = CivilianRenderer::make_visual_spec();
  rig.idle = carthage_civilian_idle_archetype();
  rig.working = carthage_builder_sickle_unit_archetype();
  rig.armed = carthage_civilian_cudgel_unit_archetype();
  rig.fill_variant = &CivilianRenderer::fill_ambient_variant;
  register_nation_civilian_rig(true, rig);
}

void register_builder_renderer(Render::GL::EntityRendererRegistry& registry) {
  ensure_builder_styles_registered();
  register_crew_rig_for_nation();
  register_humanoid_renderer(
      registry, "troops/carthage/builder", std::make_shared<BuilderRenderer const>());

  auto& ar = Render::GL::RenderArchetypeRegistry::instance();
  ar.register_archetype("carthage_headwrap",
                        [] { (void)carthage_headwrap_archetype(); });
  ar.register_archetype("carthage_robes", [] { (void)carthage_robes_archetype(); });
  ar.register_archetype("carthage_civilian_sash",
                        [] { (void)carthage_civilian_sash_archetype(); });
  ar.register_archetype("carthage_civilian_pack",
                        [] { (void)carthage_civilian_pack_archetype(); });
  ar.register_archetype("carthage_builder_hammer",
                        [] { (void)carthage_hammer_archetype(); });
  ar.register_archetype("carthage_builder_saw", [] { (void)carthage_saw_archetype(); });
  ar.register_archetype("carthage_builder_chisel",
                        [] { (void)carthage_chisel_archetype(); });
  ar.register_archetype("carthage_builder_sickle",
                        [] { (void)carthage_sickle_archetype(); });
  ar.register_archetype("worker_long_mallet", [] {
    (void)WorkerTools::archetype(WorkerTools::Kind::LongMallet);
  });
}

void register_civilian_renderer(Render::GL::EntityRendererRegistry& registry) {
  ensure_builder_styles_registered();
  register_civilian_rig_for_nation();
  register_humanoid_renderer(
      registry, "troops/carthage/civilian", std::make_shared<CivilianRenderer const>());

  auto& ar = Render::GL::RenderArchetypeRegistry::instance();
  ar.register_archetype("carthage_civilian_cudgel",
                        [] { (void)civilian_cudgel_archetype(); });
}

} // namespace Render::GL::Carthage
