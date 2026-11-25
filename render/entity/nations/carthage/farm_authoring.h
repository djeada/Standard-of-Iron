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
#include "render/entity/nations/carthage/building_palette.h"
#include "render/frame/render_archetype.h"

namespace Render::GL::Carthage {

namespace farm_detail {}

auto build_farm_desc(BuildingState state, int stage) -> BuildingArchetypeDesc;

} // namespace Render::GL::Carthage
