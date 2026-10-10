#include "app/session/skirmish_loader.h"

#include <QCoreApplication>
#include <QDebug>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>
#include <QString>
#include <qdir.h>
#include <qfiledevice.h>
#include <qglobal.h>
#include <qjsonarray.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qlist.h>
#include <qset.h>
#include <qstringview.h>
#include <qvariant.h>
#include <qvectornd.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <mutex>
#include <set>
#include <unordered_map>
#include <vector>

#include "app/session/level_loader.h"
#include "game/core/component_core.h"
#include "game/core/world.h"
#include "game/map/json_keys.h"
#include "game/map/map_context.h"
#include "game/map/map_transformer.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/session/selection_service.h"
#include "game/session/session_context.h"
#include "game/systems/ai_system.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/systems/navigation/nav_grid.h"
#include "game/systems/navigation/wall_network_service.h"
#include "game/systems/owner_registry.h"
#include "game/systems/troop_count_registry.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"
#include "render/ground/ambient_fog_renderer.h"
#include "render/ground/biome_renderer.h"
#include "render/ground/firecamp_renderer.h"
#include "render/ground/fog_renderer.h"
#include "render/ground/ground_renderer.h"
#include "render/ground/map_boundary_fog_renderer.h"
#include "render/ground/plant_renderer.h"
#include "render/ground/rain_renderer.h"
#include "render/ground/stone_renderer.h"
#include "render/ground/terrain_feature_manager.h"
#include "render/ground/terrain_renderer.h"
#include "render/ground/terrain_scatter_manager.h"
#include "render/ground/tree_renderer.h"
#include "render/mist_volume.h"
#include "render/mist_volume_builder.h"
#include "render/scene_renderer.h"
#include "utils/resource_utils.h"

