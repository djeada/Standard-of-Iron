#pragma once

#include <QColor>

#include <cstdint>

namespace Game::Map::Minimap {

namespace Palette {

constexpr QColor PARCHMENT_BASE{214, 196, 161};
constexpr QColor PARCHMENT_DARK{176, 156, 125};
constexpr QColor PARCHMENT_STAIN{138, 112, 78, 34};

constexpr QColor INK_DARK{45, 35, 25};
constexpr QColor INK_MEDIUM{80, 65, 50};
constexpr QColor INK_LIGHT{120, 100, 80};

constexpr QColor MOUNTAIN_SHADOW{70, 52, 38};
constexpr QColor MOUNTAIN_FACE{115, 96, 75};
constexpr QColor MOUNTAIN_HIGHLIGHT{198, 185, 165};
constexpr QColor HILL_BASE{130, 112, 85};

constexpr QColor WATER_DARK{62, 86, 104};
constexpr QColor WATER_MAIN{86, 120, 142};
constexpr QColor WATER_LIGHT{138, 169, 184};
constexpr QColor WATER_WASH{120, 150, 165, 48};
constexpr QColor WATER_GLOW{204, 219, 224, 88};

constexpr QColor FOREST_BASE{73, 108, 61};
constexpr QColor FOREST_DARK{42, 72, 37};

constexpr QColor ROAD_MAIN{112, 82, 47};
constexpr QColor ROAD_SHADOW{69, 49, 27};
constexpr QColor ROAD_HIGHLIGHT{166, 132, 82};

constexpr QColor STRUCTURE_STONE{110, 95, 75};
constexpr QColor LANDMARK_FILL{132, 118, 96};
constexpr QColor UNDEAD_ZONE{92, 62, 112};
constexpr QColor STRUCTURE_SHADOW{62, 44, 28};

constexpr QColor TEAM_BLUE_DARK{18, 45, 122};
constexpr QColor TEAM_RED_DARK{130, 16, 12};

} // namespace Palette

inline auto hash_coords(int x, int y, int seed = 0) -> float {
  const auto n = static_cast<std::uint32_t>(x) + static_cast<std::uint32_t>(y) * 57U +
                 static_cast<std::uint32_t>(seed) * 131U;
  const std::uint32_t shifted = (n << 13U) ^ n;
  const std::uint32_t mixed =
      (shifted * (shifted * shifted * 15731U + 789221U) + 1376312589U) & 0x7fffffffU;
  return 1.0F - static_cast<float>(mixed) / 1073741824.0F;
}

} // namespace Game::Map::Minimap
