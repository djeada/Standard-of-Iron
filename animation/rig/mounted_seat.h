#pragma once

#include <QVector3D>

namespace Animation::Rig {

// Rider seat frame and weapon reach used by gameplay hit detection.
//
// Combat traces need to know where a mounted rider sits and how far a weapon
// reaches.  Those numbers come from the character rig, but gameplay must not
// reach into the renderer to get them: the simulation runs headless (see
// tools/balance_sim) and a renderer dependency makes that impossible.
//
// The values are frozen here rather than recomputed because their inputs are
// already constant -- the trace has always used a default-seeded horse and a
// default-constructed spear config, not the per-entity rig.  Freezing therefore
// changes no behaviour.
//
// MountedSeatFrameMatchesRig (tests/render/horse/) recomputes these from the
// live rig and fails if they drift, so a rig change surfaces as a decision
// rather than a silent shift in where attacks land.
namespace MountedSeat {

inline constexpr QVector3D position{0.0F, 2.18F, -0.1785F};
inline constexpr QVector3D forward{0.0F, 0.0F, 1.0F};
inline constexpr QVector3D right{1.0F, 0.0F, 0.0F};
inline constexpr QVector3D up{0.0F, 1.0F, 0.0F};

} // namespace MountedSeat

namespace WeaponReach {

// Default spear shaft plus head, matching SpearRenderConfig's defaults.
inline constexpr float spear_shaft = 1.20F;
inline constexpr float spear_head = 0.18F;
inline constexpr float spear_total = spear_shaft + spear_head;

} // namespace WeaponReach

} // namespace Animation::Rig
