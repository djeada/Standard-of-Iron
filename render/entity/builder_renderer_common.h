#pragma once

#include <QVector3D>

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

struct BuilderStyleConfig {
  std::optional<QVector3D> cloth_color;
  std::optional<QVector3D> skin_color;
  std::optional<QVector3D> leather_color;
  std::optional<QVector3D> leather_dark_color;
  std::optional<QVector3D> metal_color;
  std::optional<QVector3D> wood_color;
  std::optional<QVector3D> apron_color;

  bool show_helmet = false;
  bool show_armor = false;
  bool show_tool_belt = true;
  bool force_beard = false;

  std::string attachment_profile;
};

struct BuilderStyleRegistration {
  std::string_view key;
  BuilderStyleConfig style;
};

void register_builder_style(std::string_view style_key,
                            const BuilderStyleConfig& style);
void register_builder_styles(std::span<const BuilderStyleRegistration> styles);

} // namespace Render::GL
