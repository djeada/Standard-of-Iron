#include "app/session/level_loader.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <qglobal.h>
#include <qstringliteral.h>

#include <memory>

#include "app/session/environment.h"
#include "game/core/component_core.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/map_transformer.h"
#include "game/map/match_loader.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"
#include "game/units/unit.h"
#include "render/scene_renderer.h"
#include "scene/camera.h"
#include "utils/resource_utils.h"

namespace App::Core {

using namespace Game::Map;

auto LevelLoader::loadFromAssets(const QString& map_path,
                                 Engine::Core::World& world,
                                 Render::GL::Renderer& renderer,
                                 Render::GL::Camera& camera,
                                 bool allow_default_player_barracks)
    -> LevelLoadResult {
  const Game::Map::MatchLoadResult match =
      Game::Map::load_match(map_path, world, allow_default_player_barracks);

  LevelLoadResult res;
  res.ok = match.ok;
  res.map_name = match.map_name;
  res.error_message = match.error_message;
  res.player_unit_id = match.player_unit_id;
  res.grid_width = match.grid_width;
  res.grid_height = match.grid_height;
  res.tile_size = match.tile_size;
  res.max_troops_per_player = match.max_troops_per_player;
  res.victory_config = match.victory_config;
  res.rain_settings = match.rain_settings;
  res.fog_zones = match.fog_zones;
  res.rivers = match.rivers;
  res.lakes = match.lakes;
  res.biome_seed = match.biome_seed;
  res.lighting_state = match.lighting_state;
  res.environment = match.environment;

  if (match.ok) {
    App::Core::Environment::apply(match.definition, renderer, camera);
    res.cam_fov = match.definition.camera.fov_y;
    res.cam_near = match.definition.camera.near_plane;
    res.cam_far = match.definition.camera.far_plane;
    return res;
  }

  App::Core::Environment::apply_default(renderer, camera);
  res.cam_fov = camera.get_fov();
  res.cam_near = camera.get_near();
  res.cam_far = camera.get_far();
  return res;
}

} // namespace App::Core
