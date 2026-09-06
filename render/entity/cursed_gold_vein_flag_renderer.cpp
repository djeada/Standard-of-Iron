#include "cursed_gold_vein_flag_renderer.h"

#include <QVector3D>

#include <string>

#include "entity_appearance.h"
#include "game/core/component_core.h"
#include "game/core/ownership_constants.h"
#include "render/entity/barracks_flag_renderer.h"
#include "render/entity/building_render_common.h"
#include "render/entity/registry.h"
#include "render/gl/backend.h"
#include "render/gl/primitives.h"
#include "render/gl/resources.h"
#include "render/submitter.h"

namespace Render::GL {
namespace {

struct VeinFlagPalette {
  QVector3D pit_timber{0.23F, 0.17F, 0.11F};
  QVector3D pit_timber_light{0.38F, 0.29F, 0.18F};
  QVector3D tarnished_gold{0.72F, 0.56F, 0.22F};
  QVector3D unclaimed_cloth{0.62F, 0.58F, 0.50F};
};

void draw_claim_flag(const DrawContext& p,
                     ISubmitter& out,
                     Mesh* unit,
                     Texture* white,
                     const QVector3D& team,
                     const BarracksFlagRenderer::ClothBannerResources* cloth) {
  const VeinFlagPalette palette;
  const QVector3D team_trim = team * 0.5F + palette.tarnished_gold * 0.5F;

  BarracksFlagRenderer::draw_hanging_banner(
      p,
      out,
      unit,
      white,
      team,
      team_trim,
      {.pole_base = QVector3D(1.53F, 0.0F, -1.28F),
       .pole_height = 3.0F,
       .pole_radius = 0.045F,
       .banner_width = 0.9F,
       .banner_height = 0.6F,
       .pole_color = palette.pit_timber,
       .beam_color = palette.pit_timber_light,
       .connector_color = palette.tarnished_gold,

       .ornament_offset = QVector3D(0.25F, 3.15F, 0.03F),
       .ornament_size = QVector3D(0.35F, 0.03F, 0.015F),
       .ornament_color = palette.tarnished_gold,
       .ring_count = 4,
       .ring_y_start = 0.4F,
       .ring_spacing = 0.5F,
       .ring_height = 0.025F,
       .ring_radius_scale = 2.0F,
       .ring_color = palette.tarnished_gold},
      cloth);
}

} // namespace

void register_cursed_gold_vein_flag_renderer(EntityRendererRegistry& registry) {
  registry.register_renderer(
      std::string(k_cursed_gold_vein_flag_renderer_key),
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

        const VeinFlagPalette palette;
        auto* unit_component = ctx.entity->get_component<Engine::Core::UnitComponent>();
        const bool neutral = unit_component == nullptr ||
                             Game::Core::is_neutral_owner(unit_component->owner_id);
        const QVector3D team =
            neutral ? palette.unclaimed_cloth : Render::entity_color(*ctx.entity);

        BarracksFlagRenderer::ClothBannerResources cloth;
        if (ctx.backend != nullptr) {
          cloth.cloth_mesh = ctx.backend->banner_mesh();
          cloth.banner_shader = ctx.backend->banner_shader();
        }

        draw_claim_flag(ctx, out, unit, white, team, &cloth);
        draw_building_selection_overlay(out, ctx, BuildingSelectionStyle{2.2F, 2.0F});
      });
}

} // namespace Render::GL
