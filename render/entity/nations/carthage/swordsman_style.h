#pragma once

#include <QVector3D>
#include <optional>
#include <string>

namespace Render::GL::Carthage {

struct KnightStyleConfig {
  std::optional<QVector3D> cloth_color;
  std::optional<QVector3D> leather_color;
  std::optional<QVector3D> leather_dark_color;
  std::optional<QVector3D> metal_color;
  std::optional<QVector3D> shield_color;
  std::optional<QVector3D> shield_trim_color;

  std::optional<float> shield_radius_scale;
  std::optional<float> shield_aspect_ratio;
  std::optional<bool> shield_cross_decal;
  std::optional<bool> has_scabbard;

  std::string shader_id;
};

void register_carthage_swordsman_style();

} 
