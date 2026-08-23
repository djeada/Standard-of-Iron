#pragma once

#include <string_view>

#include "building_render_common.h"
#include "building_state.h"
#include "registry.h"
#include "render/render_archetype.h"

namespace Render::GL {

using TempleArchetypeResolver = const RenderArchetype& (*)(BuildingState);

struct TempleRendererConfig {
  std::string_view nation_slug;
  TempleArchetypeResolver archetype;
  BuildingSelectionStyle selection;
};

void register_temple_renderer_variant(EntityRendererRegistry& registry,
                                      const TempleRendererConfig& config);

} // namespace Render::GL
