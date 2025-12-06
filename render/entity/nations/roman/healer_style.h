#pragma once

#include <QVector3D>
#include <optional>
#include <string>

namespace Render::GL::Roman {

struct HealerStyleConfig {
  std::optional<QVector3D> cloth_color;
  std::optional<QVector3D> leather_color;
  std::optional<QVector3D> leather_dark_color;
  std::optional<QVector3D> metal_color;
  std::optional<QVector3D> wood_color;
  std::optional<QVector3D> cape_color;

  bool show_helmet = false; // Roman medicus wears no helmet
  bool show_armor = false;  // Simple tunic, no heavy armor
  bool show_cape = true;    // Red sash/trim indicating medical role

  std::string attachment_profile;
  std::string shader_id;
};

void register_roman_healer_style();

} // namespace Render::GL::Roman
