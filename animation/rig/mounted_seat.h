#pragma once

#include <QVector3D>

namespace Animation::Rig {

namespace MountedSeat {

inline constexpr QVector3D position{0.0F, 2.18F, -0.1785F};
inline constexpr QVector3D forward{0.0F, 0.0F, 1.0F};
inline constexpr QVector3D right{1.0F, 0.0F, 0.0F};
inline constexpr QVector3D up{0.0F, 1.0F, 0.0F};

} // namespace MountedSeat

namespace WeaponReach {

inline constexpr float spear_shaft = 1.20F;
inline constexpr float spear_head = 0.18F;
inline constexpr float spear_total = spear_shaft + spear_head;

} // namespace WeaponReach

} // namespace Animation::Rig
