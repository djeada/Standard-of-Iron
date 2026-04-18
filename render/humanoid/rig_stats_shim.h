#pragma once

// Internal plumbing for the Stage 2 humanoid rig split.
//
// rig.cpp holds file-scope statics (s_pose_cache, s_render_stats, etc.).
// When skin.cpp was extracted out of rig.cpp it still needs to bump one
// of those counters (facial-hair skipped-by-distance). Exposing the full
// HumanoidRenderStats across translation units would leak every render
// detail into skin.cpp; instead, a single narrowly-scoped shim is provided.
//
// Do NOT widen this header. If another counter needs to be bumped from
// another translation unit, add another free function with the same
// pattern rather than exposing the static itself.

namespace Render::GL::detail {

void increment_facial_hair_skipped_distance();

} // namespace Render::GL::detail
