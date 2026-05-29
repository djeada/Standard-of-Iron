#include "skirmish_loader.h"

#include <QCoreApplication>
#include <QDebug>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>
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
#include <mutex>
#include <set>
#include <unordered_map>
#include <vector>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/json_keys.h"
#include "game/map/level_loader.h"
#include "game/map/map_transformer.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/systems/ai_system.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/command_service.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/marketplace_system.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/selection_system.h"
#include "game/systems/troop_count_registry.h"
#include "game/systems/wall_network_service.h"
#include "game/visuals/team_colors.h"
#include "render/ground/ambient_fog_renderer.h"
#include "render/ground/biome_renderer.h"
#include "render/ground/firecamp_renderer.h"
#include "render/ground/fog_renderer.h"
#include "render/ground/ground_renderer.h"
#include "render/ground/map_boundary_fog_renderer.h"
#include "render/ground/olive_renderer.h"
#include "render/ground/pine_renderer.h"
#include "render/ground/plant_renderer.h"
#include "render/ground/rain_renderer.h"
#include "render/ground/stone_renderer.h"
#include "render/ground/terrain_feature_manager.h"
#include "render/ground/terrain_renderer.h"
#include "render/ground/terrain_scatter_manager.h"
#include "render/scene_renderer.h"
#include "units/spawn_type.h"
#include "units/troop_type.h"
#include "utils/resource_utils.h"

namespace Game::Map {

using namespace JsonKeys;

SkirmishLoader::SkirmishLoader(Engine::Core::World& world,
                               Render::GL::Renderer& renderer,
                               Render::GL::Camera& camera)
    : m_world(world)
    , m_renderer(renderer)
    , m_camera(camera) {
}

void SkirmishLoader::reset_game_state() {
  if (auto* selection_system = m_world.get_system<Game::Systems::SelectionSystem>()) {
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

  Game::Systems::BuildingCollisionRegistry::instance().clear();
  Game::Systems::MarketplaceSystem::instance().clear();

  auto& owner_registry = Game::Systems::OwnerRegistry::instance();
  owner_registry.clear();

  Game::Map::MapTransformer::clear_player_team_overrides();

  auto& visibility_service = Game::Map::VisibilityService::instance();
  visibility_service.reset();

  auto& terrain_service = Game::Map::TerrainService::instance();
  terrain_service.clear();

  auto& stats_registry = Game::Systems::GlobalStatsRegistry::instance();
  stats_registry.clear();

  auto& troop_registry = Game::Systems::TroopCountRegistry::instance();
  troop_registry.clear();

  Game::Systems::NationRegistry::instance().clear_player_assignments();

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
  const QString resolved_map_path = Utils::Resources::resolve_resource_path(map_path);
  QFile map_file(resolved_map_path);
  if (map_file.open(QIODevice::ReadOnly)) {
    const QByteArray data = map_file.readAll();
    map_file.close();
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
      QJsonObject const obj = doc.object();
      auto collect_player_ids = [&map_player_ids, &obj](const char* key) {
        if (!obj.contains(key) || !obj[key].isArray()) {
          return;
        }

        const QJsonArray entries = obj[key].toArray();
        for (const auto& entry_val : entries) {
          if (!entry_val.isObject()) {
            continue;
          }
          QJsonObject const entry = entry_val.toObject();
          if (!entry.contains(PLAYER_ID)) {
            continue;
          }

          const int player_id = entry[PLAYER_ID].toInt();
          if (player_id > 0) {
            map_player_ids.insert(player_id);
          }
        }
      };

      collect_player_ids(SPAWNS);
      collect_player_ids(BUILDINGS);
      collect_player_ids(WALLS);
    }
  } else {
    qWarning() << "Could not open map file for reading player IDs:"
               << resolved_map_path;
  }

  auto& owner_registry = Game::Systems::OwnerRegistry::instance();

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
  QVariantList saved_player_configs;
  std::set<int> processed_player_ids;
  bool is_spectator_mode = false;
  bool has_human_player = false;

  if (!player_configs.isEmpty()) {

    for (const QVariant& config_var : player_configs) {
      const QVariantMap config = config_var.toMap();
      int player_id = config.value("player_id", -1).toInt();
      const int team_id = config.value("team_id", 0).toInt();
      const QString color_hex = config.value("colorHex", "#FFFFFF").toString();
      const bool is_human = config.value("isHuman", false).toBool();
      const QString nation_id_str = config.value("nationId").toString();

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
          chosen_nation = parsed.value_or(
              Game::Systems::NationRegistry::instance().default_nation_id());
        } else {
          chosen_nation = Game::Systems::NationRegistry::instance().default_nation_id();
        }
        nation_overrides[player_id] = chosen_nation;

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
    result.error_message = "Invalid team configuration: At least two teams must "
                           "be selected to start a match.";
    m_renderer.unlock_world_for_modification();
    m_renderer.resume();
    qWarning() << "SkirmishLoader: " << result.error_message;
    return result;
  }

  Game::Map::MapTransformer::set_local_owner_id(player_owner_id);
  Game::Map::MapTransformer::setPlayerTeamOverrides(team_overrides);

  auto& nation_registry = Game::Systems::NationRegistry::instance();

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

  auto level_result = Game::Map::LevelLoader::loadFromAssets(
      map_path, m_world, m_renderer, m_camera, allow_default_player_barracks);
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
      const QString color_hex = config.value("colorHex", "#FFFFFF").toString();

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

    auto entities = m_world.get_entities_with<Engine::Core::UnitComponent>();
    pump_events();
    std::unordered_map<int, int> owner_entity_count;
    for (auto* entity : entities) {
      auto* unit = entity->get_component<Engine::Core::UnitComponent>();
      auto* renderable = entity->get_component<Engine::Core::RenderableComponent>();
      if ((unit != nullptr) && (renderable != nullptr)) {
        const QVector3D team_color = Game::Visuals::team_colorForOwner(unit->owner_id);
        renderable->color[0] = team_color.x();
        renderable->color[1] = team_color.y();
        renderable->color[2] = team_color.z();
        owner_entity_count[unit->owner_id]++;
      }
    }
  }
  pump_events();

  if (m_on_owners_updated) {
    m_on_owners_updated();
  }

  auto& terrain_service = Game::Map::TerrainService::instance();

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
                            terrain_service.road_segments());
    }
  }

  if (level_result.ok) {
    const QVector3D light_dir = level_result.lighting_settings.light_direction;
    const float ambient = level_result.lighting_settings.ambient_strength;
    m_renderer.set_lighting(light_dir, ambient);
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
    m_ambient_fog->configure(level_result.fog_zones);
  }

  constexpr int default_map_size = 100;
  const int map_width = level_result.ok ? level_result.grid_width : default_map_size;
  const int map_height = level_result.ok ? level_result.grid_height : default_map_size;
  Game::Systems::CommandService::initialize(map_width, map_height);
  Game::Systems::WallNetworkService::refresh_world(m_world);

  if (m_on_visibility_initialized) {
    m_on_visibility_initialized(m_world,
                                player_owner_id,
                                map_width,
                                map_height,
                                level_result.tile_size,
                                is_spectator_mode);
  }
  pump_events();

  if (m_scatter != nullptr) {
    m_scatter->refresh_grass();
  }

  m_renderer.unlock_world_for_modification();
  m_renderer.resume();

  Engine::Core::Entity* focus_entity = nullptr;

  auto candidates = m_world.get_entities_with<Engine::Core::UnitComponent>();
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
  result.lighting_settings = level_result.lighting_settings;
  result.is_spectator_mode = is_spectator_mode;

  return result;
}
} // namespace Game::Map
