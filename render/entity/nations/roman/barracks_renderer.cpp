#include "render/entity/nations/roman/barracks_renderer.h"

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
#include "render/entity/nations/roman/barracks_authoring.h"
#include "render/entity/nations/roman/building_architecture.h"
#include "render/entity/nations/roman/building_palette.h"
#include "render/entity/registry.h"
#include "render/frame/draw_resources.h"
#include "render/frame/render_archetype.h"
#include "render/frame/submitter.h"
#include "render/geom/primitives.h"
#include "render/math/math_utils.h"

namespace Render::GL::Roman {

using namespace barracks_detail;

namespace {

auto barracks_archetype(BuildingState state,
                        Mesh*,
                        Texture*) -> const RenderArchetype& {
  static const BuildingArchetypeSet& k_set = building_archetype_set("roman_barracks");
  return k_set.for_state(state);
}

void draw_vexillum(const DrawContext& p,
                   ISubmitter& out,
                   Mesh* unit,
                   Texture* white,
                   const RomanPalette& c,
                   const BarracksFlagRenderer::ClothBannerResources* cloth) {
  BarracksFlagRenderer::draw_hanging_banner(
      p,
      out,
      unit,
      white,
      c.team,
      c.team_trim,
      {.pole_base = QVector3D(-1.50F, 0.0F, 1.42F),
       .pole_height = 3.0F,
       .pole_radius = 0.045F,
       .banner_width = 0.9F,
       .banner_height = 0.6F,
       .pole_color = c.cedar,
       .beam_color = c.cedar,
       .connector_color = c.limestone,
       .ornament_offset = QVector3D(0.25F, 3.15F, 0.03F),
       .ornament_size = QVector3D(0.35F, 0.03F, 0.015F),
       .ornament_color = c.gold,
       .ring_count = 4,
       .ring_y_start = 0.4F,
       .ring_spacing = 0.5F,
       .ring_height = 0.025F,
       .ring_radius_scale = 2.0F,
       .ring_color = c.gold},
      cloth);
}

void draw_rally_flag(const DrawContext& p,
                     ISubmitter& out,
                     Texture* white,
                     const RomanPalette& c,
                     const BarracksFlagRenderer::ClothBannerResources* cloth) {
  BarracksFlagRenderer::FlagColors const colors{.team = c.team,
                                                .team_trim = c.team_trim,
                                                .timber = c.cedar,
                                                .timber_light = c.limestone,
                                                .wood_dark = c.cedar_dark};
  BarracksFlagRenderer::draw_rally_flag_if_any(p, out, white, colors, cloth);
}

void draw_stockpile_yard(const DrawContext& p,
                         ISubmitter& out,
                         Mesh* unit,
                         Texture* white,
                         const RomanPalette& c) {
  draw_barracks_stockpile(
      p,
      out,
      unit,
      white,
      StockpileYardStyle{.gravel = QVector3D(0.62F, 0.55F, 0.44F),
                         .earth_light = QVector3D(0.72F, 0.65F, 0.53F),
                         .stone_light = c.limestone,
                         .stone_mid = c.limestone_shade,
                         .stone_dark = c.limestone_dark,
                         .timber = c.cedar,
                         .timber_dark = c.cedar_dark,
                         .ore = QVector3D(0.35F, 0.34F, 0.36F),
                         .ore_rust = QVector3D(0.46F, 0.28F, 0.16F)});
}

void draw_barracks_ornaments(const DrawContext& p,
                             ISubmitter& out,
                             Mesh* unit,
                             Texture* white,
                             const QVector3D& team,
                             const BarracksFlagRenderer::ClothBannerResources* cloth) {
  RomanPalette const c = make_palette(team);
  static const BuildingArchetypeSet& k_grounds_set =
      building_archetype_set("roman_barracks_grounds");
  submit_building_instance(out, p, k_grounds_set.for_state(resolve_building_state(p)));
  draw_stockpile_yard(p, out, unit, white, c);
  draw_vexillum(p, out, unit, white, c, cloth);
  draw_rally_flag(p, out, white, c, cloth);
}

const std::array<TorchMount, 4> k_torches{{
    TorchMount{.at = QVector3D(-1.45F, 0.81F, 1.05F),
               .outward = QVector3D(0.0F, 0.0F, 1.0F)},
    TorchMount{.at = QVector3D(-0.85F, 0.81F, 1.05F),
               .outward = QVector3D(0.0F, 0.0F, 1.0F)},
    TorchMount{.at = QVector3D(-0.52F, 0.8F, 1.112F),
               .outward = QVector3D(0.0F, 0.0F, 1.0F)},
    TorchMount{.at = QVector3D(1.16F, 0.8F, 1.112F),
               .outward = QVector3D(0.0F, 0.0F, 1.0F)},
}};

const std::array<QVector3D, 1> k_smoke_vents{k_cookhouse_vent};

const std::array<QVector3D, 1> k_sentry_left{QVector3D(-0.34F, 0.16F, 1.42F)};
const std::array<QVector3D, 1> k_sentry_right{QVector3D(0.86F, 0.16F, 1.42F)};
const std::array<QVector3D, 5> k_porter_route{QVector3D(-2.45F, 0.0F, 1.85F),
                                              QVector3D(-1.20F, 0.0F, 2.05F),
                                              QVector3D(0.26F, 0.0F, 1.95F),
                                              QVector3D(0.26F, 0.16F, 1.40F),
                                              QVector3D(0.26F, 0.16F, 0.80F)};
const std::array<QVector3D, 4> k_stroll_route{QVector3D(2.40F, 0.0F, 2.20F),
                                              QVector3D(-2.40F, 0.0F, 2.20F),
                                              QVector3D(-2.40F, 0.0F, -2.20F),
                                              QVector3D(2.40F, 0.0F, -2.20F)};
const std::array<QVector3D, 1> k_rest_spot{QVector3D(1.95F, 0.0F, 1.90F)};
const std::array<WalkSurface, 1> k_walk_surfaces{{
    WalkSurface{
        .min_x = -1.74F, .max_x = 1.74F, .min_z = -1.54F, .max_z = 1.54F, .top = 0.16F},
}};
const std::array<AmbientPerson, 5> k_people{{
    AmbientPerson{.role = AmbientRole::Stand,
                  .route = k_sentry_left,
                  .facing = QVector3D(-0.34F, 0.4F, 3.2F)},
    AmbientPerson{.role = AmbientRole::Stand,
                  .route = k_sentry_right,
                  .facing = QVector3D(0.86F, 0.4F, 3.2F)},
    AmbientPerson{.role = AmbientRole::Porter,
                  .route = k_porter_route,
                  .facing = QVector3D(0.26F, 0.4F, -0.4F)},
    AmbientPerson{.role = AmbientRole::Stroll,
                  .route = k_stroll_route,
                  .facing = QVector3D(0.0F, 0.4F, 3.0F),
                  .linger = AmbientRole::Stand},
    AmbientPerson{.role = AmbientRole::Squat,
                  .route = k_rest_spot,
                  .facing = QVector3D(1.2F, 0.3F, 2.6F)},
}};

} // namespace

void register_barracks_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_barracks_renderer_variant(
      registry,
      BarracksRendererConfig{.nation_slug = "roman",
                             .archetype = &barracks_archetype,
                             .draw_ornaments = &draw_barracks_ornaments,
                             .selection = BuildingSelectionStyle{2.6F, 2.2F},
                             .torches = k_torches,
                             .people = k_people,
                             .walk_surfaces = k_walk_surfaces,
                             .smoke_vents = k_smoke_vents,
                             .grounds = &k_grounds});
}

} // namespace Render::GL::Roman
