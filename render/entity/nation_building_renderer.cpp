#include "render/entity/nation_building_renderer.h"

#include <QVector3D>

#include <array>
#include <string>
#include <string_view>

#include "render/entity/building_archetype_library.h"
#include "render/entity/building_render_common.h"
#include "render/entity/building_state.h"
#include "render/entity/registry.h"
#include "render/frame/entity_appearance.h"
#include "render/frame/render_archetype.h"

namespace Render::GL {

namespace {

struct NationBuildingLook {
  std::string_view building_type;
  BuildingSelectionStyle selection;
};

// Each draws its own catalog entry (building_archetype_catalog.cpp):
// "<nation>_<building>", built from the barracks yard or the portico until the
// building art pass.
constexpr std::array<NationBuildingLook, 4> k_looks{{
    {"mercenary_camp", BuildingSelectionStyle{2.0F, 1.8F}},
    {"elephant_stables", BuildingSelectionStyle{2.6F, 2.2F}},
    {"fabrica", BuildingSelectionStyle{2.2F, 2.0F}},
    {"castra", BuildingSelectionStyle{2.4F, 2.4F}},
}};

void register_look(EntityRendererRegistry& registry,
                   std::string_view nation_slug,
                   const NationBuildingLook& look) {
  const std::string archetype_set =
      std::string(nation_slug) + "_" + std::string(look.building_type);
  const BuildingSelectionStyle selection = look.selection;
  register_building_renderer(
      registry,
      nation_slug,
      look.building_type,
      [archetype_set,
       selection,
       set = static_cast<const BuildingArchetypeSet*>(nullptr)](
          const DrawContext& ctx,
          const EntityPresentation& presentation,
          ISubmitter& out) mutable {
        if (ctx.entity == nullptr) {
          return;
        }
        if (set == nullptr) {
          set = &building_archetype_set(archetype_set);
        }
        const std::array<QVector3D, 1> palette{Render::entity_color(*ctx.entity)};
        submit_building_instance(
            out,
            ctx,
            set->for_state(presented_building_state(presentation, ctx)),
            palette);
        draw_building_selection_overlay(out, ctx, selection);
      });
}

} // namespace

void register_nation_building_renderers(EntityRendererRegistry& registry) {
  for (const auto& look : k_looks) {
    register_look(registry, "roman", look);
    register_look(registry, "carthage", look);
  }
}

} // namespace Render::GL
