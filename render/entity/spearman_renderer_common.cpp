#include "spearman_renderer_common.h"

#include <QMatrix4x4>

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "../../game/core/component.h"
#include "../../game/systems/nation_id.h"
#include "../creature/archetype_registry.h"
#include "../creature/pipeline/unit_visual_spec.h"
#include "../equipment/armor/armor_light_carthage.h"
#include "../equipment/armor/carthage_shoulder_cover.h"
#include "../equipment/armor/shoulder_cover_archetype.h"
#include "../equipment/equipment_registry.h"
#include "../equipment/helmets/carthage_heavy_helmet.h"
#include "../equipment/humanoid_equipment_archetype.h"
#include "../equipment/weapons/spear_renderer.h"
#include "../humanoid/facial_hair_catalog.h"
#include "../humanoid/humanoid_renderer_base.h"
#include "../humanoid/humanoid_spec.h"
#include "../humanoid/skeleton.h"
#include "../humanoid/style_palette.h"
#include "../palette.h"
#include "nations/equipment_loadout_catalog.h"

namespace Render::GL {
namespace {

constexpr std::string_view k_default_style_key = "default";
constexpr float k_team_mix_weight = 0.6F;
constexpr float k_style_mix_weight = 0.4F;
constexpr std::uint8_t k_template_variant_bucket_count = 8U;

using Render::GL::Humanoid::mix_palette_color;
using Render::GL::Humanoid::saturate_color;

using StyleRegistry = std::unordered_map<std::string, SpearmanStyleConfig>;

auto style_registry() -> StyleRegistry& {
  static StyleRegistry styles;
  return styles;
}

auto resolve_style(const DrawContext& ctx) -> const SpearmanStyleConfig& {
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
  if (auto fallback = styles.find(std::string(k_default_style_key));
      fallback != styles.end()) {
    return fallback->second;
  }
  static const SpearmanStyleConfig empty{};
  return empty;
}

auto loadout_handle_for_slot(const Nation::ResolvedEquipmentLoadout& loadout,
                             SpearmanLoadoutSlot slot) -> EquipmentHandle {
  switch (slot) {
  case SpearmanLoadoutSlot::Helmet:
    return loadout.helmet_handle;
  case SpearmanLoadoutSlot::Greaves:
    return loadout.greaves_handle;
  case SpearmanLoadoutSlot::Shoulder:
    return loadout.shoulder_handle;
  case SpearmanLoadoutSlot::Armor:
    return loadout.armor_handle;
  case SpearmanLoadoutSlot::Spear:
    return loadout.spear_handle;
  case SpearmanLoadoutSlot::Cloak:
    return loadout.cloak_handle;
  }
  return k_invalid_equipment_handle;
}

auto canonical_spear_cfg() -> const SpearRenderConfig& {
  static const SpearRenderConfig cfg = []() {
    SpearRenderConfig result;
    result.shaft_color = QVector3D(0.5F, 0.3F, 0.2F) * QVector3D(0.85F, 0.75F, 0.65F);
    result.spearhead_color = QVector3D(0.75F, 0.76F, 0.80F);
    result.spear_length = 1.15F;
    result.shaft_radius = 0.018F;
    result.spearhead_length = 0.16F;
    return result;
  }();
  return cfg;
}

auto append_spearman_role_colors_common(const HumanoidVariant& variant,
                                        QVector3D* out,
                                        std::uint32_t base_count,
                                        std::size_t max_count,
                                        bool include_facial_hair) -> std::uint32_t {
  auto count = base_count;
  count += Render::GL::carthage_heavy_helmet_fill_role_colors(
      variant.palette, out + count, max_count - count);
  if (max_count <= count) {
    return count;
  }
  count += Render::GL::carthage_shoulder_cover_fill_role_colors(
      variant.palette, out + count, max_count - count);
  if (max_count <= count) {
    return count;
  }
  count += Render::GL::spear_fill_role_colors(
      variant.palette, canonical_spear_cfg(), out + count, max_count - count);
  if (max_count <= count) {
    return count;
  }
  count += Render::GL::armor_light_carthage_fill_role_colors(
      variant.palette, out + count, max_count - count);
  if (!include_facial_hair || max_count <= count) {
    return count;
  }
  return Render::Humanoid::facial_hair_role_colors(variant, out, count, max_count);
}

auto append_spearman_role_colors(const void* variant_void,
                                 QVector3D* out,
                                 std::uint32_t base_count,
                                 std::size_t max_count) -> std::uint32_t {
  if (variant_void == nullptr || max_count <= base_count) {
    return base_count;
  }
  return append_spearman_role_colors_common(
      *static_cast<const HumanoidVariant*>(variant_void),
      out,
      base_count,
      max_count,
      false);
}

auto append_spearman_role_colors_with_facial_hair(const void* variant_void,
                                                  QVector3D* out,
                                                  std::uint32_t base_count,
                                                  std::size_t max_count)
    -> std::uint32_t {
  if (variant_void == nullptr || max_count <= base_count) {
    return base_count;
  }
  return append_spearman_role_colors_common(
      *static_cast<const HumanoidVariant*>(variant_void),
      out,
      base_count,
      max_count,
      true);
}

struct SpearmanArchetypeSet {
  Render::Creature::ArchetypeId clean{Render::Creature::k_invalid_archetype};
  Render::Creature::ArchetypeId full_beard{Render::Creature::k_invalid_archetype};
  Render::Creature::ArchetypeId long_beard{Render::Creature::k_invalid_archetype};
  Render::Creature::ArchetypeId short_beard{Render::Creature::k_invalid_archetype};
};

auto spearman_archetypes() -> const SpearmanArchetypeSet& {
  static const SpearmanArchetypeSet ids = []() {
    using namespace Render::Creature::Pipeline;

    static const auto k_head_bone =
        static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Head);
    static const auto k_helmet_base_role_byte =
        static_cast<std::uint8_t>(Render::Humanoid::k_humanoid_role_count + 1U);
    static const auto k_shoulder_base_role_byte = static_cast<std::uint8_t>(
        k_helmet_base_role_byte + Render::GL::k_carthage_heavy_helmet_role_count);
    static const auto k_spear_base_role_byte = static_cast<std::uint8_t>(
        k_shoulder_base_role_byte + Render::GL::k_carthage_shoulder_cover_role_count);
    static const auto k_armor_base_role_byte = static_cast<std::uint8_t>(
        k_spear_base_role_byte + Render::GL::k_spear_role_count);
    static const auto k_facial_hair_base_role_byte = static_cast<std::uint8_t>(
        k_armor_base_role_byte + Render::GL::k_armor_light_carthage_role_count);
    static const auto k_chest_bone =
        static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest);
    static const auto k_shoulder_l_bone =
        static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::ShoulderL);
    static const auto k_shoulder_r_bone =
        static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::ShoulderR);
    static const auto k_head_bind_matrix =
        Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(
            Render::Humanoid::HumanoidBone::Head)];
    const auto& bind_frames = Render::Humanoid::humanoid_bind_body_frames();
    static const auto k_shoulder_l_bind_matrix =
        Render::GL::make_shoulder_cover_transform(QMatrix4x4{},
                                                  bind_frames.shoulder_l.origin,
                                                  -bind_frames.torso.right,
                                                  bind_frames.torso.up);
    static const auto k_shoulder_r_bind_matrix =
        Render::GL::make_shoulder_cover_transform(QMatrix4x4{},
                                                  bind_frames.shoulder_r.origin,
                                                  bind_frames.torso.right,
                                                  bind_frames.torso.up);
    static const Render::Creature::StaticAttachmentSpec k_shoulder_l_spec =
        Render::GL::carthage_shoulder_cover_make_static_attachment(
            k_shoulder_l_bone, k_shoulder_base_role_byte, k_shoulder_l_bind_matrix);
    static const Render::Creature::StaticAttachmentSpec k_shoulder_r_spec =
        Render::GL::carthage_shoulder_cover_make_static_attachment(
            k_shoulder_r_bone, k_shoulder_base_role_byte, k_shoulder_r_bind_matrix);
    static const std::array<Render::Creature::StaticAttachmentSpec, 4> k_spear_specs =
        Render::GL::spear_make_static_attachments(canonical_spear_cfg(),
                                                  k_spear_base_role_byte);
    static const Render::Creature::StaticAttachmentSpec k_armor_spec =
        Render::GL::armor_light_carthage_make_static_attachment(k_chest_bone,
                                                                k_armor_base_role_byte);
    static const std::array<Render::Creature::StaticAttachmentSpec, 13>
        k_base_attachments{
            Render::GL::carthage_heavy_helmet_make_static_attachment(
                Render::GL::carthage_heavy_helmet_shell_archetype(),
                k_head_bone,
                k_helmet_base_role_byte,
                k_head_bind_matrix),
            Render::GL::carthage_heavy_helmet_make_static_attachment(
                Render::GL::carthage_heavy_helmet_neck_guard_archetype(),
                k_head_bone,
                k_helmet_base_role_byte,
                k_head_bind_matrix),
            Render::GL::carthage_heavy_helmet_make_static_attachment(
                Render::GL::carthage_heavy_helmet_cheek_guards_archetype(),
                k_head_bone,
                k_helmet_base_role_byte,
                k_head_bind_matrix),
            Render::GL::carthage_heavy_helmet_make_static_attachment(
                Render::GL::carthage_heavy_helmet_face_plate_archetype(),
                k_head_bone,
                k_helmet_base_role_byte,
                k_head_bind_matrix),
            Render::GL::carthage_heavy_helmet_make_static_attachment(
                Render::GL::carthage_heavy_helmet_crest_archetype(),
                k_head_bone,
                k_helmet_base_role_byte,
                k_head_bind_matrix),
            Render::GL::carthage_heavy_helmet_make_static_attachment(
                Render::GL::carthage_heavy_helmet_rivets_archetype(),
                k_head_bone,
                k_helmet_base_role_byte,
                k_head_bind_matrix),
            k_shoulder_l_spec,
            k_shoulder_r_spec,
            k_spear_specs[0],
            k_spear_specs[1],
            k_spear_specs[2],
            k_spear_specs[3],
            k_armor_spec,
        };

    auto const make_bearded_attachments = [&](Render::GL::FacialHairStyle style) {
      std::array<Render::Creature::StaticAttachmentSpec, 14> attachments{};
      std::copy(
          k_base_attachments.begin(), k_base_attachments.end(), attachments.begin());
      attachments.back() = Render::Humanoid::facial_hair_make_static_attachment(
          style, k_facial_hair_base_role_byte);
      return attachments;
    };

    static const auto k_full_beard_attachments =
        make_bearded_attachments(Render::GL::FacialHairStyle::FullBeard);
    static const auto k_long_beard_attachments =
        make_bearded_attachments(Render::GL::FacialHairStyle::LongBeard);
    static const auto k_short_beard_attachments =
        make_bearded_attachments(Render::GL::FacialHairStyle::ShortBeard);

    auto& registry = Render::Creature::ArchetypeRegistry::instance();
    SpearmanArchetypeSet result{};
    result.clean = registry.register_unit_archetype(
        "troops/carthage/spearman",
        CreatureKind::Humanoid,
        std::span<const Render::Creature::StaticAttachmentSpec>(
            k_base_attachments.data(), k_base_attachments.size()),
        &append_spearman_role_colors);
    result.full_beard = registry.register_unit_archetype(
        "troops/carthage/spearman/full_beard",
        CreatureKind::Humanoid,
        std::span<const Render::Creature::StaticAttachmentSpec>(
            k_full_beard_attachments.data(), k_full_beard_attachments.size()),
        &append_spearman_role_colors_with_facial_hair);
    result.long_beard = registry.register_unit_archetype(
        "troops/carthage/spearman/long_beard",
        CreatureKind::Humanoid,
        std::span<const Render::Creature::StaticAttachmentSpec>(
            k_long_beard_attachments.data(), k_long_beard_attachments.size()),
        &append_spearman_role_colors_with_facial_hair);
    result.short_beard = registry.register_unit_archetype(
        "troops/carthage/spearman/short_beard",
        CreatureKind::Humanoid,
        std::span<const Render::Creature::StaticAttachmentSpec>(
            k_short_beard_attachments.data(), k_short_beard_attachments.size()),
        &append_spearman_role_colors_with_facial_hair);
    return result;
  }();
  return ids;
}

