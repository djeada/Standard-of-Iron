#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <array>
#include <cstdint>
#include <span>

#include "render/creature/quadruped/mesh_graph.h"
#include "render/creature/spec.h"
#include "wildlife_rig.h"

namespace Render::Wildlife {

inline constexpr std::uint8_t k_wolf_role_fur = 1U;
inline constexpr std::uint8_t k_wolf_role_saddle = 2U;
inline constexpr std::uint8_t k_wolf_role_pale = 3U;
inline constexpr std::uint8_t k_wolf_role_cream = 4U;
inline constexpr std::uint8_t k_wolf_role_limb = 5U;
inline constexpr std::uint8_t k_wolf_role_paw = 6U;
inline constexpr std::uint8_t k_wolf_role_nose = 7U;
inline constexpr std::uint8_t k_wolf_role_eye = 8U;
inline constexpr std::uint8_t k_wolf_role_iris = 9U;
inline constexpr std::size_t k_wolf_role_count = 9U;

enum class WolfGait : std::uint8_t {
  Stand = 0,
  Stalk,
  Walk,
  Run
};

struct WolfDrive {
  float stride_phase{0.0F};
  float speed_ratio{0.0F};
  float crouch{0.0F};
  float ear_pin{0.0F};
  float lunge{0.0F};
  float jaw_open{0.0F};

  float head_shake{0.0F};
  float collapse{0.0F};
  WolfGait gait{WolfGait::Stand};
};

[[nodiscard]] auto wolf_gait_advance(WolfGait gait) noexcept -> float;

[[nodiscard]] auto wolf_bind_pose() noexcept -> const RigPose&;
[[nodiscard]] auto wolf_pose(const WolfDrive& drive) noexcept -> RigPose;
[[nodiscard]] auto wolf_bind_palette() noexcept -> std::span<const QMatrix4x4>;
[[nodiscard]] auto
wolf_mesh_nodes() noexcept -> std::span<const Render::Creature::Quadruped::MeshNode>;
[[nodiscard]] auto wolf_minimal_mesh_nodes() noexcept
    -> std::span<const Render::Creature::Quadruped::MeshNode>;
[[nodiscard]] auto
wolf_creature_spec() noexcept -> const Render::Creature::CreatureSpec&;

} // namespace Render::Wildlife
