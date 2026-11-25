#pragma once

#include <QVector3D>

#include <array>
#include <cmath>
#include <cstdint>

#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_archetype_library.h"
#include "render/entity/building_decay.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/building_state.h"
#include "render/entity/nations/carthage/building_palette.h"
#include "render/entity/temple_layout.h"
#include "render/frame/render_archetype.h"

namespace Render::GL::Carthage {

namespace temple_detail {}

auto build_temple_desc(BuildingState state) -> BuildingArchetypeDesc;

} // namespace Render::GL::Carthage
