#include "render/entity/nations/carthage/barracks_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>

#include "game/core/component.h"
#include "game/visuals/team_colors.h"
#include "render/entity/barracks_flag_renderer.h"
#include "render/entity/barracks_renderer_common.h"
#include "render/entity/barracks_stockpile.h"
#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_archetype_library.h"
#include "render/entity/building_decay.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/building_render_common.h"
#include "render/entity/building_state.h"
#include "render/entity/nations/carthage/barracks_authoring.h"
#include "render/entity/nations/carthage/building_palette.h"
#include "render/entity/registry.h"
#include "render/frame/draw_resources.h"
#include "render/frame/render_archetype.h"
#include "render/frame/submitter.h"
#include "render/geom/primitives.h"
#include "render/math/math_utils.h"

namespace Render::GL::Carthage {

using namespace barracks_detail;

namespace {

auto barracks_archetype(BuildingState state,
                        Mesh*,
                        Texture*) -> const RenderArchetype& {
  static const BuildingArchetypeSet& k_set =
      building_archetype_set("carthage_barracks");
  return k_set.for_state(state);
}

void draw_standards(const DrawContext& p,
                    ISubmitter& out,
                    Mesh* unit,
                    Texture* white,
                    const CarthagePalette& c,
                    const BarracksFlagRenderer::ClothBannerResources* cloth) {
  BarracksFlagRenderer::draw_hanging_banner(
      p,
      out,
      unit,
      white,
      c.team,
      c.team_trim,
      {.pole_base = QVector3D(-1.70F, 0.0F, 1.34F),
       .pole_height = 2.9F,
       .pole_radius = 0.045F,
       .banner_width = 0.8F,
       .banner_height = 0.5F,
       .pole_color = c.wood,
       .beam_color = c.wood,
       .connector_color = c.sandstone_light,
       .ornament_offset = QVector3D(0.0F, 3.1F, 0.0F),
       .ornament_size = QVector3D(0.12F, 0.10F, 0.12F),
       .ornament_color = c.bronze,
       .ring_count = 3,
       .ring_y_start = 0.5F,
       .ring_spacing = 0.6F,
       .ring_height = 0.03F,
       .ring_radius_scale = 2.2F,
       .ring_color = c.bronze},
      cloth);
}

void draw_rally_flag(const DrawContext& p,
                     ISubmitter& out,
                     Texture* white,
                     const CarthagePalette& c,
                     const BarracksFlagRenderer::ClothBannerResources* cloth) {
  BarracksFlagRenderer::FlagColors const colors{.team = c.team,
                                                .team_trim = c.team_trim,
                                                .timber = c.wood,
                                                .timber_light = c.sandstone_light,
                                                .wood_dark = c.wood_dark};
  BarracksFlagRenderer::draw_rally_flag_if_any(p, out, white, colors, cloth);
}

void draw_stockpile_yard(const DrawContext& p,
                         ISubmitter& out,
                         Mesh* unit,
                         Texture* white,
                         const CarthagePalette& c) {
  draw_barracks_stockpile(
      p,
      out,
      unit,
      white,
      StockpileYardStyle{.gravel = QVector3D(0.58F, 0.49F, 0.36F),
                         .earth_light = QVector3D(0.69F, 0.60F, 0.45F),
                         .stone_light = c.sandstone_light,
                         .stone_mid = c.sandstone,
                         .stone_dark = c.stone_dark,
                         .timber = c.wood,
                         .timber_dark = c.wood_dark,
                         .ore = c.iron * 1.55F,
                         .ore_rust = c.brick_dark});
}

void draw_barracks_ornaments(const DrawContext& p,
                             ISubmitter& out,
                             Mesh* unit,
                             Texture* white,
                             const QVector3D& team,
                             const BarracksFlagRenderer::ClothBannerResources* cloth) {
  CarthagePalette const c = make_palette(team);
  static const BuildingArchetypeSet& k_grounds_set =
      building_archetype_set("carthage_barracks_grounds");
  submit_building_instance(out, p, k_grounds_set.for_state(resolve_building_state(p)));
  draw_stockpile_yard(p, out, unit, white, c);
  draw_standards(p, out, unit, white, c, cloth);
  draw_rally_flag(p, out, white, c, cloth);
}

const std::array<TorchMount, 4> k_torches{{
    TorchMount{.at = QVector3D(-0.53F, 0.82F, 1.185F),
               .outward = QVector3D(0.0F, 0.0F, 1.0F),
               .ghost = true},
    TorchMount{.at = QVector3D(0.53F, 0.82F, 1.185F),
               .outward = QVector3D(0.0F, 0.0F, 1.0F),
               .ghost = true},
    TorchMount{.at = QVector3D(-1.28F, 0.9F, 0.92F),
               .outward = QVector3D(0.0F, 0.0F, 1.0F),
               .ghost = true},
    TorchMount{.at = QVector3D(1.28F, 0.9F, 0.92F),
               .outward = QVector3D(0.0F, 0.0F, 1.0F),
               .ghost = true},
}};

const std::array<QVector3D, 1> k_smoke_vents{k_oven_vent};

const std::array<QVector3D, 1> k_sentry_left{QVector3D(-0.84F, 0.22F, 1.30F)};
const std::array<QVector3D, 1> k_sentry_right{QVector3D(0.84F, 0.22F, 1.30F)};
const std::array<QVector3D, 5> k_porter_route{QVector3D(2.50F, 0.0F, 1.80F),
                                              QVector3D(1.00F, 0.0F, 2.10F),
                                              QVector3D(0.0F, 0.0F, 2.00F),
                                              QVector3D(0.0F, 0.22F, 1.42F),
                                              QVector3D(0.0F, 0.22F, 0.92F)};
const std::array<QVector3D, 4> k_stroll_route{QVector3D(-2.45F, 0.0F, 2.25F),
                                              QVector3D(2.45F, 0.0F, 2.25F),
                                              QVector3D(2.45F, 0.0F, -2.25F),
                                              QVector3D(-2.45F, 0.0F, -2.25F)};
const std::array<QVector3D, 1> k_rest_spot{QVector3D(-2.00F, 0.0F, 1.85F)};
const std::array<WalkSurface, 1> k_walk_surfaces{{
    WalkSurface{.min_x = -1.78F,
                .max_x = 1.78F,
                .min_z = -1.48F,
                .max_z = 1.48F,
                .top = k_platform_top},
}};
const std::array<AmbientPerson, 5> k_people{{
    AmbientPerson{.role = AmbientRole::Stand,
                  .route = k_sentry_left,
                  .facing = QVector3D(-0.84F, 0.4F, 3.2F)},
    AmbientPerson{.role = AmbientRole::Stand,
                  .route = k_sentry_right,
                  .facing = QVector3D(0.84F, 0.4F, 3.2F)},
    AmbientPerson{.role = AmbientRole::Porter,
                  .route = k_porter_route,
                  .facing = QVector3D(0.0F, 0.4F, -0.4F)},
    AmbientPerson{.role = AmbientRole::Stroll,
                  .route = k_stroll_route,
                  .facing = QVector3D(0.0F, 0.4F, 3.0F),
                  .linger = AmbientRole::Stand},
    AmbientPerson{.role = AmbientRole::Squat,
                  .route = k_rest_spot,
                  .facing = QVector3D(-1.2F, 0.3F, 2.6F)},
}};

} // namespace

void register_barracks_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_barracks_renderer_variant(
      registry,
      BarracksRendererConfig{.nation_slug = "carthage",
                             .archetype = &barracks_archetype,
                             .draw_ornaments = &draw_barracks_ornaments,
                             .selection = BuildingSelectionStyle{2.4F, 2.0F},
                             .torches = k_torches,
                             .people = k_people,
                             .walk_surfaces = k_walk_surfaces,
                             .smoke_vents = k_smoke_vents,
                             .grounds = &k_grounds});
}

} // namespace Render::GL::Carthage
