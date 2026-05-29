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

// TODO(phase2): Builder renderers have complex tool-specific archetypes (hammer, saw,
// chisel), work tunics, aprons, arm guards, and tool belts that are highly entangled
// with nation-specific implementations. Full extraction to common pattern requires
// careful separation of:
// - Tool archetype generation (builder_work_tunic_archetype, builder_hammer_archetype,
// etc.)
// - Role color filling functions (builder_work_tunic_fill_role_colors,
// builder_tool_belt_fill_role_colors, etc.)
// - Static attachment specs (builder_work_tunic_make_static_attachment, etc.)
// - Extra role color callbacks
// For now, only style registry is extracted to common. Nation-specific renderers remain
// separate.

} // namespace Render::GL
