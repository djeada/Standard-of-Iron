#pragma once

#include <QVector3D>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "../creature/pipeline/creature_asset.h"
#include "../humanoid/humanoid_proportion_profiles.h"
#include "../humanoid/humanoid_specs.h"
#include "registry.h"

namespace Render::GL {

struct ArcherStyleConfig {
  std::optional<QVector3D> cloth_color;
  std::optional<QVector3D> skin_color;
  std::optional<QVector3D> leather_color;
  std::optional<QVector3D> leather_dark_color;
  std::optional<QVector3D> metal_color;
  std::optional<QVector3D> wood_color;
  std::optional<QVector3D> cape_color;
  std::optional<QVector3D> fletching_color;
  std::optional<QVector3D> bow_string_color;

  bool show_helmet = true;
  bool show_armor = true;
  bool show_shoulder_decor = true;
  bool show_cape = true;
  bool force_beard = false;

  std::string attachment_profile;
  std::string armor_id;
};

struct ArcherStyleRegistration {
  std::string_view key;
  ArcherStyleConfig style;
};

void register_archer_style(std::string_view style_key, const ArcherStyleConfig& style);
void register_archer_styles(std::span<const ArcherStyleRegistration> styles);

struct ArcherVariationTuning {
  float height_scale{1.0F};
  float bulk_scale{1.0F};
  float stance_width{1.0F};
  float arm_swing_amp{1.0F};
};

enum class ArcherLoadoutSlot : std::uint8_t {
  Helmet,
  Greaves,
  Quiver,
  Armor,
  Bow,
  Cloak,
};

struct ArcherRendererProfile {
  std::string_view default_renderer_key;
  std::string_view default_style_key;
  std::string_view base_debug_name;
  Humanoid::ProportionProfile proportion_profile;
  ArcherVariationTuning variation;
  float kneel_depth_multiplier{1.125F};
  Render::Creature::Pipeline::CreatureAssetId creature_asset_id{
      Render::Creature::Pipeline::k_humanoid_asset};
  std::array<ArcherLoadoutSlot, 6> loadout_slots{};
  std::size_t loadout_slot_count{0U};
  bool apply_skin_override{false};
  bool apply_carthage_variant_traits{false};
  void (*ensure_styles_registered)(){nullptr};
};

struct ArcherRendererRegistration {
  std::string_view renderer_key;
  std::string_view style_key;
  Render::Creature::Pipeline::CreatureAssetId creature_asset_id{
      Render::Creature::Pipeline::k_humanoid_asset};
};

void register_archer_renderer_profile(
    EntityRendererRegistry& registry,
    const ArcherRendererProfile& profile,
    std::span<const ArcherRendererRegistration> renderers);

} // namespace Render::GL
