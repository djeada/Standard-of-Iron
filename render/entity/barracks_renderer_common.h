#pragma once

#include <QVector3D>

#include <span>
#include <string_view>

#include "render/frame/render_archetype.h"
#include "render/entity/ambient_people.h"
#include "render/entity/barracks_activity.h"
#include "render/entity/barracks_flag_renderer.h"
#include "render/entity/building_render_common.h"
#include "render/entity/building_state.h"
#include "render/entity/building_torches.h"
#include "render/entity/registry.h"

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
  BuildingSelectionStyle selection;
  std::span<const TorchMount> torches{};
  std::span<const AmbientPerson> people{};
  std::span<const WalkSurface> walk_surfaces{};
  std::span<const QVector3D> smoke_vents{};
  const BarracksGrounds* grounds{nullptr};
};

void register_barracks_renderer_variant(EntityRendererRegistry& registry,
                                        const BarracksRendererConfig& config);

} // namespace Render::GL
