#pragma once

#include <QVector3D>

#include <span>
#include <string_view>

#include "ambient_people.h"
#include "barracks_activity.h"
#include "barracks_flag_renderer.h"
#include "building_render_common.h"
#include "building_state.h"
#include "building_torches.h"
#include "registry.h"
#include "render/render_archetype.h"

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
