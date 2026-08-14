#include "ruins_renderer.h"

#include <QVector4D>

#include "decoration_gpu.h"
#include "game/map/scatter/ground_utils.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "render/scene_renderer.h"
#include "scatter_runtime.h"

namespace {

using namespace Render::Ground;

}

namespace Render::GL {

RuinsRenderer::RuinsRenderer() = default;
RuinsRenderer::~RuinsRenderer() = default;

void RuinsRenderer::configure(const Game::Map::TerrainHeightMap& height_map,
                              const Game::Map::BiomeSettings& biome_settings,
                              const std::vector<Game::Map::WorldProp>& world_props) {
  configure_biome_common(biome_settings);
  m_state.params.light_direction = m_light_direction;
  generate_instances(world_props, height_map);
}

void RuinsRenderer::set_light_direction(const QVector3D& dir) {
  set_light_direction_common(dir, PropBatchParams::default_light_direction());
}

void RuinsRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  submit_prop_common(renderer, resources, TerrainScatterCmd::Species::Ruins);
}

void RuinsRenderer::generate_instances(
    const std::vector<Game::Map::WorldProp>& world_props,
    const Game::Map::TerrainHeightMap& height_map) {

  const auto& terrain_service = world().terrain_or_empty();
  const float tile_size = height_map.get_tile_size();
  const int width = height_map.get_width();
  const int map_height = height_map.get_height();
  const float half_w = static_cast<float>(width) * 0.5F;
  const float half_h = static_cast<float>(map_height) * 0.5F;
  const auto surface_profile = Game::Map::make_surface_profile(m_biome_settings);

  for (const auto& prop : world_props) {
    if (prop.type != Game::Map::WorldProp::Type::Ruins) {
      continue;
    }
    const float wx = (prop.x - half_w) * tile_size;
    const float wz = (prop.z - half_h) * tile_size;
    const QVector3D resolved =
        terrain_service.resolve_surface_world_position(wx, wz, 0.0F, 0.0F);

    uint32_t state = hash_coords(static_cast<int>(prop.x),
                                 static_cast<int>(prop.z),
                                 m_biome_settings.seed ^ 0x8B4E12D7U);
    float const stone_mix = remap(rand_01(state), 0.18F, 0.68F);
    QVector3D color = surface_profile.rock_low * (1.0F - stone_mix) +
                      surface_profile.rock_high * stone_mix;
    QVector3D const age_tint(0.31F, 0.32F, 0.30F);
    float const age_mix = remap(rand_01(state), 0.18F, 0.34F);
    color = color * (1.0F - age_mix) + age_tint * age_mix;
    color *= 0.78F;

    PropInstanceGpu inst;
    inst.pos_scale = QVector4D(resolved.x(),
                               resolved.y(),
                               resolved.z(),
                               prop.scale * Game::Map::world_prop_render_scale(
                                                Game::Map::WorldProp::Type::Ruins));
    inst.color_rot = QVector4D(color.x(), color.y(), color.z(), prop.rotation);
    m_state.instances.push_back(inst);
  }

  m_state.instance_count = m_state.instances.size();
  m_state.instances_dirty = m_state.instance_count > 0;
}

} // namespace Render::GL
