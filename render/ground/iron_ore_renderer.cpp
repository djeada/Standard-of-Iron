#include "render/ground/iron_ore_renderer.h"

#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "game/map/scatter/ground_utils.h"
#include "game/map/scatter/scatter_composition.h"
#include "game/map/scatter/scatter_world.h"
#include "game/map/scatter/spawn_validator.h"
#include "map/biome_settings.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "render/frame/decoration_gpu.h"
#include "render/frame/frame_submitter.h"
#include "render/frame/terrain_contact.h"
#include "render/ground/scatter/scatter_generators.h"
#include "render/ground/scatter_runtime.h"

namespace {

using std::uint32_t;
using namespace Render::Ground;

constexpr float k_arcane_ore_glow = 1.05F;
constexpr float k_haunted_ore_bonus = 0.30F;

} // namespace

namespace Render::GL {

IronOreRenderer::IronOreRenderer() = default;
IronOreRenderer::~IronOreRenderer() = default;

void IronOreRenderer::configure(const Game::Map::TerrainHeightMap& height_map,
                                const Game::Map::BiomeSettings& biome_settings,
                                const std::vector<Game::Map::WorldProp>& world_props) {
  configure_biome_common(biome_settings);
  m_state.track_visible_instances = true;
  m_state.params.light_direction = m_light_direction;

  m_state.params.magic_strength =
      k_arcane_ore_glow +
      k_haunted_ore_bonus * world().terrain_or_empty().supernatural_presence();
  generate_instances(world_props, height_map);
}

void IronOreRenderer::set_light_direction(const QVector3D& dir) {
  set_light_direction_common(dir, PropBatchParams::default_light_direction());
}

void IronOreRenderer::submit(FrameSubmitter& renderer, const DrawResources* resources) {
  if (submit_prop_common(renderer, resources, TerrainScatterCmd::Species::IronOre) ==
      0) {
    return;
  }

  for (const auto& inst : m_state.visible_instances) {
    const QVector3D ore_pos = inst.pos_scale.toVector3D();
    const float scale = std::max(inst.pos_scale.w(), 0.1F);
    const float phase = inst.color_rot.w();
    const float time = m_state.params.time;
    const float pulse = 0.84F + 0.16F * std::sin((time * 1.9F) + phase);
    Render::LocalLight votive;
    votive.position = ore_pos + QVector3D(0.0F, scale * 0.6F, 0.0F);
    votive.color = QVector3D(0.95F, 0.18F, 0.16F);
    votive.radius = std::clamp(scale * 2.8F, 3.0F, 8.0F);
    votive.intensity = 0.70F * pulse;
    renderer.local_light(votive);
  }
}

void IronOreRenderer::generate_instances(
    const std::vector<Game::Map::WorldProp>& world_props,
    const Game::Map::TerrainHeightMap& height_map) {
  adopt_generated(Ground::Scatter::generate_iron_ore(
      {{}, nullptr, m_biome_settings, world().terrain_or_empty(), world_props}));
}

auto IronOreRenderer::plan_world_props_refresh(
    std::vector<Game::Map::WorldProp> world_props,
    std::shared_ptr<const Game::Map::ScatterWorld> world)
    -> Ground::Scatter::ScatterJob {
  return [this,
          biome = m_biome_settings,
          world_props = std::move(world_props),
          world = std::move(world)]() mutable -> Ground::Scatter::ScatterAdoption {
    auto instances = Ground::Scatter::generate_iron_ore(
        {{}, nullptr, biome, world->terrain(), world_props, world.get()});
    const float magic_strength =
        k_arcane_ore_glow +
        k_haunted_ore_bonus * world->terrain().supernatural_presence();
    return [this, magic_strength, instances = std::move(instances)]() mutable {
      m_state.params.magic_strength = magic_strength;
      adopt_generated(std::move(instances));
    };
  };
}

} // namespace Render::GL
