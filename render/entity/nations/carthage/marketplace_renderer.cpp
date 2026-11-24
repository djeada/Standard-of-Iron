#include "render/entity/nations/carthage/marketplace_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "game/core/component.h"
#include "game/visuals/team_colors.h"
#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_archetype_library.h"
#include "render/entity/building_decay.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/building_render_common.h"
#include "render/entity/building_state.h"
#include "render/entity/marketplace_renderer_common.h"
#include "render/entity/nations/carthage/building_palette.h"
#include "render/entity/nations/carthage/marketplace_authoring.h"
#include "render/entity/registry.h"
#include "render/frame/draw_resources.h"
#include "render/frame/render_archetype.h"
#include "render/frame/submitter.h"
#include "render/geom/primitives.h"
#include "render/geom/transforms.h"

namespace Render::GL::Carthage {

using namespace marketplace_detail;

namespace {

auto marketplace_palette_slots(const QVector3D& team) -> std::array<QVector3D, 1> {
  return {QVector3D(std::clamp(team.x(), 0.0F, 1.0F),
                    std::clamp(team.y(), 0.0F, 1.0F),
                    std::clamp(team.z(), 0.0F, 1.0F))};
}

auto marketplace_archetype(BuildingState state) -> const RenderArchetype& {
  static const BuildingArchetypeSet& k_set =
      building_archetype_set("carthage_marketplace");
  return k_set.for_state(state);
}

const std::array<TorchMount, 4> k_torches{{
    TorchMount{.at = QVector3D(-1.22F, 0.64F, 0.50F),
               .outward = QVector3D(-1.0F, 0.0F, 0.0F),
               .ghost = true},
    TorchMount{.at = QVector3D(-1.22F, 0.64F, -0.50F),
               .outward = QVector3D(-1.0F, 0.0F, 0.0F),
               .ghost = true},
    TorchMount{.at = QVector3D(0.54F, 0.58F, 0.40F),
               .outward = QVector3D(-1.0F, 0.0F, 0.0F),
               .ghost = true},
    TorchMount{.at = QVector3D(0.54F, 0.58F, -0.40F),
               .outward = QVector3D(-1.0F, 0.0F, 0.0F),
               .ghost = true},
}};

const std::array<MarketAwning, 7> k_awnings{{
    MarketAwning{.back = QVector3D(-0.40F, 0.855F, 1.05F),
                 .front = QVector3D(-0.40F, 0.735F, 0.59F),
                 .width_axis = QVector3D(1.0F, 0.0F, 0.0F),
                 .half_width = 0.30F,
                 .stripe_a = QVector3D(0.16F, 0.20F, 0.46F),
                 .stripe_b = QVector3D(0.80F, 0.56F, 0.16F),
                 .stripes = 5},
    MarketAwning{.back = QVector3D(-0.40F, 0.855F, -1.05F),
                 .front = QVector3D(-0.40F, 0.735F, -0.59F),
                 .width_axis = QVector3D(1.0F, 0.0F, 0.0F),
                 .half_width = 0.30F,
                 .stripe_a = QVector3D(0.52F, 0.10F, 0.08F),
                 .stripe_b = QVector3D(0.86F, 0.78F, 0.62F),
                 .stripes = 5},
    MarketAwning{.back = QVector3D(-1.02F, 0.785F, -0.78F),
                 .front = QVector3D(-1.01F, 0.52F, -0.78F),
                 .width_axis = QVector3D(1.0F, 0.0F, 0.0F),
                 .half_width = 0.09F,
                 .stripe_a = QVector3D(0.38F, 0.08F, 0.28F),
                 .stripe_b = QVector3D(0.44F, 0.10F, 0.32F),
                 .stripes = 2},
    MarketAwning{.back = QVector3D(-0.84F, 0.785F, -0.78F),
                 .front = QVector3D(-0.83F, 0.56F, -0.78F),
                 .width_axis = QVector3D(1.0F, 0.0F, 0.0F),
                 .half_width = 0.08F,
                 .stripe_a = QVector3D(0.30F, 0.06F, 0.24F),
                 .stripe_b = QVector3D(0.36F, 0.08F, 0.28F),
                 .stripes = 2},
    MarketAwning{.back = QVector3D(-0.69F, 0.785F, -0.78F),
                 .front = QVector3D(-0.68F, 0.54F, -0.78F),
                 .width_axis = QVector3D(1.0F, 0.0F, 0.0F),
                 .half_width = 0.06F,
                 .stripe_a = QVector3D(0.16F, 0.20F, 0.46F),
                 .stripe_b = QVector3D(0.20F, 0.24F, 0.52F),
                 .stripes = 2},
    MarketAwning{.back = QVector3D(0.535F, 0.735F, -0.80F),
                 .front = QVector3D(0.52F, 0.63F, -0.80F),
                 .width_axis = QVector3D(0.0F, 0.0F, 1.0F),
                 .half_width = 0.32F,
                 .stripe_a = QVector3D(0.52F, 0.10F, 0.08F),
                 .stripe_b = QVector3D(0.60F, 0.14F, 0.10F),
                 .stripes = 4},
    MarketAwning{.back = QVector3D(0.535F, 0.735F, 0.80F),
                 .front = QVector3D(0.52F, 0.63F, 0.80F),
                 .width_axis = QVector3D(0.0F, 0.0F, 1.0F),
                 .half_width = 0.32F,
                 .stripe_a = QVector3D(0.16F, 0.20F, 0.46F),
                 .stripe_b = QVector3D(0.80F, 0.56F, 0.16F),
                 .stripes = 4},
}};

const std::array<MarketHanging, 8> k_hangings{{
    MarketHanging{.pivot = QVector3D(-0.40F, 0.78F, 1.00F),
                  .length = 0.11F,
                  .radius = 0.022F,
                  .color = QVector3D(0.80F, 0.56F, 0.16F),
                  .beads = 3},
    MarketHanging{.pivot = QVector3D(-0.40F, 0.78F, -1.00F),
                  .length = 0.11F,
                  .radius = 0.024F,
                  .color = QVector3D(0.54F, 0.10F, 0.04F),
                  .beads = 3},
    MarketHanging{.pivot = QVector3D(0.55F, 0.73F, -0.20F),
                  .length = 0.13F,
                  .radius = 0.026F,
                  .color = QVector3D(0.20F, 0.29F, 0.09F),
                  .beads = 2},
    MarketHanging{.pivot = QVector3D(0.55F, 0.73F, 0.20F),
                  .length = 0.13F,
                  .radius = 0.022F,
                  .color = QVector3D(0.66F, 0.38F, 0.09F),
                  .beads = 3},
    MarketHanging{.pivot = QVector3D(-0.56F, 0.708F, 0.605F),
                  .length = 0.14F,
                  .radius = 0.018F,
                  .color = QVector3D(0.54F, 0.10F, 0.04F),
                  .beads = 4},
    MarketHanging{.pivot = QVector3D(-0.24F, 0.708F, 0.605F),
                  .length = 0.12F,
                  .radius = 0.020F,
                  .color = QVector3D(0.20F, 0.29F, 0.09F),
                  .beads = 3},
    MarketHanging{.pivot = QVector3D(-0.56F, 0.708F, -0.605F),
                  .length = 0.13F,
                  .radius = 0.022F,
                  .color = QVector3D(0.40F, 0.18F, 0.08F),
                  .beads = 3},
    MarketHanging{.pivot = QVector3D(-0.24F, 0.708F, -0.605F),
                  .length = 0.15F,
                  .radius = 0.019F,
                  .color = QVector3D(0.86F, 0.82F, 0.72F),
                  .beads = 4},
}};

const std::array<QVector3D, 5> k_buyer_a{QVector3D(-1.80F, 0.0F, -0.20F),
                                         QVector3D(-1.40F, 0.0F, -0.18F),
                                         QVector3D(-1.30F, 0.14F, -0.18F),
                                         QVector3D(-0.78F, 0.14F, -0.40F),
                                         QVector3D(-0.40F, 0.14F, -0.46F)};
const std::array<QVector3D, 5> k_buyer_b{QVector3D(-1.80F, 0.0F, 0.22F),
                                         QVector3D(-1.40F, 0.0F, 0.20F),
                                         QVector3D(-1.30F, 0.14F, 0.22F),
                                         QVector3D(0.06F, 0.14F, 0.34F),
                                         QVector3D(0.42F, 0.14F, 0.10F)};
const std::array<QVector3D, 4> k_stroll_route{QVector3D(-0.90F, 0.14F, 0.36F),
                                              QVector3D(0.05F, 0.14F, 0.32F),
                                              QVector3D(0.08F, 0.14F, -0.34F),
                                              QVector3D(-0.44F, 0.14F, -0.36F)};
const std::array<QVector3D, 5> k_porter_route{QVector3D(-1.80F, 0.0F, 0.38F),
                                              QVector3D(-1.40F, 0.0F, 0.38F),
                                              QVector3D(-1.30F, 0.14F, 0.38F),
                                              QVector3D(0.05F, 0.14F, 0.40F),
                                              QVector3D(0.46F, 0.14F, 0.64F)};
const std::array<QVector3D, 1> k_seller_a{QVector3D(-0.40F, 0.14F, 1.05F)};
const std::array<QVector3D, 1> k_seller_b{QVector3D(-0.40F, 0.14F, -1.05F)};
const std::array<QVector3D, 1> k_shopkeeper{QVector3D(0.86F, 0.14F, 0.02F)};
const std::array<QVector3D, 1> k_dyer{QVector3D(-0.86F, 0.14F, -0.90F)};
const std::array<WalkSurface, 1> k_walk_surfaces{{
    WalkSurface{
        .min_x = -1.28F, .max_x = 1.28F, .min_z = -1.28F, .max_z = 1.28F, .top = 0.14F},
}};
const std::array<AmbientPerson, 8> k_people{{
    AmbientPerson{.role = AmbientRole::Stroll,
                  .route = k_buyer_a,
                  .facing = QVector3D(-0.40F, 0.4F, -0.82F),
                  .linger = AmbientRole::Haggle},
    AmbientPerson{.role = AmbientRole::Stroll,
                  .route = k_buyer_b,
                  .facing = QVector3D(0.67F, 0.4F, 0.0F),
                  .linger = AmbientRole::Haggle},
    AmbientPerson{.role = AmbientRole::Haggle,
                  .route = k_seller_a,
                  .facing = QVector3D(-0.40F, 0.3F, 0.40F)},
    AmbientPerson{.role = AmbientRole::Weave,
                  .route = k_seller_b,
                  .facing = QVector3D(-0.40F, 0.3F, -0.40F)},
    AmbientPerson{.role = AmbientRole::Haggle,
                  .route = k_shopkeeper,
                  .facing = QVector3D(0.30F, 0.4F, 0.0F)},
    AmbientPerson{.role = AmbientRole::Stroll,
                  .route = k_stroll_route,
                  .facing = QVector3D(-0.30F, 0.3F, 0.0F),
                  .linger = AmbientRole::Weave},
    AmbientPerson{.role = AmbientRole::Porter,
                  .route = k_porter_route,
                  .facing = QVector3D(0.67F, 0.35F, 0.80F)},
    AmbientPerson{.role = AmbientRole::Squat,
                  .route = k_dyer,
                  .facing = QVector3D(-0.86F, 0.2F, -1.02F)},
}};

} // namespace

void register_marketplace_renderer(EntityRendererRegistry& registry) {
  register_marketplace_renderer_variant(
      registry,
      MarketplaceRendererConfig{.nation_slug = "carthage",
                                .archetype = &marketplace_archetype,
                                .palette_slots = &marketplace_palette_slots,
                                .selection = BuildingSelectionStyle{1.7F, 1.7F},
                                .torches = k_torches,
                                .people = k_people,
                                .walk_surfaces = k_walk_surfaces,
                                .awnings = k_awnings,
                                .hangings = k_hangings});
}

} // namespace Render::GL::Carthage
