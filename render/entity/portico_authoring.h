#pragma once

#include <QVector3D>

#include <string_view>

#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_state.h"
#include "render/entity/market_lot.h"

// The portico: a colonnaded walk with a range of shops behind it, a
// marketplace in the "portico" style. One lot is three bays; a run of lots is
// a stoa, and a square ringed with them is a forum or an agora. Authored as
// BuildingArchetypeDescs and baked by the building archetype catalog.
namespace Render::GL {

struct PorticoLook {
  std::string_view name_prefix;
  // Round shafts with moulded capitals, or square piers with a painted band.
  bool round_columns{true};
  // A tiled gable, or a flat roof behind a parapet.
  bool tiled_roof{true};
  QVector3D stone;
  QVector3D stone_shade;
  QVector3D stone_dark;
  QVector3D marble;
  QVector3D timber;
  QVector3D timber_dark;
  QVector3D tile;
  QVector3D tile_light;
  QVector3D tile_dark;
  QVector3D deck;
};

[[nodiscard]] auto roman_portico_look() -> const PorticoLook&;
[[nodiscard]] auto carthage_portico_look() -> const PorticoLook&;

auto build_portico_desc(const PorticoLook& look,
                        BuildingState state,
                        int variant) -> BuildingArchetypeDesc;

} // namespace Render::GL
