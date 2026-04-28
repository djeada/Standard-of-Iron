#ifndef RENDER_CREATURE_PART_GRAPH_H
#define RENDER_CREATURE_PART_GRAPH_H

#include "skeleton.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cstdint>
#include <span>
#include <string_view>

namespace Render::GL {
class Mesh;
class ISubmitter;
struct Material;
} // namespace Render::GL

namespace Render::Creature {

enum class PrimitiveShape : std::uint8_t {
  None = 0,
  Sphere,
  Cylinder,
  Capsule,
  Cone,
  Box,
  Mesh,

  OrientedCylinder,

  OrientedSphere,
};

enum class MeshSkinning : std::uint8_t {
  Rigid = 0,
  HorseWhole,
  ElephantWhole,
};

enum class CreatureLOD : std::uint8_t {
  Full = 0,
  Minimal = 1,
  Billboard = 2,
};

inline constexpr std::uint8_t kLodFull = 1U << 0;
inline constexpr std::uint8_t kLodMinimal = 1U << 1;
inline constexpr std::uint8_t kLodBillboard = 1U << 2;
inline constexpr std::uint8_t kLodAll = kLodFull | kLodMinimal | kLodBillboard;

[[nodiscard]] constexpr auto lod_bit(CreatureLOD l) noexcept -> std::uint8_t {
  switch (l) {
  case CreatureLOD::Full:
    return kLodFull;
  case CreatureLOD::Minimal:
    return kLodMinimal;
  case CreatureLOD::Billboard:
    return kLodBillboard;
  }
  return 0;
}

struct PrimitiveParams {
  BoneIndex anchor_bone{kInvalidBone};
  BoneIndex tail_bone{kInvalidBone};

  QVector3D head_offset{};
  QVector3D tail_offset{};

  float radius{1.0F};

  float depth_radius{0.0F};
  QVector3D half_extents{1.0F, 1.0F, 1.0F};
};

struct PrimitiveInstance {
  std::string_view debug_name{};
  PrimitiveShape shape{PrimitiveShape::None};
  PrimitiveParams params{};

  Render::GL::Mesh *custom_mesh{nullptr};
  MeshSkinning mesh_skinning{MeshSkinning::Rigid};

  QVector3D color{1.0F, 1.0F, 1.0F};
  std::uint8_t color_role{0};

  Render::GL::Material *material{nullptr};
  float alpha{1.0F};
  int material_id{0};

  std::uint8_t lod_mask{kLodAll};
};

struct PartGraph {
  std::span<const PrimitiveInstance> primitives{};
};

struct PartSubmissionStats {
  std::uint32_t submitted{0};
  std::uint32_t skipped_lod{0};
  std::uint32_t skipped_invalid{0};
};

auto submit_part_graph(
    const SkeletonTopology &topology, const PartGraph &graph,
    std::span<const QMatrix4x4> palette, CreatureLOD lod,
    const QMatrix4x4 &world_from_unit, Render::GL::ISubmitter &out,
    std::span<const QVector3D> role_colors = {}) -> PartSubmissionStats;

[[nodiscard]] auto validate_part_graph(const SkeletonTopology &topology,
                                       const PartGraph &graph) noexcept -> bool;

} // namespace Render::Creature

#endif
