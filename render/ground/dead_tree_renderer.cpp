#include "dead_tree_renderer.h"

#include <QVector4D>

#include <algorithm>
#include <cmath>

#include "decoration_gpu.h"
#include "game/map/scatter/ground_utils.h"
#include "game/map/scatter/scatter_composition.h"
#include "game/map/scatter/spawn_validator.h"
#include "gl/render_constants.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "render/scene_renderer.h"
#include "scatter_runtime.h"

namespace {

using namespace Render::Ground;

constexpr float k_base_color_r = 0.30F;
constexpr float k_base_color_g = 0.27F;
constexpr float k_base_color_b = 0.22F;

} // namespace

namespace Render::GL {

DeadTreeRenderer::DeadTreeRenderer() = default;
DeadTreeRenderer::~DeadTreeRenderer() = default;

void DeadTreeRenderer::configure(const Game::Map::TerrainHeightMap& height_map,
                                 const Game::Map::BiomeSettings& biome_settings,
                                 const std::vector<Game::Map::WorldProp>& world_props) {
  configure_height_scatter_common(height_map, biome_settings, world_props);
  m_state.params.light_direction = m_light_direction;
  rebuild_dead_tree_instances();
}

void DeadTreeRenderer::refresh_world_props(
    const std::vector<Game::Map::WorldProp>& world_props) {
  m_world_props = world_props;
  rebuild_dead_tree_instances();
}

void DeadTreeRenderer::rebuild_dead_tree_instances() {
  m_state.instances.clear();
  append_world_prop_dead_trees();
  finish_instance_rebuild();
}

void DeadTreeRenderer::set_light_direction(const QVector3D& dir) {
  set_light_direction_common(dir, PropBatchParams::default_light_direction());
}

void DeadTreeRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  submit_prop_common(renderer, resources, TerrainScatterCmd::Species::DeadTree);
}

void DeadTreeRenderer::append_world_prop_dead_trees() {
  const auto& terrain_service = world().terrain_or_empty();

  for (const auto& prop : m_world_props) {
    if (prop.type != Game::Map::WorldProp::Type::DeadTree) {
      continue;
    }
    const QVector3D resolved = terrain_service.world_prop_footprint_world_position(
        prop, Game::Map::world_prop_ground_bounding_radius(prop.type, prop.scale));

    uint32_t state = hash_coords(static_cast<int>(prop.x),
                                 static_cast<int>(prop.z),
                                 m_biome_settings.seed ^ 0x51A3C7D9U);
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
    m_state.instances.push_back(inst);
  }
}

} // namespace Render::GL
