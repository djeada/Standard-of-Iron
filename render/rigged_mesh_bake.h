// Stage 15.5a — identity-pose PartGraph → RiggedMesh bake.
//
// Walks a `PartGraph` under an identity bind pose and produces a
// CPU-side rigged vertex buffer that preserves enough bone information
// to drive GPU skinning in later sub-stages (15.5c+). Two entry points:
//
//   * `bake_rigged_mesh_cpu` — pure CPU; safe for headless unit tests.
//   * `bake_rigged_mesh`     — same, then wraps the CPU output in a
//                              `Render::GL::RiggedMesh` (GL upload is
//                              still lazy on first draw).
//
// Vertex authoring contract
// -------------------------
//   * `position_bone_local` / `normal_bone_local` are in each vertex's
//     anchor-bone local frame. Under the identity bind pose used here
//     the bone-local frame *is* world (bind = I → inverse_bind = I), so
//     numerically we just store `primitive_world * vertex_local`.
//
//   * Single-bone primitives (Sphere / Cone / Box / OrientedSphere /
//     Mesh): every vertex gets `bone_indices[0] = anchor_bone`,
//     `bone_weights = {1, 0, 0, 0}`.
//
//   * Two-bone primitives (Cylinder / Capsule / OrientedCylinder): each
//     vertex's position along the primitive's local +Y axis decides the
//     anchor↔tail blend. With the unit meshes spanning Y ∈ [-0.5, +0.5]
//     and `cylinder_between` orienting local +Y from anchor → tail:
//         t = clamp(vtx.local.y + 0.5, 0, 1)
//         bone_indices[0] = anchor, weights[0] = (1 - t)
//         bone_indices[1] = tail,   weights[1] =      t
//
// TODO(stage-15.5b): once real inverse-bind matrices exist, store
// `inverse_bind * primitive_world * vertex_local` instead of the bare
// world-space copy this stage emits. The `bind_pose` parameter is
// already in the signature so the caller doesn't need to change.

#pragma once

#include "creature/part_graph.h"
#include "rigged_mesh.h"

#include <QMatrix4x4>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace Render::Creature {

// One entry per bone in the skeleton. For this stage every entry is
// expected to be the identity matrix; the plumbing exists so future
// stages can pass real bind-pose world matrices without a signature
// change.
using BoneWorldMatrix = QMatrix4x4;

struct BakeInput {
  const PartGraph *graph{nullptr};
  std::span<const BoneWorldMatrix> bind_pose{};
};

struct BakedRiggedMeshCpu {
  std::vector<Render::GL::RiggedVertex> vertices;
  std::vector<std::uint32_t> indices;
};

// CPU-only bake. Produces an empty result (empty vectors) if the input
// is malformed (null graph, out-of-range bone refs, missing custom
// mesh, unsupported shape). A primitive that is entirely LOD-masked
// out is not skipped — this is a bake, not a per-frame submission, so
// all LODs are included.
[[nodiscard]] auto bake_rigged_mesh_cpu(const BakeInput &in)
    -> BakedRiggedMeshCpu;

// Convenience wrapper: bake on the CPU and move the result into a
// `RiggedMesh` instance. GL resources are still created lazily on the
// first `draw()` / `bind_vao()` call, so this is safe to invoke without
// a current GL context.
[[nodiscard]] auto bake_rigged_mesh(const BakeInput &in)
    -> std::unique_ptr<Render::GL::RiggedMesh>;

} // namespace Render::Creature
