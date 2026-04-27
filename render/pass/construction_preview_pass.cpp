#include "construction_preview_pass.h"

#include "../scene_renderer.h"
#include "frame_context.h"

#include "../../game/map/visibility_service.h"

namespace Render::Pass {

void ConstructionPreviewPass::execute(FrameContext &ctx) {
  if (ctx.renderer == nullptr || ctx.world == nullptr) {
    return;
  }
  ctx.renderer->render_construction_previews_public(ctx.world, ctx.visibility,
                                                    ctx.visibility_enabled);
}

} // namespace Render::Pass
