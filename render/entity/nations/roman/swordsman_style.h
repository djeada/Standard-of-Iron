#pragma once

#include <QVector3D>

#include <optional>
#include <string>

namespace Render::GL::Roman {

struct KnightStyleConfig {
  std::optional<QVector3D> cloth_color;
  std::optional<QVector3D> leather_color;
  std::optional<QVector3D> leather_dark_color;
  std::optional<QVector3D> metal_color;
};

void register_roman_swordsman_style();

} // namespace Render::GL::Roman
