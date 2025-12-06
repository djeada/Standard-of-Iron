#pragma once

#include <QVector3D>
#include <optional>
#include <string>

namespace Render::GL::Carthage {

struct HealerStyleConfig {
  std::optional<QVector3D> cloth_color;
  std::optional<QVector3D> leather_color;
  std::optional<QVector3D> leather_dark_color;
  std::optional<QVector3D> metal_color;
  std::optional<QVector3D> wood_color;
  std::optional<QVector3D> cape_color;

  bool show_helmet = false; // Healers don't wear helmets
  bool show_armor = false;  // Simple tunic, no heavy armor
  bool show_cape = true;    // Purple trim/sash

  // Carthaginian/Phoenician healers traditionally depicted with beards
  bool force_beard = true;

  std::string attachment_profile;
  std::string shader_id;
};

void register_carthage_healer_style();

} // namespace Render::GL::Carthage
