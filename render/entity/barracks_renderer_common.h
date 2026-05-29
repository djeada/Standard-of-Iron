#pragma once

#include <QVector3D>

#include <string_view>

#include "../render_archetype.h"
#include "barracks_flag_renderer.h"
#include "building_render_common.h"
#include "building_state.h"
#include "registry.h"

namespace Render::GL {
class Mesh;
class Texture;

using BarracksArchetypeResolver = const RenderArchetype& (*)(BuildingState,
                                                             Mesh*,
                                                             Texture*);
using BarracksOrnamentDrawer =
    void (*)(const DrawContext&,
             ISubmitter&,
             Mesh*,
             Texture*,
             const QVector3D&,
             const BarracksFlagRenderer::ClothBannerResources*);

struct BarracksRendererConfig {
  std::string_view nation_slug;
  BarracksArchetypeResolver archetype;
  BarracksOrnamentDrawer draw_ornaments;
  BuildingHealthBarStyle health_bar;
  BuildingSelectionStyle selection;
};

void register_barracks_renderer_variant(EntityRendererRegistry& registry,
                                        const BarracksRendererConfig& config);

} // namespace Render::GL
