#pragma once

#include <QVector3D>

#include <cstdint>

#include "elephant_gait.h"
#include "render/elephant/schema/attachment_schema.h"

namespace Render::GL {

struct AnimationInputs;

struct ElephantDimensions {
  float body_length{};
  float body_width{};
  float body_height{};
  float barrel_center_y{};

  float neck_length{};
  float neck_width{};

  float head_length{};
  float head_width{};
  float head_height{};

  float trunk_length{};
  float trunk_base_radius{};
  float trunk_tip_radius{};

  float ear_width{};
  float ear_height{};
  float ear_thickness{};

  float leg_length{};
  float leg_radius{};
  float foot_radius{};

  float tail_length{};

  float tusk_length{};
  float tusk_radius{};

  float howdah_width{};
  float howdah_length{};
  float howdah_height{};

  float idle_bob_amplitude{};
  float move_bob_amplitude{};
};

struct ElephantVariant {
  QVector3D skin_color;
  QVector3D skin_highlight;
  QVector3D skin_shadow;
  QVector3D ear_inner_color;
  QVector3D tusk_color;
  QVector3D toenail_color;
  QVector3D howdah_wood_color;
  QVector3D howdah_fabric_color;
  QVector3D howdah_metal_color;
};

struct ElephantProfile {
  ElephantDimensions dims{};
  ElephantVariant variant;
  ElephantGait gait{};
};

auto make_elephant_dimensions(uint32_t seed) -> ElephantDimensions;
auto make_elephant_variant(uint32_t seed,
                           const QVector3D& fabric_base,
                           const QVector3D& metal_base) -> ElephantVariant;
auto make_elephant_profile(uint32_t seed,
                           const QVector3D& fabric_base,
                           const QVector3D& metal_base) -> ElephantProfile;

} // namespace Render::GL
