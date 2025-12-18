#pragma once

#include <QVector3D>
#include <optional>
#include <string>

namespace Render::GL::Carthage {

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

  bool force_beard = false;

  std::string attachment_profile;
  std::string shader_id;
  std::string armor_id;
};

void register_carthage_archer_style();

} // namespace Render::GL::Carthage
