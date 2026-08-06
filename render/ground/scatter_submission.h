#pragma once

#include "../scene_renderer.h"
#include "scatter_renderer_state.h"

namespace Render::Ground::Scatter {

template <typename Instance, typename Params>
void submit_visible_chunks(Render::GL::Renderer& renderer,
                           const FilteredRendererState<Instance, Params>& state,
                           Render::GL::TerrainScatterCmd command) {
  for (const auto& chunk : state.spatial_chunks) {
    if (chunk.visible_count == 0 || chunk.buffer == nullptr) {
      continue;
    }
    if (!renderer.submission_visibility().accepts_sphere(
            chunk.center, chunk.radius, Render::GL::SubmissionFogMode::Ignore)) {
      continue;
    }
    command.instance_buffer = chunk.buffer.get();
    command.instance_count = chunk.visible_count;
    renderer.terrain_scatter(command);
  }
}

template <typename Instance, typename Params, typename ChunkBudget>
void submit_visible_chunks_lod(Render::GL::Renderer& renderer,
                               const FilteredRendererState<Instance, Params>& state,
                               Render::GL::TerrainScatterCmd command,
                               ChunkBudget chunk_budget) {
  for (const auto& chunk : state.spatial_chunks) {
    if (chunk.visible_count == 0 || chunk.buffer == nullptr) {
      continue;
    }
    if (!renderer.submission_visibility().accepts_sphere(
            chunk.center, chunk.radius, Render::GL::SubmissionFogMode::Ignore)) {
      continue;
    }
    const std::size_t budget = chunk_budget(chunk.center, chunk.visible_count);
    if (budget == 0) {
      continue;
    }
    command.instance_buffer = chunk.buffer.get();
    command.instance_count = std::min(budget, chunk.visible_count);
    renderer.terrain_scatter(command);
  }
}

} // namespace Render::Ground::Scatter
