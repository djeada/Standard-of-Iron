#include "wildlife_terrain_probe.h"

#include "../map/terrain.h"
#include "../map/terrain_service.h"

namespace Game::Wildlife {

namespace {

constexpr float k_boundary_margin = 1.5F;

class TerrainServiceProbe final : public ITerrainProbe {
public:
  [[nodiscard]] auto is_blocked(float world_x, float world_z) const -> bool override {
    const auto& terrain = Game::Map::TerrainService::instance();
    if (!terrain.is_initialized()) {
      return false;
    }
    if (!bounds().contains(world_x, world_z)) {
      return true;
    }
    return terrain.is_forbidden_world(world_x, world_z);
  }

  [[nodiscard]] auto ground_height(float world_x,
                                   float world_z) const -> float override {
    const auto& terrain = Game::Map::TerrainService::instance();
    if (!terrain.is_initialized()) {
      return 0.0F;
    }
    return terrain.resolve_surface_world_y(world_x, world_z);
  }

  [[nodiscard]] auto bounds() const -> WorldBounds override {
    const auto& terrain = Game::Map::TerrainService::instance();
    const auto* height_map = terrain.get_height_map();
    if (height_map == nullptr) {
      return {};
    }
    float const tile_size = height_map->get_tile_size();
    float const half_x =
        ((static_cast<float>(height_map->get_width()) * 0.5F) - 0.5F) * tile_size;
    float const half_z =
        ((static_cast<float>(height_map->get_height()) * 0.5F) - 0.5F) * tile_size;
    WorldBounds result;
    result.min_x = -half_x + k_boundary_margin;
    result.max_x = half_x - k_boundary_margin;
    result.min_z = -half_z + k_boundary_margin;
    result.max_z = half_z - k_boundary_margin;
    return result;
  }
};

} // namespace

auto terrain_service_probe() -> ITerrainProbe& {
  static TerrainServiceProbe probe;
  return probe;
}

} // namespace Game::Wildlife
