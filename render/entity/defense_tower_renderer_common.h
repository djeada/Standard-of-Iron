#pragma once

#include <QVector3D>

#include <string_view>

#include "building_render_common.h"
#include "building_state.h"
#include "registry.h"
#include "render/render_archetype.h"

namespace Render::GL {

using DefenseTowerArchetypeResolver = const RenderArchetype& (*)(BuildingState);
using DefenseTowerBannerDrawer = void (*)(const DrawContext&,
                                          ISubmitter&,
                                          const QVector3D&,
                                          BuildingState);

struct DefenseTowerRendererConfig {
  std::string_view nation_slug;
  DefenseTowerArchetypeResolver archetype;
  DefenseTowerBannerDrawer draw_banner;
  BuildingSelectionStyle selection;
  float night_brazier_deck_y = 2.90F;
  float night_brazier_offset = 0.62F;
};

void register_defense_tower_renderer_variant(EntityRendererRegistry& registry,
                                             const DefenseTowerRendererConfig& config);

} // namespace Render::GL
