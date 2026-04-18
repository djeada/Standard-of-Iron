// Stage 16.2 — declarative PartGraph + primitive walker.
//
// A PartGraph is a flat list of `PrimitiveInstance`s, each anchored to
// one or two bones of a `SkeletonTopology`. The walker composes each
// primitive's model matrix from the bone palette and submits it via the
// standard `ISubmitter` interface. This is the single place where
// cylinder/sphere/capsule/cone/box math lives for creature rendering —
// per-species `rig.cpp` files no longer call `Render::Geom::cylinder_
// between()` directly.
//
// Authoring model
// ---------------
// Each primitive declares:
//   * `shape`              — which unit mesh to use (Sphere, Cylinder,
//                            Capsule, Cone, Box) or a `custom_mesh` handle.
//   * `anchor_bone` (and  — which bone(s) the primitive tracks. For
//      optional `tail_bone`) cylinder/capsule/cone the long axis goes
//                            from anchor→tail; for sphere/box only the
//                            anchor is used.
//   * `head_offset` /      — per-bone local offsets (in the bone's own
//     `tail_offset`          basis: +X right, +Y long axis, +Z forward),
//                            matching the SocketDef convention.
//   * `radius` / `half_extents` — shape dimensions.
//   * `color`, `material`, — pass-through to `ISubmitter::part()`. If
//     `alpha`, `material_id`  `material == nullptr` the walker degrades to
//                            `ISubmitter::mesh()` automatically.
//   * `lod_mask`           — bitmask of LOD bands that should submit this
//                            primitive. `kLodAll` means "always render".
//
// A single PartGraph is enough to describe one creature's mesh assembly;
// species modules own their own graph data and pass a `std::span` into
// the walker per frame.

#pragma once

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
  Mesh, // use PrimitiveInstance::custom_mesh
  // Stage 16.Fa — elliptical-cross-section cylinder between two
  // bones. Uses `params.radius` for the X radius (right axis)
  // and `params.depth_radius` for the Z radius (forward axis),
  // with the anchor bone's column(0) supplying the right
  // reference direction passed to Render::Geom::oriented_cylinder.
  OrientedCylinder,
  // Stage 16.Fb — ellipsoid aligned to the anchor bone's own
  // basis. `params.half_extents` give the three semi-axes in
  // (right, up, forward) order. The anchor bone must be
  // orthonormal; scale is applied to a unit sphere mesh.
  OrientedSphere,
};

// Level-of-detail band. One render path walks every primitive; a per-
// primitive bit mask decides whether it is included at a given band.
enum class CreatureLOD : std::uint8_t {
  Full = 0,
  Reduced = 1,
  Minimal = 2,
  Billboard = 3,
};

inline constexpr std::uint8_t kLodFull = 1U << 0;
inline constexpr std::uint8_t kLodReduced = 1U << 1;
inline constexpr std::uint8_t kLodMinimal = 1U << 2;
inline constexpr std::uint8_t kLodBillboard = 1U << 3;
inline constexpr std::uint8_t kLodAll =
    kLodFull | kLodReduced | kLodMinimal | kLodBillboard;

[[nodiscard]] constexpr auto lod_bit(CreatureLOD l) noexcept -> std::uint8_t {
  switch (l) {
  case CreatureLOD::Full:
    return kLodFull;
  case CreatureLOD::Reduced:
    return kLodReduced;
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

  // Offsets expressed in each referenced bone's own basis (X=right,
  // Y=long, Z=forward). Matches SocketDef convention.
  QVector3D head_offset{};
  QVector3D tail_offset{};

  // Dimensions.
  float radius{1.0F};
  // Stage 16.Fa — secondary radius along the bone's forward axis,
  // used by `OrientedCylinder`. For non-oriented shapes this is
  // ignored; the walker falls back to `radius`.
  float depth_radius{0.0F};
  QVector3D half_extents{1.0F, 1.0F, 1.0F};
};

struct PrimitiveInstance {
  std::string_view debug_name{};
  PrimitiveShape shape{PrimitiveShape::None};
  PrimitiveParams params{};

  // Only consulted when shape == PrimitiveShape::Mesh.
  Render::GL::Mesh *custom_mesh{nullptr};

  // Colour source. If `color_role == 0`, the walker uses the literal
  // `color` field. If `color_role > 0`, the walker indexes into the
  // `role_colors` span passed at submit time (index = color_role - 1);
  // this lets a static PartGraph declare "this is the Cloth colour"
  // without baking in a specific variant.
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

// Walk `graph` and submit every primitive whose lod_mask matches `lod`.
// Each primitive's model is composed as:
//   world_from_unit * primitive_model(palette[anchor], palette[tail], params)
//
// Preconditions:
//   * `palette.size() >= topology.bones.size()` — the walker reads bones
//     indexed by PrimitiveParams::{anchor_bone,tail_bone}.
//   * `topology` is only used for bone-count bounds checking; the walker
//     does NOT re-evaluate the skeleton.
//
// `role_colors` is the lookup table for primitives with non-zero
// `color_role`. An empty span is fine — any primitive with
// color_role > 0 then falls back to its literal `color` field.
//
// The returned stats are cumulative and useful for tests and profiling.
auto submit_part_graph(const SkeletonTopology &topology,
                       const PartGraph &graph,
                       std::span<const QMatrix4x4> palette, CreatureLOD lod,
                       const QMatrix4x4 &world_from_unit,
                       Render::GL::ISubmitter &out,
                       std::span<const QVector3D> role_colors = {})
    -> PartSubmissionStats;

// Debug-only: asserts every primitive's bone references are in range.
[[nodiscard]] auto validate_part_graph(const SkeletonTopology &topology,
                                       const PartGraph &graph) noexcept
    -> bool;

} // namespace Render::Creature
