#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "game/core/component.h"
#include "game/visuals/team_colors.h"
#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_archetype_library.h"
#include "render/entity/building_decay.h"
#include "render/entity/building_ornaments.h"
#include "render/entity/building_state.h"
#include "render/entity/market_goods.h"
#include "render/entity/nations/roman/building_architecture.h"
#include "render/entity/nations/roman/building_palette.h"
#include "render/frame/render_archetype.h"
#include "render/geom/primitives.h"
#include "render/geom/transforms.h"

namespace Render::GL::Roman {

namespace marketplace_detail {}

auto build_marketplace_desc(BuildingState state) -> BuildingArchetypeDesc;

} // namespace Render::GL::Roman
