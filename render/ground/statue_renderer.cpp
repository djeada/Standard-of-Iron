#include "statue_renderer.h"

#include <QVector4D>

#include "../scene_renderer.h"
#include "decoration_gpu.h"
#include "game/map/scatter/ground_utils.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "scatter_runtime.h"

namespace {

using namespace Render::Ground;

}

namespace Render::GL {

StatueRenderer::StatueRenderer() = default;
StatueRenderer::~StatueRenderer() = default;

void StatueRenderer::configure(const Game::Map::TerrainHeightMap& height_map,
                               const Game::Map::BiomeSettings& biome_settings,
                               const std::vector<Game::Map::WorldProp>& world_props) {
  configure_biome_common(biome_settings);
  m_state.params.light_direction = m_light_direction;
  generate_instances(world_props, height_map);
}

void StatueRenderer::set_light_direction(const QVector3D& dir) {
  set_light_direction_common(dir, PropBatchParams::default_light_direction());
}

void StatueRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  submit_prop_common(renderer, resources, TerrainScatterCmd::Species::Statue);
}

void StatueRenderer::generate_instances(
    const std::vector<Game::Map::WorldProp>& world_props,
    const Game::Map::TerrainHeightMap& height_map) {

  auto& terrain_service = Game::Map::TerrainService::instance();
  const float tile_size = height_map.get_tile_size();
  const int width = height_map.get_width();
  const int map_height = height_map.get_height();
  const float half_w = static_cast<float>(width) * 0.5F;
  const float half_h = static_cast<float>(map_height) * 0.5F;

  for (const auto& prop : world_props) {
    if (prop.type != Game::Map::WorldProp::Type::Statue) {
      continue;
    }
    const float wx = (prop.x - half_w) * tile_size;
    const float wz = (prop.z - half_h) * tile_size;
    const QVector3D resolved =
        terrain_service.resolve_surface_world_position(wx, wz, 0.0F, 0.0F);

    uint32_t state = hash_coords(static_cast<int>(prop.x),
                                 static_cast<int>(prop.z),
                                 m_biome_settings.seed ^ 0x1D7B4E63U);

    QVector3D const pentelic_marble(0.955F, 0.940F, 0.900F);
    QVector3D const travertine(0.905F, 0.845F, 0.735F);
    QVector3D const grey_limestone(0.790F, 0.795F, 0.780F);

    float const quarry = rand_01(state);
    QVector3D color =
        quarry < 0.55F
            ? pentelic_marble
            : (quarry < 0.82F ? pentelic_marble * 0.45F + travertine * 0.55F
                              : pentelic_marble * 0.35F + grey_limestone * 0.65F);
    float const weathering = remap(rand_01(state), 0.0F, 0.14F);
    color *= 1.0F - weathering;
    color *= remap(rand_01(state), 0.96F, 1.04F);

    PropInstanceGpu inst;
    inst.pos_scale = QVector4D(resolved.x(),
                               resolved.y(),
                               resolved.z(),
                               prop.scale * Game::Map::world_prop_render_scale(
                                                Game::Map::WorldProp::Type::Statue));
    inst.color_rot = QVector4D(color.x(), color.y(), color.z(), prop.rotation);
    m_state.instances.push_back(inst);
  }

  m_state.instance_count = m_state.instances.size();
  m_state.instances_dirty = m_state.instance_count > 0;
}

} // namespace Render::GL
