#pragma once

#include "dimensions.h"

#include <QVector3D>

namespace Render::Horse {

inline constexpr float k_horse_visual_scale = 1.32F;
inline constexpr float k_horse_body_visual_width_scale = 1.42F;
inline constexpr float k_horse_body_visual_height_scale = 1.12F;
inline constexpr float k_horse_torso_height_scale = 1.36F;
inline constexpr float k_horse_torso_mesh_height_scale = 1.16F;

[[nodiscard]] inline auto horse_body_visual_width(
    const Render::GL::HorseDimensions &dims) noexcept -> float {
  return dims.body_width * k_horse_body_visual_width_scale *
         k_horse_visual_scale;
}

[[nodiscard]] inline auto horse_body_visual_height(
    const Render::GL::HorseDimensions &dims) noexcept -> float {
  return dims.body_height * k_horse_body_visual_height_scale *
         k_horse_visual_scale;
}

[[nodiscard]] inline auto horse_body_visual_length(
    const Render::GL::HorseDimensions &dims) noexcept -> float {
  return dims.body_length * k_horse_visual_scale;
}

[[nodiscard]] inline auto horse_head_visual_width(
    const Render::GL::HorseDimensions &dims) noexcept -> float {
  return dims.head_width * k_horse_visual_scale;
}

[[nodiscard]] inline auto horse_head_visual_height(
    const Render::GL::HorseDimensions &dims) noexcept -> float {
  return dims.head_height * k_horse_visual_scale;
}

[[nodiscard]] inline auto horse_head_visual_length(
    const Render::GL::HorseDimensions &dims) noexcept -> float {
  return dims.head_length * k_horse_visual_scale;
}

[[nodiscard]] inline auto
horse_torso_lift(const Render::GL::HorseDimensions &dims) noexcept -> float {
  return horse_body_visual_height(dims) * 0.96F;
}

[[nodiscard]] inline auto horse_front_leg_attach_local(
    const Render::GL::HorseDimensions &dims) noexcept -> QVector3D {
  float const bh = horse_body_visual_height(dims);
  float const bl = horse_body_visual_length(dims);
  float const torso_lift = horse_torso_lift(dims);
  float const underside = bh * 0.3400F * k_horse_torso_height_scale *
                          k_horse_torso_mesh_height_scale;
  return {0.0F, bh * 0.20F + torso_lift - underside * 0.36F, bl * 0.33F};
}

[[nodiscard]] inline auto horse_rear_leg_attach_local(
    const Render::GL::HorseDimensions &dims) noexcept -> QVector3D {
  float const bh = horse_body_visual_height(dims);
  float const bl = horse_body_visual_length(dims);
  float const torso_lift = horse_torso_lift(dims);
  float const underside = bh * 0.2480F * k_horse_torso_height_scale *
                          k_horse_torso_mesh_height_scale;
  return {0.0F, bh * 0.20F + torso_lift - underside * 0.36F, -bl * 0.30F};
}

[[nodiscard]] inline auto horse_neck_base_local(
    const Render::GL::HorseDimensions &dims) noexcept -> QVector3D {
  float const bh = horse_body_visual_height(dims);
  float const bl = horse_body_visual_length(dims);
  return {0.0F, bh * 1.82F, bl * 0.52F};
}

[[nodiscard]] inline auto horse_neck_top_local(
    const Render::GL::HorseDimensions &dims) noexcept -> QVector3D {
  float const bh = horse_body_visual_height(dims);
  float const bl = horse_body_visual_length(dims);
  return {0.0F, bh * 2.42F, bl * 0.76F};
}

[[nodiscard]] inline auto horse_tail_base_local(
    const Render::GL::HorseDimensions &dims) noexcept -> QVector3D {
  float const bh = horse_body_visual_height(dims);
  float const bl = horse_body_visual_length(dims);
  return {0.0F, bh * 1.82F, -bl * 0.56F};
}

[[nodiscard]] inline auto horse_tail_tip_local(
    const Render::GL::HorseDimensions &dims) noexcept -> QVector3D {
  float const bh = horse_body_visual_height(dims);
  float const bl = horse_body_visual_length(dims);
  return {0.0F, bh * 1.36F, -bl * 0.70F};
}

} // namespace Render::Horse
