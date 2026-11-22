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

constexpr float k_base_color_b = 0.18F;
constexpr float k_base_color_g = 0.21F;
constexpr float k_base_color_r = 0.24F;

} // namespace

auto generate_cursed_gold_veins(const ScatterRequest& request)
    -> std::vector<PropInstanceGpu> {
  std::vector<PropInstanceGpu> instances;
  const auto& world_props = request.world_props;

  const auto& terrain_service = request.terrain;

  for (const auto& prop : world_props) {
    if (prop.type != Game::Map::WorldProp::Type::CursedGoldVein) {
      continue;
    }
    const QVector3D resolved = terrain_service.world_prop_footprint_world_position(
        prop, Game::Map::world_prop_ground_bounding_radius(prop.type, prop.scale));

    PropInstanceGpu inst;
    inst.pos_scale =
        QVector4D(resolved.x(),
                  resolved.y(),
                  resolved.z(),
                  prop.scale * Game::Map::world_prop_render_scale(
                                   Game::Map::WorldProp::Type::CursedGoldVein));
    inst.color_rot =
        QVector4D(k_base_color_r, k_base_color_g, k_base_color_b, prop.rotation);
    instances.push_back(inst);
  }
  return instances;
}

} // namespace Render::Ground::Scatter