auto spearman_variant_table() -> const Render::Creature::ArchetypeVariantTable& {
  static const Render::Creature::ArchetypeVariantTable k_table = []() {
    Render::Creature::ArchetypeVariantTable t{};

    t.variant_trigger_pose = Render::Creature::PoseIntent::Count;
    t.variant_stride = 8;
    t.variant_is_seed_based = false;
    auto const& a = spearman_archetypes();

    t.archetype_for_variant[static_cast<std::size_t>(
        Render::GL::FacialHairStyle::None)] = a.clean;
    t.archetype_for_variant[static_cast<std::size_t>(
        Render::GL::FacialHairStyle::Stubble)] = a.clean;
    t.archetype_for_variant[static_cast<std::size_t>(
        Render::GL::FacialHairStyle::Goatee)] = a.clean;
    t.archetype_for_variant[static_cast<std::size_t>(
        Render::GL::FacialHairStyle::Mustache)] = a.clean;
    t.archetype_for_variant[static_cast<std::size_t>(
        Render::GL::FacialHairStyle::MustacheAndBeard)] = a.clean;

    t.archetype_for_variant[static_cast<std::size_t>(
        Render::GL::FacialHairStyle::ShortBeard)] = a.short_beard;
    t.archetype_for_variant[static_cast<std::size_t>(
        Render::GL::FacialHairStyle::FullBeard)] = a.full_beard;
    t.archetype_for_variant[static_cast<std::size_t>(
        Render::GL::FacialHairStyle::LongBeard)] = a.long_beard;
    return t;
  }();
  return k_table;
}

