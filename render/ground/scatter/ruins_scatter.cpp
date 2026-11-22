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

auto generate_ruins(const ScatterRequest& request) -> std::vector<PropInstanceGpu> {
  std::vector<PropInstanceGpu> instances;
  const auto& world_props = request.world_props;

  const auto& terrain_service = request.terrain;
  const auto surface_profile = Game::Map::make_surface_profile(request.biome);

  for (const auto& prop : world_props) {
    if (prop.type != Game::Map::WorldProp::Type::Ruins) {
      continue;
    }
    const QVector3D resolved = terrain_service.world_prop_footprint_world_position(
        prop, Game::Map::world_prop_ground_bounding_radius(prop.type, prop.scale));

    uint32_t state = hash_coords(static_cast<int>(prop.x),
                                 static_cast<int>(prop.z),
                                 request.biome.seed ^ 0x8B4E12D7U);
    float const stone_mix = remap(rand_01(state), 0.18F, 0.68F);
    QVector3D color = surface_profile.rock_low * (1.0F - stone_mix) +
                      surface_profile.rock_high * stone_mix;
    QVector3D const cut_stone(0.76F, 0.70F, 0.60F);
    color = color * 0.45F + cut_stone * 0.55F;
    QVector3D const age_tint(0.46F, 0.45F, 0.41F);
    float const age_mix = remap(rand_01(state), 0.14F, 0.28F);
    color = color * (1.0F - age_mix) + age_tint * age_mix;
    color *= 0.92F;

    PropInstanceGpu inst;
    inst.pos_scale = QVector4D(resolved.x(),
                               resolved.y(),
                               resolved.z(),
                               prop.scale * Game::Map::world_prop_render_scale(
                                                Game::Map::WorldProp::Type::Ruins));
    inst.color_rot = QVector4D(color.x(), color.y(), color.z(), prop.rotation);
    instances.push_back(inst);
  }
  return instances;
}

} // namespace Render::Ground::Scatter
