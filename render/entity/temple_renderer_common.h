#pragma once

#include <string_view>

#include "../render_archetype.h"
#include "building_render_common.h"
#include "building_state.h"
#include "registry.h"

namespace Render::GL {

using TempleArchetypeResolver = const RenderArchetype& (*)(BuildingState);

struct TempleRendererConfig {
  std::string_view nation_slug;
  TempleArchetypeResolver archetype;
  BuildingHealthBarStyle health_bar;
  BuildingSelectionStyle selection;
};

void register_temple_renderer_variant(EntityRendererRegistry& registry,
                                      const TempleRendererConfig& config);

} // namespace Render::GL
