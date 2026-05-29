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

struct SwordsmanStyleConfig {
  std::optional<QVector3D> cloth_color;
  std::optional<QVector3D> skin_color;
  std::optional<QVector3D> leather_color;
  std::optional<QVector3D> leather_dark_color;
  std::optional<QVector3D> metal_color;
  std::optional<QVector3D> shield_color;
  std::optional<QVector3D> shield_trim_color;

  std::optional<float> shield_radius_scale;
  std::optional<float> shield_aspect_ratio;
  std::optional<bool> shield_cross_decal;
  std::optional<bool> has_scabbard;
};

struct SwordsmanStyleRegistration {
  std::string_view key;
  SwordsmanStyleConfig style;
};

void register_swordsman_style(std::string_view style_key,
                              const SwordsmanStyleConfig& style);
void register_swordsman_styles(std::span<const SwordsmanStyleRegistration> styles);

enum class SwordsmanLoadoutSlot : std::uint8_t {
  Helmet,
  Greaves,
  Shoulder,
  Shield,
  Armor,
  Sword,
  Cloak,
};

struct SwordsmanVariationTuning {
  float bulk_scale{1.0F};
  float stance_width{1.0F};
};

struct SwordsmanRendererProfile {
  Humanoid::ProportionProfile proportion_profile;
  SwordsmanVariationTuning variation;
  float kneel_depth_multiplier{0.825F};
  std::array<SwordsmanLoadoutSlot, 7> loadout_slots{};
  std::size_t loadout_slot_count{0U};
  bool apply_skin_override{false};
  void (*ensure_styles_registered)(){nullptr};
};

struct SwordsmanRendererRegistration {
  std::string_view renderer_key;
  Render::Creature::Pipeline::CreatureAssetId creature_asset_id{
      Render::Creature::Pipeline::k_humanoid_sword_asset};
};

void register_swordsman_renderer_profile(
    EntityRendererRegistry& registry,
    const SwordsmanRendererProfile& profile,
    std::span<const SwordsmanRendererRegistration> renderers);

} // namespace Render::GL
