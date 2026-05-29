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
#include "registry.h"

namespace Render::GL {

struct SpearmanStyleConfig {
  std::optional<QVector3D> cloth_color;
  std::optional<QVector3D> leather_color;
  std::optional<QVector3D> leather_dark_color;
  std::optional<QVector3D> metal_color;
  std::optional<QVector3D> spear_shaft_color;
  std::optional<QVector3D> spearhead_color;
  std::optional<float> spear_length_scale;
  std::optional<float> spear_shaft_radius_scale;
  bool force_beard = false;
  std::string armor_id;
};

struct SpearmanStyleRegistration {
  std::string_view key;
  SpearmanStyleConfig style;
};

void register_spearman_style(std::string_view style_key,
                             const SpearmanStyleConfig& style);
void register_spearman_styles(std::span<const SpearmanStyleRegistration> styles);

enum class SpearmanLoadoutSlot : std::uint8_t {
  Helmet,
  Greaves,
  Shoulder,
  Armor,
  Spear,
  Cloak,
};

struct SpearmanVariationTuning {
  float bulk_scale{1.0F};
  float stance_width{1.0F};
};

struct SpearmanRendererProfile {
  Humanoid::ProportionProfile proportion_profile;
  SpearmanVariationTuning variation;
  float kneel_depth_multiplier{0.875F};
  float lean_amount_multiplier{1.0F};
  std::array<SpearmanLoadoutSlot, 6> loadout_slots{};
  std::size_t loadout_slot_count{0U};
  bool apply_carthage_beard_traits{false};
  bool use_carthage_beard_archetypes{false};
  void (*ensure_styles_registered)(){nullptr};
};

struct SpearmanRendererRegistration {
  std::string_view renderer_key;
  Render::Creature::Pipeline::CreatureAssetId creature_asset_id{
      Render::Creature::Pipeline::k_humanoid_spear_asset};
};

void register_spearman_renderer_profile(
    EntityRendererRegistry& registry,
    const SpearmanRendererProfile& profile,
    std::span<const SpearmanRendererRegistration> renderers);

} // namespace Render::GL
