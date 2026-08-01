#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "humanoid_spec.h"
#include "skeleton.h"

namespace Render::Humanoid {

namespace GripAxisDetail {

inline constexpr QVector3D k_skeleton_right_hint{1.0F, 0.0F, 0.0F};

[[nodiscard]] inline auto safe_normalized(const QVector3D& value,
                                          const QVector3D& fallback) -> QVector3D {
  if (value.lengthSquared() < 1.0e-10F) {
    return fallback;
  }
  QVector3D normalized = value;
  normalized.normalize();
  return normalized;
}

[[nodiscard]] inline auto rotate_bone_local(const QVector3D& axis,
                                            const QVector3D& bone_local) -> QVector3D {
  QVector3D const y = axis;
  QVector3D x =
      k_skeleton_right_hint - y * QVector3D::dotProduct(k_skeleton_right_hint, y);
  x = safe_normalized(x, QVector3D(0.0F, 0.0F, 1.0F));
  QVector3D const z = QVector3D::crossProduct(x, y).normalized();
  return x * bone_local.x() + y * bone_local.y() + z * bone_local.z();
}

[[nodiscard]] inline auto bind_hand_bone(bool right_hand) -> QMatrix4x4 {
  auto const bone = right_hand ? HumanoidBone::HandR : HumanoidBone::HandL;
  return humanoid_bind_palette()[static_cast<std::size_t>(bone)];
}

} // namespace GripAxisDetail

[[nodiscard]] inline auto
hand_axis_for_weapon_direction(const QVector3D& wanted_direction,
                               const QVector3D& baked_direction,
                               bool right_hand = true) -> QVector3D {
  using namespace GripAxisDetail;

  QMatrix4x4 const bind = bind_hand_bone(right_hand);
  QVector3D const bind_x = bind.column(0).toVector3D();
  QVector3D const bind_y = bind.column(1).toVector3D();
  QVector3D const bind_z = bind.column(2).toVector3D();

  QVector3D const baked = safe_normalized(baked_direction, QVector3D(0.0F, 1.0F, 0.0F));
  QVector3D const wanted =
      safe_normalized(wanted_direction, QVector3D(0.0F, 1.0F, 0.0F));

  QVector3D const bone_local(QVector3D::dotProduct(baked, bind_x),
                             QVector3D::dotProduct(baked, bind_y),
                             QVector3D::dotProduct(baked, bind_z));

  float const cos_cone = std::clamp(bone_local.y(), -1.0F, 1.0F);
  float const sin_cone = std::sqrt(std::max(0.0F, 1.0F - cos_cone * cos_cone));

  QVector3D basis_a = QVector3D::crossProduct(wanted, QVector3D(0.0F, 1.0F, 0.0F));
  if (basis_a.lengthSquared() < 1.0e-8F) {
    basis_a = QVector3D::crossProduct(wanted, QVector3D(0.0F, 0.0F, 1.0F));
  }
  basis_a = safe_normalized(basis_a, QVector3D(1.0F, 0.0F, 0.0F));
  QVector3D const basis_b =
      safe_normalized(QVector3D::crossProduct(wanted, basis_a), QVector3D(0, 0, 1));

  auto axis_at = [&](float roll) -> QVector3D {
    return wanted * cos_cone +
           (basis_a * std::cos(roll) + basis_b * std::sin(roll)) * sin_cone;
  };
  auto error_at = [&](float roll) -> float {
    QVector3D const axis = safe_normalized(axis_at(roll), bind_y);
    return (rotate_bone_local(axis, bone_local) - wanted).lengthSquared();
  };

  constexpr int k_coarse_samples = 48;
  constexpr float k_two_pi = 2.0F * std::numbers::pi_v<float>;
  float best_roll = 0.0F;
  float best_error = error_at(0.0F);
  for (int i = 1; i < k_coarse_samples; ++i) {
    float const roll =
        k_two_pi * static_cast<float>(i) / static_cast<float>(k_coarse_samples);
    float const error = error_at(roll);
    if (error < best_error) {
      best_error = error;
      best_roll = roll;
    }
  }

  float window = k_two_pi / static_cast<float>(k_coarse_samples);
  for (int refinement = 0; refinement < 14; ++refinement) {
    float const low_error = error_at(best_roll - window);
    float const high_error = error_at(best_roll + window);
    if (low_error < best_error && low_error <= high_error) {
      best_error = low_error;
      best_roll -= window;
    } else if (high_error < best_error) {
      best_error = high_error;
      best_roll += window;
    }
    window *= 0.5F;
  }

  return safe_normalized(axis_at(best_roll), bind_y);
}

} // namespace Render::Humanoid
