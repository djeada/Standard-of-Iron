#pragma once

#include <QVector3D>

#include <string_view>

#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_state.h"
#include "render/entity/market_lot.h"

// The dock: a marketplace in the "dock" style, set with its front to the water.
// A stone quay with a warehouse on it, goods waiting, a crane, and a vessel
// alongside. One lot is one berth; a row of them is a harbour front. The
// vessel is part of the building's mesh and lies outside its footprint, over
// water nobody walks. Authored as BuildingArchetypeDescs and baked by the
// building archetype catalog.
namespace Render::GL {

struct DockLook {
  std::string_view name_prefix;
  bool tiled_roof{true};
  // A horse-head prow, as Punic ships carried; otherwise a plain stem and a
  // goose-neck stern.
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
