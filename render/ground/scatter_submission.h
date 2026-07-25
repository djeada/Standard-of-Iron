#pragma once

#include "../scene_renderer.h"
#include "scatter_renderer_state.h"

namespace Render::Ground::Scatter {

template <typename Instance, typename Params>
void submit_visible_chunks(Render::GL::Renderer& renderer,
                           const FilteredRendererState<Instance, Params>& state,
                           Render::GL::TerrainScatterCmd command) {
  for (const auto& chunk : state.spatial_chunks) {
    if (chunk.instances.empty() || chunk.buffer == nullptr) {
      continue;
    }
    if (!renderer.submission_visibility().accepts_sphere(
            chunk.center, chunk.radius, Render::GL::SubmissionFogMode::Ignore)) {
      continue;
    }
    command.instance_buffer = chunk.buffer.get();
    command.instance_count = chunk.instances.size();
    renderer.terrain_scatter(command);
  }
}

// As above, but each surviving chunk is asked how many of its instances are
// still worth drawing. Frustum culling alone bounds nothing when the camera
// pulls back: the whole map stays in view and every instance is still
// transformed, however little of it reaches a pixel.
template <typename Instance, typename Params, typename ChunkBudget>
void submit_visible_chunks_lod(Render::GL::Renderer& renderer,
                               const FilteredRendererState<Instance, Params>& state,
                               Render::GL::TerrainScatterCmd command,
                               ChunkBudget chunk_budget) {
  for (const auto& chunk : state.spatial_chunks) {
    if (chunk.instances.empty() || chunk.buffer == nullptr) {
      continue;
    }
    if (!renderer.submission_visibility().accepts_sphere(
            chunk.center, chunk.radius, Render::GL::SubmissionFogMode::Ignore)) {
      continue;
    }
    const std::size_t budget = chunk_budget(chunk.center, chunk.instances.size());
    if (budget == 0) {
      continue;
    }
    command.instance_buffer = chunk.buffer.get();
    command.instance_count = std::min(budget, chunk.instances.size());
    renderer.terrain_scatter(command);
  }
}

} // namespace Render::Ground::Scatter
