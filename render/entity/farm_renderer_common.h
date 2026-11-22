#pragma once

#include <QVector3D>

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include "game/core/component_economy.h"
#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_render_common.h"
#include "render/entity/building_state.h"
#include "render/entity/building_torches.h"
#include "render/entity/farm_field.h"
#include "render/entity/registry.h"
#include "render/frame/render_archetype.h"

namespace Render::GL {

using FarmArchetypeResolver = const RenderArchetype& (*)(BuildingState, int stage);

struct FarmRendererConfig {
  std::string_view nation_slug;
  FarmArchetypeResolver archetype;
  BuildingSelectionStyle selection;
  std::span<const TorchMount> torches{};
};

void register_farm_renderer_variant(EntityRendererRegistry& registry,
                                    const FarmRendererConfig& config);

auto farm_stage_archetype(std::string_view nation_slug,
                          BuildingState state,
                          int stage) -> const RenderArchetype&;

} // namespace Render::GL
