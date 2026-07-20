#pragma once

#include <QVector3D>

#include "dimensions.h"

namespace Render::Horse {

struct HorseConformation {
  float visual_scale{1.32F};
  float body_width{1.18F};
  float body_height{1.04F};
  float head_width{1.30F};
  float head_height{1.18F};
  float head_length{1.32F};
  float torso_lift{0.54F};
  float torso_chain_drop{0.10F};
  float neck_base_height{1.34F};
  float neck_poll_height{2.22F};
  float neck_base_forward{0.39F};
  float neck_poll_forward{0.58F};
  float tail_base_height{1.38F};
  float tail_tip_height{0.92F};
};

inline constexpr HorseConformation k_cavalry_horse_conformation{};

inline constexpr float k_horse_visual_scale = k_cavalry_horse_conformation.visual_scale;
inline constexpr float k_horse_body_visual_width_scale =
    k_cavalry_horse_conformation.body_width;
inline constexpr float k_horse_body_visual_height_scale =
    k_cavalry_horse_conformation.body_height;
inline constexpr float k_horse_torso_height_scale = 1.24F;
inline constexpr float k_horse_torso_mesh_height_scale = 1.12F;
inline constexpr float k_horse_head_visual_width_scale =
    k_cavalry_horse_conformation.head_width;
inline constexpr float k_horse_head_visual_height_scale =
    k_cavalry_horse_conformation.head_height;
inline constexpr float k_horse_head_visual_length_scale =
    k_cavalry_horse_conformation.head_length;
inline constexpr float k_horse_muzzle_length_scale = 0.80F;
inline constexpr float k_horse_torso_part_height_scale = 1.08F;
inline constexpr float k_horse_torso_chain_drop_fraction =
    k_cavalry_horse_conformation.torso_chain_drop;
inline constexpr float k_horse_rear_torso_extra_length_scale = 1.00F;
inline constexpr float k_horse_neck_width_boost = 2.32F;
inline constexpr float k_horse_neck_base_body_height_scale =
    k_cavalry_horse_conformation.neck_base_height;
inline constexpr float k_horse_neck_top_body_height_scale =
    k_cavalry_horse_conformation.neck_poll_height;
inline constexpr float k_horse_neck_base_body_length_scale =
    k_cavalry_horse_conformation.neck_base_forward;
inline constexpr float k_horse_neck_forward_visual_scale =
    k_cavalry_horse_conformation.neck_poll_forward;
inline constexpr float k_horse_head_setback_scale = 0.08F;

[[nodiscard]] inline auto
horse_body_visual_width(const Render::GL::HorseDimensions& dims) noexcept -> float {
  return dims.body_width * k_horse_body_visual_width_scale * k_horse_visual_scale;
}

[[nodiscard]] inline auto
horse_body_visual_height(const Render::GL::HorseDimensions& dims) noexcept -> float {
  return dims.body_height * k_horse_body_visual_height_scale * k_horse_visual_scale;
}

[[nodiscard]] inline auto
horse_body_visual_length(const Render::GL::HorseDimensions& dims) noexcept -> float {
  return dims.body_length * k_horse_visual_scale;
}

[[nodiscard]] inline auto
horse_head_visual_width(const Render::GL::HorseDimensions& dims) noexcept -> float {
  return dims.head_width * k_horse_visual_scale * k_horse_head_visual_width_scale;
}

[[nodiscard]] inline auto
horse_head_visual_height(const Render::GL::HorseDimensions& dims) noexcept -> float {
  return dims.head_height * k_horse_visual_scale * k_horse_head_visual_height_scale;
}

[[nodiscard]] inline auto
horse_head_visual_length(const Render::GL::HorseDimensions& dims) noexcept -> float {
  return dims.head_length * k_horse_visual_scale * k_horse_head_visual_length_scale;
}

[[nodiscard]] inline auto
horse_torso_lift(const Render::GL::HorseDimensions& dims) noexcept -> float {
  return horse_body_visual_height(dims) * k_cavalry_horse_conformation.torso_lift;
}

[[nodiscard]] inline auto
horse_torso_chain_drop(const Render::GL::HorseDimensions& dims) noexcept -> float {
  return horse_torso_lift(dims) * k_horse_torso_chain_drop_fraction;
}

[[nodiscard]] inline auto
horse_torso_visual_lift(const Render::GL::HorseDimensions& dims) noexcept -> float {
  return horse_torso_lift(dims) - horse_torso_chain_drop(dims);
}

[[nodiscard]] inline auto horse_front_leg_attach_local(
    const Render::GL::HorseDimensions& dims) noexcept -> QVector3D {
  float const bh = horse_body_visual_height(dims);
  float const bl = horse_body_visual_length(dims);
  float const torso_lift = horse_torso_lift(dims);
  float const underside =
      bh * 0.3400F * k_horse_torso_height_scale * k_horse_torso_mesh_height_scale;
  return {0.0F, bh * 0.22F + torso_lift - underside * 0.34F, bl * 0.33F};
}

[[nodiscard]] inline auto horse_rear_leg_attach_local(
    const Render::GL::HorseDimensions& dims) noexcept -> QVector3D {
  float const bh = horse_body_visual_height(dims);
  float const bl = horse_body_visual_length(dims);
  float const torso_lift = horse_torso_lift(dims);
  float const underside =
      bh * 0.2480F * k_horse_torso_height_scale * k_horse_torso_mesh_height_scale;
  return {0.0F, bh * 0.20F + torso_lift - underside * 0.36F, -bl * 0.30F};
}

[[nodiscard]] inline auto
horse_neck_base_local(const Render::GL::HorseDimensions& dims) noexcept -> QVector3D {
  float const bh = horse_body_visual_height(dims);
  float const bl = horse_body_visual_length(dims);
  float const torso_drop = horse_torso_chain_drop(dims);
  return {0.0F,
          bh * k_horse_neck_base_body_height_scale - torso_drop,
          bl * k_horse_neck_base_body_length_scale};
}

[[nodiscard]] inline auto
horse_neck_top_local(const Render::GL::HorseDimensions& dims) noexcept -> QVector3D {
  float const bh = horse_body_visual_height(dims);
  QVector3D const neck_base = horse_neck_base_local(dims);
  return {0.0F,
          bh * k_horse_neck_top_body_height_scale,
          neck_base.z() + dims.neck_length * k_horse_visual_scale *
                              k_horse_neck_forward_visual_scale};
}

[[nodiscard]] inline auto
horse_tail_base_local(const Render::GL::HorseDimensions& dims) noexcept -> QVector3D {
  float const bh = horse_body_visual_height(dims);
  float const bl = horse_body_visual_length(dims);
  float const torso_drop = horse_torso_chain_drop(dims);
  return {0.0F,
          bh * k_cavalry_horse_conformation.tail_base_height - torso_drop,
          -bl * 0.48F};
}

[[nodiscard]] inline auto
horse_tail_tip_local(const Render::GL::HorseDimensions& dims) noexcept -> QVector3D {
  float const bh = horse_body_visual_height(dims);
  float const bl = horse_body_visual_length(dims);
  float const torso_drop = horse_torso_chain_drop(dims);
  return {0.0F,
          bh * k_cavalry_horse_conformation.tail_tip_height - torso_drop,
          -bl * 0.68F};
}

} // namespace Render::Horse
