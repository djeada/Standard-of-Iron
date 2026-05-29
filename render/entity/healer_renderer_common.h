#pragma once

#include <QVector3D>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "../creature/pipeline/creature_asset.h"
#include "../creature/pipeline/unit_visual_spec.h"
#include "../humanoid/humanoid_proportion_profiles.h"
#include "../humanoid/humanoid_renderer_base.h"
#include "registry.h"

namespace Render::GL {

struct HealerStyleConfig {
  std::optional<QVector3D> cloth_color;
  std::optional<QVector3D> skin_color;
  std::optional<QVector3D> leather_color;
  std::optional<QVector3D> leather_dark_color;
  std::optional<QVector3D> metal_color;
  std::optional<QVector3D> wood_color;
  std::optional<QVector3D> cape_color;

  bool show_helmet = false;
  bool show_armor = false;
  bool show_cape = true;
  bool force_beard = false;

  std::string attachment_profile;
};

struct HealerStyleRegistration {
  std::string_view key;
  HealerStyleConfig style;
};

void register_healer_style(std::string_view style_key, const HealerStyleConfig& style);
void register_healer_styles(std::span<const HealerStyleRegistration> styles);

using HealerVisualSpecFactory = const Render::Creature::Pipeline::
    UnitVisualSpec& (*)(std::string_view renderer_key,
                        std::string_view style_key,
                        Render::Creature::Pipeline::CreatureAssetId creature_asset_id,
                        const HealerStyleConfig& style,
                        const Humanoid::ProportionProfile& profile);

using HealerVariantDecorator = void (*)(const DrawContext& ctx,
                                        std::uint32_t seed,
                                        const HealerStyleConfig& style,
                                        HumanoidVariant& variant);

struct HealerRendererProfile {
  Humanoid::ProportionProfile proportion_profile;
  float kneel_depth_multiplier{1.0F};
  HealerVisualSpecFactory visual_spec_factory{nullptr};
  HealerVariantDecorator variant_decorator{nullptr};
  void (*ensure_styles_registered)(){nullptr};
};

struct HealerRendererRegistration {
  std::string_view renderer_key;
  std::string_view style_key;
  Render::Creature::Pipeline::CreatureAssetId creature_asset_id{
      Render::Creature::Pipeline::k_humanoid_asset};
};

void register_healer_renderer_profile(
    EntityRendererRegistry& registry,
    const HealerRendererProfile& profile,
    std::span<const HealerRendererRegistration> renderers);

} // namespace Render::GL
