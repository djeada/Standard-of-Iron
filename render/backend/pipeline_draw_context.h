#pragma once

#include "render/backend/gl_state_cache.h"
#include "render/frame/draw_queue.h"
#include "render/frame/pass/frame_context.h"
#include "render/gl/mesh_gpu_table.h"

namespace Render::GL {

// What a pipeline draws a run of queued commands with: the queue, the frame,
// the binding state shared by every pipeline in the frame, and the backend's
// GPU copies of the meshes the commands name.
struct PipelineDrawContext {
  const DrawQueue& queue;
  Render::Pass::FrameContext& frame;
  GlStateCache& state;
  MeshGpuTable& meshes;
};

} // namespace Render::GL
