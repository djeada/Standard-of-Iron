#include "horse_anatomy.h"

#include <algorithm>
#include <cmath>

#include "horse_layout.h"
#include "horse_source_asset.h"

namespace Render::Horse {

auto HorseAnatomyValidation::valid() const noexcept -> bool {
  return std::none_of(faults.begin(), faults.end(), [](bool fault) { return fault; });
}

auto HorseAnatomyValidation::has(HorseAnatomyFault fault) const noexcept -> bool {
  return faults[static_cast<std::size_t>(fault)];
}

auto make_horse_anatomy(const Render::GL::HorseDimensions& dims) noexcept
    -> HorseAnatomy {

  HorseAnatomy result{};
  result.body_width = 0.74F;
  result.body_height = 0.91F;
  result.body_length = 1.92F;
  result.head_width = 0.43F;
  result.head_height = 0.64F;
  result.head_length = 0.82F;
  result.barrel_center = {0.0F, 1.60F, -0.12F};
  result.withers = {0.0F, 2.20F, 0.66F};
  result.back_center = {0.0F, 2.057F, -0.25F};
  result.croup = {0.0F, 2.11F, -0.62F};
  result.chest = {0.0F, 1.61F, 0.73F};
  result.neck_base = {0.0F, 1.71F, 0.88F};
  result.poll = {0.0F, 2.46F, 1.49F};
  result.face_tip = {0.0F, 2.10F, 2.03F};
  result.tail_base = {0.0F, 1.91F, -1.02F};
  result.tail_tip = {0.0F, 0.75F, -1.22F};
  result.front_leg_root = {0.0F, 1.40F, 0.80F};
  result.rear_leg_root = {0.0F, 1.75F, -0.74F};
  result.saddle_center = {0.0F, 2.08F, -0.25F};
  result.seat = {0.0F, 2.18F, -0.21F};
  result.body_length *= k_horse_length_scale;
  result.head_length *= k_horse_length_scale;
  for (QVector3D* point : {&result.barrel_center,
                           &result.withers,
                           &result.back_center,
                           &result.croup,
                           &result.chest,
                           &result.neck_base,
                           &result.poll,
                           &result.face_tip,
                           &result.tail_base,
                           &result.tail_tip,
                           &result.front_leg_root,
                           &result.rear_leg_root,
                           &result.saddle_center,
                           &result.seat}) {
    point->setZ(point->z() * k_horse_length_scale);
  }
  result.withers_height = result.withers.y();
  (void)dims;
  return result;
}

auto validate_horse_anatomy(const HorseAnatomy& a) noexcept -> HorseAnatomyValidation {
  HorseAnatomyValidation result{};
  auto set = [&](HorseAnatomyFault fault, bool value) {
    result.faults[static_cast<std::size_t>(fault)] = value;
  };

  float const shoulder_to_buttock = a.body_length * 1.35F;
  float const square_ratio = shoulder_to_buttock / std::max(a.withers_height, 0.001F);
  set(HorseAnatomyFault::BodyNotSquare, square_ratio < 0.72F || square_ratio > 1.20F);

  float const neck_length = (a.poll - a.neck_base).length();
  set(HorseAnatomyFault::NeckTooShort,
      neck_length < a.head_length * 0.90F || neck_length > a.head_length * 1.90F);
  QVector3D const neck_delta = a.poll - a.neck_base;
  set(HorseAnatomyFault::NeckTooUpright,
      neck_delta.z() < std::abs(neck_delta.y()) * 0.42F);
  set(HorseAnatomyFault::PollNotAboveWithers, a.poll.y() <= a.withers.y());
  set(HorseAnatomyFault::LegRootsOutsideBody,
      a.front_leg_root.z() <= a.back_center.z() ||
          a.rear_leg_root.z() >= a.back_center.z());
  set(HorseAnatomyFault::SaddleOutsideBack,
      a.saddle_center.z() <= a.croup.z() || a.saddle_center.z() >= a.withers.z() ||
          a.saddle_center.y() < a.back_center.y());
  set(HorseAnatomyFault::FaceNotSloped,
      a.face_tip.z() <= a.poll.z() || a.face_tip.y() >= a.poll.y());
  return result;
}

} // namespace Render::Horse
