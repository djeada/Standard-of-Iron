#include "primitive_flush_pass.h"

#include "../draw_queue.h"
#include "../primitive_batch.h"
#include "frame_context.h"

namespace Render::Pass {

void PrimitiveFlushPass::execute(FrameContext &ctx) {
  auto *batcher = ctx.primitive_batcher;
  auto *queue = ctx.queue;
  if (batcher == nullptr || queue == nullptr) {
    return;
  }
  if (batcher->total_count() == 0) {
    return;
  }

  Render::GL::PrimitiveBatchParams params;
  params.view_proj = ctx.view_proj;

  if (batcher->sphere_count() > 0) {
    Render::GL::PrimitiveBatchCmd cmd;
    cmd.type = Render::GL::PrimitiveType::Sphere;
    cmd.instances = batcher->sphere_data();
    cmd.params = params;
    queue->submit(std::move(cmd));
  }

  if (batcher->cylinder_count() > 0) {
    Render::GL::PrimitiveBatchCmd cmd;
    cmd.type = Render::GL::PrimitiveType::Cylinder;
    cmd.instances = batcher->cylinder_data();
    cmd.params = params;
    queue->submit(std::move(cmd));
  }

  if (batcher->cone_count() > 0) {
    Render::GL::PrimitiveBatchCmd cmd;
    cmd.type = Render::GL::PrimitiveType::Cone;
    cmd.instances = batcher->cone_data();
    cmd.params = params;
    queue->submit(std::move(cmd));
  }
}

} // namespace Render::Pass
