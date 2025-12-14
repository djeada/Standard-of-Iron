#include "skirmish_loader.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/json_keys.h"
#include "game/map/level_loader.h"
#include "game/map/map_transformer.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/command_service.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/selection_system.h"
#include "game/systems/troop_count_registry.h"
#include "game/visuals/team_colors.h"
#include "render/ground/biome_renderer.h"
#include "render/ground/bridge_renderer.h"
#include "render/ground/firecamp_renderer.h"
#include "render/ground/fog_renderer.h"
#include "render/ground/ground_renderer.h"
#include "render/ground/olive_renderer.h"
#include "render/ground/pine_renderer.h"
#include "render/ground/plant_renderer.h"
#include "render/ground/river_renderer.h"
#include "render/ground/riverbank_renderer.h"
#include "render/ground/road_renderer.h"
#include "render/ground/stone_renderer.h"
#include "render/ground/terrain_renderer.h"
#include "render/scene_renderer.h"
#include "units/spawn_type.h"
#include "units/troop_type.h"
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>
#include <algorithm>
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
#include <set>
#include <unordered_map>
#include <vector>

namespace Game::Map {

using namespace JsonKeys;

SkirmishLoader::SkirmishLoader(Engine::Core::World &world,
                               Render::GL::Renderer &renderer,
                               Render::GL::Camera &camera)
    : m_world(world), m_renderer(renderer), m_camera(camera) {}

void SkirmishLoader::reset_game_state() {
  if (auto *selection_system =
          m_world.get_system<Game::Systems::SelectionSystem>()) {
    selection_system->clear_selection();
  }

  m_renderer.pause();
  m_renderer.lock_world_for_modification();
  m_renderer.set_selected_entities({});
  m_renderer.set_hovered_entity_id(0);

  m_world.clear();

  Game::Systems::BuildingCollisionRegistry::instance().clear();

  auto &owner_registry = Game::Systems::OwnerRegistry::instance();
  owner_registry.clear();

  Game::Map::MapTransformer::clearPlayerTeamOverrides();

  auto &visibility_service = Game::Map::VisibilityService::instance();
  visibility_service.reset();

  auto &terrain_service = Game::Map::TerrainService::instance();
  terrain_service.clear();

  auto &stats_registry = Game::Systems::GlobalStatsRegistry::instance();
  stats_registry.clear();

  auto &troop_registry = Game::Systems::TroopCountRegistry::instance();
  troop_registry.clear();

  Game::Systems::NationRegistry::instance().clear_player_assignments();

  if (m_fog != nullptr) {
    m_fog->update_mask(0, 0, 1.0F, {});
  }
}

auto SkirmishLoader::start(const QString &map_path,
                           const QVariantList &playerConfigs,
                           int selected_player_id,
                           int &out_selected_player_id) -> SkirmishLoadResult {
  SkirmishLoadResult result;

  reset_game_state();

  QSet<int> map_player_ids;
  QFile map_file(map_path);
  if (map_file.open(QIODevice::ReadOnly)) {
    const QByteArray data = map_file.readAll();
    map_file.close();
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
      QJsonObject obj = doc.object();
      if (obj.contains(SPAWNS) && obj[SPAWNS].isArray()) {
        const QJsonArray spawns = obj[SPAWNS].toArray();
        for (const auto &spawn_val : spawns) {
          if (spawn_val.isObject()) {
            QJsonObject spawn = spawn_val.toObject();
            if (spawn.contains(PLAYER_ID)) {
              const int player_id = spawn[PLAYER_ID].toInt();
              if (player_id > 0) {
                map_player_ids.insert(player_id);
              }
            }
          }
        }
      }
    }
  } else {
    qWarning() << "Could not open map file for reading player IDs:" << map_path;
  }

  auto &owner_registry = Game::Systems::OwnerRegistry::instance();

  int player_owner_id = selected_player_id;

