#pragma once

#include <string_view>

#include "../render_archetype.h"
#include "../submitter.h"
#include "wall_renderer_common.h"

namespace Render::GL {

auto build_wall_gate_archetype(std::string_view name_prefix,
                               const WallPalette& palette,
                               const WallGeometry& geometry) -> RenderArchetype;

void submit_wall_gate(ISubmitter& out,
                      const DrawContext& ctx,
                      const RenderArchetype& frame,
                      const WallPalette& palette,
                      const WallGeometry& geometry);

} // namespace Render::GL
