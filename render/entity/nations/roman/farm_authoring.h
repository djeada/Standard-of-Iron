#pragma once

#include <QVector3D>

#include <array>
#include <cmath>
#include <string>

#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_decay.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/building_state.h"
#include "render/entity/farm_field.h"
#include "render/entity/nations/roman/building_architecture.h"
#include "render/entity/nations/roman/building_palette.h"
#include "render/frame/render_archetype.h"

// The farm geometry, authored as BuildingArchetypeDescs and baked by
// the building archetype catalog. Nothing here draws or reads frame state.
namespace Render::GL::Roman {

// Palette and layout shared by the authoring and the farm renderer.
namespace farm_detail {} // namespace farm_detail

auto build_farm_desc(BuildingState state, int stage) -> BuildingArchetypeDesc;

} // namespace Render::GL::Roman