auto beard_variant_bucket(const DrawContext& ctx, std::uint32_t seed) -> std::uint8_t {
  if (ctx.has_variant_override) {
    return static_cast<std::uint8_t>(ctx.variant_override %
                                     k_template_variant_bucket_count);
  }

  std::uint32_t mixed = seed;
  mixed ^= mixed >> 16U;
  mixed *= 2246822519U;
  mixed ^= mixed >> 13U;
  mixed *= 3266489917U;
  mixed ^= mixed >> 16U;
  return static_cast<std::uint8_t>(mixed % k_template_variant_bucket_count);
}

class SpearmanRenderer final : public HumanoidRendererBase {
public:
  SpearmanRenderer(const SpearmanRendererProfile& profile,
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

    if (m_profile.ensure_styles_registered != nullptr) {
      m_profile.ensure_styles_registered();
    }
    if (m_visual_spec_baked) {
      return m_visual_spec_cache;
    }

    const auto loadout = Render::GL::Nation::resolve_equipment_loadout(m_renderer_key);
    std::array<EquipmentHandle, 6> handles{};
    for (std::size_t i = 0; i < m_profile.loadout_slot_count; ++i) {
      handles[i] = loadout_handle_for_slot(loadout, m_profile.loadout_slots[i]);
    }

    UnitVisualSpec spec{};
    spec.kind = CreatureKind::Humanoid;
    spec.debug_name = m_renderer_key;
    spec.creature_asset_id = m_creature_asset_id;
    spec.scaling = m_profile.proportion_profile.as_pipeline_scaling();
    spec.owned_legacy_slots = LegacySlotMask::AllHumanoid;
    spec.archetype_id = resolve_humanoid_equipment_archetype(
        m_renderer_key,
        Render::Creature::ArchetypeRegistry::k_humanoid_base,
        std::span<const EquipmentHandle>(handles.data(), m_profile.loadout_slot_count));
    if (m_profile.use_carthage_beard_archetypes) {
      spec.animation_manifest.variant_table = &spearman_variant_table();
    }
    m_visual_spec_cache = spec;
    m_visual_spec_baked = true;
    return m_visual_spec_cache;
  }

