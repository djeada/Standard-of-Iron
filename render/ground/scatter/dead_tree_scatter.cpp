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

constexpr float k_base_color_b = 0.22F;
constexpr float k_base_color_g = 0.27F;
constexpr float k_base_color_r = 0.30F;

} // namespace

auto generate_dead_trees(const ScatterRequest& request)
    -> std::vector<PropInstanceGpu> {
  std::vector<PropInstanceGpu> instances;
  const auto& terrain_service = request.terrain;

  for (const auto& prop : request.world_props) {
    if (prop.type != Game::Map::WorldProp::Type::DeadTree) {
      continue;
    }
    const QVector3D resolved = terrain_service.world_prop_footprint_world_position(
        prop, Game::Map::world_prop_ground_bounding_radius(prop.type, prop.scale));

    uint32_t state = hash_coords(static_cast<int>(prop.x),
                                 static_cast<int>(prop.z),
                                 request.biome.seed ^ 0x51A3C7D9U);
    QVector3D const base_color(k_base_color_r, k_base_color_g, k_base_color_b);
    QVector3D const dry_color(0.43F, 0.36F, 0.27F);
    float const color_var = remap(rand_01(state), 0.35F, 0.85F);
    QVector3D const color = base_color * (1.0F - color_var) + dry_color * color_var;

    PropInstanceGpu inst;
    inst.pos_scale = QVector4D(resolved.x(),
                               resolved.y(),
                               resolved.z(),
                               prop.scale * Game::Map::world_prop_render_scale(
                                                Game::Map::WorldProp::Type::DeadTree));
    inst.color_rot = QVector4D(color.x(), color.y(), color.z(), prop.rotation);
    instances.push_back(inst);
  }
  return instances;
}

} // namespace Render::Ground::Scatter
