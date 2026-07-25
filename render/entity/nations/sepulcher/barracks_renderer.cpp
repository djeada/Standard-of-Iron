#include "barracks_renderer.h"

#include <QVector3D>

#include "../../../gl/backend.h"
#include "../../../gl/primitives.h"
#include "../../../gl/resources.h"
#include "../../../submitter.h"
#include "../../barracks_flag_renderer.h"
#include "../../building_render_common.h"
#include "../../registry.h"
#include "game/core/component.h"

namespace Render::GL::Sepulcher {
namespace {

struct SepulcherPalette {
  QVector3D grave_iron{0.20F, 0.21F, 0.24F};
  QVector3D grave_iron_light{0.32F, 0.33F, 0.37F};
  QVector3D bone{0.78F, 0.75F, 0.66F};
  QVector3D witchlight{0.42F, 0.86F, 0.62F};
};

void draw_grave_banner(const DrawContext& p,
                       ISubmitter& out,
                       Mesh* unit,
                       Texture* white,
                       const QVector3D& team,
                       const BarracksFlagRenderer::ClothBannerResources* cloth) {
  const SepulcherPalette palette;

  const QVector3D team_trim = team * 0.55F + palette.grave_iron * 0.45F;

  BarracksFlagRenderer::draw_hanging_banner(
      p,
      out,
      unit,
      white,
      team,
      team_trim,
      {.pole_base = QVector3D(-2.10F, 0.0F, -1.70F),
       .pole_height = 3.2F,
       .pole_radius = 0.055F,
       .banner_width = 0.95F,
       .banner_height = 1.15F,
       .connector_drop_ratio = 0.5F,
       .pole_color = palette.grave_iron,
       .beam_color = palette.bone,
       .connector_color = palette.grave_iron_light,

       .ornament_offset = QVector3D(0.0F, 3.34F, 0.0F),
       .ornament_size = QVector3D(0.05F, 0.30F, 0.05F),
       .ornament_color = palette.bone,
       .ring_count = 3,
       .ring_y_start = 0.6F,
       .ring_spacing = 0.72F,
       .ring_height = 0.03F,
       .ring_radius_scale = 1.8F,
       .ring_color = palette.bone},
      cloth);
}

} // namespace

void register_barracks_renderer(EntityRendererRegistry& registry) {
  register_building_renderer(
      registry,
      "iron_sepulcher",
      "barracks",
      [](const DrawContext& ctx, ISubmitter& out) {
        if (ctx.resources == nullptr || ctx.entity == nullptr) {
          return;
        }
        auto* renderable =
            ctx.entity->get_component<Engine::Core::RenderableComponent>();
        if (renderable == nullptr) {
          return;
        }

        Mesh* unit = ctx.resources->unit();
        if (unit == nullptr) {
          unit = get_unit_cube();
        }
        Texture* white = ctx.resources->white();
        const QVector3D team(
            renderable->color[0], renderable->color[1], renderable->color[2]);

        BarracksFlagRenderer::ClothBannerResources cloth;
        if (ctx.backend != nullptr) {
          cloth.cloth_mesh = ctx.backend->banner_mesh();
          cloth.banner_shader = ctx.backend->banner_shader();
        }

        draw_grave_banner(ctx, out, unit, white, team, &cloth);
        draw_building_health_bar(
            out, ctx, BuildingHealthBarStyle{1.2F, 0.10F, 2.4F, true});
        draw_building_selection_overlay(out, ctx, BuildingSelectionStyle{2.4F, 2.0F});
      });
}

} // namespace Render::GL::Sepulcher
