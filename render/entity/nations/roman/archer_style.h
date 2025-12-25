#pragma once

#include <QVector3D>
#include <optional>
#include <string>

namespace Render::GL::Roman {

struct ArcherStyleConfig {
  std::optional<QVector3D> cloth_color;
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

  std::string attachment_profile;
  std::string shader_id;
};

void register_roman_archer_style();

} 
