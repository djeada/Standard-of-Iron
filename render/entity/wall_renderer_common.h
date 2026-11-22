#pragma once

#include <QVector3D>

#include <array>
#include <string_view>
#include <utility>

#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_render_common.h"
#include "render/entity/wall_authoring.h"
#include "render/frame/render_archetype.h"
#include "render/frame/submitter.h"

namespace Render::GL {

struct WallArchetypeSet {
  std::array<const BuildingArchetypeSet*, 6> variants{};
  const BuildingArchetypeSet* walk_span{nullptr};
  const BuildingArchetypeSet* walk_landing{nullptr};
  const BuildingArchetypeSet* walk_stair{nullptr};
};

void submit_wall_ladder(ISubmitter& out,
                        const DrawContext& ctx,
                        const BuildingArchetypeSet& archetype);

auto wall_archetype_set(std::string_view name_prefix) -> WallArchetypeSet;
void submit_wall_segment_variant(ISubmitter& out,
                                 const DrawContext& ctx,
                                 const WallArchetypeSet& archetypes,
                                 WallVariant variant);

} // namespace Render::GL
