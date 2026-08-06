#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <array>
#include <cstdint>
#include <span>

#include "../creature/quadruped/mesh_graph.h"
#include "../creature/spec.h"
#include "wildlife_rig.h"

namespace Render::Wildlife {

inline constexpr std::uint8_t k_sheep_role_wool = 1U;
inline constexpr std::uint8_t k_sheep_role_wool_light = 2U;
inline constexpr std::uint8_t k_sheep_role_wool_shade = 3U;
inline constexpr std::uint8_t k_sheep_role_wool_grubby = 4U;
inline constexpr std::uint8_t k_sheep_role_face = 5U;
inline constexpr std::uint8_t k_sheep_role_hoof = 6U;
inline constexpr std::uint8_t k_sheep_role_nose = 7U;
inline constexpr std::uint8_t k_sheep_role_eye = 8U;
inline constexpr std::size_t k_sheep_role_count = 8U;

enum class SheepGait : std::uint8_t {
  Stand = 0,
  Walk,
  Run
};

struct SheepDrive {
  float graze{0.0F};
  float stride_phase{0.0F};
  float speed_ratio{0.0F};
  float alert{0.0F};
  float collapse{0.0F};
  SheepGait gait{SheepGait::Stand};
};

// Ground the body covers per gait cycle. The renderer divides distance travelled by
// this to get the animation phase, which is what keeps planted hooves from sliding.
[[nodiscard]] auto sheep_gait_advance(SheepGait gait) noexcept -> float;

[[nodiscard]] auto sheep_bind_pose() noexcept -> const RigPose&;
[[nodiscard]] auto sheep_pose(const SheepDrive& drive) noexcept -> RigPose;
[[nodiscard]] auto sheep_bind_palette() noexcept -> std::span<const QMatrix4x4>;
[[nodiscard]] auto
sheep_mesh_nodes() noexcept -> std::span<const Render::Creature::Quadruped::MeshNode>;
[[nodiscard]] auto sheep_minimal_mesh_nodes() noexcept
    -> std::span<const Render::Creature::Quadruped::MeshNode>;
[[nodiscard]] auto
sheep_creature_spec() noexcept -> const Render::Creature::CreatureSpec&;

} // namespace Render::Wildlife
