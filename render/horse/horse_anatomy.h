#pragma once

#include <QVector3D>

#include <array>
#include <cstdint>

#include "dimensions.h"

namespace Render::Horse {

struct HorseAnatomy {
  QVector3D barrel_center{};
  QVector3D withers{};
  QVector3D back_center{};
  QVector3D croup{};
  QVector3D chest{};
  QVector3D neck_base{};
  QVector3D poll{};
  QVector3D face_tip{};
  QVector3D tail_base{};
  QVector3D tail_tip{};
  QVector3D front_leg_root{};
  QVector3D rear_leg_root{};
  QVector3D saddle_center{};
  QVector3D seat{};

  float body_width{};
  float body_height{};
  float body_length{};
  float head_width{};
  float head_height{};
  float head_length{};
  float withers_height{};
};

enum class HorseAnatomyFault : std::uint8_t {
  BodyNotSquare,
  NeckTooShort,
  NeckTooUpright,
  PollNotAboveWithers,
  LegRootsOutsideBody,
  SaddleOutsideBack,
  FaceNotSloped,
  Count,
};

struct HorseAnatomyValidation {
  std::array<bool, static_cast<std::size_t>(HorseAnatomyFault::Count)> faults{};

  [[nodiscard]] auto valid() const noexcept -> bool;
  [[nodiscard]] auto has(HorseAnatomyFault fault) const noexcept -> bool;
};

[[nodiscard]] auto
make_horse_anatomy(const Render::GL::HorseDimensions& dims) noexcept -> HorseAnatomy;

[[nodiscard]] auto
validate_horse_anatomy(const HorseAnatomy& anatomy) noexcept -> HorseAnatomyValidation;

} // namespace Render::Horse
