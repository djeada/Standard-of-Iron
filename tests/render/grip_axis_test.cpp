#include <QVector3D>

#include <array>
#include <gtest/gtest.h>

#include "render/humanoid/runtime/grip_axis.h"
#include "render/humanoid/schema/skeleton_schema.h"

namespace {

using Render::Humanoid::hand_axis_for_weapon_direction;
namespace Detail = Render::Humanoid::GripAxisDetail;

auto weapon_direction_for_axis(const QVector3D& axis,
                               const QVector3D& baked,
                               bool right_hand) -> QVector3D {
  QMatrix4x4 const bind = Detail::bind_hand_bone(right_hand);
  QVector3D const bone_local(QVector3D::dotProduct(baked, bind.column(0).toVector3D()),
                             QVector3D::dotProduct(baked, bind.column(1).toVector3D()),
                             QVector3D::dotProduct(baked, bind.column(2).toVector3D()));
  return Detail::rotate_bone_local(axis, bone_local);
}

void expect_weapon_points_where_asked(const QVector3D& wanted,
                                      const QVector3D& baked,
                                      bool right_hand = true) {
  QVector3D const wanted_n = wanted.normalized();
  QVector3D const baked_n = baked.normalized();
  QVector3D const axis = hand_axis_for_weapon_direction(wanted_n, baked_n, right_hand);
  ASSERT_NEAR(axis.length(), 1.0F, 1.0e-3F);

  QVector3D const weapon = weapon_direction_for_axis(axis, baked_n, right_hand);
  EXPECT_NEAR(QVector3D::dotProduct(weapon.normalized(), wanted_n), 1.0F, 1.0e-3F)
      << "weapon pointed (" << weapon.x() << ", " << weapon.y() << ", " << weapon.z()
      << ") instead of (" << wanted_n.x() << ", " << wanted_n.y() << ", "
      << wanted_n.z() << ")";
}

const QVector3D k_baked_spear{0.0453777F, 0.4174751F, 0.9075546F};
const QVector3D k_baked_bow{0.0F, 1.0F, 0.0F};

TEST(GripAxis, BracedSpearPointsForwardNotBackIntoTheSoldier) {
  QVector3D const braced(0.05F, 0.46F, 1.0F);
  expect_weapon_points_where_asked(braced, k_baked_spear);

  QVector3D const axis = hand_axis_for_weapon_direction(
      braced.normalized(), k_baked_spear.normalized(), true);
  QVector3D const weapon =
      weapon_direction_for_axis(axis, k_baked_spear.normalized(), true);
  EXPECT_GT(weapon.z(), 0.0F) << "the spear points behind the soldier";
}

TEST(GripAxis, RestingSpearKeepsItsAuthoredCarryAngle) {
  expect_weapon_points_where_asked(QVector3D(0.05F, 0.55F, 0.85F), k_baked_spear);
}

TEST(GripAxis, ThrustingSpearLevelsOut) {
  expect_weapon_points_where_asked(QVector3D(0.02F, 0.05F, 1.0F), k_baked_spear);
}

TEST(GripAxis, DrawnBowStandsUpright) {
  expect_weapon_points_where_asked(QVector3D(0.0F, 1.0F, 0.0F), k_baked_bow);
}

TEST(GripAxis, ReadyBowTiltsBackWithoutFlipping) {
  QVector3D const ready(0.0F, 0.90F, 0.44F);
  expect_weapon_points_where_asked(ready, k_baked_bow);

  QVector3D const axis =
      hand_axis_for_weapon_direction(ready.normalized(), k_baked_bow, true);
  QVector3D const weapon = weapon_direction_for_axis(axis, k_baked_bow, true);
  EXPECT_GT(weapon.y(), 0.0F) << "the bow hangs upside down";
}

TEST(GripAxis, LeftHandWeaponsResolveToo) {
  expect_weapon_points_where_asked(QVector3D(0.0F, 0.46F, 1.0F), k_baked_spear, false);
}

} // namespace
