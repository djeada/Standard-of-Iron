#pragma once

#include <QVector3D>
#include <optional>
#include <string>

namespace Render::GL::Carthage {

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

  bool force_beard = true;

  std::string attachment_profile;
  std::string shader_id;
};

void register_carthage_healer_style();

} 
