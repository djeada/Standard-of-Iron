#pragma once

#include <QVector3D>
#include <optional>
#include <string>

namespace Render::GL::Roman {

struct SpearmanStyleConfig {
  std::optional<QVector3D> cloth_color;
  std::optional<QVector3D> leather_color;
  std::optional<QVector3D> leather_dark_color;
  std::optional<QVector3D> metal_color;
  std::optional<QVector3D> spear_shaft_color;
  std::optional<QVector3D> spearhead_color;
  std::optional<float> spear_length_scale;
  std::optional<float> spear_shaft_radius_scale;
  std::string shader_id;
};

void register_roman_spearman_style();

} 
