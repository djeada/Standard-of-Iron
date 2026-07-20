#pragma once

#include <QMatrix4x4>

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

#include "../creature/quadruped/mesh_graph.h"
#include "../creature/skeleton.h"

namespace Render::Elephant {

inline constexpr std::size_t k_elephant_source_bone_count = 32U;

inline constexpr float k_elephant_mesh_scale_x = 0.31625F;
inline constexpr float k_elephant_mesh_scale_y = 0.275F;
inline constexpr float k_elephant_mesh_scale_z = 0.1925F;
inline constexpr float k_elephant_mesh_ground_y = -3.3134756088F;

} // namespace Render::Elephant

namespace Render::GL {
struct HowdahAttachmentFrame;
}

namespace Render::Elephant {

struct ElephantSourceAssetStatus {
  bool loaded{false};
  std::size_t vertex_count{0U};
  std::size_t triangle_count{0U};
  std::size_t clip_count{0U};
  std::string error{};
};

[[nodiscard]] auto elephant_source_mesh_nodes() noexcept
    -> std::span<const Render::Creature::Quadruped::MeshNode>;
[[nodiscard]] auto
elephant_source_bind_palette() noexcept -> std::span<const QMatrix4x4>;
[[nodiscard]] auto
elephant_source_bone_defs() noexcept -> std::span<const Render::Creature::BoneDef>;
[[nodiscard]] auto
elephant_source_sample_clip(std::string_view source_clip,
                            float normalized_phase,
                            std::span<QMatrix4x4> out) noexcept -> bool;
[[nodiscard]] auto
elephant_source_pose_howdah(std::string_view source_clip,
                            float normalized_phase,
                            Render::GL::HowdahAttachmentFrame& frame) noexcept -> bool;
[[nodiscard]] auto
elephant_source_asset_status() noexcept -> const ElephantSourceAssetStatus&;

} // namespace Render::Elephant