  void get_variant(const DrawContext& ctx,
                   uint32_t seed,
                   HumanoidVariant& variant) const override {
    QVector3D const team_tint = resolve_team_tint(ctx);
    variant.palette = make_humanoid_palette(team_tint, seed);
    auto const& style = resolve_style(ctx);
    apply_palette_overrides(style, team_tint, variant);

    if (m_profile.apply_carthage_beard_traits) {
      apply_carthage_beard_traits(ctx, seed, style, variant);
    }
  }

private:
  const SpearmanRendererProfile& m_profile;
  std::string_view m_renderer_key;
  Render::Creature::Pipeline::CreatureAssetId m_creature_asset_id;

  void apply_palette_overrides(const SpearmanStyleConfig& style,
                               const QVector3D& team_tint,
                               HumanoidVariant& variant) const {
    auto apply_color = [&](const std::optional<QVector3D>& override_color,
                           QVector3D& target) {
      target = mix_palette_color(
          target, override_color, team_tint, k_team_mix_weight, k_style_mix_weight);
    };

    apply_color(style.cloth_color, variant.palette.cloth);
    apply_color(style.leather_color, variant.palette.leather);
    apply_color(style.leather_dark_color, variant.palette.leather_dark);
    apply_color(style.metal_color, variant.palette.metal);
  }

