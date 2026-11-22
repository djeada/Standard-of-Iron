#include "render/entity/nations/carthage/farm_renderer.h"

#include <QVector3D>

#include <array>
#include <cmath>
#include <string>

#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_decay.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/building_render_common.h"
#include "render/entity/building_state.h"
#include "render/entity/farm_renderer_common.h"
#include "render/entity/nations/carthage/building_palette.h"
#include "render/entity/nations/carthage/farm_authoring.h"
#include "render/entity/registry.h"
#include "render/frame/render_archetype.h"

namespace Render::GL::Carthage {

using namespace farm_detail;

namespace {

auto farm_archetype(BuildingState state, int stage) -> const RenderArchetype& {
  return farm_stage_archetype("carthage", state, stage);
}

const std::array<TorchMount, 3> k_torches{{
    TorchMount{.at = QVector3D(0.985F, 0.09F, 0.18F),
               .outward = QVector3D(1.0F, 0.0F, 0.0F)},
    TorchMount{.at = QVector3D(0.985F, 0.09F, 0.44F),
               .outward = QVector3D(1.0F, 0.0F, 0.0F)},
    TorchMount{.at = QVector3D(0.35F, 0.24F, 0.48F),
               .outward = QVector3D(0.0F, 0.0F, -1.0F)},
}};

} // namespace

void register_farm_renderer(EntityRendererRegistry& registry) {
  register_farm_renderer_variant(
      registry,
      FarmRendererConfig{.nation_slug = "carthage",
                         .archetype = &farm_archetype,
                         .selection = BuildingSelectionStyle{2.2F, 2.2F},
                         .torches = k_torches});
}

} // namespace Render::GL::Carthage
