#pragma once

#include <QVector3D>

namespace Animation::Rig {

namespace MountedSeat {

// Where a rider sits, frozen here so that gameplay and the renderer cannot
// disagree about it. Derived from the authored saddle landmark (0, 2.18,
// -0.21) with the horse's length scale on z and the shared horse scale on all
// axes; MountedSeatFrame.FrozenGameplayValuesMatchTheRig fails if the rig moves
// and this is not updated with it.
inline constexpr QVector3D position{0.0F, 1.6132F, -0.13209F};
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