  void apply_carthage_beard_traits(const DrawContext& ctx,
                                   std::uint32_t seed,
                                   const SpearmanStyleConfig& style,
                                   HumanoidVariant& variant) const {
    auto next_rand = [](uint32_t& s) -> float {
      s = s * 1664525U + 1013904223U;
      return float(s & 0x7FFFFFU) / float(0x7FFFFFU);
    };

    uint32_t beard_seed = seed ^ 0xBEEFFAU;
    std::uint8_t const beard_bucket = beard_variant_bucket(ctx, seed);
    bool const wants_beard = style.force_beard || beard_bucket != 0U;

    if (wants_beard) {
      if (beard_bucket <= 3U) {
        variant.facial_hair.style = FacialHairStyle::FullBeard;
        variant.facial_hair.length = 1.0F + next_rand(beard_seed) * 0.7F;
      } else if (beard_bucket <= 6U) {
        variant.facial_hair.style = FacialHairStyle::LongBeard;
        variant.facial_hair.length = 1.3F + next_rand(beard_seed) * 0.9F;
      } else {
        variant.facial_hair.style = FacialHairStyle::ShortBeard;
        variant.facial_hair.length = 0.9F + next_rand(beard_seed) * 0.5F;
      }

      float const color_roll = next_rand(beard_seed);
      if (color_roll < 0.60F) {
        variant.facial_hair.color = QVector3D(0.18F + next_rand(beard_seed) * 0.10F,
                                              0.14F + next_rand(beard_seed) * 0.08F,
                                              0.10F + next_rand(beard_seed) * 0.06F);
      } else if (color_roll < 0.85F) {
        variant.facial_hair.color = QVector3D(0.30F + next_rand(beard_seed) * 0.12F,
                                              0.24F + next_rand(beard_seed) * 0.10F,
                                              0.16F + next_rand(beard_seed) * 0.08F);
      } else {
        variant.facial_hair.color = QVector3D(0.35F + next_rand(beard_seed) * 0.10F,
                                              0.20F + next_rand(beard_seed) * 0.08F,
                                              0.12F + next_rand(beard_seed) * 0.06F);
      }

      variant.facial_hair.thickness = 0.95F + next_rand(beard_seed) * 0.30F;
      variant.facial_hair.coverage = 0.80F + next_rand(beard_seed) * 0.20F;

      if (next_rand(beard_seed) < 0.12F) {
        variant.facial_hair.greyness = 0.12F + next_rand(beard_seed) * 0.30F;
      } else {
        variant.facial_hair.greyness = 0.0F;
      }
    } else {
      variant.facial_hair.style = FacialHairStyle::None;
    }
  }
};

} // namespace

void register_spearman_style(std::string_view style_key,
                             const SpearmanStyleConfig& style) {
  style_registry()[std::string(style_key)] = style;
}

void register_spearman_styles(std::span<const SpearmanStyleRegistration> styles) {
  for (const auto& style : styles) {
    register_spearman_style(style.key, style.style);
  }
}

void register_spearman_renderer_profile(
    EntityRendererRegistry& registry,
    const SpearmanRendererProfile& profile,
    std::span<const SpearmanRendererRegistration> renderers) {
  if (profile.ensure_styles_registered != nullptr) {
    profile.ensure_styles_registered();
  }

  for (const auto& renderer : renderers) {
    auto renderer_instance = std::make_shared<SpearmanRenderer>(
        profile, renderer.renderer_key, renderer.creature_asset_id);
    registry.register_renderer(
        std::string(renderer.renderer_key),
        [renderer_instance](const DrawContext& ctx, ISubmitter& out) {
          renderer_instance->render(ctx, out);
        });
  }
}

} // namespace Render::GL