  if (!map_player_ids.contains(player_owner_id)) {
    if (!map_player_ids.isEmpty()) {
      QList<int> sorted_ids = map_player_ids.values();
      std::sort(sorted_ids.begin(), sorted_ids.end());
      player_owner_id = sorted_ids.first();
      qWarning() << "Selected player ID" << selected_player_id
                 << "not found in map spawns. Using" << player_owner_id
                 << "instead.";
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

  if (!playerConfigs.isEmpty()) {

    for (const QVariant &config_var : playerConfigs) {
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
              Game::Systems::nationIDFromString(nation_id_str.toStdString());
          chosen_nation = parsed.value_or(
              Game::Systems::NationRegistry::instance().default_nation_id());
        } else {
          chosen_nation =
              Game::Systems::NationRegistry::instance().default_nation_id();
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
  for (const auto &[player_id, team_id] : team_overrides) {
    unique_teams.insert(team_id);
  }

  if (team_overrides.size() >= 2 && unique_teams.size() < 2) {
    result.errorMessage = "Invalid team configuration: At least two teams must "
                          "be selected to start a match.";
    m_renderer.unlock_world_for_modification();
    m_renderer.resume();
    qWarning() << "SkirmishLoader: " << result.errorMessage;
    return result;
  }

  Game::Map::MapTransformer::set_local_owner_id(player_owner_id);
  Game::Map::MapTransformer::setPlayerTeamOverrides(team_overrides);

  auto &nation_registry = Game::Systems::NationRegistry::instance();

  for (auto it = map_player_ids.begin(); it != map_player_ids.end(); ++it) {
    int player_id = *it;
    auto nat_it = nation_overrides.find(player_id);
    if (nat_it != nation_overrides.end()) {
      nation_registry.set_player_nation(player_id, nat_it->second);
    } else {
      nation_registry.set_player_nation(player_id,
                                        nation_registry.default_nation_id());
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
      map_path, m_world, m_renderer, m_camera);

  if (!level_result.ok && !level_result.errorMessage.isEmpty()) {
    result.errorMessage = level_result.errorMessage;
    m_renderer.unlock_world_for_modification();
    m_renderer.resume();
    return result;
  }

  constexpr float color_scale = 255.0F;
  constexpr int hex_color_length = 7;
  constexpr int hex_base = 16;

  if (!saved_player_configs.isEmpty()) {
    for (const QVariant &config_var : saved_player_configs) {
      const QVariantMap config = config_var.toMap();
      const int player_id = config.value("player_id", -1).toInt();
      const QString color_hex = config.value("colorHex", "#FFFFFF").toString();

      if (player_id >= 0 && color_hex.startsWith("#") &&
          color_hex.length() == hex_color_length) {
        bool conversion_ok = false;
        const int red = color_hex.mid(1, 2).toInt(&conversion_ok, hex_base);
        const int green = color_hex.mid(3, 2).toInt(&conversion_ok, hex_base);
        const int blue = color_hex.mid(5, 2).toInt(&conversion_ok, hex_base);
        owner_registry.set_owner_color(player_id, red / color_scale,
                                       green / color_scale, blue / color_scale);
      }
    }

    auto entities = m_world.get_entities_with<Engine::Core::UnitComponent>();
    std::unordered_map<int, int> owner_entity_count;
    for (auto *entity : entities) {
      auto *unit = entity->get_component<Engine::Core::UnitComponent>();
      auto *renderable =
          entity->get_component<Engine::Core::RenderableComponent>();
      if ((unit != nullptr) && (renderable != nullptr)) {
        const QVector3D team_color =
            Game::Visuals::team_colorForOwner(unit->owner_id);
        renderable->color[0] = team_color.x();
        renderable->color[1] = team_color.y();
        renderable->color[2] = team_color.z();
        owner_entity_count[unit->owner_id]++;
      }
    }
  }

  if (m_onOwnersUpdated) {
    m_onOwnersUpdated();
  }

  auto &terrain_service = Game::Map::TerrainService::instance();

  if (m_ground != nullptr) {
    if (level_result.ok) {
      m_ground->configure(level_result.tile_size, level_result.grid_width,
                          level_result.grid_height);
    } else {
      m_ground->configure_extent(50.0F);
    }
    if (terrain_service.is_initialized()) {
      m_ground->setBiome(terrain_service.biome_settings());
    }
  }

  if (m_terrain != nullptr) {
    if (terrain_service.is_initialized() &&
        (terrain_service.get_height_map() != nullptr)) {
      m_terrain->configure(*terrain_service.get_height_map(),
                           terrain_service.biome_settings());
    }
  }

  if (m_biome != nullptr) {
    if (terrain_service.is_initialized() &&
        (terrain_service.get_height_map() != nullptr)) {
      m_biome->configure(*terrain_service.get_height_map(),
                         terrain_service.biome_settings());
    }
  }

  if (m_river != nullptr) {
    if (terrain_service.is_initialized() &&
        (terrain_service.get_height_map() != nullptr)) {
      m_river->configure(terrain_service.get_height_map()->getRiverSegments(),
                         terrain_service.get_height_map()->getTileSize());
    }
  }

  if (m_road != nullptr) {
    if (terrain_service.is_initialized() &&
        (terrain_service.get_height_map() != nullptr)) {
      m_road->configure(terrain_service.road_segments(),
                        terrain_service.get_height_map()->getTileSize());
    }
  }

  if (m_riverbank != nullptr) {
    if (terrain_service.is_initialized() &&
        (terrain_service.get_height_map() != nullptr)) {
      m_riverbank->configure(
          terrain_service.get_height_map()->getRiverSegments(),
          *terrain_service.get_height_map());
    }
  }

  if (m_bridge != nullptr) {
    if (terrain_service.is_initialized() &&
        (terrain_service.get_height_map() != nullptr)) {
      m_bridge->configure(terrain_service.get_height_map()->getBridges(),
                          terrain_service.get_height_map()->getTileSize());
    }
  }

  if (m_stone != nullptr) {
    if (terrain_service.is_initialized() &&
        (terrain_service.get_height_map() != nullptr)) {
      m_stone->configure(*terrain_service.get_height_map(),
                         terrain_service.biome_settings());
    }
  }

  if (m_plant != nullptr) {
    if (terrain_service.is_initialized() &&
        (terrain_service.get_height_map() != nullptr)) {
      m_plant->configure(*terrain_service.get_height_map(),
                         terrain_service.biome_settings());
    }
  }

  if (m_pine != nullptr) {
    if (terrain_service.is_initialized() &&
        (terrain_service.get_height_map() != nullptr)) {
      m_pine->configure(*terrain_service.get_height_map(),
                        terrain_service.biome_settings());
    }
  }

  if (m_olive != nullptr) {
    if (terrain_service.is_initialized() &&
        (terrain_service.get_height_map() != nullptr)) {
      m_olive->configure(*terrain_service.get_height_map(),
                         terrain_service.biome_settings());
    }
  }

  if (m_firecamp != nullptr) {
    if (terrain_service.is_initialized() &&
        (terrain_service.get_height_map() != nullptr)) {
      m_firecamp->configure(*terrain_service.get_height_map(),
                            terrain_service.biome_settings());

      const auto &fire_camps = terrain_service.fire_camps();
      if (!fire_camps.empty()) {
        std::vector<QVector3D> positions;
        std::vector<float> intensities;
        std::vector<float> radii;

        const auto *height_map = terrain_service.get_height_map();
        const float tile_size = height_map->getTileSize();
        const int width = height_map->getWidth();
        const int height = height_map->getHeight();
        const float half_width = width * 0.5F;
        const float half_height = height * 0.5F;

        for (const auto &fc : fire_camps) {

          float const world_x = (fc.x - half_width) * tile_size;
          float const world_z = (fc.z - half_height) * tile_size;
          float const world_y =
              terrain_service.get_terrain_height(world_x, world_z);

          positions.emplace_back(world_x, world_y, world_z);
          intensities.push_back(fc.intensity);
          radii.push_back(fc.radius);
        }

        m_firecamp->setExplicitFireCamps(positions, intensities, radii);
      }
    }
  }

  constexpr int default_map_size = 100;
  const int map_width =
      level_result.ok ? level_result.grid_width : default_map_size;
  const int map_height =
      level_result.ok ? level_result.grid_height : default_map_size;
  Game::Systems::CommandService::initialize(map_width, map_height);

  auto &visibility_service = Game::Map::VisibilityService::instance();
  visibility_service.initialize(map_width, map_height, level_result.tile_size);
  
  if (is_spectator_mode) {
    visibility_service.reveal_all();
  } else {
    visibility_service.computeImmediate(m_world, player_owner_id);
  }

  if ((m_fog != nullptr) && visibility_service.is_initialized()) {
    m_fog->update_mask(
        visibility_service.getWidth(), visibility_service.getHeight(),
        visibility_service.getTileSize(), visibility_service.snapshotCells());

    if (m_onVisibilityMaskReady) {
      m_onVisibilityMaskReady();
    }
  }

  if (m_biome != nullptr) {
    m_biome->refreshGrass();
  }

  m_renderer.unlock_world_for_modification();
  m_renderer.resume();

  Engine::Core::Entity *focus_entity = nullptr;

  auto candidates = m_world.get_entities_with<Engine::Core::UnitComponent>();
  for (auto *entity : candidates) {
    if (entity == nullptr) {
      continue;
    }
    auto *unit = entity->get_component<Engine::Core::UnitComponent>();
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
    if (auto *transform =
            focus_entity->get_component<Engine::Core::TransformComponent>()) {
      result.focusPosition = QVector3D(
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
  result.victoryConfig = level_result.victoryConfig;
  result.is_spectator_mode = is_spectator_mode;

  return result;
}
} // namespace Game::Map
