#include "match_loader.h"

#include <QCoreApplication>
#include <QDebug>

#include <memory>

#include "../core/component_core.h"
#include "../core/world.h"
#include "../session/session_context.h"
#include "../systems/nation_registry.h"
#include "../systems/owner_registry.h"
#include "../units/factory.h"
#include "../units/spawn_type.h"
#include "../units/unit.h"
#include "environment_lighting.h"
#include "map_loader.h"
#include "map_transformer.h"
#include "terrain_service.h"
#include "utils/resource_utils.h"

namespace Game::Map {

namespace {

void spawn_fallback_archer(Engine::Core::World& world, MatchLoadResult& result) {
  auto& session = Game::Session::session_for(world);
  auto& nations = session.nations();
  auto registry = MapTransformer::get_factory_registry();
  if (!registry) {
    return;
  }

  Game::Units::SpawnParams params;
  params.position = QVector3D(0.0F, 0.0F, 0.0F);
  params.player_id = 0;
  params.spawn_type = Game::Units::SpawnType::Archer;
  params.ai_controlled = !session.owners().is_player(params.player_id);
  if (const auto* nation = nations.get_nation_for_player(params.player_id)) {
    params.nation_id = nation->id;
  } else {
    params.nation_id = nations.default_nation_id();
  }

  if (auto unit = registry->create(Game::Units::SpawnType::Archer, world, params)) {
    result.player_unit_id = unit->id();
  } else {
    qWarning() << "MatchLoader: fallback archer spawn failed";
  }
}

[[nodiscard]] auto local_player_has_barracks(Engine::Core::World& world) -> bool {
  auto& owners = Game::Session::session_for(world).owners();
  for (auto [entity_id, unit] : world.view<Engine::Core::UnitComponent>()) {
    if (unit.spawn_type == Game::Units::SpawnType::Barracks &&
        owners.is_player(unit.owner_id)) {
      return true;
    }
  }
  return false;
}

void spawn_default_barracks(Engine::Core::World& world) {
  auto& session = Game::Session::session_for(world);
  auto& nations = session.nations();
  auto registry = MapTransformer::get_factory_registry();
  if (!registry) {
    return;
  }

  Game::Units::SpawnParams params;
  params.position = QVector3D(-4.0F, 0.0F, -3.0F);
  params.player_id = session.owners().get_local_player_id();
  params.spawn_type = Game::Units::SpawnType::Barracks;
  params.ai_controlled = !session.owners().is_player(params.player_id);
  if (const auto* nation = nations.get_nation_for_player(params.player_id)) {
    params.nation_id = nation->id;
  } else {
    params.nation_id = nations.default_nation_id();
  }
  registry->create(Game::Units::SpawnType::Barracks, world, params);
}

} // namespace

auto load_match(const QString& map_path,
                Engine::Core::World& world,
                bool allow_default_player_barracks) -> MatchLoadResult {
  MatchLoadResult result;

  auto& session = Game::Session::session_for(world);

  auto units = std::make_shared<Game::Units::UnitFactoryRegistry>();
  Game::Units::register_built_in_units(*units);
  MapTransformer::setFactoryRegistry(units);

  const QString resolved = Utils::Resources::resolve_resource_path(map_path);

  MapDefinition definition;
  QString error;
  if (!MapLoader::load_from_json_file(resolved, definition, &error)) {
    result.ok = false;
    result.error_message =
        QCoreApplication::translate("LevelLoader", "Map load failed: %1").arg(error);
    qWarning() << "MatchLoader: map load failed:" << error << "(path:" << resolved
               << ')';
    return result;
  }

  result.ok = true;
  result.map_name = definition.name;
  result.rain_settings = definition.rain;
  result.fog_zones = definition.fog_zones;
  result.rivers = definition.rivers;
  result.lakes = definition.lakes;
  result.biome_seed = definition.biome.seed;
  result.environment = definition.environment;

  const EnvironmentClock initial_clock(definition.environment);
  result.lighting_state = initial_clock.lighting(
      {.rain = definition.rain.enabled && definition.rain.type == WeatherType::Rain
                   ? definition.rain.intensity
                   : 0.0F,
       .snow = definition.rain.enabled && definition.rain.type == WeatherType::Snow
                   ? definition.rain.intensity
                   : 0.0F});

  session.terrain().initialize(definition);

  result.grid_width = definition.grid.width;
  result.grid_height = definition.grid.height;
  result.tile_size = definition.grid.tile_size;
  result.max_troops_per_player = definition.max_troops_per_player;
  result.victory_config = definition.victory;

  auto transformed = MapTransformer::apply_to_world(definition, world);
  if (!transformed.unit_ids.empty()) {
    result.player_unit_id = transformed.unit_ids.front();
  } else {
    spawn_fallback_archer(world, result);
  }

  if (allow_default_player_barracks && !local_player_has_barracks(world)) {
    spawn_default_barracks(world);
  }

  result.definition = std::move(definition);
  return result;
}

} // namespace Game::Map
