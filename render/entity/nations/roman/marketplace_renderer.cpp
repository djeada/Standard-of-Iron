#include "render/entity/nations/roman/marketplace_renderer.h"

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
#include "render/entity/nations/roman/building_architecture.h"
#include "render/entity/nations/roman/building_palette.h"
#include "render/entity/nations/roman/marketplace_authoring.h"
#include "render/entity/registry.h"
#include "render/frame/draw_resources.h"
#include "render/frame/render_archetype.h"
#include "render/frame/submitter.h"
#include "render/geom/primitives.h"
#include "render/geom/transforms.h"

namespace Render::GL::Roman {

using namespace marketplace_detail;

namespace {

auto marketplace_palette_slots(const QVector3D& team) -> std::array<QVector3D, 1> {
  return {QVector3D(std::clamp(team.x(), 0.0F, 1.0F),
                    std::clamp(team.y(), 0.0F, 1.0F),
                    std::clamp(team.z(), 0.0F, 1.0F))};
}

auto marketplace_archetype(BuildingState state) -> const RenderArchetype& {
  static const BuildingArchetypeSet& k_set =
      building_archetype_set("roman_marketplace");
  return k_set.for_state(state);
}

const std::array<TorchMount, 4> k_torches{{
    TorchMount{.at = QVector3D(-1.18F, 0.66F, 0.52F),
               .outward = QVector3D(-1.0F, 0.0F, 0.0F)},
    TorchMount{.at = QVector3D(-1.18F, 0.66F, -0.52F),
               .outward = QVector3D(-1.0F, 0.0F, 0.0F)},
    TorchMount{.at = QVector3D(0.54F, 0.60F, 0.40F),
               .outward = QVector3D(-1.0F, 0.0F, 0.0F)},
    TorchMount{.at = QVector3D(0.54F, 0.60F, -0.40F),
               .outward = QVector3D(-1.0F, 0.0F, 0.0F)},
}};

const std::array<MarketAwning, 2> k_awnings{{
    MarketAwning{.back = QVector3D(-0.40F, 0.875F, 1.03F),
                 .front = QVector3D(-0.40F, 0.755F, 0.57F),
                 .width_axis = QVector3D(1.0F, 0.0F, 0.0F),
                 .half_width = 0.30F,
                 .stripe_a = QVector3D(0.60F, 0.10F, 0.07F),
                 .stripe_b = QVector3D(0.86F, 0.80F, 0.66F),
                 .stripes = 6},
    MarketAwning{.back = QVector3D(-0.40F, 0.875F, -1.03F),
                 .front = QVector3D(-0.40F, 0.755F, -0.57F),
                 .width_axis = QVector3D(1.0F, 0.0F, 0.0F),
                 .half_width = 0.30F,
                 .stripe_a = QVector3D(0.22F, 0.30F, 0.52F),
                 .stripe_b = QVector3D(0.86F, 0.80F, 0.66F),
                 .stripes = 6},
}};

const std::array<MarketHanging, 10> k_hangings{{
    MarketHanging{.pivot = QVector3D(0.55F, 0.76F, -0.98F),
                  .length = 0.14F,
                  .radius = 0.022F,
                  .color = QVector3D(0.86F, 0.82F, 0.70F),
                  .beads = 3},
    MarketHanging{.pivot = QVector3D(0.55F, 0.76F, -0.62F),
                  .length = 0.16F,
                  .radius = 0.024F,
                  .color = QVector3D(0.46F, 0.18F, 0.12F),
                  .beads = 3},
    MarketHanging{.pivot = QVector3D(0.55F, 0.76F, -0.20F),
                  .length = 0.12F,
                  .radius = 0.030F,
                  .color = QVector3D(0.30F, 0.36F, 0.12F),
                  .beads = 2},
    MarketHanging{.pivot = QVector3D(0.55F, 0.76F, 0.22F),
                  .length = 0.15F,
                  .radius = 0.022F,
                  .color = QVector3D(0.84F, 0.64F, 0.20F),
                  .beads = 3},
    MarketHanging{.pivot = QVector3D(-0.40F, 0.80F, 0.96F),
                  .length = 0.10F,
                  .radius = 0.020F,
                  .color = QVector3D(0.86F, 0.82F, 0.70F),
                  .beads = 2},
    MarketHanging{.pivot = QVector3D(-0.40F, 0.80F, -0.96F),
                  .length = 0.10F,
                  .radius = 0.024F,
                  .color = QVector3D(0.46F, 0.18F, 0.12F),
                  .beads = 2},
    MarketHanging{.pivot = QVector3D(-0.56F, 0.728F, 0.585F),
                  .length = 0.13F,
                  .radius = 0.019F,
                  .color = QVector3D(0.88F, 0.84F, 0.72F),
                  .beads = 4},
    MarketHanging{.pivot = QVector3D(-0.24F, 0.728F, 0.585F),
                  .length = 0.12F,
                  .radius = 0.022F,
                  .color = QVector3D(0.58F, 0.30F, 0.12F),
                  .beads = 3},
    MarketHanging{.pivot = QVector3D(-0.56F, 0.728F, -0.585F),
                  .length = 0.15F,
                  .radius = 0.021F,
                  .color = QVector3D(0.50F, 0.20F, 0.14F),
                  .beads = 3},
    MarketHanging{.pivot = QVector3D(-0.24F, 0.728F, -0.585F),
                  .length = 0.11F,
                  .radius = 0.024F,
                  .color = QVector3D(0.72F, 0.50F, 0.22F),
                  .beads = 2},
}};

const std::array<QVector3D, 5> k_buyer_a{QVector3D(-1.80F, 0.0F, -0.20F),
                                         QVector3D(-1.34F, 0.0F, -0.18F),
                                         QVector3D(-1.28F, 0.16F, -0.18F),
                                         QVector3D(-0.78F, 0.16F, -0.40F),
                                         QVector3D(-0.40F, 0.16F, -0.44F)};
const std::array<QVector3D, 5> k_buyer_b{QVector3D(-1.80F, 0.0F, 0.22F),
                                         QVector3D(-1.34F, 0.0F, 0.20F),
                                         QVector3D(-1.28F, 0.16F, 0.22F),
                                         QVector3D(0.06F, 0.16F, 0.34F),
                                         QVector3D(0.42F, 0.16F, 0.10F)};
const std::array<QVector3D, 4> k_stroll_route{QVector3D(-0.90F, 0.16F, 0.36F),
                                              QVector3D(0.05F, 0.16F, 0.32F),
                                              QVector3D(0.08F, 0.16F, -0.32F),
                                              QVector3D(-0.90F, 0.16F, -0.34F)};
const std::array<QVector3D, 5> k_porter_route{QVector3D(-1.80F, 0.0F, 0.40F),
                                              QVector3D(-1.34F, 0.0F, 0.40F),
                                              QVector3D(-1.28F, 0.16F, 0.40F),
                                              QVector3D(0.05F, 0.16F, 0.40F),
                                              QVector3D(0.46F, 0.16F, 0.62F)};
const std::array<QVector3D, 1> k_seller_a{QVector3D(-0.40F, 0.16F, 1.03F)};
const std::array<QVector3D, 1> k_seller_b{QVector3D(-0.40F, 0.16F, -1.03F)};
const std::array<QVector3D, 1> k_shopkeeper{QVector3D(0.86F, 0.16F, 0.02F)};
const std::array<QVector3D, 1> k_fountain_rest{QVector3D(-0.30F, 0.16F, 0.30F)};
const std::array<WalkSurface, 1> k_walk_surfaces{{
    WalkSurface{
        .min_x = -1.24F, .max_x = 1.24F, .min_z = -1.24F, .max_z = 1.24F, .top = 0.16F},
}};
const std::array<AmbientPerson, 8> k_people{{
    AmbientPerson{.role = AmbientRole::Stroll,
                  .route = k_buyer_a,
                  .facing = QVector3D(-0.40F, 0.4F, -0.80F),
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
                  .facing = QVector3D(0.67F, 0.35F, 0.79F)},
    AmbientPerson{.role = AmbientRole::Squat,
                  .route = k_fountain_rest,
                  .facing = QVector3D(-0.30F, 0.3F, 0.0F)},
}};

} // namespace

void register_marketplace_renderer(EntityRendererRegistry& registry) {
  register_marketplace_renderer_variant(
      registry,
      MarketplaceRendererConfig{.nation_slug = "roman",
                                .archetype = &marketplace_archetype,
                                .palette_slots = &marketplace_palette_slots,
                                .selection = BuildingSelectionStyle{1.8F, 1.8F},
                                .torches = k_torches,
                                .people = k_people,
                                .walk_surfaces = k_walk_surfaces,
                                .awnings = k_awnings,
                                .hangings = k_hangings});
}

} // namespace Render::GL::Roman
