#include "render/entity/nations/carthage/temple_renderer.h"

#include <QVector3D>

#include <array>
#include <cmath>
#include <cstdint>

#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_archetype_library.h"
#include "render/entity/building_decay.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/building_render_common.h"
#include "render/entity/building_state.h"
#include "render/entity/nations/carthage/building_palette.h"
#include "render/entity/nations/carthage/temple_authoring.h"
#include "render/entity/registry.h"
#include "render/entity/temple_renderer_common.h"
#include "render/frame/render_archetype.h"

namespace Render::GL::Carthage {

using namespace temple_detail;

namespace {

auto temple_archetype(BuildingState state) -> const RenderArchetype& {
  static const BuildingArchetypeSet& k_set = building_archetype_set("carthage_temple");
  return k_set.for_state(state);
}

const std::array<TorchMount, 6> k_torches{{
    TorchMount{.at = QVector3D(-1.772F, 2.0F, 1.6F),
               .outward = QVector3D(-1.0F, 0.0F, 0.0F),
               .ghost = true},
    TorchMount{.at = QVector3D(-1.772F, 2.0F, -1.6F),
               .outward = QVector3D(-1.0F, 0.0F, 0.0F),
               .ghost = true},
    TorchMount{.at = QVector3D(-0.556F, 2.0F, 1.08F),
               .outward = QVector3D(-1.0F, 0.0F, 0.0F),
               .ghost = true},
    TorchMount{.at = QVector3D(-0.556F, 2.0F, -1.08F),
               .outward = QVector3D(-1.0F, 0.0F, 0.0F),
               .ghost = true},
    TorchMount{.at = QVector3D(-1.5F, 0.98F, 1.04F),
               .outward = QVector3D(-1.0F, 0.0F, 0.0F),
               .brazier = true,
               .ghost = true},
    TorchMount{.at = QVector3D(-1.5F, 0.98F, -1.04F),
               .outward = QVector3D(-1.0F, 0.0F, 0.0F),
               .brazier = true,
               .ghost = true},
}};
const std::array<QVector3D, 2> k_priest_a{QVector3D(-0.8F, 0.0F, 0.2F),
                                          QVector3D(-2.3F, 0.0F, 0.2F)};
const std::array<QVector3D, 2> k_priest_b{QVector3D(-0.8F, 0.0F, -0.2F),
                                          QVector3D(-2.3F, 0.0F, -0.2F)};
const std::array<QVector3D, 1> k_kneel_a{QVector3D(-2.04F, 1.06F, 0.05F)};
const std::array<QVector3D, 2> k_pilgrim_route{QVector3D(-4.3F, 0.0F, -0.3F),
                                               QVector3D(-2.0F, 0.0F, -0.7F)};
const std::array<WalkSurface, 9> k_walk_surfaces{{
    WalkSurface{.min_x = -3.486F,
                .max_x = -3.314F,
                .min_z = -1.33F,
                .max_z = 1.33F,
                .top = 0.164F},
    WalkSurface{.min_x = -3.33F,
                .max_x = -3.158F,
                .min_z = -1.33F,
                .max_z = 1.33F,
                .top = 0.328F},
    WalkSurface{.min_x = -3.174F,
                .max_x = -3.002F,
                .min_z = -1.33F,
                .max_z = 1.33F,
                .top = 0.492F},
    WalkSurface{.min_x = -3.018F,
                .max_x = -2.846F,
                .min_z = -1.33F,
                .max_z = 1.33F,
                .top = 0.656F},
    WalkSurface{.min_x = -2.862F,
                .max_x = -2.69F,
                .min_z = -1.33F,
                .max_z = 1.33F,
                .top = 0.82F},
    WalkSurface{.min_x = -2.706F,
                .max_x = -2.534F,
                .min_z = -1.33F,
                .max_z = 1.33F,
                .top = 0.984F},
    WalkSurface{
        .min_x = -2.62F, .max_x = 2.76F, .min_z = -2.32F, .max_z = 2.32F, .top = 0.98F},
    WalkSurface{.min_x = -2.64F,
                .max_x = -1.44F,
                .min_z = -1.12F,
                .max_z = 1.12F,
                .top = 1.076F},
    WalkSurface{
        .min_x = -2.64F, .max_x = -1.44F, .min_z = -0.3F, .max_z = 0.3F, .top = 1.08F},
}};
const std::array<AmbientPerson, 4> k_people{{
    AmbientPerson{.role = AmbientRole::Stroll,
                  .route = k_priest_a,
                  .facing = QVector3D(0.4F, 1.4F, 0.4F),
                  .linger = AmbientRole::Kneel,
                  .priest = true},
    AmbientPerson{.role = AmbientRole::Stroll,
                  .route = k_priest_b,
                  .facing = QVector3D(0.4F, 1.4F, -0.4F),
                  .linger = AmbientRole::Kneel,
                  .priest = true},
    AmbientPerson{.role = AmbientRole::Kneel,
                  .route = k_kneel_a,
                  .facing = QVector3D(-0.3F, 1.2F, 0.05F)},
    AmbientPerson{.role = AmbientRole::Stroll,
                  .route = k_pilgrim_route,
                  .facing = QVector3D(-0.3F, 1.2F, 0.0F),
                  .linger = AmbientRole::Kneel},
}};
const std::array<QVector3D, 2> k_incense{QVector3D(-2.84F, 0.9F, 1.88F),
                                         QVector3D(-2.84F, 0.9F, -1.88F)};

} // namespace

void register_temple_renderer(EntityRendererRegistry& registry) {
  register_temple_renderer_variant(
      registry,
      TempleRendererConfig{.nation_slug = "carthage",
                           .archetype = &temple_archetype,
                           .selection = BuildingSelectionStyle{3.8F, 3.8F},
                           .torches = k_torches,
                           .people = k_people,
                           .walk_surfaces = k_walk_surfaces,
                           .incense = k_incense});
}

} // namespace Render::GL::Carthage
