#pragma once

#include "../scene_renderer.h"
#include "scatter_renderer_state.h"

namespace Render::Ground::Scatter {

template <typename Instance, typename Params>
void submit_visible_chunks(
    Render::GL::Renderer& renderer,
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

} // namespace Render::Ground::Scatter
