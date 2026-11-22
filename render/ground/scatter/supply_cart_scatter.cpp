#include <QVector3D>
#include <QVector4D>

#include <cstdint>
#include <vector>

#include "game/map/scatter/ground_utils.h"
#include "game/map/terrain_service.h"
#include "render/frame/decoration_gpu.h"
#include "render/ground/scatter/scatter_generators.h"
#include "render/ground/scatter_runtime.h"

namespace Render::Ground::Scatter {

namespace {

using namespace Render::Ground;
using Render::GL::PropInstanceGpu;
using std::uint32_t;

} // namespace

auto generate_supply_carts(const ScatterRequest& request)
    -> std::vector<PropInstanceGpu> {
  std::vector<PropInstanceGpu> instances;
  const auto& world_props = request.world_props;

  const auto& terrain_service = request.terrain;
  for (const auto& prop : world_props) {
    if (prop.type != Game::Map::WorldProp::Type::SupplyCart) {
      continue;
    }
    const QVector3D resolved = terrain_service.world_prop_footprint_world_position(
        prop, Game::Map::world_prop_ground_bounding_radius(prop.type, prop.scale));

    uint32_t state = hash_coords(static_cast<int>(prop.x),
                                 static_cast<int>(prop.z),
                                 request.biome.seed ^ 0x5C28B1E7U);
    QVector3D const heartwood(0.42F, 0.23F, 0.10F);
    QVector3D const oak(0.56F, 0.35F, 0.16F);
    float const wood_mix = rand_01(state);
    QVector3D wood_color = heartwood * (1.0F - wood_mix) + oak * wood_mix;
    wood_color *= remap(rand_01(state), 0.88F, 1.06F);

    PropInstanceGpu inst;
    inst.pos_scale =
        QVector4D(resolved.x(),
                  resolved.y(),
                  resolved.z(),
                  prop.scale * Game::Map::world_prop_render_scale(
                                   Game::Map::WorldProp::Type::SupplyCart));
    inst.color_rot =
        QVector4D(wood_color.x(), wood_color.y(), wood_color.z(), prop.rotation);
    instances.push_back(inst);
  }
  return instances;
}

} // namespace Render::Ground::Scatter
