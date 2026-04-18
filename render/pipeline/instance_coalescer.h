// Stage 9 — GPU-instancing coalescer.
//
// The scene_renderer emits one DrawPartCmd per limb/equipment part per unit.
// For large crowds this is O(units × parts) draw calls. Many of these share
// a (mesh, material, texture, material_id) tuple and differ only in the
// world matrix + tint — exactly the shape of a GL_INSTANCED draw call.
//
// This module is the stateless pre-pass that inspects the post-sort draw
// queue and groups adjacent compatible DrawPartCmds into an
// InstancedPartBatch describing a single future glDrawElementsInstanced
// call. The pass is pure data transformation — the backend decides whether
// to honour a batch as a true instanced call (fast path) or expand it back
// into N DrawPartCmds (safe fallback).
//
// Invariants honoured by the coalescer:
//   * DrawPartCmds are compatible iff they share
//     (mesh, material, texture, material_id, priority) AND carry no bone
//     palette (skinned draws can't share one glDrawElementsInstanced call
//     without a bone-palette-texture pipeline — that arrives in Stage 7).
//   * Runs shorter than `min_run` are left alone and passed through the
//     backend as individual DrawPartCmds. `min_run=4` is the default and
//     matches the point where GL instancing outperforms per-call submits
//     on typical Mesa + NV setups.
//   * Transparent parts (alpha < k_opaque_threshold) are never fused, since
//     correct back-to-front blending order would be broken.
//   * The coalescer MUST NOT reorder commands — only *group* them. It walks
//     the input once, emitting either a single InstancedPartBatch or an
//     unbatched span.

#pragma once

#include "../draw_part.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>
#include <span>
#include <vector>

namespace Render::GL {
struct DrawPartCmd;
} // namespace Render::GL

namespace Render::Pipeline {

struct InstanceEntry {
  QMatrix4x4 world;
  QVector3D color{1.0F, 1.0F, 1.0F};
  float alpha{1.0F};
};

// One fused run from the source command stream. `start` / `count` point into
// the caller's DrawPartCmd array; `instances` holds the per-instance data
// copied out for the instanced draw call. `header` captures the (mesh /
// material / texture / priority) tuple once — all members are stable
// pointers owned upstream.
struct InstancedPartBatch {
  const Render::GL::DrawPartCmd *header{nullptr};
  std::size_t start{0};
  std::size_t count{0};
  std::vector<InstanceEntry> instances;
};

struct CoalesceStats {
  std::size_t input_parts{0};
  std::size_t batched_parts{0}; // parts covered by any InstancedPartBatch
  std::size_t batch_count{0};
};

// Pure: walk `parts` once and emit batches for compatible adjacent runs of
// length >= `min_run`. Unbatched parts are implicitly those NOT covered by
// any returned batch's [start, start+count) range, so the caller can iterate
// batches + fall back to per-part draws for the gaps.
[[nodiscard]] auto
coalesce_instances(std::span<const Render::GL::DrawPartCmd> parts,
                   std::size_t min_run = 4) -> std::vector<InstancedPartBatch>;

// Helper so tests can verify "how many parts got batched" without scanning
// the returned span themselves.
[[nodiscard]] auto
compute_coalesce_stats(std::span<const InstancedPartBatch> batches,
                       std::size_t input_parts) -> CoalesceStats;

// Predicate: are two DrawPartCmds eligible to share a single
// glDrawElementsInstanced call? Exposed so tests and the backend can query
// the same rule.
[[nodiscard]] auto
parts_are_compatible(const Render::GL::DrawPartCmd &a,
                     const Render::GL::DrawPartCmd &b) noexcept -> bool;

} // namespace Render::Pipeline
