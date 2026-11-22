#pragma once

#include <string_view>

#include "render/entity/building_archetype_desc.h"
#include "render/entity/wall_gate_authoring.h"
#include "render/entity/wall_renderer_common.h"
#include "render/frame/render_archetype.h"
#include "render/frame/submitter.h"

namespace Render::GL {

auto wall_gate_archetype(std::string_view name_prefix) -> const BuildingArchetypeSet&;
auto wall_gate_leaf_archetype(std::string_view name_prefix)
    -> const BuildingArchetypeSet&;

void submit_wall_gate(ISubmitter& out,
                      const DrawContext& ctx,
                      const BuildingArchetypeSet& frame,
                      const BuildingArchetypeSet& leaf);

} // namespace Render::GL
