#include "render/entity/nations/roman/temple_renderer.h"

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
#include "render/entity/nations/roman/building_palette.h"
#include "render/entity/nations/roman/temple_authoring.h"
#include "render/entity/registry.h"
#include "render/entity/temple_renderer_common.h"
#include "render/frame/render_archetype.h"

namespace Render::GL::Roman {

using namespace temple_detail;

namespace {

auto temple_archetype(BuildingState state) -> const RenderArchetype& {
  static const BuildingArchetypeSet& k_set = building_archetype_set("roman_temple");
  return k_set.for_state(state);
}

const std::array<TorchMount, 6> k_torches{{
    TorchMount{.at = QVector3D(-0.36F, 2.06F, 0.96F),
               .outward = QVector3D(-1.0F, 0.0F, 0.0F)},
    TorchMount{.at = QVector3D(-0.36F, 2.06F, -0.96F),
               .outward = QVector3D(-1.0F, 0.0F, 0.0F)},
    TorchMount{.at = QVector3D(-2.344F, 2.06F, 1.52F),
               .outward = QVector3D(-1.0F, 0.0F, 0.0F)},
    TorchMount{.at = QVector3D(-2.344F, 2.06F, -1.52F),
               .outward = QVector3D(-1.0F, 0.0F, 0.0F)},
    TorchMount{.at = QVector3D(-1.66F, 0.984F, 1.2F),
               .outward = QVector3D(-1.0F, 0.0F, 0.0F),
               .brazier = true},
    TorchMount{.at = QVector3D(-1.66F, 0.984F, -1.2F),
               .outward = QVector3D(-1.0F, 0.0F, 0.0F),
               .brazier = true},
}};
const std::array<QVector3D, 2> k_priest_a{QVector3D(-0.6F, 0.0F, 1.02F),
                                          QVector3D(-2.45F, 0.0F, 1.02F)};
const std::array<QVector3D, 2> k_priest_b{QVector3D(-0.6F, 0.0F, -1.02F),
                                          QVector3D(-2.45F, 0.0F, -1.02F)};
const std::array<QVector3D, 1> k_kneel_a{QVector3D(-1.9F, 0.984F, 0.1F)};
const std::array<QVector3D, 2> k_pilgrim_route{QVector3D(-4.3F, 0.0F, 0.2F),
                                               QVector3D(-1.6F, 0.0F, -0.15F)};
const std::array<WalkSurface, 7> k_walk_surfaces{{
    WalkSurface{.min_x = -3.486F,
                .max_x = -3.314F,
                .min_z = -1.61F,
                .max_z = 1.61F,
                .top = 0.164F},
    WalkSurface{.min_x = -3.33F,
                .max_x = -3.158F,
                .min_z = -1.61F,
                .max_z = 1.61F,
                .top = 0.328F},
    WalkSurface{.min_x = -3.174F,
                .max_x = -3.002F,
                .min_z = -1.61F,
                .max_z = 1.61F,
                .top = 0.492F},
    WalkSurface{.min_x = -3.018F,
                .max_x = -2.846F,
                .min_z = -1.61F,
                .max_z = 1.61F,
                .top = 0.656F},
    WalkSurface{.min_x = -2.862F,
                .max_x = -2.69F,
                .min_z = -1.61F,
                .max_z = 1.61F,
                .top = 0.82F},
    WalkSurface{.min_x = -2.706F,
                .max_x = -2.534F,
                .min_z = -1.61F,
                .max_z = 1.61F,
                .top = 0.984F},
    WalkSurface{
        .min_x = -2.62F, .max_x = 2.76F, .min_z = -2.2F, .max_z = 2.2F, .top = 0.984F},
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
                  .facing = QVector3D(-0.3F, 1.2F, 0.1F)},
    AmbientPerson{.role = AmbientRole::Stroll,
                  .route = k_pilgrim_route,
                  .facing = QVector3D(-0.3F, 1.2F, 0.0F),
                  .linger = AmbientRole::Kneel},
}};
const std::array<QVector3D, 2> k_incense{QVector3D(-2.88F, 0.7F, 1.84F),
                                         QVector3D(-2.88F, 0.7F, -1.84F)};

} // namespace

void register_temple_renderer(EntityRendererRegistry& registry) {
  register_temple_renderer_variant(
      registry,
      TempleRendererConfig{.nation_slug = "roman",
                           .archetype = &temple_archetype,
                           .selection = BuildingSelectionStyle{3.8F, 3.8F},
                           .torches = k_torches,
                           .people = k_people,
                           .walk_surfaces = k_walk_surfaces,
                           .incense = k_incense});
}

} // namespace Render::GL::Roman
