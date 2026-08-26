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
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/json_keys.h"
#include "game/map/map_transformer.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/session/session_context.h"
#include "game/systems/ai_system.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/selection_system.h"
#include "game/systems/troop_count_registry.h"
#include "game/systems/wall_network_service.h"
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
#include "render/scene_renderer.h"
#include "utils/resource_utils.h"

namespace App::Core {

using namespace Game::Map;

using namespace JsonKeys;

namespace {

constexpr float k_water_mist_strength = 0.42F;
constexpr float k_miasma_strength_base = 0.30F;
constexpr float k_miasma_strength_per_density = 0.55F;

void simplify_polyline(const std::vector<QVector2D>& points,
                       std::size_t first,
                       std::size_t last,
                       float epsilon,
                       std::vector<bool>& keep) {
  if (last <= first + 1) {
    return;
  }
  const QVector2D chord = points[last] - points[first];
  const float chord_length = chord.length();
  float worst_distance = -1.0F;
  std::size_t worst_index = first;
  for (std::size_t i = first + 1; i < last; ++i) {
    const QVector2D offset = points[i] - points[first];
    const float distance =
        chord_length > 1e-4F
            ? std::abs(offset.x() * chord.y() - offset.y() * chord.x()) / chord_length
            : offset.length();
    if (distance > worst_distance) {
      worst_distance = distance;
      worst_index = i;
    }
  }
  if (worst_distance > epsilon) {
    keep[worst_index] = true;
    simplify_polyline(points, first, worst_index, epsilon, keep);
    simplify_polyline(points, worst_index, last, epsilon, keep);
  }
}

auto build_mist_volumes(const LevelLoadResult& level,
                        const Game::Map::TerrainService& terrain)
    -> std::vector<Render::MistVolume> {
  std::vector<Render::MistVolume> volumes;

  auto surface_y = [&terrain](float world_x, float world_z) {
    return terrain.is_initialized()
               ? terrain.resolve_surface_world_y(world_x, world_z, 0.0F)
               : 0.0F;
  };

  for (const auto& zone : level.fog_zones) {
    Render::MistVolume mist;
    const float base_y = surface_y(zone.x, zone.z);
    mist.start = QVector3D(zone.x, base_y, zone.z);
    mist.end = mist.start;
    mist.radius = std::max(std::max(zone.width, zone.height) * 0.5F, 1.0F);
    mist.strength = std::clamp(k_miasma_strength_base +
                                   zone.density * k_miasma_strength_per_density,
                               0.0F,
                               0.85F);
    mist.kind = Render::MistVolume::Kind::Miasma;
    volumes.push_back(mist);
  }

  constexpr float k_join_epsilon_sq = 0.25F;
  std::size_t river_index = 0;
  while (river_index < level.rivers.size()) {
    const float width = level.rivers[river_index].width;
    std::vector<QVector2D> points;
    points.emplace_back(level.rivers[river_index].start.x(),
                        level.rivers[river_index].start.z());
    while (river_index < level.rivers.size()) {
      const auto& segment = level.rivers[river_index];
      const QVector2D seg_start(segment.start.x(), segment.start.z());
      if ((seg_start - points.back()).lengthSquared() > k_join_epsilon_sq) {
        break;
      }
      points.emplace_back(segment.end.x(), segment.end.z());
      ++river_index;
    }

    std::vector<bool> keep(points.size(), false);
    keep.front() = true;
    keep.back() = true;
    simplify_polyline(points, 0, points.size() - 1, std::max(1.5F, width * 0.4F), keep);

    QVector2D previous = points.front();
    for (std::size_t i = 1; i < points.size(); ++i) {
      if (!keep[i]) {
        continue;
      }
      const QVector2D mid = (previous + points[i]) * 0.5F;
      Render::MistVolume mist;
      const float base_y = surface_y(mid.x(), mid.y());
      mist.start = QVector3D(previous.x(), base_y, previous.y());
      mist.end = QVector3D(points[i].x(), base_y, points[i].y());
      mist.radius = std::max(width * 0.5F, 0.5F);
      mist.strength = k_water_mist_strength;
      mist.kind = Render::MistVolume::Kind::WaterMist;
      volumes.push_back(mist);
      previous = points[i];
    }
  }

  for (const auto& lake : level.lakes) {
    Render::MistVolume mist;
    const float base_y = surface_y(lake.center.x(), lake.center.z());
    mist.start = QVector3D(lake.center.x(), base_y, lake.center.z());
    mist.end = mist.start;
    mist.radius = std::max(std::max(lake.width, lake.depth) * 0.5F, 1.0F);
    mist.strength = k_water_mist_strength;
    mist.kind = Render::MistVolume::Kind::WaterMist;
    volumes.push_back(mist);
  }

  if (volumes.size() > static_cast<std::size_t>(Render::k_max_mist_volumes)) {
    volumes.resize(static_cast<std::size_t>(Render::k_max_mist_volumes));
  }
  return volumes;
}

} // namespace

SkirmishLoader::SkirmishLoader(Engine::Core::World& world,
                               Render::GL::Renderer& renderer,
                               Render::GL::Camera& camera)
    : m_world(world)
    , m_renderer(renderer)
    , m_camera(camera) {
}

void SkirmishLoader::reset_game_state() {
  Game::Map::MapTransformer::set_spectator_mode(false);

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

  m_renderer.clear_entity_render_caches();

  auto& session = Game::Session::session_for(m_world);
  session.building_collision().clear();

  session.owners().clear();

  Game::Map::MapTransformer::clear_player_team_overrides();

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
        for (const auto entry_val : entries) {
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
      collect_player_ids(STRUCTURES);
    }
  } else {
    qWarning() << "Could not open map file for reading player IDs:"
               << resolved_map_path;
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
          chosen_nation = parsed.value_or(session.nations().default_nation_id());
        } else {
          chosen_nation = session.nations().default_nation_id();
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
    result.error_message =
        QCoreApplication::translate("SkirmishLoader",
                                    "Invalid team configuration: At least two teams "
                                    "must be selected to start a match.");
    m_renderer.unlock_world_for_modification();
    m_renderer.resume();
    qWarning() << "SkirmishLoader: " << result.error_message;
    return result;
  }

  Game::Map::MapTransformer::set_local_owner_id(player_owner_id);
  Game::Map::MapTransformer::set_spectator_mode(is_spectator_mode);
  Game::Map::MapTransformer::setPlayerTeamOverrides(team_overrides);

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

  auto level_result = App::Core::LevelLoader::loadFromAssets(
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

  m_renderer.set_mist_volumes(build_mist_volumes(level_result, terrain_service));

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
                                is_spectator_mode);
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