namespace App::Core {

using namespace Game::Map;

using namespace JsonKeys;

SkirmishLoader::SkirmishLoader(Engine::Core::World& world,
                               Render::GL::Renderer& renderer,
                               Render::GL::Camera& camera)
    : m_world(world)
    , m_renderer(renderer)
    , m_camera(camera) {
}

void SkirmishLoader::reset_game_state() {
  if (auto* selection_system = &Game::Session::session_for(m_world).selection()) {
    selection_system->clear_selection();
  }

  m_renderer.pause();
  m_renderer.lock_world_for_modification();
  const std::lock_guard<std::recursive_mutex> world_lock(m_world.get_entity_mutex());

  if (auto* ai_system = m_world.get_system<Game::Systems::AISystem>()) {
    ai_system->shutdown_workers();
  }

  m_renderer.set_selected_entities({});
  m_renderer.set_hovered_entity_id(0);

  m_world.clear();

  m_renderer.clear_entity_render_caches();

  auto& session = Game::Session::session_for(m_world);
  session.building_collision().clear();

  session.owners().clear();

  session.visibility().reset();

  session.terrain().clear();

  session.stats().clear();

  session.troop_counts().clear();

  session.nations().clear_player_assignments();

  if (m_fog != nullptr) {
    m_fog->set_enabled(true);
    m_fog->update_mask(0, 0, 1.0F, {});
  }
}

auto SkirmishLoader::start(const QString& map_path,
                           const QVariantList& player_configs,
                           int selected_player_id,
                           bool allow_default_player_barracks,
                           int& out_selected_player_id) -> SkirmishLoadResult {
  SkirmishLoadResult result;

  auto pump_events = []() {
    QCoreApplication::processEvents(QEventLoop::AllEvents);
  };

  reset_game_state();
  pump_events();

  QSet<int> map_player_ids;
  QString map_error;
  const Game::Map::MapContext map_context =
      Game::Map::MapContextStore::acquire(map_path, &map_error);
  if (const auto* map_def = map_context.definition(); map_def != nullptr) {
    for (const auto& spawn : map_def->spawns) {
      if (spawn.player_id > 0) {
        map_player_ids.insert(spawn.player_id);
      }
    }
    for (const auto& structure : map_def->structures) {
      if (structure.player_id > 0) {
        map_player_ids.insert(structure.player_id);
      }
    }
  } else {
    qWarning() << "Could not load map for reading player IDs:" << map_path << map_error;
  }

  auto& session = Game::Session::session_for(m_world);
  auto& owner_registry = session.owners();

  int player_owner_id = selected_player_id;

  if (!map_player_ids.contains(player_owner_id)) {
    if (!map_player_ids.isEmpty()) {
      QList<int> sorted_ids = map_player_ids.values();
      std::sort(sorted_ids.begin(), sorted_ids.end());
      player_owner_id = sorted_ids.first();
      qWarning() << "Selected player ID" << selected_player_id
                 << "not found in map spawns. Using" << player_owner_id << "instead.";
      out_selected_player_id = player_owner_id;
    } else {
      qWarning() << "No valid player spawns found in map. Using default "
                    "player ID"
                 << player_owner_id;
    }
  }

  owner_registry.set_local_player_id(player_owner_id);

  std::unordered_map<int, int> team_overrides;
  std::unordered_map<int, Game::Systems::NationID> nation_overrides;
  std::unordered_map<int, QString> base_assignments;
  QVariantList saved_player_configs;
  std::set<int> processed_player_ids;
  bool is_spectator_mode = false;
  bool has_human_player = false;

  if (!player_configs.isEmpty()) {

    for (const QVariant& config_var : player_configs) {
      const QVariantMap config = config_var.toMap();
      int player_id = config.value("player_id", -1).toInt();
      const int team_id = config.value("team_id", 0).toInt();

      const QString color_hex = config.value("colorHex").toString();
      const bool is_human = config.value("isHuman", false).toBool();
      const QString nation_id_str = config.value("nationId").toString();
      const QString base_key = config.value("baseKey").toString();

      if (is_human) {
        has_human_player = true;
        if (player_id != player_owner_id) {
          player_id = player_owner_id;
        }
      }

      if (processed_player_ids.contains(player_id)) {
        continue;
      }

      if (player_id >= 0) {
        processed_player_ids.insert(player_id);
        team_overrides[player_id] = team_id;

        Game::Systems::NationID chosen_nation;
        if (!nation_id_str.isEmpty()) {
          auto parsed =
              Game::Systems::nation_id_from_string(nation_id_str.toStdString());
          chosen_nation = parsed.value_or(session.nations().default_nation_id());
        } else {
          chosen_nation = session.nations().default_nation_id();
        }
        nation_overrides[player_id] = chosen_nation;
        if (!base_key.isEmpty()) {
          base_assignments[player_id] = base_key;
        }

        QVariantMap updated_config = config;
        updated_config["player_id"] = player_id;
        saved_player_configs.append(updated_config);
      }
    }

    is_spectator_mode = !has_human_player && !saved_player_configs.isEmpty();
  }

  std::set<int> unique_teams;
  for (const auto& [player_id, team_id] : team_overrides) {
    unique_teams.insert(team_id);
  }

  if (team_overrides.size() >= 2 && unique_teams.size() < 2) {
    result.error_message =
        QCoreApplication::translate("SkirmishLoader",
                                    "Invalid team configuration: At least two teams "
                                    "must be selected to start a match.");
    m_renderer.unlock_world_for_modification();
    m_renderer.resume();
    qWarning() << "SkirmishLoader: " << result.error_message;
    return result;
  }

  result.resolved_player_configs = saved_player_configs;

  Game::Map::MapTransformer::set_local_owner_id(player_owner_id);
  const Game::Map::MapTransformOptions transform_options{
      .player_team_overrides = team_overrides,
      .base_assignments = base_assignments,
      .spectator_mode = is_spectator_mode};

  auto& nation_registry = session.nations();

  for (int const player_id : map_player_ids) {
    auto nat_it = nation_overrides.find(player_id);
    if (nat_it != nation_overrides.end()) {
      nation_registry.set_player_nation(player_id, nat_it->second);
    } else {
      nation_registry.set_player_nation(player_id, nation_registry.default_nation_id());
    }
  }

  if (map_player_ids.isEmpty()) {
    auto nat_it = nation_overrides.find(player_owner_id);
    if (nat_it != nation_overrides.end()) {
      nation_registry.set_player_nation(player_owner_id, nat_it->second);
    } else {
      nation_registry.set_player_nation(player_owner_id,
                                        nation_registry.default_nation_id());
    }
  }

  auto level_result =
      App::Core::LevelLoader::loadFromAssets(map_path,
                                             m_world,
                                             m_renderer,
                                             m_camera,
                                             allow_default_player_barracks,
                                             transform_options);
  pump_events();

  if (!level_result.ok && !level_result.error_message.isEmpty()) {
    result.error_message = level_result.error_message;
    m_renderer.unlock_world_for_modification();
    m_renderer.resume();
    return result;
  }

  constexpr float color_scale = 255.0F;
  constexpr int hex_color_length = 7;
  constexpr int hex_base = 16;

  if (!saved_player_configs.isEmpty()) {
    for (const QVariant& config_var : saved_player_configs) {
      const QVariantMap config = config_var.toMap();
      const int player_id = config.value("player_id", -1).toInt();

      const QString color_hex = config.value("colorHex").toString();

      if (player_id >= 0 && color_hex.startsWith("#") &&
          color_hex.length() == hex_color_length) {
        bool conversion_ok = false;
        const int red = color_hex.mid(1, 2).toInt(&conversion_ok, hex_base);
        const int green = color_hex.mid(3, 2).toInt(&conversion_ok, hex_base);
        const int blue = color_hex.mid(5, 2).toInt(&conversion_ok, hex_base);
        owner_registry.set_owner_color(
            player_id, red / color_scale, green / color_scale, blue / color_scale);
      }
    }
  }
  pump_events();

  if (m_on_owners_updated) {
    m_on_owners_updated();
  }

  auto& terrain_service = session.terrain();

  if (m_ground != nullptr) {
    if (level_result.ok) {
      m_ground->configure(
          level_result.tile_size, level_result.grid_width, level_result.grid_height);
    } else {
      m_ground->configure_extent(50.0F);
    }
    if (terrain_service.is_initialized()) {
      m_ground->set_biome(terrain_service.biome_settings());
    }
  }

  if (m_terrain != nullptr) {
    if (terrain_service.is_initialized() &&
        (terrain_service.get_height_map() != nullptr)) {
      m_terrain->configure(*terrain_service.get_height_map(),
                           terrain_service.biome_settings());
    }
  }

  if (m_scatter != nullptr) {
    if (terrain_service.is_initialized() &&
        (terrain_service.get_height_map() != nullptr)) {
      m_scatter->configure(*terrain_service.get_height_map(),
                           terrain_service.biome_settings(),
                           terrain_service.authored_world_props(),
                           terrain_service.world_props());
    }
  }

  if (m_features != nullptr) {
    if (terrain_service.is_initialized() &&
        (terrain_service.get_height_map() != nullptr)) {
      m_features->configure(*terrain_service.get_height_map(),
                            terrain_service.road_segments(),
                            terrain_service.biome_settings());
    }
  }

  if (level_result.ok) {
    const auto& lighting = level_result.lighting_state;
    const QVector3D light_dir = lighting.primary_direction;
    m_renderer.set_environment_lighting(lighting);
    if (m_ground != nullptr) {
      m_ground->set_light_direction(light_dir);
    }
    if (m_terrain != nullptr) {
      m_terrain->set_light_direction(light_dir);
    }
    if (m_scatter != nullptr) {
      m_scatter->set_light_direction(light_dir);
    }
  }

  pump_events();

  if (m_rain != nullptr) {
    const float world_width = level_result.grid_width * level_result.tile_size;
    const float world_height = level_result.grid_height * level_result.tile_size;
    m_rain->configure(world_width, world_height, level_result.biome_seed);
    m_rain->set_enabled(level_result.rain_settings.enabled);
    m_rain->set_intensity(level_result.rain_settings.enabled
                              ? level_result.rain_settings.intensity
                              : 0.0F);
  }

  if (m_boundary_fog != nullptr) {
    m_boundary_fog->configure(
        level_result.grid_width, level_result.grid_height, level_result.tile_size);
  }

  if (m_ambient_fog != nullptr && !level_result.fog_zones.empty()) {
    std::vector<Game::Map::FogZone> fog_zones = level_result.fog_zones;
    for (auto& zone : fog_zones) {
      zone.y = terrain_service.resolve_surface_world_y(zone.x, zone.z, 0.0F);
    }
    m_ambient_fog->configure(fog_zones);
  }

  m_renderer.set_mist_volumes(Render::build_mist_volumes(
      {.fog_zones = &level_result.fog_zones,
       .rivers = &level_result.rivers,
       .lakes = &level_result.lakes},
      [&terrain_service](float world_x, float world_z) {
        return terrain_service.is_initialized()
                   ? terrain_service.resolve_surface_world_y(world_x, world_z, 0.0F)
                   : 0.0F;
      }));

  constexpr int default_map_size = 100;
  const int map_width = level_result.ok ? level_result.grid_width : default_map_size;
  const int map_height = level_result.ok ? level_result.grid_height : default_map_size;
  Game::Systems::NavGrid::initialize(map_width, map_height);
  Game::Systems::WallNetworkService::refresh_world(m_world);

  if (m_on_visibility_initialized) {
    m_on_visibility_initialized(m_world,
                                player_owner_id,
                                map_width,
                                map_height,
                                level_result.tile_size,
                                is_spectator_mode,
                                level_result.scouted_areas);
  }
  pump_events();

  if (m_scatter != nullptr) {
    m_scatter->refresh_grass();
  }

  m_renderer.unlock_world_for_modification();
  m_renderer.resume();

  Engine::Core::Entity* focus_entity = nullptr;

  auto candidates = m_world.collect_entities_with<Engine::Core::UnitComponent>();
  for (auto* entity : candidates) {
    if (entity == nullptr) {
      continue;
    }
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      continue;
    }
    if (unit->spawn_type == Game::Units::SpawnType::Barracks &&
        unit->owner_id == player_owner_id && unit->health > 0) {
      focus_entity = entity;
      break;
    }
  }

  if ((focus_entity == nullptr) && level_result.player_unit_id != 0U) {
    focus_entity = m_world.get_entity(level_result.player_unit_id);
  }

  if (focus_entity != nullptr) {
    if (auto* transform =
            focus_entity->get_component<Engine::Core::TransformComponent>()) {
      result.focus_position = QVector3D(
          transform->position.x, transform->position.y, transform->position.z);
      result.has_focus_position = true;
    }
  }

  result.ok = true;
  result.map_name = level_result.map_name;
  result.player_unit_id = level_result.player_unit_id;
  result.cam_fov = level_result.cam_fov;
  result.cam_near = level_result.cam_near;
  result.cam_far = level_result.cam_far;
  result.grid_width = level_result.grid_width;
  result.grid_height = level_result.grid_height;
  result.tile_size = level_result.tile_size;
  result.max_troops_per_player = level_result.max_troops_per_player;
  result.victory_config = level_result.victory_config;
  result.rain_settings = level_result.rain_settings;
  result.biome_seed = level_result.biome_seed;
  result.lighting_state = level_result.lighting_state;
  result.environment = level_result.environment;
  result.is_spectator_mode = is_spectator_mode;

  return result;
}
} // namespace App::Core
