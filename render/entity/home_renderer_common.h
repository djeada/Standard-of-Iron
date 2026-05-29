#pragma once

#include <QVector3D>

#include <array>
#include <string_view>

#include "../render_archetype.h"
#include "building_render_common.h"
#include "building_state.h"
#include "registry.h"

namespace Render::GL {

using HomeArchetypeResolver = const RenderArchetype& (*)(BuildingState);
using HomePaletteSlotsResolver = std::array<QVector3D, 1> (*)(const QVector3D&);

struct HomeRendererConfig {
  std::string_view nation_slug;
  HomeArchetypeResolver archetype;
  HomePaletteSlotsResolver palette_slots;
  BuildingHealthBarStyle health_bar;
  BuildingSelectionStyle selection;
};

void register_home_renderer_variant(EntityRendererRegistry& registry,
                                    const HomeRendererConfig& config);

} // namespace Render::GL
