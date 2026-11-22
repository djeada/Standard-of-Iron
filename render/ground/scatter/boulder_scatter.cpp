#include <QVector3D>
#include <QVector4D>

#include <cstdint>
#include <vector>

#include "game/map/scatter/ground_utils.h"
#include "game/map/terrain_service.h"
#include "render/frame/decoration_gpu.h"
#include "render/ground/scatter/scatter_generators.h"
#include "render/ground/scatter_runtime.h"
#include "render/ground/stone_ground_fit.h"

namespace Render::Ground::Scatter {

namespace {

using namespace Render::Ground;
using Render::GL::StoneInstanceGpu;
using std::uint32_t;

} // namespace

auto generate_boulders(const ScatterRequest& request) -> std::vector<StoneInstanceGpu> {
  std::vector<StoneInstanceGpu> instances;
  if (request.grid.empty()) {
    return instances;
  }
  const int grid_width = request.grid.width;
  const int grid_height = request.grid.height;
  const float tile_size = request.grid.tile_size;
  const auto& height_data = *request.grid.heights;
  const auto& terrain_service = request.terrain;
  const auto surface_profile = Game::Map::make_surface_profile(request.biome);

  for (const auto& prop : request.world_props) {
    if (prop.type != Game::Map::WorldProp::Type::Boulder) {
      continue;
    }
    const QVector3D resolved = terrain_service.world_prop_world_position(prop);

    uint32_t state = hash_coords(static_cast<int>(prop.x),
                                 static_cast<int>(prop.z),
                                 request.biome.seed ^ 0xD3A4B1C2U);
    QVector3D const color = stone_instance_color(
        surface_profile.rock_low, surface_profile.rock_high, 0.35F, 0.45F, state);

    float const render_scale = prop.scale * Game::Map::world_prop_render_scale(
                                                Game::Map::WorldProp::Type::Boulder);
    float const gx = world_to_grid_coord(resolved.x(), grid_width, tile_size);
    float const gz = world_to_grid_coord(resolved.z(), grid_height, tile_size);
    QVector3D const ground_normal =
        sample_ground_normal(height_data, grid_width, grid_height, tile_size, gx, gz);

    StoneInstanceGpu inst;
    inst.pos_scale = QVector4D(resolved.x(), resolved.y(), resolved.z(), render_scale);
    inst.color_rot = QVector4D(color.x(), color.y(), color.z(), prop.rotation);
    inst.ground_fit = pack_stone_ground_fit(ground_normal,
                                            rand_01(state),
                                            render_scale,
                                            stone_sink_fraction(ground_normal, state));
    instances.push_back(inst);
  }
  return instances;
}

} // namespace Render::Ground::Scatter
