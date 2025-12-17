#pragma once

#include <QVector3D>
#include <optional>
#include <string>

namespace Render::GL::Roman {

struct BuilderStyleConfig {
  std::optional<QVector3D> cloth_color;
  std::optional<QVector3D> leather_color;
  std::optional<QVector3D> leather_dark_color;
  std::optional<QVector3D> metal_color;
  std::optional<QVector3D> wood_color;
  std::optional<QVector3D> apron_color;

  bool show_helmet = false;
  bool show_armor = false;
  bool show_tool_belt = true;

  std::string attachment_profile;
  std::string shader_id;
};

void register_roman_builder_style();

} // namespace Render::GL::Roman
