#pragma once

#include <QVector3D>

#include <string_view>

#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_state.h"
#include "render/entity/market_lot.h"

namespace Render::GL {

struct DockLook {
  std::string_view name_prefix;
  bool tiled_roof{true};

  bool horse_prow{false};
  QVector3D stone;
  QVector3D stone_shade;
  QVector3D stone_dark;
  QVector3D timber;
  QVector3D timber_light;
  QVector3D timber_dark;
  QVector3D hull;
  QVector3D tile;
  QVector3D tile_light;
  QVector3D tile_dark;
  QVector3D deck;
  QVector3D clay;
};

[[nodiscard]] auto roman_dock_look() -> const DockLook&;
[[nodiscard]] auto carthage_dock_look() -> const DockLook&;

auto build_dock_desc(const DockLook& look,
                     BuildingState state,
                     int variant) -> BuildingArchetypeDesc;

} // namespace Render::GL
