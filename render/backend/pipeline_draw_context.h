#pragma once

#include "render/backend/gl_state_cache.h"
#include "render/frame/draw_queue.h"
#include "render/frame/pass/frame_context.h"
#include "render/gl/instance_buffer_table.h"
#include "render/gl/mesh_gpu_table.h"
#include "render/gl/texture_table.h"

namespace Render::GL {

struct PipelineDrawContext {
  const DrawQueue& queue;
  Render::Pass::FrameContext& frame;
  GlStateCache& state;
  MeshGpuTable& meshes;
  InstanceBufferTable& instances;
  TextureTable& textures;
};

} // namespace Render::GL
