#pragma once

#include <QMatrix4x4>

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

#include "../creature/quadruped/mesh_graph.h"
#include "../creature/skeleton.h"

namespace Render::GL {
struct MountedAttachmentFrame;
}

namespace Render::Horse {

inline constexpr std::size_t k_horse_source_bone_count = 50U;
inline constexpr float k_horse_length_scale = 0.85F;
inline constexpr float k_horse_mesh_scale_x = 0.59F;
inline constexpr float k_horse_mesh_scale_y = 0.59F;
inline constexpr float k_horse_mesh_scale_z = 0.5015F;
inline constexpr float k_horse_mesh_ground_y = -0.0112192873F;

struct HorseSourceAssetStatus {
  bool loaded{false};
  std::size_t vertex_count{0U};
  std::size_t triangle_count{0U};
  std::size_t clip_count{0U};
  std::string error{};
};

[[nodiscard]] auto horse_source_mesh_nodes() noexcept
    -> std::span<const Render::Creature::Quadruped::MeshNode>;

[[nodiscard]] auto horse_source_bind_palette() noexcept -> std::span<const QMatrix4x4>;

[[nodiscard]] auto
horse_source_bone_defs() noexcept -> std::span<const Render::Creature::BoneDef>;

[[nodiscard]] auto horse_source_sample_clip(std::string_view source_clip,
                                            float normalized_phase,
                                            std::span<QMatrix4x4> out) noexcept -> bool;

[[nodiscard]] auto horse_source_pose_mount_frame(
    std::string_view source_clip,
    float normalized_phase,
    Render::GL::MountedAttachmentFrame& frame) noexcept -> bool;

[[nodiscard]] auto
horse_source_asset_status() noexcept -> const HorseSourceAssetStatus&;

} // namespace Render::Horse
