#pragma once

#include <QVector3D>

#include <string_view>

#include "../render_archetype.h"
#include "building_render_common.h"
#include "building_state.h"
#include "registry.h"

namespace Render::GL {

using DefenseTowerArchetypeResolver = const RenderArchetype& (*)(BuildingState);
using DefenseTowerHealthStyleResolver = BuildingHealthBarStyle (*)(BuildingState);
using DefenseTowerBannerDrawer = void (*)(const DrawContext&,
                                          ISubmitter&,
                                          const QVector3D&,
                                          BuildingState);

struct DefenseTowerRendererConfig {
  std::string_view nation_slug;
  DefenseTowerArchetypeResolver archetype;
  DefenseTowerHealthStyleResolver health_bar_style;
  DefenseTowerBannerDrawer draw_banner;
  BuildingSelectionStyle selection;
};

void register_defense_tower_renderer_variant(EntityRendererRegistry& registry,
                                             const DefenseTowerRendererConfig& config);

} // namespace Render::GL
