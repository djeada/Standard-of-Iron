#include "game_engine.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QCursor>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QOpenGLContext>
#include <QPainter>
#include <QPointer>
#include <QQuickWindow>
#include <QSet>
#include <QSize>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QVariant>
#include <QVariantMap>
#include <qbuffer.h>
#include <qcoreapplication.h>
#include <qdir.h>
#include <qevent.h>
#include <qglobal.h>
#include <qimage.h>
#include <qjsonobject.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qobjectdefs.h>
#include <qpoint.h>
#include <qsize.h>
#include <qstringliteral.h>
#include <qstringview.h>
#include <qtmetamacros.h>
#include <qvectornd.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../controllers/action_vfx.h"
#include "../controllers/command_controller.h"
#include "../models/audio_system_proxy.h"
#include "../models/cursor_manager.h"
#include "../models/hover_tracker.h"
#include "../models/selected_units_model.h"
#include "ambient_state_manager.h"
#include "app/models/cursor_mode.h"
#include "app/utils/engine_view_helpers.h"
#include "app/utils/movement_utils.h"
#include "app/utils/selection_utils.h"
#include "audio_event_handler.h"
#include "audio_resource_loader.h"
#include "camera_controller.h"
#include "campaign_manager.h"
#include "core/system.h"
#include "game/audio/audio_system.h"
#include "game/core/component.h"
#include "game/core/event_manager.h"
#include "game/core/world.h"
#include "game/game_config.h"
#include "game/map/campaign_loader.h"
#include "game/map/environment.h"
#include "game/map/level_loader.h"
#include "game/map/map_catalog.h"
#include "game/map/map_loader.h"
#include "game/map/map_transformer.h"
#include "game/map/minimap/map_preview_generator.h"
#include "game/map/minimap/minimap_generator.h"
#include "game/map/minimap/minimap_utils.h"
#include "game/map/minimap/unit_layer.h"
#include "game/map/mission_context.h"
#include "game/map/mission_loader.h"
#include "game/map/skirmish_loader.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/map/world_bootstrap.h"
#include "game/systems/ai_system.h"
#include "game/systems/ai_system/ai_strategy.h"
#include "game/systems/arrow_system.h"
#include "game/systems/ballista_attack_system.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/camera_service.h"
#include "game/systems/camera_visibility_service.h"
#include "game/systems/capture_system.h"
#include "game/systems/catapult_attack_system.h"
#include "game/systems/civilian_delivery_system.h"
#include "game/systems/cleanup_system.h"
#include "game/systems/combat_rules.h"
#include "game/systems/combat_system.h"
#include "game/systems/command_service.h"
#include "game/systems/formation_planner.h"
#include "game/systems/game_state_serializer.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/guard_system.h"
#include "game/systems/healing_beam_system.h"
#include "game/systems/healing_system.h"
#include "game/systems/movement_system.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/pathfinding.h"
#include "game/systems/patrol_system.h"
#include "game/systems/picking_service.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/production_service.h"
#include "game/systems/production_system.h"
#include "game/systems/projectile_system.h"
#include "game/systems/rain_manager.h"
#include "game/systems/rpg_combat_system/rpg_combat_processor.h"
#include "game/systems/save_load_service.h"
#include "game/systems/selection_system.h"
#include "game/systems/terrain_alignment_system.h"
#include "game/systems/troop_count_registry.h"
#include "game/systems/troop_profile_service.h"
#include "game/systems/victory_service.h"
#include "game/units/commander_catalog.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_config.h"
#include "game/units/troop_type.h"
#include "game/visuals/team_colors.h"
#include "game_state_restorer.h"
#include "input_command_handler.h"
#include "level_orchestrator.h"
#include "loading_progress_tracker.h"
#include "minimap_manager.h"
#include "mission_commander_setup.h"
#include "mission_definition_view.h"
#include "production_manager.h"
#include "render/entity/combat_dust_renderer.h"
#include "render/entity/healer_aura_renderer.h"
#include "render/entity/healing_beam_renderer.h"
#include "render/entity/healing_waves_renderer.h"
#include "render/geom/arrow.h"
#include "render/geom/formation_arrow.h"
#include "render/geom/patrol_flags.h"
#include "render/geom/stone.h"
#include "render/gl/bootstrap.h"
#include "render/gl/camera.h"
#include "render/ground/biome_renderer.h"
#include "render/ground/firecamp_renderer.h"
#include "render/ground/fog_renderer.h"
#include "render/ground/ground_renderer.h"
#include "render/ground/olive_renderer.h"
#include "render/ground/pine_renderer.h"
#include "render/ground/plant_renderer.h"
#include "render/ground/rain_renderer.h"
#include "render/ground/stone_renderer.h"
#include "render/ground/terrain_feature_manager.h"
#include "render/ground/terrain_renderer.h"
#include "render/ground/terrain_scatter_manager.h"
#include "render/ground/terrain_surface_manager.h"
#include "render/profiling/frame_profile.h"
#include "render/scene_renderer.h"
#include "render/terrain_scene_proxy.h"
#include "renderer_bootstrap.h"
#include "selection_query_service.h"
#include "utils/resource_utils.h"
#include "visibility_coordinator.h"

namespace {

auto mission_terrain_to_ambient_sound(const QString& terrain_type) -> QString {
  const QString lowered = terrain_type.trimmed().toLower();
  if (lowered == "mountain") {
    return QStringLiteral("ambient.mountain_camp_night");
  }
  if (lowered == "plains") {
    return QStringLiteral("ambient.mediterranean_plains");
  }
  if (lowered == "forest") {
    return QStringLiteral("ambient.forest_ambush");
  }
  if (lowered == "river") {
    return QStringLiteral("ambient.river_crossing");
  }
  return QStringLiteral("ambient.battlefield_dry_wind_distant_march_01");
}

auto biome_to_ambient_sound(Game::Map::GroundType ground_type) -> QString {
  switch (ground_type) {
  case Game::Map::GroundType::AlpineMix:
    return QStringLiteral("ambient.alpine_mountain_pass");
  case Game::Map::GroundType::GrassDry:
    return QStringLiteral("ambient.mediterranean_plains");
  case Game::Map::GroundType::SoilRocky:
    return QStringLiteral("ambient.battlefield_dry_wind_distant_march_01");
  case Game::Map::GroundType::SoilFertile:
    return QStringLiteral("ambient.roman_army_camp_01");
  case Game::Map::GroundType::ForestMud:
  default:
    return QStringLiteral("ambient.forest_ambush");
  }
}

auto nation_audio_tag(Game::Systems::NationID nation_id) -> QString {
  switch (nation_id) {
  case Game::Systems::NationID::RomanRepublic:
    return QStringLiteral("roman");
  case Game::Systems::NationID::Carthage:
    return QStringLiteral("carthage");
  }
  return QStringLiteral("roman");
}

auto ambient_state_tag(Engine::Core::AmbientState state) -> QString {
  switch (state) {
  case Engine::Core::AmbientState::PEACEFUL:
    return QStringLiteral("peaceful");
  case Engine::Core::AmbientState::TENSE:
    return QStringLiteral("tense");
  case Engine::Core::AmbientState::COMBAT:
    return QStringLiteral("combat");
  case Engine::Core::AmbientState::VICTORY:
    return QStringLiteral("victory");
  case Engine::Core::AmbientState::DEFEAT:
    return QStringLiteral("defeat");
  }
  return QStringLiteral("peaceful");
}

auto render_stage_logging_enabled() -> bool {
  return qEnvironmentVariableIsSet("SOI_RENDER_STAGE_LOG");
}

void log_render_stage_once(const char* stage, const QString& detail) {
  if (!render_stage_logging_enabled()) {
    return;
  }

  static std::mutex mutex;
  static std::unordered_set<std::string> emitted_stages;

  std::lock_guard<std::mutex> const lock(mutex);
  if (!emitted_stages.emplace(stage).second) {
    return;
  }

  qInfo().noquote() << QStringLiteral("SOI render stage [%1]: %2")
                           .arg(QString::fromLatin1(stage), detail)
                    << "thread" << QThread::currentThread();
}

auto resolve_mission_file_path(const QString& mission_id) -> QString {
  const QStringList search_paths = {QStringLiteral("assets/missions/%1.json"),
                                    QStringLiteral("../assets/missions/%1.json"),
                                    QStringLiteral("../../assets/missions/%1.json"),
                                    QStringLiteral(":/assets/missions/%1.json"),
                                    QStringLiteral("/assets/missions/%1.json"),
                                    QStringLiteral("/../assets/missions/%1.json")};

  for (const auto& pattern : search_paths) {
    QString candidate = pattern.arg(mission_id);
    candidate = Utils::Resources::resolve_resource_path(candidate);
    if (QFile::exists(candidate)) {
      return candidate;
    }
  }
  return {};
}

auto classify_wave_direction(const QVector3D& entry_point) -> QString {
  const float x = entry_point.x();
  const float z = entry_point.z();
  const float ax = std::abs(x);
  const float az = std::abs(z);

  constexpr float k_direction_bias = 1.25F;
  if (ax > az * k_direction_bias) {
    return x >= 0.0F ? QStringLiteral("east") : QStringLiteral("west");
  }
  if (az > ax * k_direction_bias) {
    return z >= 0.0F ? QStringLiteral("south") : QStringLiteral("north");
  }
  if (x >= 0.0F && z >= 0.0F) {
    return QStringLiteral("southeast");
  }
  if (x >= 0.0F && z < 0.0F) {
    return QStringLiteral("northeast");
  }
  if (x < 0.0F && z >= 0.0F) {
    return QStringLiteral("southwest");
  }
  return QStringLiteral("northwest");
}

auto build_available_commander_entry(const Game::Units::CommanderDefinition& definition,
                                     bool is_default) -> QVariantMap {
  QVariantMap entry;
  entry["id"] = QString::fromStdString(definition.id);
  entry["troop"] =
      QString::fromStdString(Game::Units::troop_typeToString(definition.troop_type));
  entry["display_name"] = QString::fromStdString(definition.display_name);
  entry["battlefield_role"] = QString::fromStdString(definition.battlefield_role);
  entry["bonus_summary"] = QString::fromStdString(definition.bonus_summary);
  entry["passive_aura"] = QString::fromStdString(definition.passive_aura);
  entry["rally_ability"] = QString::fromStdString(definition.rally_ability);
  entry["is_default"] = is_default;
  return entry;
}

auto build_condition_list(const std::vector<Game::Mission::Condition>& conditions)
    -> QVariantList {
  QVariantList list;
  for (const auto& condition : conditions) {
    QVariantMap cond;
    cond["type"] = condition.type;
    cond["description"] = condition.description;
    if (condition.duration.has_value()) {
      cond["duration"] = condition.duration.value();
    }
    if (condition.structure_type.has_value()) {
      cond["structure_type"] = condition.structure_type.value();
    }
    if (!condition.structure_types.empty()) {
      QVariantList types;
      for (const auto& type : condition.structure_types) {
        types.append(type);
      }
      cond["structure_types"] = types;
    }
    if (condition.min_count.has_value()) {
      cond["min_count"] = condition.min_count.value();
    }
    if (condition.wave_count.has_value()) {
      cond["wave_count"] = condition.wave_count.value();
    }
    list.append(cond);
  }
  return list;
}

auto build_resource_map(int owner_id) -> QVariantMap {
  QVariantMap resources;
  Game::Systems::ResourceAmounts const amounts =
      Game::Systems::PlayerResourceRegistry::instance().get_all(owner_id);
  for (Game::Systems::ResourceType const type : Game::Systems::k_all_resource_types) {
    resources[QLatin1String(Game::Systems::resource_type_key(type))] =
        amounts.get(type);
  }
  return resources;
}

auto build_player_state_map(int owner_id, int population_cap) -> QVariantMap {
  QVariantMap state;
  state["owner_id"] = owner_id;
  state["population"] =
      Game::Systems::TroopCountRegistry::instance().get_troop_count(owner_id);
  state["population_cap"] = population_cap;
  state["resources"] = build_resource_map(owner_id);
  return state;
}

} // namespace

GameEngine::GameEngine(QObject* parent)
    : QObject(parent)
    , m_commander_input(this, this)
    , m_selected_units_model(new SelectedUnitsModel(this, this)) {

  Game::Systems::NationRegistry::instance().initialize_defaults();
  Game::Systems::TroopCountRegistry::instance().initialize();
  Game::Systems::GlobalStatsRegistry::instance().initialize();

  m_world = std::make_unique<Engine::Core::World>();

  auto rendering = RendererBootstrap::initialize_rendering();
  m_renderer = std::move(rendering.renderer);
  m_rts_camera = std::move(rendering.camera);
  m_commander_camera = std::make_unique<Render::GL::Camera>(*m_rts_camera);
  set_active_camera(m_rts_camera.get());
  enter_rts_runtime_mode();
  m_terrain_scene = std::move(rendering.terrain_scene);
  m_surface = std::move(rendering.surface);
  m_features = std::move(rendering.features);
  m_scatter = std::move(rendering.scatter);
  m_fog = std::move(rendering.fog);
  m_boundary_fog = std::move(rendering.boundary_fog);
  m_rain = std::move(rendering.rain);

  RendererBootstrap::initialize_world_systems(*m_world);

  m_picking_service = std::make_unique<Game::Systems::PickingService>();
  m_victory_service = std::make_unique<Game::Systems::VictoryService>();
  m_save_load_service = std::make_unique<Game::Systems::SaveLoadService>();
  m_camera_service = std::make_unique<Game::Systems::CameraService>();
  m_rain_manager = std::make_unique<Game::Systems::RainManager>();

  m_loading_progress_tracker = std::make_unique<LoadingProgressTracker>(this);
  connect(m_loading_progress_tracker.get(),
          &LoadingProgressTracker::progress_changed,
          this,
          [this](float progress) { emit loading_progress_changed(progress); });
  connect(m_loading_progress_tracker.get(),
          &LoadingProgressTracker::stage_changed,
          this,
          [this](LoadingProgressTracker::LoadingStage, QString detail) {
            emit loading_stage_changed(std::move(detail));
          });

  auto* selection_system = m_world->get_system<Game::Systems::SelectionSystem>();
  m_selection_controller = std::make_unique<Game::Systems::SelectionController>(
      m_world.get(), selection_system, m_picking_service.get());
  m_command_controller = std::make_unique<App::Controllers::CommandController>(
      m_world.get(), selection_system, m_picking_service.get());

  m_cursor_manager = std::make_unique<CursorManager>();
  m_hover_tracker = std::make_unique<HoverTracker>(m_picking_service.get());

  m_map_catalog = std::make_unique<Game::Map::MapCatalog>(this);
  connect(m_map_catalog.get(),
          &Game::Map::MapCatalog::map_loaded,
          this,
          [this](const QVariantMap& map_data) {
            m_available_maps.append(map_data);
            emit available_maps_changed();
          });
  connect(m_map_catalog.get(),
          &Game::Map::MapCatalog::loading_changed,
          this,
          [this](bool loading) {
            m_maps_loading = loading;
            emit maps_loading_changed();
          });
  connect(m_map_catalog.get(), &Game::Map::MapCatalog::all_maps_loaded, this, [this]() {
    emit available_maps_changed();
  });

  if (AudioSystem::get_instance().initialize()) {
    qInfo() << "AudioSystem initialized successfully";
    AudioResourceLoader::load_audio_resources();
  } else {
    qWarning() << "Failed to initialize AudioSystem";
  }

  m_audio_systemProxy = std::make_unique<App::Models::AudioSystemProxy>(this);

  m_minimap_manager = std::make_unique<MinimapManager>();
  m_visibility_coordinator = std::make_unique<VisibilityCoordinator>();
  m_visibility_coordinator->set_presenters(m_fog.get(), m_minimap_manager.get());
  m_ambient_state_manager = std::make_unique<AmbientStateManager>();

  m_input_handler = std::make_unique<InputCommandHandler>(m_world.get(),
                                                          m_selection_controller.get(),
                                                          m_command_controller.get(),
                                                          m_cursor_manager.get(),
                                                          m_hover_tracker.get(),
                                                          m_picking_service.get(),
                                                          m_rts_camera.get());

  m_camera_controller = std::make_unique<CameraController>(
      m_rts_camera.get(), m_camera_service.get(), m_world.get());

  m_production_manager = std::make_unique<ProductionManager>(
      m_world.get(), m_picking_service.get(), m_rts_camera.get(), this);
  connect(m_production_manager.get(),
          &ProductionManager::placing_construction_changed,
          this,
          &GameEngine::placing_construction_changed);
  connect(m_production_manager.get(),
          &ProductionManager::construction_preview_valid_changed,
          this,
          &GameEngine::construction_preview_valid_changed);
  connect(m_production_manager.get(),
          &ProductionManager::construction_placement_rejected,
          this,
          [this](const QString& reason) {
            if (!reason.isEmpty()) {
              set_error(reason);
            }
          });

  m_campaign_manager = std::make_unique<CampaignManager>(this);
  connect(m_campaign_manager.get(),
          &CampaignManager::available_campaigns_changed,
          this,
          &GameEngine::available_campaigns_changed);

  m_selection_query_service =
      std::make_unique<SelectionQueryService>(m_world.get(), this);

  m_audio_event_handler =
      std::make_unique<Game::Audio::AudioEventHandler>(m_world.get());
  if (m_audio_event_handler->initialize()) {
    qInfo() << "AudioEventHandler initialized successfully";
    AudioResourceLoader::load_audio_resources(AudioLoadPolicy::Screen);
    configure_audio_manifest_mappings();

    qInfo() << "Audio mappings configured";
  } else {
    qWarning() << "Failed to initialize AudioEventHandler";
  }

  connect(m_cursor_manager.get(), &CursorManager::mode_changed, this, [this]() {
    if (m_cursor_manager && (m_window != nullptr)) {
      m_cursor_manager->update_cursor_shape(m_window);
    }
    emit cursor_mode_changed();
  });
  connect(m_cursor_manager.get(),
          &CursorManager::global_cursor_changed,
          this,
          &GameEngine::global_cursor_changed);

  connect(m_selection_controller.get(),
          &Game::Systems::SelectionController::selection_changed,
          this,
          &GameEngine::selected_units_changed);
  connect(m_selection_controller.get(),
          &Game::Systems::SelectionController::selection_changed,
          this,
          &GameEngine::sync_selection_flags);
  connect(m_selection_controller.get(),
          &Game::Systems::SelectionController::selection_model_refresh_requested,
          this,
          &GameEngine::selected_units_data_changed);
  connect(m_command_controller.get(),
          &App::Controllers::CommandController::attack_target_selected,
          [this]() {
            if (auto* sel_sys = m_world->get_system<Game::Systems::SelectionSystem>()) {
              const auto& sel = sel_sys->get_selected_units();
              if (!sel.empty()) {
                auto* cam = m_camera;
                auto* picking = m_picking_service.get();
                if ((cam != nullptr) && (picking != nullptr)) {
                  Engine::Core::EntityID const target_id =
                      Game::Systems::PickingService::pick_unit_first(0.0F,
                                                                     0.0F,
                                                                     *m_world,
                                                                     *cam,
                                                                     m_viewport.width,
                                                                     m_viewport.height,
                                                                     0);
                  if (target_id != 0) {
                    App::Controllers::ActionVFX::spawn_attack_arrow(m_world.get(),
                                                                    target_id);
                  }
                }
              }
            }
          });

  connect(m_command_controller.get(),
          &App::Controllers::CommandController::troop_limit_reached,
          [this]() {
            AudioSystem::get_instance().play_sound(
                "population_limit_horn", 0.9F, false, 8, AudioCategory::SFX);
            set_error("Maximum troop limit reached. Cannot produce more units.");
          });
  connect(m_command_controller.get(),
          &App::Controllers::CommandController::insufficient_manpower,
          [this]() {
            AudioSystem::get_instance().play_sound(
                "low_resources_click", 0.85F, false, 7, AudioCategory::SFX);
            set_error("Not enough manpower. Build homes or wait for families.");
          });
  connect(m_command_controller.get(),
          &App::Controllers::CommandController::hold_mode_changed,
          this,
          &GameEngine::hold_mode_changed);
  connect(m_command_controller.get(),
          &App::Controllers::CommandController::guard_mode_changed,
          this,
          &GameEngine::guard_mode_changed);
  connect(m_command_controller.get(),
          &App::Controllers::CommandController::formation_mode_changed,
          this,
          &GameEngine::formation_mode_changed);
  connect(m_command_controller.get(),
          &App::Controllers::CommandController::formation_placement_started,
          [this]() { emit placing_formation_changed(); });
  connect(m_command_controller.get(),
          &App::Controllers::CommandController::formation_placement_ended,
          [this]() { emit placing_formation_changed(); });

  connect(
      this, SIGNAL(selected_units_changed()), m_selected_units_model, SLOT(refresh()));
  connect(this,
          SIGNAL(selected_units_data_changed()),
          m_selected_units_model,
          SLOT(refresh()));

  emit selected_units_changed();

  m_unit_died_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>(
          [this](const Engine::Core::UnitDiedEvent& e) {
            on_unit_died(e);

            if (Game::Units::is_troop_spawn(e.spawn_type) &&
                e.owner_id != m_runtime.local_owner_id &&
                e.killer_owner_id == m_runtime.local_owner_id) {

              int const production_cost =
                  Game::Units::TroopConfig::instance().get_production_cost(
                      e.spawn_type);
              m_enemy_troops_defeated += production_cost;
              emit enemy_troops_defeated_changed();
            }
          });

  m_unit_spawned_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::UnitSpawnedEvent>(
          [this](const Engine::Core::UnitSpawnedEvent& e) { on_unit_spawned(e); });

  m_combat_hit_subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::CombatHitEvent>(
          [this](const Engine::Core::CombatHitEvent& e) {
            if (m_controlled_commander_id == 0 ||
                e.attacker_id != m_controlled_commander_id || m_world == nullptr) {
              return;
            }
            auto* target_ent = m_world->get_entity(e.target_id);
            if (target_ent == nullptr) {
              return;
            }
            auto* tf = target_ent->get_component<Engine::Core::TransformComponent>();
            if (tf == nullptr) {
              return;
            }

            float max_health = 0.0F;
            if (const auto* rpg_health =
                    target_ent->get_component<Engine::Core::RpgHealthComponent>();
                rpg_health != nullptr && rpg_health->rpg_max_hp > 0) {
              max_health = static_cast<float>(rpg_health->rpg_max_hp);
            } else if (const auto* unit =
                           target_ent->get_component<Engine::Core::UnitComponent>();
                       unit != nullptr && unit->max_health > 0) {
              max_health = static_cast<float>(unit->max_health);
            }

            float const damage_ratio =
                max_health > 0.0F
                    ? std::clamp(static_cast<float>(e.damage) / max_health, 0.0F, 1.5F)
                    : 0.0F;
            int const lane = static_cast<int>(m_rpg_damage_event_sequence % 5U) - 2;
            ++m_rpg_damage_event_sequence;

            if (static_cast<int>(m_rpg_damage_events.size()) >=
                k_max_rpg_damage_events) {
              m_rpg_damage_events.erase(m_rpg_damage_events.begin());
            }

            m_rpg_damage_events.push_back({tf->position.x,
                                           tf->position.y + 1.8F,
                                           tf->position.z,
                                           e.damage,
                                           damage_ratio,
                                           lane,
                                           e.is_killing_blow});
          });
}

GameEngine::~GameEngine() {

  if (m_audio_event_handler) {
    m_audio_event_handler->shutdown();
  }
  AudioSystem::get_instance().shutdown();
  qInfo() << "AudioSystem shut down";
}

void GameEngine::cleanup_opengl_resources() {
  qInfo() << "Cleaning up OpenGL resources...";

  QOpenGLContext* context = QOpenGLContext::currentContext();
  const bool has_valid_context = (context != nullptr);

  if (!has_valid_context) {
    qInfo() << "No valid OpenGL context, skipping OpenGL cleanup";
  }

  if (m_renderer && has_valid_context) {
    m_renderer->shutdown();
    qInfo() << "Renderer shut down";
  }

  m_terrain_scene.reset();

  m_surface.reset();
  m_features.reset();
  m_scatter.reset();
  m_fog.reset();
  m_boundary_fog.reset();
  m_rain.reset();
  m_rain_manager.reset();

  m_renderer.reset();
  m_resources.reset();

  qInfo() << "OpenGL resources cleaned up";
}

void GameEngine::on_map_clicked(qreal sx, qreal sy) {
  if (m_window == nullptr) {
    return;
  }
  ensure_initialized();
  if (m_input_handler) {
    m_input_handler->on_map_clicked(sx, sy, m_runtime.local_owner_id, m_viewport);
  }
}

void GameEngine::on_right_click(qreal sx, qreal sy) {
  if (m_window == nullptr) {
    return;
  }
  ensure_initialized();
  if (m_input_handler) {
    m_input_handler->on_right_click(sx, sy, m_runtime.local_owner_id, m_viewport);
  }
}

void GameEngine::on_right_double_click(qreal sx, qreal sy) {
  if (m_window == nullptr) {
    return;
  }
  ensure_initialized();

  if (m_right_mouse_gesture.placement_was_active_on_press) {
    m_right_mouse_gesture.double_click_handled = true;
    return;
  }

  bool const started_formation_placement =
      m_right_mouse_gesture.started_formation_placement && m_input_handler &&
      m_input_handler->is_placing_formation();
  if (started_formation_placement) {
    m_input_handler->on_formation_cancel();
  } else if (m_right_mouse_gesture.suppress_release_click ||
             is_placing_construction()) {
    m_right_mouse_gesture.double_click_handled = true;
    return;
  }

  if (m_input_handler) {
    m_input_handler->on_right_double_click(
        sx, sy, m_runtime.local_owner_id, m_viewport);
  }
  m_right_mouse_gesture.double_click_handled = true;
}

auto GameEngine::on_right_press(qreal sx, qreal sy) -> bool {
  if (m_window == nullptr) {
    return false;
  }
  ensure_initialized();
  m_right_mouse_gesture.reset();
  m_right_mouse_gesture.active = true;
  m_right_mouse_gesture.press_position = QPointF(sx, sy);
  m_right_mouse_gesture.placement_was_active_on_press =
      is_placing_formation() || is_placing_construction();

  if (is_placing_formation()) {
    on_formation_cancel();
    m_right_mouse_gesture.suppress_release_click = true;
    return true;
  }
  if (is_placing_construction()) {
    on_construction_cancel();
    m_right_mouse_gesture.suppress_release_click = true;
    return true;
  }
  if (m_input_handler) {
    m_right_mouse_gesture.suppress_release_click =
        m_input_handler->on_right_press(sx, sy, m_runtime.local_owner_id, m_viewport);
    m_right_mouse_gesture.started_formation_placement =
        !m_right_mouse_gesture.placement_was_active_on_press &&
        m_right_mouse_gesture.suppress_release_click &&
        m_input_handler->is_placing_formation();
    return m_right_mouse_gesture.suppress_release_click;
  }
  return false;
}

void GameEngine::on_right_move(qreal sx, qreal sy) {
  if (m_window == nullptr) {
    return;
  }
  ensure_initialized();
  if (!m_right_mouse_gesture.active) {
    return;
  }

  QPointF const delta = QPointF(sx, sy) - m_right_mouse_gesture.press_position;
  if ((delta.x() * delta.x() + delta.y() * delta.y()) > 36.0) {
    m_right_mouse_gesture.dragged = true;
  }

  if (m_input_handler && m_input_handler->is_placing_formation()) {
    m_input_handler->on_right_drag_orient(sx, sy, m_viewport);
  }
}

void GameEngine::on_right_release(qreal sx, qreal sy) {
  if (m_window == nullptr) {
    m_right_mouse_gesture.reset();
    return;
  }
  ensure_initialized();

  if (m_right_mouse_gesture.double_click_handled) {
    m_right_mouse_gesture.reset();
    return;
  }

  if (m_input_handler && m_input_handler->is_placing_formation()) {
    m_input_handler->on_formation_confirm();
    m_right_mouse_gesture.reset();
    return;
  }

  if (!m_right_mouse_gesture.dragged && !m_right_mouse_gesture.suppress_release_click &&
      m_input_handler) {
    m_input_handler->on_right_click(sx, sy, m_runtime.local_owner_id, m_viewport);
  }

  m_right_mouse_gesture.reset();
}

void GameEngine::on_right_drag_orient(qreal sx, qreal sy) {
  on_right_move(sx, sy);
}

void GameEngine::on_attack_click(qreal sx, qreal sy) {
  if (m_window == nullptr) {
    return;
  }
  ensure_initialized();
  if (m_input_handler) {
    m_input_handler->on_attack_click(sx, sy, m_viewport);
  }
}

void GameEngine::reset_movement(Engine::Core::Entity* entity) {
  InputCommandHandler::reset_movement(entity);
}

void GameEngine::on_stop_command() {
  if (!m_input_handler) {
    return;
  }
  ensure_initialized();
  m_input_handler->on_stop_command();
}

void GameEngine::on_hold_command() {
  if (!m_input_handler) {
    return;
  }
  ensure_initialized();
  m_input_handler->on_hold_command();
}

void GameEngine::on_guard_command() {
  if (!m_input_handler) {
    return;
  }
  ensure_initialized();
  m_input_handler->on_guard_command();
}

void GameEngine::on_formation_command() {
  if (!m_input_handler) {
    return;
  }
  ensure_initialized();
  m_input_handler->on_formation_command();
}

void GameEngine::on_run_command() {
  if (!m_input_handler) {
    return;
  }
  ensure_initialized();
  m_input_handler->on_run_command();
}

void GameEngine::on_heal_command() {
  if (!m_cursor_manager) {
    return;
  }
  ensure_initialized();
  m_cursor_manager->set_mode(CursorMode::Heal);
}

void GameEngine::on_build_command() {
  if (!m_cursor_manager) {
    return;
  }
  ensure_initialized();
  m_cursor_manager->set_mode(CursorMode::Build);
}

void GameEngine::on_civilian_delivery_click(qreal sx, qreal sy) {
  if (!m_input_handler || (m_camera == nullptr)) {
    return;
  }
  ensure_initialized();
  m_input_handler->on_civilian_delivery_click(
      sx, sy, m_runtime.local_owner_id, m_viewport);
}

void GameEngine::on_guard_click(qreal sx, qreal sy) {
  if (!m_input_handler || (m_camera == nullptr)) {
    return;
  }
  ensure_initialized();
  m_input_handler->on_guard_click(sx, sy, m_viewport);
}

auto GameEngine::any_selected_in_hold_mode() const -> bool {
  if (!m_input_handler) {
    return false;
  }
  return m_input_handler->any_selected_in_hold_mode();
}

auto GameEngine::any_selected_in_guard_mode() const -> bool {
  if (!m_input_handler) {
    return false;
  }
  return m_input_handler->any_selected_in_guard_mode();
}

auto GameEngine::any_selected_in_formation_mode() const -> bool {
  if (!m_input_handler) {
    return false;
  }
  return m_input_handler->any_selected_in_formation_mode();
}

auto GameEngine::any_selected_in_run_mode() const -> bool {
  if (!m_input_handler) {
    return false;
  }
  return m_input_handler->any_selected_in_run_mode();
}

auto GameEngine::is_placing_formation() const -> bool {
  if (m_command_controller) {
    return m_command_controller->is_placing_formation();
  }
  return false;
}

bool GameEngine::is_campaign_mission() const {
  if (!m_campaign_manager) {
    return false;
  }
  return m_campaign_manager->current_mission_context().is_campaign();
}

void GameEngine::on_formation_mouse_move(qreal sx, qreal sy) {
  if (!m_input_handler) {
    return;
  }
  ensure_initialized();
  m_input_handler->on_formation_mouse_move(sx, sy, m_viewport);
}

void GameEngine::on_formation_scroll(float delta) {
  if (!m_input_handler) {
    return;
  }
  ensure_initialized();
  m_input_handler->on_formation_scroll(delta);
}

void GameEngine::on_formation_confirm() {
  if (!m_input_handler) {
    return;
  }
  ensure_initialized();
  m_input_handler->on_formation_confirm();
}

void GameEngine::on_formation_cancel() {
  if (!m_input_handler) {
    return;
  }
  ensure_initialized();
  m_input_handler->on_formation_cancel();
}

auto GameEngine::is_placing_construction() const -> bool {
  return m_production_manager ? m_production_manager->is_placing_construction() : false;
}

auto GameEngine::construction_preview_valid() const -> bool {
  return m_production_manager ? m_production_manager->construction_preview_valid()
                              : false;
}

void GameEngine::on_construction_mouse_move(qreal sx, qreal sy) {
  ensure_initialized();
  if (m_production_manager) {
    m_production_manager->on_construction_mouse_move(sx, sy, m_viewport);
  }
}

void GameEngine::on_construction_confirm() {
  ensure_initialized();
  if (m_production_manager) {
    m_production_manager->on_construction_confirm();
  }
}

void GameEngine::on_construction_cancel() {
  if (m_production_manager) {
    m_production_manager->on_construction_cancel();
  }
}

void GameEngine::on_patrol_click(qreal sx, qreal sy) {
  if (!m_input_handler || (m_camera == nullptr)) {
    return;
  }
  ensure_initialized();
  m_input_handler->on_patrol_click(sx, sy, m_viewport);
}

auto GameEngine::control_mode() const -> QString {
  return m_control_mode == PlayerControlMode::Commander ? QStringLiteral("commander")
                                                        : QStringLiteral("rts");
}

auto GameEngine::game_mode() const -> QString {
  return m_game_mode == GameMode::Rpg ? QStringLiteral("rpg") : QStringLiteral("rts");
}

void GameEngine::apply_game_mode_render_policy() {
  if (m_renderer != nullptr) {
    m_renderer->set_world_render_mode(m_game_mode == GameMode::Rpg
                                          ? Render::GL::Renderer::WorldRenderMode::Rpg
                                          : Render::GL::Renderer::WorldRenderMode::Rts);
  }
  if (m_fog != nullptr) {
    m_fog->set_soft_reveal_enabled(m_game_mode == GameMode::Rpg);
  }
}

auto GameEngine::commander_control_available() const -> bool {
  return find_local_commander() != nullptr;
}

auto GameEngine::commander_input() -> QObject* {
  return &m_commander_input;
}

auto GameEngine::find_local_commander() const -> Engine::Core::Entity* {
  if (m_world == nullptr) {
    return nullptr;
  }

  for (auto* entity : m_world->get_entities_with<Engine::Core::CommanderComponent>()) {
    if (entity == nullptr) {
      continue;
    }
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (unit == nullptr || transform == nullptr) {
      continue;
    }
    if (unit->owner_id == m_runtime.local_owner_id && unit->health > 0) {
      return entity;
    }
  }
  return nullptr;
}

auto GameEngine::controlled_commander_entity() -> Engine::Core::Entity* {
  if (m_world == nullptr || m_controlled_commander_id == 0) {
    return nullptr;
  }
  return m_world->get_entity(m_controlled_commander_id);
}

void GameEngine::store_rts_selection() {
  m_saved_rts_selection_ids.clear();
  if (m_selection_controller) {
    m_selection_controller->get_selected_unit_ids(m_saved_rts_selection_ids);
  }
}

void GameEngine::select_controlled_commander() {
  if (m_controlled_commander_id == 0 || !m_selection_controller) {
    return;
  }

  m_selection_controller->select_single_unit(m_controlled_commander_id,
                                             m_runtime.local_owner_id);
}

void GameEngine::restore_rts_selection() {
  if (m_world == nullptr) {
    m_saved_rts_selection_ids.clear();
    return;
  }

  auto* selection_system = m_world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    m_saved_rts_selection_ids.clear();
    return;
  }

  selection_system->clear_selection();
  for (const auto id : m_saved_rts_selection_ids) {
    auto* entity = m_world->get_entity(id);
    auto* unit = entity != nullptr
                     ? entity->get_component<Engine::Core::UnitComponent>()
                     : nullptr;
    if (unit == nullptr || unit->health <= 0 ||
        unit->owner_id != m_runtime.local_owner_id) {
      continue;
    }
    selection_system->select_unit(id);
  }

  sync_selection_flags();
  emit selected_units_changed();
  m_saved_rts_selection_ids.clear();
}

void GameEngine::set_active_camera(Render::GL::Camera* camera) {
  m_camera = camera;
  if (m_renderer != nullptr) {
    m_renderer->set_camera(m_camera);
    if (m_viewport.width > 0 && m_viewport.height > 0) {
      m_renderer->set_viewport(m_viewport.width, m_viewport.height);
    }
  }
  Game::Systems::CameraVisibilityService::instance().set_camera(m_camera);
}

bool GameEngine::enter_commander_control_mode() {
  ensure_initialized();
  if (m_level.is_spectator_mode) {
    return false;
  }

  auto* commander = find_local_commander();
  if (commander == nullptr || m_commander_camera == nullptr) {
    return false;
  }

  if (is_placing_formation()) {
    on_formation_cancel();
  }
  if (is_placing_construction()) {
    on_construction_cancel();
  }
  set_cursor_mode(CursorMode::Normal);

  m_rts_camera_snapshot.follow_selection = m_follow_selection_enabled;
  m_rts_camera_snapshot.valid = true;
  store_rts_selection();

  m_follow_selection_enabled = false;
  if (m_camera_controller) {
    m_camera_controller->follow_selection(false);
  }

  m_controlled_commander_id = commander->get_id();
  if (auto* commander_data =
          commander->get_component<Engine::Core::CommanderComponent>()) {
    commander_data->rally_requested = false;
    commander_data->rally_requires_manual_trigger = true;
    commander_data->fpv_controlled = true;
    commander_data->combo_step = 0;
    commander_data->power_strike_active = false;
    commander_data->just_struck_enemy = false;
    commander_data->last_strike_combo_step = 0U;
    commander_data->jump_active = false;
    commander_data->jump_phase = 0.0F;
    commander_data->jump_height_offset = 0.0F;
    commander_data->posture = 0.0F;
    commander_data->punish_window_remaining = 0.0F;
    commander_data->shield_bash_cooldown_remaining = 0.0F;
    commander_data->vanguard_rush_cooldown_remaining = 0.0F;
    commander_data->second_wind_cooldown_remaining = 0.0F;
  }
  if (auto* transform = commander->get_component<Engine::Core::TransformComponent>()) {
    m_commander_control.set_view_yaw(transform->rotation.y);
  }
  m_commander_control.set_view_pitch(0.0F);
  reset_commander_input();
  set_active_camera(m_commander_camera.get());

  enter_commander_runtime_mode();
  if (auto* rpg = Engine::Core::get_or_add_component<Engine::Core::RpgHealthComponent>(
          commander)) {
    auto* unit = commander->get_component<Engine::Core::UnitComponent>();
    if (!rpg->active) {

      rpg->rpg_max_hp = (unit != nullptr) ? unit->max_health * 3 : 150;
      rpg->rpg_hp = rpg->rpg_max_hp;
    }
    rpg->active = true;
  }
  (void)Engine::Core::get_or_add_component<Engine::Core::RpgCommanderTargetComponent>(
      commander);
  if (auto* rpg_action =
          Engine::Core::get_or_add_component<Engine::Core::RpgCommanderActionComponent>(
              commander)) {
    rpg_action->phase = Engine::Core::RpgCommanderActionPhase::None;
    rpg_action->melee_attack_style = 0;
    rpg_action->melee_attack_sequence = 0;
    rpg_action->active_target_id = 0;
    rpg_action->last_hit_target_id = 0;
    rpg_action->last_damage = 0;
    rpg_action->phase_time = 0.0F;
  }

  emit game_mode_changed();
  if (m_world != nullptr && m_commander_camera != nullptr) {
    (void)m_commander_control.update(*m_world,
                                     m_controlled_commander_id,
                                     m_runtime.local_owner_id,
                                     *m_commander_camera,
                                     0.0F);
  }
  select_controlled_commander();
  emit control_mode_changed();
  return true;
}

void GameEngine::exit_commander_control_mode() {
  exit_commander_runtime_mode();
  reset_commander_input();
  clear_controlled_commander_state();
  enter_rts_runtime_mode();
  m_controlled_commander_id = 0;

  set_active_camera(m_rts_camera.get());
  if (m_rts_camera_snapshot.valid) {
    m_follow_selection_enabled = m_rts_camera_snapshot.follow_selection;
    if (m_camera_controller) {
      m_camera_controller->follow_selection(m_follow_selection_enabled);
    }
  }
  restore_rts_selection();

  emit game_mode_changed();
  emit control_mode_changed();
}

void GameEngine::request_enter_commander_control_mode() {
  (void)enter_commander_control_mode();
}

void GameEngine::request_exit_commander_control_mode() {
  exit_commander_control_mode();
}

void GameEngine::enter_rts_runtime_mode() {
  m_control_mode = PlayerControlMode::Rts;
  m_game_mode = GameMode::Rts;
  apply_game_mode_render_policy();
}

void GameEngine::enter_commander_runtime_mode() {
  m_control_mode = PlayerControlMode::Commander;
  m_game_mode = GameMode::Rpg;
  apply_game_mode_render_policy();
}

void GameEngine::exit_commander_runtime_mode() {
  m_rpg_hit_stop_timer = 0.0F;
  m_rpg_telegraphs.clear();
}

void GameEngine::toggle_commander_control_mode() {
  if (m_control_mode == PlayerControlMode::Commander) {
    request_exit_commander_control_mode();
    return;
  }
  request_enter_commander_control_mode();
}

void GameEngine::commander_key_down(int key, int modifiers) {
  (void)modifiers;
  m_commander_control.key_down(key);
}

void GameEngine::commander_key_up(int key, int modifiers) {
  (void)modifiers;
  m_commander_control.key_up(key);
}

void GameEngine::reset_commander_input() {
  m_commander_control.reset();
}

void GameEngine::commander_primary_action() {
  if (m_world == nullptr) {
    return;
  }

  if (!m_commander_control.primary_action(
          *m_world, m_controlled_commander_id, m_runtime.local_owner_id)) {
    exit_commander_control_mode();
  }
}

void GameEngine::commander_primary_action_down() {
  m_commander_control.primary_action_down();
  commander_primary_action();
}

void GameEngine::commander_primary_action_up() {
  m_commander_control.primary_action_up();
}

void GameEngine::commander_secondary_action_down() {
  m_commander_control.secondary_action_down();
}

void GameEngine::commander_secondary_action_up() {
  m_commander_control.secondary_action_up();
  if (m_world != nullptr) {
    m_commander_control.release_guard(
        *m_world, m_controlled_commander_id, m_runtime.local_owner_id);
  }
}

void GameEngine::commander_trigger_rally() {
  if (m_control_mode != PlayerControlMode::Commander) {
    return;
  }
  begin_commander_flag_rally();
}

void GameEngine::begin_commander_flag_rally() {
  if (m_world == nullptr || m_cursor_manager == nullptr) {
    return;
  }

  auto* commander = m_control_mode == PlayerControlMode::Commander
                        ? controlled_commander_entity()
                        : find_local_commander();
  if (commander == nullptr) {
    if (m_control_mode == PlayerControlMode::Commander) {
      exit_commander_control_mode();
    }
    return;
  }
  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
  if (commander_data == nullptr || commander_data->flag_rally_in_progress) {
    return;
  }

  if (m_control_mode == PlayerControlMode::Commander) {
    reset_commander_input();
  }
  m_commander_rally_preview_pos = std::nullopt;
  set_cursor_mode(CursorMode::PlaceCommanderRally);
  seed_commander_rally_preview_from_view_center();
}

void GameEngine::confirm_commander_flag_rally(qreal sx, qreal sy) {
  if (m_cursor_manager == nullptr ||
      m_cursor_manager->mode() != CursorMode::PlaceCommanderRally) {
    return;
  }
  if (m_world == nullptr || m_picking_service == nullptr || m_camera == nullptr) {
    set_cursor_mode(CursorMode::Normal);
    return;
  }

  QVector3D hit;
  if (!m_picking_service->screen_to_ground(
          QPointF(sx, sy), *m_camera, m_viewport.width, m_viewport.height, hit)) {
    return;
  }

  auto* commander = m_control_mode == PlayerControlMode::Commander
                        ? controlled_commander_entity()
                        : find_local_commander();
  auto* unit =
      commander != nullptr ? commander->get_component<Engine::Core::UnitComponent>()
                           : nullptr;
  auto* commander_data =
      commander != nullptr
          ? commander->get_component<Engine::Core::CommanderComponent>()
          : nullptr;
  if (commander == nullptr || unit == nullptr || commander_data == nullptr ||
      commander_data->flag_rally_in_progress ||
      unit->owner_id != m_runtime.local_owner_id || unit->health <= 0) {
    cancel_commander_flag_rally();
    return;
  }

  std::vector<Engine::Core::EntityID> const commander_selection{commander->get_id()};
  auto const move_plan =
      Game::Systems::CommandService::plan_ground_move(*m_world, commander_selection, hit);
  if (move_plan.positions.empty()) {
    cancel_commander_flag_rally();
    return;
  }

  commander_data->begin_flag_rally(
      move_plan.resolved_target.x(), move_plan.resolved_target.z(), false);
  if (m_control_mode == PlayerControlMode::Commander) {
    commander_data->fpv_controlled = false;
    reset_commander_input();
  }
  if (auto* stamina = commander->get_component<Engine::Core::StaminaComponent>()) {
    stamina->run_requested = false;
    stamina->is_running = false;
  }
  Game::Systems::CommandService::issue_ground_move(*m_world, commander_selection, move_plan);

  m_commander_rally_preview_pos = std::nullopt;
  set_cursor_mode(CursorMode::Normal);
}

void GameEngine::cancel_commander_flag_rally() {
  m_commander_rally_preview_pos = std::nullopt;
  if (m_cursor_manager &&
      m_cursor_manager->mode() == CursorMode::PlaceCommanderRally) {
    set_cursor_mode(CursorMode::Normal);
  }
}

auto GameEngine::is_placing_commander_rally() const -> bool {
  return m_cursor_manager &&
         m_cursor_manager->mode() == CursorMode::PlaceCommanderRally;
}

auto GameEngine::has_commander_rally_preview() const -> bool {
  return m_commander_rally_preview_pos.has_value();
}

auto GameEngine::get_commander_rally_preview() const -> QVector3D {
  return m_commander_rally_preview_pos.value_or(QVector3D{});
}

auto GameEngine::has_commander_rally_flag() const -> bool {
  if (m_world == nullptr) {
    return false;
  }
  for (auto* entity :
       m_world->get_entities_with<Engine::Core::CommanderComponent>()) {
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->owner_id != m_runtime.local_owner_id) {
      continue;
    }
    auto* commander_data =
        entity->get_component<Engine::Core::CommanderComponent>();
    if (commander_data != nullptr && commander_data->flag_rally_flag_active) {
      return true;
    }
  }
  return false;
}

auto GameEngine::get_commander_rally_flag_position() const -> QVector3D {
  if (m_world == nullptr) {
    return {};
  }
  for (auto* entity :
       m_world->get_entities_with<Engine::Core::CommanderComponent>()) {
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->owner_id != m_runtime.local_owner_id) {
      continue;
    }
    auto* commander_data =
        entity->get_component<Engine::Core::CommanderComponent>();
    if (commander_data != nullptr && commander_data->flag_rally_flag_active) {
      return {commander_data->flag_rally_flag_x,
              0.0F,
              commander_data->flag_rally_flag_z};
    }
  }
  return {};
}

void GameEngine::seed_commander_rally_preview_from_view_center() {
  if (m_picking_service == nullptr || m_camera == nullptr || m_viewport.width <= 0 ||
      m_viewport.height <= 0) {
    return;
  }

  QVector3D hit;
  if (!m_picking_service->screen_to_ground(QPointF(m_viewport.width * 0.5,
                                                   m_viewport.height * 0.5),
                                           *m_camera,
                                           m_viewport.width,
                                           m_viewport.height,
                                           hit)) {
    return;
  }
  m_commander_rally_preview_pos =
      Game::Systems::CommandService::snap_to_walkable_ground(hit);
}

void GameEngine::restore_controlled_commander_direct_control_if_ready() {
  if (m_control_mode != PlayerControlMode::Commander) {
    return;
  }

  auto* commander = controlled_commander_entity();
  auto* unit =
      commander != nullptr ? commander->get_component<Engine::Core::UnitComponent>()
                           : nullptr;
  auto* commander_data =
      commander != nullptr
          ? commander->get_component<Engine::Core::CommanderComponent>()
          : nullptr;
  if (commander == nullptr || unit == nullptr || commander_data == nullptr ||
      unit->health <= 0 || commander_data->fpv_controlled ||
      commander_data->flag_rally_in_progress || is_placing_commander_rally()) {
    return;
  }

  commander_data->fpv_controlled = true;
  reset_commander_input();
}

void GameEngine::commander_dodge() {
  if (m_control_mode == PlayerControlMode::Commander) {
    m_commander_control.request_dodge();
  }
}

void GameEngine::commander_jump() {
  if (m_control_mode == PlayerControlMode::Commander) {
    m_commander_control.request_jump();
  }
}

void GameEngine::commander_cycle_lock_on() {
  if (m_control_mode != PlayerControlMode::Commander || m_world == nullptr) {
    return;
  }
  m_commander_control.cycle_lock_on_target(
      *m_world, m_controlled_commander_id, m_runtime.local_owner_id);
}

void GameEngine::commander_special_action() {
  if (m_control_mode == PlayerControlMode::Commander) {
    m_commander_control.special_action();
  }
}

void GameEngine::commander_vanguard_rush() {
  if (m_control_mode == PlayerControlMode::Commander) {
    m_commander_control.request_vanguard_rush();
  }
}

void GameEngine::commander_second_wind() {
  if (m_control_mode == PlayerControlMode::Commander) {
    m_commander_control.request_second_wind();
  }
}

void GameEngine::commander_toggle_camera_mode() {
  if (m_control_mode != PlayerControlMode::Commander || m_world == nullptr) {
    return;
  }
  m_commander_control.toggle_close_camera_mode(
      *m_world, m_controlled_commander_id, m_runtime.local_owner_id);
}

void GameEngine::commander_mouse_move(qreal dx, qreal dy) {
  m_commander_control.mouse_move(dx, dy);
}

void GameEngine::commander_mouse_look_at(qreal sx,
                                         qreal sy,
                                         qreal center_sx,
                                         qreal center_sy) {
  m_commander_control.mouse_look_at(sx, sy, center_sx, center_sy, m_window);
}

void GameEngine::commander_center_mouse(qreal center_sx, qreal center_sy) {
  m_commander_control.center_mouse(center_sx, center_sy, m_window);
}

void GameEngine::poll_commander_mouse_look() {
  m_commander_control.poll_mouse_look(m_window);
}

void GameEngine::update_cursor(Qt::CursorShape new_cursor) {
  if (m_window == nullptr) {
    return;
  }
  if (m_runtime.current_cursor != new_cursor) {
    m_runtime.current_cursor = new_cursor;
    QPointer<QQuickWindow> const safe_window(m_window);
    QMetaObject::invokeMethod(
        m_window,
        [safe_window, new_cursor]() {
          if (safe_window != nullptr) {
            safe_window->setCursor(new_cursor);
          }
        },
        Qt::AutoConnection);
  }
}

void GameEngine::set_error(const QString& error_message) {
  if (m_runtime.last_error != error_message) {
    m_runtime.last_error = error_message;
    qCritical() << "GameEngine error:" << error_message;
    emit last_error_changed();
  }
}

void GameEngine::set_cursor_mode(CursorMode mode) {
  if (!m_cursor_manager) {
    return;
  }
  m_cursor_manager->set_mode(mode);
  m_cursor_manager->update_cursor_shape(m_window);
}

void GameEngine::set_cursor_mode(const QString& mode) {
  set_cursor_mode(CursorModeUtils::fromString(mode));
}

auto GameEngine::cursor_mode() const -> QString {
  if (!m_cursor_manager) {
    return "normal";
  }
  return m_cursor_manager->mode_string();
}

auto GameEngine::global_cursor_x() const -> qreal {
  if (!m_cursor_manager) {
    return 0;
  }
  return m_cursor_manager->global_cursor_x(m_window);
}

auto GameEngine::global_cursor_y() const -> qreal {
  if (!m_cursor_manager) {
    return 0;
  }
  return m_cursor_manager->global_cursor_y(m_window);
}

void GameEngine::set_hover_at_screen(qreal sx, qreal sy) {
  if (m_window == nullptr) {
    return;
  }
  ensure_initialized();
  if (m_input_handler) {
    m_input_handler->set_hover_at_screen(sx, sy, m_viewport);
  }
  update_civilian_delivery_availability();

  // Update rally flag preview position when in placement mode.
  if (m_cursor_manager &&
      m_cursor_manager->mode() == CursorMode::PlaceCommanderRally &&
      m_picking_service != nullptr && m_camera != nullptr) {
    QVector3D hit;
    if (m_picking_service->screen_to_ground(
            QPointF(sx, sy), *m_camera, m_viewport.width, m_viewport.height, hit)) {
      m_commander_rally_preview_pos =
          Game::Systems::CommandService::snap_to_walkable_ground(hit);
    }
  }
}

void GameEngine::on_click_select(qreal sx, qreal sy, bool additive) {
  if (m_window == nullptr) {
    return;
  }
  ensure_initialized();
  if (m_input_handler) {
    m_input_handler->on_click_select(
        sx, sy, additive, m_runtime.local_owner_id, m_viewport);
  }
}

void GameEngine::on_area_selected(
    qreal x1, qreal y1, qreal x2, qreal y2, bool additive) {
  if (m_window == nullptr) {
    return;
  }
  ensure_initialized();
  if (m_input_handler) {
    m_input_handler->on_area_selected(
        x1, y1, x2, y2, additive, m_runtime.local_owner_id, m_viewport);
  }
}

void GameEngine::select_all_troops() {
  ensure_initialized();
  if (m_input_handler) {
    m_input_handler->select_all_troops(m_runtime.local_owner_id);
  }
}

void GameEngine::select_unit_by_id(int unit_id) {
  ensure_initialized();
  if (m_input_handler) {
    m_input_handler->select_unit_by_id(unit_id, m_runtime.local_owner_id);
  }
}

void GameEngine::ensure_initialized() {
  QString error;
  Game::Map::WorldBootstrap::ensure_initialized(m_runtime.initialized,
                                                *m_renderer,
                                                *m_camera,
                                                m_surface ? m_surface->ground()
                                                          : nullptr,
                                                &error);
  if (!error.isEmpty()) {
    set_error(error);
  }
}

auto GameEngine::enemy_troops_defeated() const -> int {
  return m_enemy_troops_defeated;
}

auto GameEngine::selected_player_state() const -> QVariantMap {
  return m_selected_player_state;
}

void GameEngine::set_selected_player_id(int id) {
  if (m_selected_player_id == id) {
    return;
  }
  m_selected_player_id = id;
  sync_selected_player_state();
  emit selected_player_id_changed();
}

auto GameEngine::scene_context() const -> AppSceneContext {
  return AppSceneContext{.world = m_world.get(),
                         .renderer = m_renderer.get(),
                         .active_camera = m_camera,
                         .ground = m_surface ? m_surface->ground() : nullptr,
                         .terrain = m_surface ? m_surface->terrain() : nullptr,
                         .features = m_features.get(),
                         .scatter = m_scatter.get(),
                         .fog = m_fog.get(),
                         .boundary_fog = m_boundary_fog.get(),
                         .rain = m_rain.get(),
                         .minimap_manager = m_minimap_manager.get(),
                         .visibility_coordinator = m_visibility_coordinator.get(),
                         .victory_service = m_victory_service.get(),
                         .rain_manager = m_rain_manager.get()};
}

auto GameEngine::get_player_stats(int owner_id) -> QVariantMap {
  QVariantMap result;

  auto& stats_registry = Game::Systems::GlobalStatsRegistry::instance();
  const auto* stats = stats_registry.get_stats(owner_id);

  if (stats != nullptr) {
    result["troopsRecruited"] = stats->troops_recruited;
    result["enemiesKilled"] = stats->enemies_killed;
    result["losses"] = stats->losses;
    result["barracksOwned"] = stats->barracks_owned;
    result["playTimeSec"] = stats->play_time_sec;
    result["gameEnded"] = stats->game_ended;
  } else {
    result["troopsRecruited"] = 0;
    result["enemiesKilled"] = 0;
    result["losses"] = 0;
    result["barracksOwned"] = 0;
    result["playTimeSec"] = 0.0F;
    result["gameEnded"] = false;
  }

  return result;
}

void GameEngine::update_rts_control_mode(float dt) {
  (void)dt;
  if (m_camera_controller) {
    m_camera_controller->update_follow(m_follow_selection_enabled);
  }
}

void GameEngine::clear_controlled_commander_state() {
  auto* commander = controlled_commander_entity();
  if (commander == nullptr) {
    return;
  }

  if (auto* commander_data =
          commander->get_component<Engine::Core::CommanderComponent>()) {
    commander_data->rally_requested = false;
    commander_data->rally_requires_manual_trigger = false;
    commander_data->fpv_controlled = false;
    commander_data->power_strike_active = false;
    commander_data->just_struck_enemy = false;
    commander_data->last_strike_combo_step = 0U;
    commander_data->jump_active = false;
    commander_data->jump_phase = 0.0F;
    commander_data->jump_height_offset = 0.0F;
    commander_data->posture = 0.0F;
    commander_data->punish_window_remaining = 0.0F;
    commander_data->shield_bash_cooldown_remaining = 0.0F;
    commander_data->vanguard_rush_cooldown_remaining = 0.0F;
    commander_data->second_wind_cooldown_remaining = 0.0F;
  }
  if (auto* guard = commander->get_component<Engine::Core::CommanderGuardComponent>()) {
    guard->active = false;
    guard->perfect_guard_remaining = 0.0F;
    guard->guard_break_remaining = 0.0F;
    guard->rearm_requires_release = false;
  }
  if (auto* rpg = commander->get_component<Engine::Core::RpgHealthComponent>()) {
    rpg->active = false;
    rpg->dodge_invincible = false;
  }
  if (auto* rpg_targets =
          commander->get_component<Engine::Core::RpgCommanderTargetComponent>()) {
    rpg_targets->explicit_lock_target_id = 0;
    rpg_targets->aim_candidate_id = 0;
    rpg_targets->recent_hit_target_id = 0;
    rpg_targets->recent_hit_timer = 0.0F;
  }
  if (auto* rpg_action =
          commander->get_component<Engine::Core::RpgCommanderActionComponent>()) {
    rpg_action->phase = Engine::Core::RpgCommanderActionPhase::None;
    rpg_action->melee_attack_style = 0;
    rpg_action->melee_attack_sequence = 0;
    rpg_action->active_target_id = 0;
    rpg_action->last_hit_target_id = 0;
    rpg_action->last_damage = 0;
    rpg_action->phase_time = 0.0F;
  }
  reset_movement(commander);
}

void GameEngine::update_commander_control_mode(float dt) {
  poll_commander_mouse_look();
  if (m_world == nullptr || m_commander_camera == nullptr) {
    exit_commander_control_mode();
    return;
  }

  if (!m_commander_control.update(*m_world,
                                  m_controlled_commander_id,
                                  m_runtime.local_owner_id,
                                  *m_commander_camera,
                                  dt)) {
    exit_commander_control_mode();
    return;
  }

  auto* cmd_ent = m_world->get_entity(m_controlled_commander_id);
  if (cmd_ent != nullptr) {
    auto* cmd = cmd_ent->get_component<Engine::Core::CommanderComponent>();
    if (cmd != nullptr && cmd->just_struck_enemy) {
      cmd->just_struck_enemy = false;
      m_rpg_hit_stop_timer = 0.10F;
    }
  }

  Game::Systems::RpgCombat::tick_rpg_combat(
      m_world.get(), m_controlled_commander_id, dt);
}

auto GameEngine::apply_runtime_time_effects(float dt) -> float {
  if (m_control_mode != PlayerControlMode::Commander) {
    return dt;
  }

  if (!m_runtime.paused && m_rpg_hit_stop_timer > 0.0F) {
    m_rpg_hit_stop_timer -= dt;
    if (m_rpg_hit_stop_timer < 0.0F) {
      m_rpg_hit_stop_timer = 0.0F;
    } else {
      dt *= 0.10F;
    }
  }
  return dt;
}

void GameEngine::update_active_runtime_simulation(float dt) {
  if (m_world == nullptr) {
    return;
  }

  if (m_control_mode == PlayerControlMode::Commander) {
    update_commander_control_mode(dt);
    m_world->update(dt);
    restore_controlled_commander_direct_control_if_ready();
    return;
  }

  m_world->update(dt);
  update_rts_control_mode(dt);
}

auto GameEngine::should_render_selected_entity(Engine::Core::EntityID id) const
    -> bool {
  return m_control_mode != PlayerControlMode::Commander ||
         id != m_controlled_commander_id;
}

void GameEngine::render_runtime_mode_effects() {
  if (m_control_mode != PlayerControlMode::Commander || m_world == nullptr ||
      m_renderer == nullptr || m_controlled_commander_id == 0) {
    return;
  }

  Engine::Core::EntityID locked_target_id = m_commander_control.locked_target_id();
  if (auto* commander = m_world->get_entity(m_controlled_commander_id)) {
    if (auto* rpg_targets =
            commander->get_component<Engine::Core::RpgCommanderTargetComponent>()) {
      locked_target_id = rpg_targets->explicit_lock_target_id;
    }
  }

  m_rpg_telegraphs.render(m_renderer.get(),
                          m_world.get(),
                          m_controlled_commander_id,
                          locked_target_id,
                          m_renderer->get_animation_time());
}

void GameEngine::update(float dt) {

  if (m_runtime.loading) {
    return;
  }

  if (m_runtime.paused) {
    dt = 0.0F;
  } else {
    dt *= m_runtime.time_scale;
    dt = apply_runtime_time_effects(dt);
  }

  update_mission_waves(dt);

  RuntimeFrameState frame_state{
      .local_owner_id = m_runtime.local_owner_id,
      .spectator_mode = m_level.is_spectator_mode,
      .viewport_width = m_viewport.width,
      .viewport_height = m_viewport.height,
      .selection_refresh_enabled = (m_selected_units_model != nullptr),
      .selection_refresh_counter = m_runtime.selection_refresh_counter};
  const FrameUpdateCallbacks callbacks{
      .on_minimap_image_changed = [this]() { emit minimap_image_changed(); },
      .on_selected_units_data_changed =
          [this]() {
            emit selected_units_data_changed();
          }};

  m_frame_orchestrator.update(
      scene_context(),
      frame_state,
      m_entity_cache,
      (!m_runtime.paused && !m_runtime.loading) ? m_ambient_state_manager.get()
                                                : nullptr,
      m_runtime.victory_state,
      dt,
      callbacks,
      [this](float step_dt) {
        log_render_stage_once(
            "simulation-update",
            QStringLiteral(
                "world systems run before render; combat queries rebuild here"));
        update_active_runtime_simulation(step_dt);
      });
  m_runtime.selection_refresh_counter = frame_state.selection_refresh_counter;
  sync_scatter_world_props();
  sync_selected_player_state();
}

void GameEngine::render(int pixel_width, int pixel_height) {
  if (!m_renderer || !m_world || !m_runtime.initialized || m_runtime.loading) {
    return;
  }

  log_render_stage_once("render-submit",
                        QStringLiteral("records draw commands from existing "
                                       "visual state; no combat queries"));

  Game::Systems::CameraVisibilityService::instance().set_camera(m_camera);

  if (pixel_width > 0 && pixel_height > 0) {
    m_viewport.width = pixel_width;
    m_viewport.height = pixel_height;
    m_renderer->set_viewport(pixel_width, pixel_height);
  }

  if (auto* selection_system = m_world->get_system<Game::Systems::SelectionSystem>()) {
    const auto& sel = selection_system->get_selected_units();
    std::vector<unsigned int> ids;
    ids.reserve(sel.size());
    for (const auto id : sel) {
      if (!should_render_selected_entity(id)) {
        continue;
      }
      ids.push_back(id);
    }
    m_renderer->set_selected_entities(ids);
  }

  m_renderer->begin_frame();

  if (m_terrain_scene) {
    m_terrain_scene->submit(*m_renderer, m_renderer->resources());
  }

  if (m_renderer && m_hover_tracker) {
    m_renderer->set_hovered_entity_id(m_hover_tracker->get_last_hovered_entity());
  }
  if (m_renderer) {
    m_renderer->set_local_owner_id(m_runtime.local_owner_id);
  }

  m_renderer->render_world(m_world.get());
  render_game_effects();
  m_renderer->end_frame();

  update_loading_overlay();
  update_cursor_position();
}

void GameEngine::render_game_effects() {
  auto* res = m_renderer->resources();
  if (res == nullptr) {
    return;
  }

  if (auto* arrow_system = m_world->get_system<Game::Systems::ArrowSystem>()) {
    Render::GL::render_arrows(m_renderer.get(), res, *arrow_system);
  }

  if (auto* projectile_system =
          m_world->get_system<Game::Systems::ProjectileSystem>()) {
    Render::GL::render_projectiles(m_renderer.get(), res, *projectile_system);
  }

  if (auto* healing_beam_system =
          m_world->get_system<Game::Systems::HealingBeamSystem>()) {
    if (auto* res = m_renderer->resources()) {
      Render::GL::render_healing_beams(m_renderer.get(), res, *healing_beam_system);
      Render::GL::render_healing_waves(m_renderer.get(), res, *healing_beam_system);
    }
  }

  Render::GL::render_healer_auras(m_renderer.get(), res, m_world.get());
  Render::GL::render_combat_dust(m_renderer.get(), res, m_world.get());

  render_runtime_mode_effects();

  std::optional<QVector3D> preview_waypoint;
  if (m_command_controller && m_command_controller->has_patrol_first_waypoint()) {
    preview_waypoint = m_command_controller->get_patrol_first_waypoint();
  }
  Render::GL::render_patrol_flags(m_renderer.get(), res, *m_world, preview_waypoint);

  // Render the commander rally flag preview (during placement mode) and
  // any placed rally flags from CommanderComponents.
  Render::GL::render_commander_rally_flags(
      m_renderer.get(),
      res,
      m_world.get(),
      m_runtime.local_owner_id,
      m_commander_rally_preview_pos);

  if (m_command_controller && m_command_controller->is_placing_formation()) {
    Render::GL::FormationPlacementInfo placement;
    placement.position = m_command_controller->get_formation_placement_position();
    placement.position.setY(Game::Map::TerrainService::instance().get_terrain_height(
        placement.position.x(), placement.position.z()));
    placement.angle_degrees = m_command_controller->get_formation_placement_angle();
    placement.active = true;

    const auto* nation =
        Game::Systems::NationRegistry::instance().get_nation_for_player(
            m_runtime.local_owner_id);
    if (nation != nullptr) {
      switch (nation->id) {
      case Game::Systems::NationID::RomanRepublic:

        placement.accent_color = QVector3D(0.85F, 0.12F, 0.10F);
        break;
      case Game::Systems::NationID::Carthage:

        placement.accent_color = QVector3D(0.62F, 0.08F, 0.78F);
        break;
      default:

        break;
      }
    }

    Render::GL::render_formation_arrow(m_renderer.get(), res, placement);
  }
}

void GameEngine::update_loading_overlay() {
  if (!m_loading_overlay_wait_for_first_frame.load(std::memory_order_acquire)) {
    return;
  }

  if (QThread::currentThread() != thread()) {
    QMetaObject::invokeMethod(
        this, [this]() { update_loading_overlay(); }, Qt::QueuedConnection);
    return;
  }

  if (!m_renderer || (m_renderer->resources() == nullptr)) {
    m_loading_overlay_frames_remaining = 5;
    m_loading_overlay_timer.restart();
    return;
  }

  if (m_loading_overlay_frames_remaining > 0) {
    m_loading_overlay_frames_remaining--;
  }

  constexpr qint64 k_loading_overlay_max_wait_ms = 15000;
  const qint64 elapsed_ms =
      m_loading_overlay_timer.isValid() ? m_loading_overlay_timer.elapsed() : 0;
  const bool enough_time = m_loading_overlay_timer.isValid() &&
                           (elapsed_ms >= m_loading_overlay_min_duration_ms);
  const bool exceeded_max_wait = m_loading_overlay_timer.isValid() &&
                                 (elapsed_ms >= k_loading_overlay_max_wait_ms);

  QStringList pending_components;
  const bool scatter_ready = !m_scatter || m_scatter->is_gpu_ready();

  if (!scatter_ready) {
    pending_components << QStringLiteral("terrain scatter");
  }

  const bool biome_gpu_ready = pending_components.isEmpty();

  if (enough_time && m_loading_overlay_frames_remaining <= 0 &&
      (biome_gpu_ready || exceeded_max_wait)) {
    if (exceeded_max_wait && !biome_gpu_ready) {
      qWarning() << "Loading overlay timed out waiting for GPU readiness"
                 << pending_components.join(", ");
    }
    m_loading_overlay_wait_for_first_frame.store(false, std::memory_order_release);
    m_loading_overlay_active = false;
    if (m_finalize_progress_after_overlay && m_loading_progress_tracker) {
      m_loading_progress_tracker->set_stage(
          LoadingProgressTracker::LoadingStage::COMPLETED);
    }
    m_finalize_progress_after_overlay = false;
    emit is_loading_changed();

    if (m_show_objectives_after_loading) {
      m_show_objectives_after_loading = false;
      emit campaign_mission_changed();
    }
  }
}

void GameEngine::update_cursor_position() {
  if (QThread::currentThread() != thread()) {
    QMetaObject::invokeMethod(
        this, [this]() { update_cursor_position(); }, Qt::QueuedConnection);
    return;
  }
  qreal const current_x = global_cursor_x();
  qreal const current_y = global_cursor_y();
  if (current_x != m_runtime.last_cursor_x || current_y != m_runtime.last_cursor_y) {
    m_runtime.last_cursor_x = current_x;
    m_runtime.last_cursor_y = current_y;
    emit global_cursor_changed();
  }
}

void GameEngine::update_civilian_delivery_availability() {
  bool available = false;
  if (m_world && m_hover_tracker) {
    const auto hovered_id = m_hover_tracker->get_last_hovered_entity();
    auto* hovered = hovered_id != 0 ? m_world->get_entity(hovered_id) : nullptr;
    auto* hovered_unit = (hovered != nullptr)
                             ? hovered->get_component<Engine::Core::UnitComponent>()
                             : nullptr;
    const bool hovered_friendly_barracks =
        (hovered_unit != nullptr) &&
        hovered_unit->owner_id == m_runtime.local_owner_id &&
        hovered_unit->spawn_type == Game::Units::SpawnType::Barracks;
    auto* hovered_production =
        hovered != nullptr ? hovered->get_component<Engine::Core::ProductionComponent>()
                           : nullptr;
    const bool barracks_has_room =
        (hovered_production != nullptr) &&
        (hovered_production->manpower_available +
             Game::Systems::k_civilian_delivery_population_grant <=
         hovered_production->max_units);

    if (hovered_friendly_barracks && barracks_has_room) {
      auto* selection_system = m_world->get_system<Game::Systems::SelectionSystem>();
      if (selection_system != nullptr) {
        for (const auto id : selection_system->get_selected_units()) {
          auto* selected_entity = m_world->get_entity(id);
          auto* selected_unit =
              (selected_entity != nullptr)
                  ? selected_entity->get_component<Engine::Core::UnitComponent>()
                  : nullptr;
          if ((selected_unit != nullptr) &&
              (selected_unit->owner_id == m_runtime.local_owner_id) &&
              (selected_unit->spawn_type == Game::Units::SpawnType::Civilian)) {
            available = true;
            break;
          }
        }
      }
    }
  }

  if (m_civilian_delivery_available != available) {
    m_civilian_delivery_available = available;
    emit civilian_delivery_available_changed();
  }
}

auto GameEngine::screen_to_ground(const QPointF& screen_pt,
                                  QVector3D& out_world) -> bool {
  return App::Utils::screen_to_ground(m_picking_service.get(),
                                      m_camera,
                                      m_window,
                                      m_viewport.width,
                                      m_viewport.height,
                                      screen_pt,
                                      out_world);
}

auto GameEngine::world_to_screen(const QVector3D& world,
                                 QPointF& out_screen) const -> bool {
  return App::Utils::world_to_screen(m_picking_service.get(),
                                     m_camera,
                                     m_window,
                                     m_viewport.width,
                                     m_viewport.height,
                                     world,
                                     out_screen);
}

void GameEngine::sync_selection_flags() {
  auto* selection_system = m_world->get_system<Game::Systems::SelectionSystem>();
  if (!m_world || (selection_system == nullptr)) {
    return;
  }

  App::Utils::sanitize_selection(m_world.get(), selection_system);

  if (selection_system->get_selected_units().empty()) {
    if (m_cursor_manager && m_cursor_manager->mode() != CursorMode::Normal) {
      set_cursor_mode(CursorMode::Normal);
    }
  }

  emit hold_mode_changed(any_selected_in_hold_mode());
  emit guard_mode_changed(any_selected_in_guard_mode());
  emit formation_mode_changed(any_selected_in_formation_mode());
  emit run_mode_changed(any_selected_in_run_mode());
  update_civilian_delivery_availability();
}

void GameEngine::camera_move(float dx, float dz) {
  ensure_initialized();
  if (m_camera_controller) {
    m_camera_controller->move(dx, dz);
  }
}

void GameEngine::camera_elevate(float dy) {
  ensure_initialized();
  if (m_camera_controller) {
    m_camera_controller->elevate(dy);
  }
}

void GameEngine::reset_camera() {
  ensure_initialized();
  if (m_camera_controller) {
    m_camera_controller->reset(m_runtime.local_owner_id, m_level);
  }
}

void GameEngine::camera_zoom(float delta) {
  ensure_initialized();
  if (m_camera_controller) {
    m_camera_controller->zoom(delta);
  }
}

auto GameEngine::camera_distance() const -> float {
  if (m_camera_controller) {
    return m_camera_controller->distance();
  }
  return 0.0F;
}

void GameEngine::camera_yaw(float degrees) {
  ensure_initialized();
  if (m_camera_controller) {
    m_camera_controller->yaw(degrees);
  }
}

void GameEngine::camera_orbit(float yaw_deg, float pitch_deg) {
  ensure_initialized();
  if (m_camera_controller) {
    m_camera_controller->orbit(yaw_deg, pitch_deg);
  }
}

void GameEngine::camera_orbit_direction(int direction, bool shift) {
  if (m_camera_controller) {
    m_camera_controller->orbit_direction(direction, shift);
  }
}

void GameEngine::camera_follow_selection(bool enable) {
  ensure_initialized();
  m_follow_selection_enabled = enable;
  if (m_camera_controller) {
    m_camera_controller->follow_selection(enable);
  }
}

void GameEngine::camera_set_follow_lerp(float alpha) {
  ensure_initialized();
  if (m_camera_controller) {
    m_camera_controller->set_follow_lerp(alpha);
  }
}

void GameEngine::on_minimap_left_click(qreal mx,
                                       qreal my,
                                       qreal minimap_width,
                                       qreal minimap_height) {
  ensure_initialized();
  if ((m_camera == nullptr) || !m_minimap_manager ||
      !m_minimap_manager->has_minimap()) {
    return;
  }

  const QImage& minimap_img = m_minimap_manager->get_image();
  if (minimap_img.isNull()) {
    return;
  }

  const auto img_width = static_cast<float>(minimap_img.width());
  const auto img_height = static_cast<float>(minimap_img.height());

  const float px =
      (static_cast<float>(mx) / static_cast<float>(minimap_width)) * img_width;
  const float py =
      (static_cast<float>(my) / static_cast<float>(minimap_height)) * img_height;

  const auto [world_x, world_z] =
      Game::Map::Minimap::pixel_to_world(px,
                                         py,
                                         m_minimap_manager->get_world_width(),
                                         m_minimap_manager->get_world_height(),
                                         img_width,
                                         img_height,
                                         m_minimap_manager->get_tile_size());

  if (m_camera != nullptr) {
    const QVector3D new_target(world_x, 0.0F, world_z);
    const QVector3D current_target = m_camera->get_target();
    const QVector3D current_position = m_camera->get_position();

    const QVector3D offset = current_position - current_target;

    m_camera->look_at(new_target + offset, new_target, m_camera->get_up_vector());
  }

  m_follow_selection_enabled = false;
  if (m_camera_controller) {
    m_camera_controller->follow_selection(false);
  }
}

void GameEngine::on_minimap_right_click(qreal mx,
                                        qreal my,
                                        qreal minimap_width,
                                        qreal minimap_height) {
  ensure_initialized();
  if (m_level.is_spectator_mode || !m_world || !m_minimap_manager ||
      !m_minimap_manager->has_minimap()) {
    return;
  }

  const QImage& minimap_img = m_minimap_manager->get_image();
  if (minimap_img.isNull()) {
    return;
  }

  const auto img_width = static_cast<float>(minimap_img.width());
  const auto img_height = static_cast<float>(minimap_img.height());

  const float px =
      (static_cast<float>(mx) / static_cast<float>(minimap_width)) * img_width;
  const float py =
      (static_cast<float>(my) / static_cast<float>(minimap_height)) * img_height;

  const auto [world_x, world_z] =
      Game::Map::Minimap::pixel_to_world(px,
                                         py,
                                         m_minimap_manager->get_world_width(),
                                         m_minimap_manager->get_world_height(),
                                         img_width,
                                         img_height,
                                         m_minimap_manager->get_tile_size());

  auto* selection_system = m_world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return;
  }

  const auto& selected = selection_system->get_selected_units();
  if (selected.empty()) {
    return;
  }

  const QVector3D target_pos(world_x, 0.0F, world_z);
  auto formation_result = Game::Systems::FormationPlanner::get_formation_with_facing(
      *m_world,
      selected,
      target_pos,
      Game::GameConfig::instance().gameplay().formation_spacing_default);

  for (size_t i = 0; i < selected.size(); ++i) {
    auto* entity = m_world->get_entity(selected[i]);
    if (entity == nullptr) {
      continue;
    }

    auto* formation_mode =
        entity->get_component<Engine::Core::FormationModeComponent>();
    if ((formation_mode == nullptr) || !formation_mode->active) {
      continue;
    }

    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if ((transform != nullptr) && (i < formation_result.facing_angles.size())) {
      transform->desired_yaw = formation_result.facing_angles[i];
      transform->has_desired_yaw = true;
    }
  }

  Game::Systems::CommandService::MoveOptions opts;
  opts.kind = Game::Systems::MoveOrderKind::FormationMove;
  opts.group_move = selected.size() > 1;
  opts.retry_individual_on_group_failure = selected.size() > 1;
  opts.preserve_formation_mode = formation_result.used_tactical_formation;
  Game::Systems::CommandService::move_units(
      *m_world, selected, formation_result.positions, opts);
}

auto GameEngine::selected_units_model() -> QAbstractItemModel* {
  return m_selected_units_model;
}

auto GameEngine::audio_system() -> QObject* {
  return m_audio_systemProxy.get();
}

void GameEngine::set_audio_frontend_context(const QString& context) {
  const QString normalized = context.trimmed().toLower();
  if (m_audio_frontend_context == normalized) {
    return;
  }

  m_audio_frontend_context = normalized;
  apply_frontend_music_context(normalized);
}

void GameEngine::apply_frontend_music_context(const QString& context) {
  if (context.isEmpty() || context == QStringLiteral("battle")) {
    AudioSystem::get_instance().stop_music();
    return;
  }

  AudioResourceLoader::load_audio_resources(AudioLoadPolicy::Screen);

  QMap<QString, QString> tags;
  tags.insert(QStringLiteral("screen_context"), context);
  tags.insert(QStringLiteral("variant"), QStringLiteral("primary"));

  QString track_id =
      AudioResourceLoader::find_first_resource_id(AudioCategory::MUSIC, tags);
  if (track_id.isEmpty()) {
    tags.remove(QStringLiteral("variant"));
    track_id = AudioResourceLoader::find_first_resource_id(AudioCategory::MUSIC, tags);
  }
  if (track_id.isEmpty()) {
    return;
  }

  AudioResourceLoader::ensure_audio_resource_loaded(track_id);
  AudioSystem::get_instance().play_music(track_id.toStdString(), 1.0F, true);
}

void GameEngine::configure_audio_manifest_mappings() {
  configure_audio_voice_mappings();
  configure_audio_ambient_mappings();
}

void GameEngine::configure_audio_voice_mappings() {
  if (!m_audio_event_handler) {
    return;
  }

  static constexpr const char* k_common_units[] = {
      "archer",
      "swordsman",
      "spearman",
      "horse_archer",
      "horse_spearman",
      "horse_swordsman",
      "healer",
      "catapult",
      "ballista",
      "builder",
      "elephant",
  };

  static constexpr std::pair<Game::Systems::NationID, const char*> k_factions[] = {
      {Game::Systems::NationID::RomanRepublic, "roman"},
      {Game::Systems::NationID::Carthage, "carthage"},
  };

  for (const auto& [nation_id, faction_name] : k_factions) {
    const QString faction_tag = nation_audio_tag(nation_id);
    for (const char* unit_name : k_common_units) {
      QMap<QString, QString> tags;
      tags.insert(QStringLiteral("faction"), faction_tag);
      tags.insert(QStringLiteral("unit"), QString::fromLatin1(unit_name));

      const QString track_id =
          AudioResourceLoader::find_first_resource_id(AudioCategory::VOICE, tags);
      if (!track_id.isEmpty()) {
        AudioResourceLoader::ensure_audio_resource_loaded(track_id);
        const std::string key =
            std::string(faction_name) + "." + std::string(unit_name);
        m_audio_event_handler->load_unit_voice_mapping(key, track_id.toStdString());
      }
    }
  }

  static constexpr const char* k_commander_units[] = {
      "roman_legion_organizer",
      "roman_veteran_consul",
      "roman_field_commander",
      "carthage_mercenary_broker",
      "carthage_cavalry_patron",
      "carthage_elephant_master",
  };

  for (const char* unit_name : k_commander_units) {
    QMap<QString, QString> tags;
    tags.insert(QStringLiteral("unit_def"), QString::fromLatin1(unit_name));
    const QString track_id =
        AudioResourceLoader::find_first_resource_id(AudioCategory::VOICE, tags);
    if (!track_id.isEmpty()) {
      AudioResourceLoader::ensure_audio_resource_loaded(track_id);
      m_audio_event_handler->load_unit_voice_mapping(unit_name, track_id.toStdString());
    }
  }
}

void GameEngine::configure_audio_ambient_mappings() {
  if (!m_audio_event_handler) {
    return;
  }

  QString faction_tag = QStringLiteral("roman");
  if (m_runtime.local_owner_id != 0) {
    if (const auto* nation =
            Game::Systems::NationRegistry::instance().get_nation_for_player(
                m_runtime.local_owner_id);
        nation != nullptr) {
      faction_tag = nation_audio_tag(nation->id);
    }
  }

  const auto resolve_music = [&](Engine::Core::AmbientState state) {
    QMap<QString, QString> tags;
    tags.insert(QStringLiteral("ambient_state"), ambient_state_tag(state));
    if (state == Engine::Core::AmbientState::VICTORY ||
        state == Engine::Core::AmbientState::DEFEAT) {
      tags.insert(QStringLiteral("faction"), faction_tag);
    }

    QString track_id =
        AudioResourceLoader::find_first_resource_id(AudioCategory::MUSIC, tags);
    if (track_id.isEmpty() && tags.contains(QStringLiteral("faction"))) {
      tags.remove(QStringLiteral("faction"));
      track_id =
          AudioResourceLoader::find_first_resource_id(AudioCategory::MUSIC, tags);
    }
    return track_id;
  };

  static constexpr Engine::Core::AmbientState k_states[] = {
      Engine::Core::AmbientState::PEACEFUL,
      Engine::Core::AmbientState::TENSE,
      Engine::Core::AmbientState::COMBAT,
      Engine::Core::AmbientState::VICTORY,
      Engine::Core::AmbientState::DEFEAT,
  };

  for (Engine::Core::AmbientState const state : k_states) {
    const QString track_id = resolve_music(state);
    if (!track_id.isEmpty()) {
      m_audio_event_handler->load_ambient_music(state, track_id.toStdString());
    }
  }
}

void GameEngine::ensure_result_audio_ready(const QString& state) {
  Engine::Core::AmbientState ambient_state;
  if (state == QStringLiteral("victory")) {
    ambient_state = Engine::Core::AmbientState::VICTORY;
  } else if (state == QStringLiteral("defeat")) {
    ambient_state = Engine::Core::AmbientState::DEFEAT;
  } else {
    return;
  }

  configure_audio_ambient_mappings();

  QMap<QString, QString> tags;
  tags.insert(QStringLiteral("ambient_state"), ambient_state_tag(ambient_state));

  if (m_runtime.local_owner_id != 0) {
    if (const auto* nation =
            Game::Systems::NationRegistry::instance().get_nation_for_player(
                m_runtime.local_owner_id);
        nation != nullptr) {
      tags.insert(QStringLiteral("faction"), nation_audio_tag(nation->id));
    }
  }

  QString track_id =
      AudioResourceLoader::find_first_resource_id(AudioCategory::MUSIC, tags);
  if (track_id.isEmpty() && tags.contains(QStringLiteral("faction"))) {
    tags.remove(QStringLiteral("faction"));
    track_id = AudioResourceLoader::find_first_resource_id(AudioCategory::MUSIC, tags);
  }
  if (!track_id.isEmpty()) {
    AudioResourceLoader::ensure_audio_resource_loaded(track_id);
  }
}

auto GameEngine::has_units_selected() const -> bool {
  if (!m_selection_controller) {
    return false;
  }
  return m_selection_controller->has_units_selected();
}

auto GameEngine::player_troop_count() const -> int {
  return m_entity_cache.player_troop_count;
}

auto GameEngine::has_selected_type(const QString& type) const -> bool {
  if (!m_selection_controller) {
    return false;
  }
  return m_selection_controller->has_selected_type(type);
}

void GameEngine::recruit_near_selected(const QString& unit_type) {
  ensure_initialized();
  if (!m_command_controller) {
    return;
  }
  m_command_controller->recruit_near_selected(unit_type, m_runtime.local_owner_id);
}

void GameEngine::start_building_placement(const QString& building_type) {
  ensure_initialized();
  if (m_production_manager) {
    m_production_manager->start_building_placement(building_type);
    set_cursor_mode(CursorMode::PlaceBuilding);
  }
}

void GameEngine::place_building_at_screen(qreal sx, qreal sy) {
  ensure_initialized();
  if (m_production_manager) {
    m_production_manager->place_building_at_screen(
        sx, sy, m_runtime.local_owner_id, m_viewport);
    set_cursor_mode(CursorMode::Normal);
  }
}

void GameEngine::cancel_building_placement() {
  if (m_production_manager) {
    m_production_manager->cancel_building_placement();
  }
  set_cursor_mode(CursorMode::Normal);
}

auto GameEngine::pending_building_type() const -> QString {
  return m_production_manager ? m_production_manager->pending_building_type()
                              : QString();
}

auto GameEngine::get_selected_production_state() const -> QVariantMap {
  return m_production_manager ? m_production_manager->get_selected_production_state(
                                    m_runtime.local_owner_id)
                              : QVariantMap();
}

auto GameEngine::get_selected_home_production_state() const -> QVariantMap {
  return m_production_manager
             ? m_production_manager->get_selected_home_production_state(
                   m_runtime.local_owner_id)
             : QVariantMap();
}

auto GameEngine::get_unit_production_info(
    const QString& unit_type, const QString& nation_id) const -> QVariantMap {
  return m_production_manager
             ? m_production_manager->get_unit_production_info(unit_type, nation_id)
             : QVariantMap();
}

auto GameEngine::get_selected_builder_production_state() const -> QVariantMap {
  return m_production_manager
             ? m_production_manager->get_selected_builder_production_state()
             : QVariantMap();
}

auto GameEngine::get_controlled_commander_status() const -> QVariantMap {
  QVariantMap result;
  result["has_commander"] = false;
  result["id"] = 0;
  result["name"] = QString();
  result["nation"] = QString();
  result["alive"] = false;
  result["health"] = 0;
  result["max_health"] = 0;
  result["health_ratio"] = 0.0;
  result["stamina_ratio"] = 1.0;
  result["is_running"] = false;
  result["can_run"] = false;
  result["rally_cooldown"] = 0.0;
  result["rally_cooldown_remaining"] = 0.0;
  result["rally_feedback_time"] = 0.0;
  result["rally_ready"] = false;
  result["rally_placing"] = false;
  result["rally_in_progress"] = false;
  result["rally_is_planting"] = false;
  result["rally_has_flag"] = false;
  result["rally_action_progress"] = 0.0;
  result["aura_active"] = false;
  result["posture"] = 0.0;
  result["posture_max"] = 100.0;
  result["posture_ratio"] = 0.0;
  result["punish_window_remaining"] = 0.0;
  result["punish_active"] = false;
  result["perfect_guard_remaining"] = 0.0;
  result["perfect_guard_active"] = false;
  result["guard_break_remaining"] = 0.0;
  result["guard_broken"] = false;
  result["finisher_ready"] = false;
  result["camera_mode"] = QStringLiteral("Chase");
  result["shield_bash_cooldown"] = 3.0;
  result["shield_bash_cooldown_remaining"] = 0.0;
  result["shield_bash_ready"] = true;
  result["vanguard_rush_cooldown"] = 4.5;
  result["vanguard_rush_cooldown_remaining"] = 0.0;
  result["vanguard_rush_ready"] = true;
  result["second_wind_cooldown"] = 8.0;
  result["second_wind_cooldown_remaining"] = 0.0;
  result["second_wind_ready"] = true;

  if (m_world == nullptr || m_controlled_commander_id == 0) {
    return result;
  }

  auto* entity = m_world->get_entity(m_controlled_commander_id);
  auto* unit = entity != nullptr ? entity->get_component<Engine::Core::UnitComponent>()
                                 : nullptr;
  auto* commander = entity != nullptr
                        ? entity->get_component<Engine::Core::CommanderComponent>()
                        : nullptr;
  if (entity == nullptr || unit == nullptr || commander == nullptr) {
    return result;
  }

  QString name;
  int health = 0;
  int max_health = 0;
  bool is_building = false;
  bool alive = false;
  QString nation;
  if (!get_unit_info(m_controlled_commander_id,
                     name,
                     health,
                     max_health,
                     is_building,
                     alive,
                     nation)) {
    return result;
  }

  float stamina_ratio = 1.0F;
  bool is_running = false;
  bool can_run = false;
  (void)get_unit_stamina_info(
      m_controlled_commander_id, stamina_ratio, is_running, can_run);

  if (!commander->display_name.empty()) {
    name = QString::fromStdString(commander->display_name);
  }

  result["has_commander"] = true;
  result["id"] = static_cast<int>(m_controlled_commander_id);
  result["name"] = name;
  result["nation"] = nation;
  result["alive"] = alive;

  auto* commander_entity = m_world->get_entity(m_controlled_commander_id);
  auto* rpg = commander_entity != nullptr
                  ? commander_entity->get_component<Engine::Core::RpgHealthComponent>()
                  : nullptr;
  if (rpg != nullptr && rpg->active) {
    result["health"] = rpg->rpg_hp;
    result["max_health"] = rpg->rpg_max_hp;
    result["health_ratio"] =
        rpg->rpg_max_hp > 0
            ? static_cast<double>(rpg->rpg_hp) / static_cast<double>(rpg->rpg_max_hp)
            : 0.0;
  } else {
    result["health"] = health;
    result["max_health"] = max_health;
    result["health_ratio"] =
        max_health > 0 ? static_cast<double>(health) / static_cast<double>(max_health)
                       : 0.0;
  }
  result["stamina_ratio"] = stamina_ratio;
  result["is_running"] = is_running;
  result["can_run"] = can_run;
  result["rally_cooldown"] = commander->rally_cooldown;
  result["rally_cooldown_remaining"] = commander->rally_cooldown_remaining;
  result["rally_feedback_time"] = commander->rally_feedback_time;
  result["rally_placing"] = is_placing_commander_rally();
  result["rally_in_progress"] = commander->flag_rally_in_progress;
  result["rally_is_planting"] = commander->is_flag_rally_planting();
  result["rally_has_flag"] = commander->flag_rally_flag_active;
  result["rally_action_progress"] =
      commander->is_flag_rally_planting() && commander->flag_rally_cost > 0.0F
          ? std::clamp(static_cast<double>(
                           1.0F - (commander->flag_rally_animation_timer /
                                   commander->flag_rally_cost)),
                       0.0,
                       1.0)
          : (commander->flag_rally_flag_active ? 1.0 : 0.0);
  result["rally_ready"] = commander->rally_cooldown_remaining <= 0.0F &&
                          !commander->flag_rally_in_progress &&
                          !is_placing_commander_rally();
  result["aura_active"] = commander->aura_active && !commander->wounded;
  result["posture"] = static_cast<double>(commander->posture);
  result["posture_max"] = static_cast<double>(commander->posture_max);
  result["posture_ratio"] =
      commander->posture_max > 0.0F
          ? static_cast<double>(commander->posture / commander->posture_max)
          : 0.0;
  result["punish_window_remaining"] =
      static_cast<double>(commander->punish_window_remaining);
  result["punish_active"] = commander->punish_window_remaining > 0.0F;
  result["finisher_ready"] = commander->combo_step >= 3;
  result["camera_mode"] =
      commander->close_camera_mode ? QStringLiteral("Close") : QStringLiteral("Chase");

  result["combo_step"] = commander->combo_step;
  result["shield_bash_cooldown"] = 3.0;
  result["shield_bash_cooldown_remaining"] =
      static_cast<double>(commander->shield_bash_cooldown_remaining);
  result["shield_bash_ready"] = commander->shield_bash_cooldown_remaining <= 0.0F;
  result["vanguard_rush_cooldown"] = 4.5;
  result["vanguard_rush_cooldown_remaining"] =
      static_cast<double>(commander->vanguard_rush_cooldown_remaining);
  result["vanguard_rush_ready"] = commander->vanguard_rush_cooldown_remaining <= 0.0F;
  result["second_wind_cooldown"] = 8.0;
  result["second_wind_cooldown_remaining"] =
      static_cast<double>(commander->second_wind_cooldown_remaining);
  result["second_wind_ready"] = commander->second_wind_cooldown_remaining <= 0.0F;

  if (auto* guard =
          commander_entity != nullptr
              ? commander_entity->get_component<Engine::Core::CommanderGuardComponent>()
              : nullptr) {
    result["perfect_guard_remaining"] =
        static_cast<double>(guard->perfect_guard_remaining);
    result["perfect_guard_active"] = guard->perfect_guard_remaining > 0.0F;
    result["guard_break_remaining"] = static_cast<double>(guard->guard_break_remaining);
    result["guard_broken"] = guard->guard_break_remaining > 0.0F;
  }

  result["locked_target_name"] = QString();
  result["locked_target_hp"] = 0;
  result["locked_target_max_hp"] = 0;
  result["locked_target_hp_ratio"] = 0.0;

  Engine::Core::EntityID locked_id = m_commander_control.locked_target_id();
  if (auto* rpg_targets =
          commander_entity != nullptr
              ? commander_entity
                    ->get_component<Engine::Core::RpgCommanderTargetComponent>()
              : nullptr) {
    locked_id = rpg_targets->explicit_lock_target_id;
  }
  if (locked_id != 0 && m_world != nullptr) {
    auto* locked_ent = m_world->get_entity(locked_id);
    if (locked_ent != nullptr) {
      auto* locked_unit = locked_ent->get_component<Engine::Core::UnitComponent>();
      auto* locked_cmd = locked_ent->get_component<Engine::Core::CommanderComponent>();
      if (locked_unit != nullptr && locked_unit->health > 0) {
        QString lname;
        int lhp = 0;
        int lmax = 0;
        bool lb = false;
        bool la = false;
        QString lnation;
        if (get_unit_info(locked_id, lname, lhp, lmax, lb, la, lnation)) {
          if (locked_cmd != nullptr && !locked_cmd->display_name.empty()) {
            lname = QString::fromStdString(locked_cmd->display_name);
          }
          result["locked_target_name"] = lname;
          result["locked_target_hp"] = lhp;
          result["locked_target_max_hp"] = lmax;
          result["locked_target_hp_ratio"] =
              lmax > 0 ? static_cast<double>(lhp) / static_cast<double>(lmax) : 0.0;
        }
      }
    }
  }

  return result;
}

auto GameEngine::pop_rpg_damage_events() -> QVariantList {
  QVariantList list;
  list.reserve(static_cast<int>(m_rpg_damage_events.size()));
  for (const auto& ev : m_rpg_damage_events) {
    QVariantMap m;
    m["x"] = static_cast<double>(ev.wx);
    m["y"] = static_cast<double>(ev.wy);
    m["z"] = static_cast<double>(ev.wz);
    m["damage"] = ev.damage;
    m["damageRatio"] = static_cast<double>(ev.damage_ratio);
    m["lane"] = ev.lane;
    m["killingBlow"] = ev.killing_blow;
    list.append(m);
  }
  m_rpg_damage_events.clear();
  return list;
}

auto GameEngine::rpg_project_world(float x, float y, float z) const -> QVariantMap {
  QVariantMap result;
  result["valid"] = false;
  result["x"] = 0.0;
  result["y"] = 0.0;
  QPointF screen;
  if (world_to_screen(QVector3D(x, y, z), screen)) {
    result["valid"] = true;
    result["x"] = screen.x();
    result["y"] = screen.y();
  }
  return result;
}

void GameEngine::start_builder_construction(const QString& item_type) {
  if (m_production_manager) {
    m_production_manager->start_builder_construction(item_type);
  }
}

auto GameEngine::get_selected_units_command_mode() const -> QString {
  return m_selection_query_service
             ? m_selection_query_service->get_selected_units_command_mode()
             : "normal";
}

auto GameEngine::get_selected_units_toggle_state(const QString& mode) const -> QString {
  return m_selection_query_service
             ? m_selection_query_service->get_selected_units_toggle_state(mode)
             : "none";
}

auto GameEngine::get_selected_units_mode_availability() const -> QVariantMap {
  return m_selection_query_service
             ? m_selection_query_service->get_selected_units_mode_availability()
             : QVariantMap();
}

void GameEngine::set_rally_at_screen(qreal sx, qreal sy) {
  ensure_initialized();
  if (m_production_manager) {
    m_production_manager->set_rally_at_screen(
        sx, sy, m_runtime.local_owner_id, m_viewport);
  }
}

void GameEngine::start_loading_maps() {
  m_available_maps.clear();
  if (m_map_catalog) {
    m_map_catalog->load_maps_async();
  }
  load_campaigns();
}

auto GameEngine::available_maps() const -> QVariantList {
  return m_available_maps;
}

auto GameEngine::available_nations() const -> QVariantList {
  QVariantList nations;
  const auto& registry = Game::Systems::NationRegistry::instance();
  const auto& all = registry.get_all_nations();
  QList<QVariantMap> ordered;
  ordered.reserve(static_cast<int>(all.size()));
  for (const auto& nation : all) {
    QVariantMap entry;
    entry.insert(QStringLiteral("id"),
                 QString::fromStdString(Game::Systems::nation_id_to_string(nation.id)));
    entry.insert(QStringLiteral("name"), QString::fromStdString(nation.display_name));
    ordered.append(entry);
  }
  std::sort(
      ordered.begin(), ordered.end(), [](const QVariantMap& a, const QVariantMap& b) {
        return a.value(QStringLiteral("name"))
                   .toString()
                   .localeAwareCompare(b.value(QStringLiteral("name")).toString()) < 0;
      });
  for (const auto& entry : ordered) {
    nations.append(entry);
  }
  return nations;
}

auto GameEngine::available_commanders(const QString& nation_id) const -> QVariantList {
  QVariantList commanders;
  const auto parsed_nation =
      Game::Systems::nation_id_from_string(nation_id.toStdString());
  const auto nation = parsed_nation.value_or(
      Game::Systems::NationRegistry::instance().default_nation_id());
  const QString default_troop =
      App::Core::resolve_commander_troop(nation_id, std::nullopt);
  auto definitions = Game::Units::commander_definitions_for_nation(nation);
  std::stable_sort(
      definitions.begin(),
      definitions.end(),
      [&default_troop](const Game::Units::CommanderDefinition* lhs,
                       const Game::Units::CommanderDefinition* rhs) {
        const bool lhs_default =
            lhs != nullptr &&
            QString::fromStdString(Game::Units::troop_typeToString(lhs->troop_type)) ==
                default_troop;
        const bool rhs_default =
            rhs != nullptr &&
            QString::fromStdString(Game::Units::troop_typeToString(rhs->troop_type)) ==
                default_troop;
        return lhs_default && !rhs_default;
      });
  for (const auto* definition : definitions) {
    if (definition == nullptr) {
      continue;
    }
    const QString troop =
        QString::fromStdString(Game::Units::troop_typeToString(definition->troop_type));
    commanders.append(build_available_commander_entry(
        *definition, troop.compare(default_troop, Qt::CaseInsensitive) == 0));
  }
  return commanders;
}

auto GameEngine::available_campaigns() const -> QVariantList {
  return m_campaign_manager ? m_campaign_manager->available_campaigns()
                            : QVariantList();
}

void GameEngine::load_campaigns() {
  if (!m_save_load_service) {
    return;
  }

  QString error;
  auto campaigns = m_save_load_service->list_campaigns(&error);
  if (!error.isEmpty()) {
    qWarning() << "Failed to load campaigns:" << error;
    return;
  }

  if (m_campaign_manager) {
    m_campaign_manager->set_available_campaigns(campaigns);
  }
}

void GameEngine::stop_mission_ambience() {
  if (!m_current_ambient_sound_id.isEmpty()) {
    AudioSystem::get_instance().stop_sound(m_current_ambient_sound_id.toStdString());
    m_current_ambient_sound_id.clear();
  }
}

void GameEngine::apply_mission_ambience(const Game::Mission::MissionDefinition* mission,
                                        const QString& map_path) {
  stop_mission_ambience();

  QString ambience_id;
  if (mission != nullptr && mission->terrain_type.has_value()) {
    ambience_id = mission_terrain_to_ambient_sound(*mission->terrain_type);
  } else if (!map_path.isEmpty()) {
    Game::Map::MapDefinition map_def;
    QString map_error;
    const QString resolved_map_path = Utils::Resources::resolve_resource_path(map_path);
    if (Game::Map::MapLoader::load_from_json_file(
            resolved_map_path, map_def, &map_error)) {
      ambience_id = biome_to_ambient_sound(map_def.biome.ground_type);
    }
  }

  if (ambience_id.isEmpty()) {
    ambience_id = QStringLiteral("ambient.battlefield_dry_wind_distant_march_01");
  }

  AudioSystem::get_instance().play_sound(
      ambience_id.toStdString(), 1.0F, true, 1, AudioCategory::AMBIENCE);
  m_current_ambient_sound_id = ambience_id;
}

void GameEngine::start_campaign_mission(const QString& mission_path) {
  clear_error();

  if (!m_campaign_manager) {
    set_error("Campaign manager not initialized");
    return;
  }

  m_selected_player_id = 1;

  m_campaign_manager->start_campaign_mission(mission_path, m_selected_player_id);

  if (!m_campaign_manager->current_mission_definition().has_value()) {
    set_error("Failed to load mission");
    return;
  }

  const auto& mission = *m_campaign_manager->current_mission_definition();
  AudioResourceLoader::load_audio_resources(AudioLoadPolicy::Mission);

  QVariantList player_configs;

  QVariantMap player_one;
  player_one.insert("player_id", 1);
  player_one.insert("playerName", mission.player_setup.nation);
  player_one.insert("colorIndex", 0);
  player_one.insert("team_id", 0);
  player_one.insert("nationId", mission.player_setup.nation);
  player_one.insert(
      "commanderTroop",
      App::Core::resolve_commander_troop(mission.player_setup.nation,
                                         mission.player_setup.commander_troop));
  player_one.insert("isHuman", true);
  player_configs.append(player_one);

  int player_id = 2;
  int default_team_id = 1;
  for (const auto& ai_setup : mission.ai_setups) {
    QVariantMap ai_player;
    ai_player.insert("player_id", player_id);
    ai_player.insert("playerName", ai_setup.nation);
    ai_player.insert("colorIndex", player_id - 1);

    int team_id = 0;
    if (ai_setup.team_id.has_value()) {
      team_id = ai_setup.team_id.value();
    } else {
      team_id = default_team_id++;
    }

    ai_player.insert("team_id", team_id);
    ai_player.insert("nationId", ai_setup.nation);
    ai_player.insert(
        "commanderTroop",
        App::Core::resolve_commander_troop(ai_setup.nation, ai_setup.commander_troop));
    ai_player.insert("isHuman", false);
    player_configs.append(ai_player);
    player_id++;
  }

  start_skirmish_internal(mission.map_path, player_configs, false);
}

void GameEngine::mark_current_mission_completed() {
  if (!m_campaign_manager) {
    return;
  }

  const QString campaign_id = m_campaign_manager->current_campaign_id();
  if (campaign_id.isEmpty()) {
    qWarning() << "No active campaign mission to mark as completed";
    return;
  }

  if (!m_save_load_service) {
    qWarning() << "Save/Load service not initialized";
    return;
  }

  QString error;
  bool const success =
      m_save_load_service->mark_campaign_completed(campaign_id, &error);
  if (!success) {
    qWarning() << "Failed to mark campaign as completed:" << error;
  } else {
    m_campaign_manager->mark_current_mission_completed();
    load_campaigns();
  }
}

QVariantMap GameEngine::get_current_mission_objectives() const {
  QVariantMap result;

  if (!m_campaign_manager) {
    return result;
  }

  const auto& mission_def = m_campaign_manager->current_mission_definition();
  if (!mission_def.has_value()) {
    return result;
  }

  const auto& mission = *mission_def;

  result["title"] = mission.title;
  result["summary"] = mission.summary;

  result["victory_conditions"] = build_condition_list(mission.victory_conditions);
  result["defeat_conditions"] = build_condition_list(mission.defeat_conditions);
  result["optional_objectives"] = build_condition_list(mission.optional_objectives);

  return result;
}

QVariantMap GameEngine::get_mission_definition(const QString& mission_id) const {
  QVariantMap result;
  if (mission_id.isEmpty()) {
    return result;
  }

  const QString mission_path = resolve_mission_file_path(mission_id);
  if (mission_path.isEmpty()) {
    qWarning() << "Mission definition not found for" << mission_id;
    return result;
  }

  Game::Mission::MissionDefinition mission;
  QString error;
  if (!Game::Mission::MissionLoader::load_from_json_file(
          mission_path, mission, &error)) {
    qWarning() << "Failed to load mission definition" << mission_id << error;
    return result;
  }

  return build_mission_definition_map(mission);
}

void GameEngine::start_skirmish(const QString& map_path,
                                const QVariantList& player_configs) {
  start_skirmish_internal(map_path, player_configs, true);
}

void GameEngine::start_skirmish_internal(const QString& map_path,
                                         const QVariantList& player_configs,
                                         bool set_skirmish_context) {

  clear_error();
  reset_preload_interaction_state();
  reset_mission_runtime_state();

  m_level.map_path = map_path;
  m_level.map_name = map_path;

  if (m_campaign_manager && set_skirmish_context) {
    m_campaign_manager->set_skirmish_context(map_path);
  }

  AudioResourceLoader::load_audio_resources(AudioLoadPolicy::Mission);

  if (!m_runtime.victory_state.isEmpty()) {
    m_runtime.victory_state = "";
    emit victory_state_changed();
  }
  if (m_victory_service) {
    m_victory_service->reset();
  }
  m_enemy_troops_defeated = 0;

  if (!m_runtime.initialized) {
    ensure_initialized();
  }

  if (!m_world || !m_renderer || (m_camera == nullptr)) {
    set_error("Cannot start skirmish: renderer not initialized");
    return;
  }

  m_finalize_progress_after_overlay = false;
  m_loading_overlay_active = true;
  m_runtime.loading = true;
  emit is_loading_changed();

  if (m_loading_progress_tracker) {
    m_loading_progress_tracker->start_loading();
  }

  QCoreApplication::processEvents(QEventLoop::AllEvents);
  QTimer::singleShot(50, this, [this, map_path, player_configs]() {
    perform_skirmish_load(map_path, player_configs);
  });
}

void GameEngine::perform_skirmish_load(const QString& map_path,
                                       const QVariantList& player_configs) {

  if (!m_world || !m_renderer || (m_camera == nullptr)) {
    set_error("Cannot start skirmish: renderer not initialized");
    m_runtime.loading = false;
    emit is_loading_changed();
    return;
  }

  if (m_hover_tracker) {
    m_hover_tracker->update_hover(-1, -1, *m_world, *m_camera, 0, 0);
  }

  LevelOrchestrator orchestrator;
  const AppSceneContext scene = scene_context();

  auto owner_update = [this]() {
    emit owner_info_changed();
  };

  const bool allow_default_player_barracks =
      !m_campaign_manager ||
      !m_campaign_manager->current_mission_context().is_campaign();

  auto load_result = orchestrator.load_skirmish(map_path,
                                                player_configs,
                                                m_selected_player_id,
                                                *m_world,
                                                scene,
                                                m_level,
                                                m_entity_cache,
                                                m_victory_service.get(),
                                                m_minimap_manager.get(),
                                                m_visibility_coordinator.get(),
                                                owner_update,
                                                allow_default_player_barracks,
                                                m_loading_progress_tracker.get());

  if (load_result.updated_player_id != m_selected_player_id) {
    m_selected_player_id = load_result.updated_player_id;
    emit selected_player_id_changed();
  }

  if (!load_result.success) {
    set_error(load_result.error_message);
    m_runtime.loading = false;
    m_loading_overlay_active = false;
    m_loading_overlay_wait_for_first_frame.store(false, std::memory_order_release);
    m_finalize_progress_after_overlay = false;
    m_show_objectives_after_loading = false;
    emit is_loading_changed();
    return;
  }

  m_runtime.local_owner_id = load_result.updated_player_id;
  configure_audio_manifest_mappings();
  const Game::Mission::MissionDefinition* mission_def = nullptr;
  if (m_campaign_manager &&
      m_campaign_manager->current_mission_definition().has_value()) {
    mission_def = &*m_campaign_manager->current_mission_definition();
  }
  apply_mission_ambience(mission_def, map_path);

  apply_skirmish_commander_setup(player_configs);
  apply_mission_setup();
  initialize_player_resources();
  AudioResourceLoader::load_audio_resources(AudioLoadPolicy::Mission);
  configure_audio_manifest_mappings();
  configure_mission_victory_conditions();
  configure_rain_system();
  finalize_skirmish_load();
}

void GameEngine::apply_mission_setup() {
  if (!m_world || !m_campaign_manager) {
    return;
  }

  if (!m_campaign_manager->current_mission_context().is_campaign()) {
    return;
  }

  if (!m_campaign_manager->current_mission_definition().has_value()) {
    return;
  }

  auto reg = Game::Map::MapTransformer::get_factory_registry();
  if (!reg) {
    qWarning() << "Mission setup skipped: unit factory registry missing";
    return;
  }

  const auto& mission = *m_campaign_manager->current_mission_definition();
  auto& owner_registry = Game::Systems::OwnerRegistry::instance();
  auto& nation_registry = Game::Systems::NationRegistry::instance();

  Game::Map::MapDefinition map_def;
  QString map_error;
  const QString resolved_map_path =
      Utils::Resources::resolve_resource_path(m_level.map_path);
  bool map_loaded =
      Game::Map::MapLoader::load_from_json_file(resolved_map_path, map_def, &map_error);
  if (!map_loaded) {
    qWarning() << "Mission setup: failed to load map definition for" << m_level.map_path
               << "resolved to" << resolved_map_path << "-" << map_error;
  }

  bool has_map_spawns = true;
  if (map_loaded) {
    has_map_spawns = !map_def.spawns.empty();
  }

  bool has_mission_spawns = !mission.player_setup.starting_units.empty() ||
                            !mission.player_setup.starting_buildings.empty();
  for (const auto& ai_setup : mission.ai_setups) {
    if (!ai_setup.starting_units.empty() || !ai_setup.starting_buildings.empty()) {
      has_mission_spawns = true;
      break;
    }
  }

  if (has_mission_spawns && !has_map_spawns) {
    std::vector<Engine::Core::EntityID> to_remove;
    auto entities = m_world->get_entities_with<Engine::Core::UnitComponent>();
    to_remove.reserve(entities.size());
    for (auto* entity : entities) {
      if (entity != nullptr) {
        to_remove.push_back(entity->get_id());
      }
    }
    for (const auto id : to_remove) {
      m_world->destroy_entity(id);
    }
  }

  auto resolve_nation_id = [&nation_registry](const QString& nation_str) {
    const auto parsed = Game::Systems::nation_id_from_string(nation_str.toStdString());
    if (parsed.has_value()) {
      return parsed.value();
    }
    return nation_registry.default_nation_id();
  };

  auto mission_position_to_world = [&](const Game::Mission::Position& pos) {
    float world_x = pos.x;
    float world_z = pos.z;
    if (map_loaded && map_def.coordSystem == Game::Map::CoordSystem::Grid) {
      const float tile = std::max(0.0001F, map_def.grid.tile_size);
      world_x = (pos.x - (map_def.grid.width * 0.5F - 0.5F)) * tile;
      world_z = (pos.z - (map_def.grid.height * 0.5F - 0.5F)) * tile;
    } else if (!map_loaded) {
      const float tile = std::max(0.0001F, m_level.tile_size);
      world_x = (pos.x - (m_level.grid_width * 0.5F - 0.5F)) * tile;
      world_z = (pos.z - (m_level.grid_height * 0.5F - 0.5F)) * tile;
    }
    return QVector3D(world_x, 0.0F, world_z);
  };

  auto apply_team_color = [&](Engine::Core::Entity* entity, int owner_id) {
    if (entity == nullptr) {
      return;
    }
    auto* renderable = entity->get_component<Engine::Core::RenderableComponent>();
    if (renderable == nullptr) {
      return;
    }
    const QVector3D team_color = Game::Visuals::team_colorForOwner(owner_id);
    renderable->color[0] = team_color.x();
    renderable->color[1] = team_color.y();
    renderable->color[2] = team_color.z();
  };

  auto parse_color = [](const QString& color_name, std::array<float, 3>& out) -> bool {
    if (color_name.isEmpty()) {
      return false;
    }

    const QString trimmed = color_name.trimmed();
    if (trimmed.startsWith('#') && trimmed.length() == 7) {
      bool ok = false;
      int const r = trimmed.mid(1, 2).toInt(&ok, 16);
      if (!ok) {
        return false;
      }
      int const g = trimmed.mid(3, 2).toInt(&ok, 16);
      if (!ok) {
        return false;
      }
      int const b = trimmed.mid(5, 2).toInt(&ok, 16);
      if (!ok) {
        return false;
      }
      constexpr float scale = 255.0F;
      out = {r / scale, g / scale, b / scale};
      return true;
    }

    const QString lowered = trimmed.toLower();
    if (lowered == "red") {
      out = {1.00F, 0.30F, 0.30F};
      return true;
    }
    if (lowered == "brown") {
      out = {0.55F, 0.36F, 0.18F};
      return true;
    }
    if (lowered == "blue") {
      out = {0.20F, 0.55F, 1.00F};
      return true;
    }
    if (lowered == "green") {
      out = {0.20F, 0.80F, 0.40F};
      return true;
    }
    if (lowered == "yellow") {
      out = {1.00F, 0.80F, 0.20F};
      return true;
    }
    if (lowered == "orange") {
      out = {0.95F, 0.55F, 0.10F};
      return true;
    }
    if (lowered == "white") {
      out = {0.95F, 0.95F, 0.95F};
      return true;
    }
    if (lowered == "black") {
      out = {0.15F, 0.15F, 0.15F};
      return true;
    }

    return false;
  };

  auto apply_owner_color = [&](int owner_id, const QString& color_name) {
    std::array<float, 3> color{};
    if (!parse_color(color_name, color)) {
      return;
    }
    owner_registry.set_owner_color(owner_id, color[0], color[1], color[2]);
  };

  auto spawn_units_for_owner = [&](int owner_id,
                                   const Game::Systems::NationID nation_id,
                                   const std::vector<Game::Mission::UnitSetup>& units) {
    const bool ai_controlled = owner_registry.is_ai(owner_id);
    for (const auto& unit_setup : units) {
      const auto spawn_type =
          Game::Units::spawn_typeFromString(unit_setup.type.toStdString());
      if (!spawn_type.has_value()) {
        qWarning() << "Mission setup: unknown unit type" << unit_setup.type;
        continue;
      }

      const int count = std::max(1, unit_setup.count);
      const int grid =
          static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count))));
      const QVector3D base_pos = mission_position_to_world(unit_setup.position);
      float base_tile_size = m_level.tile_size;
      if (map_loaded && map_def.coordSystem == Game::Map::CoordSystem::Grid) {
        base_tile_size = map_def.grid.tile_size;
      }
      const float spacing = std::max(0.5F, base_tile_size * 1.2F);

      for (int i = 0; i < count; ++i) {
        const int row = i / grid;
        const int col = i % grid;
        const float offset_x = (float(col) - (grid - 1) * 0.5F) * spacing;
        const float offset_z = (float(row) - (grid - 1) * 0.5F) * spacing;
        const QVector3D pos =
            QVector3D(base_pos.x() + offset_x, base_pos.y(), base_pos.z() + offset_z);

        Game::Units::SpawnParams sp;
        sp.position = pos;
        sp.player_id = owner_id;
        sp.spawn_type = spawn_type.value();
        sp.ai_controlled = ai_controlled;
        sp.nation_id = nation_id;

        auto unit = reg->create(sp.spawn_type, *m_world, sp);
        if (!unit) {
          qWarning() << "Mission setup: failed to spawn unit" << unit_setup.type
                     << "for owner" << owner_id;
          continue;
        }
        apply_team_color(m_world->get_entity(unit->id()), owner_id);
      }
    }
  };

  auto spawn_commander_for_owner =
      [&](int owner_id,
          const Game::Systems::NationID nation_id,
          const QString& commander_troop,
          const App::Core::ResolvedCommanderPosition& position) {
        if (commander_troop.trimmed().isEmpty()) {
          return;
        }
        const auto spawn_type =
            Game::Units::spawn_typeFromString(commander_troop.trimmed().toStdString());
        if (!spawn_type.has_value()) {
          qWarning() << "Mission setup: unknown commander troop" << commander_troop;
          return;
        }
        const auto troop_type = Game::Units::spawn_typeToTroopType(*spawn_type);
        if (!troop_type.has_value() || !Game::Units::is_commander_troop(*troop_type)) {
          qWarning() << "Mission setup: non-commander troop configured as commander"
                     << commander_troop;
          return;
        }
        if (m_world == nullptr) {
          return;
        }
        for (auto* entity :
             m_world->get_entities_with<Engine::Core::CommanderComponent>()) {
          if (entity == nullptr) {
            continue;
          }
          const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
          if (unit != nullptr && unit->owner_id == owner_id && unit->health > 0) {
            return;
          }
        }
        Game::Units::SpawnParams sp;
        if (position.space == App::Core::CommanderPositionSpace::World) {
          sp.position = QVector3D(position.position.x, 0.0F, position.position.z);
        } else {
          sp.position = mission_position_to_world(position.position);
        }
        sp.player_id = owner_id;
        sp.spawn_type = *spawn_type;
        sp.ai_controlled = owner_registry.is_ai(owner_id);
        sp.nation_id = nation_id;
        auto unit = reg->create(sp.spawn_type, *m_world, sp);
        if (!unit) {
          qWarning() << "Mission setup: failed to spawn commander" << commander_troop
                     << "for owner" << owner_id;
          return;
        }
        apply_team_color(m_world->get_entity(unit->id()), owner_id);
      };

  auto existing_owner_spawn_anchors = [&](int owner_id) {
    std::vector<App::Core::ExistingOwnerSpawnAnchor> anchors;
    if (m_world == nullptr) {
      return anchors;
    }

    auto entities = m_world->get_entities_with<Engine::Core::UnitComponent>();
    anchors.reserve(entities.size());
    for (auto* entity : entities) {
      if (entity == nullptr) {
        continue;
      }

      const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
      const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
      if (unit == nullptr || transform == nullptr || unit->owner_id != owner_id ||
          unit->health <= 0) {
        continue;
      }

      App::Core::ExistingOwnerSpawnAnchor anchor;
      anchor.position = {transform->position.x, transform->position.z};
      anchor.is_building = Game::Units::is_building_spawn(unit->spawn_type);
      anchors.push_back(anchor);
    }
    return anchors;
  };

  auto spawn_buildings_for_owner =
      [&](int owner_id,
          const Game::Systems::NationID nation_id,
          const std::vector<Game::Mission::BuildingSetup>& buildings) {
        const bool ai_controlled = owner_registry.is_ai(owner_id);
        for (const auto& building_setup : buildings) {
          const auto spawn_type =
              Game::Units::spawn_typeFromString(building_setup.type.toStdString());
          if (!spawn_type.has_value()) {
            qWarning() << "Mission setup: unknown building type" << building_setup.type;
            continue;
          }

          const QVector3D pos = mission_position_to_world(building_setup.position);

          Game::Units::SpawnParams sp;
          sp.position = pos;
          sp.player_id = owner_id;
          sp.spawn_type = spawn_type.value();
          sp.ai_controlled = ai_controlled;
          sp.nation_id = nation_id;
          sp.max_population = building_setup.max_population;

          auto unit = reg->create(sp.spawn_type, *m_world, sp);
          if (!unit) {
            qWarning() << "Mission setup: failed to spawn building"
                       << building_setup.type << "for owner" << owner_id;
            continue;
          }
          apply_team_color(m_world->get_entity(unit->id()), owner_id);
        }
      };

  const int local_owner_id = m_runtime.local_owner_id;
  if (owner_registry.get_owner_type(local_owner_id) ==
      Game::Systems::OwnerType::Neutral) {
    owner_registry.register_owner_with_id(local_owner_id,
                                          Game::Systems::OwnerType::Player,
                                          "Player " + std::to_string(local_owner_id));
  }
  owner_registry.set_owner_team(local_owner_id, 0);

  const auto player_nation_id = resolve_nation_id(mission.player_setup.nation);
  nation_registry.set_player_nation(local_owner_id, player_nation_id);
  apply_owner_color(local_owner_id, mission.player_setup.color);

  const QString player_commander_troop = App::Core::resolve_commander_troop(
      mission.player_setup.nation, mission.player_setup.commander_troop);
  const auto player_commander_pos = App::Core::resolve_commander_position(
      mission.player_setup.starting_units,
      mission.player_setup.starting_buildings,
      existing_owner_spawn_anchors(local_owner_id),
      {68.0F, 70.0F});
  spawn_commander_for_owner(
      local_owner_id, player_nation_id, player_commander_troop, player_commander_pos);
  spawn_units_for_owner(
      local_owner_id, player_nation_id, mission.player_setup.starting_units);
  spawn_buildings_for_owner(
      local_owner_id, player_nation_id, mission.player_setup.starting_buildings);

  int ai_owner_id = 2;
  int default_team_id = 1;
  for (const auto& ai_setup : mission.ai_setups) {
    if (owner_registry.get_owner_type(ai_owner_id) ==
        Game::Systems::OwnerType::Neutral) {
      owner_registry.register_owner_with_id(ai_owner_id,
                                            Game::Systems::OwnerType::AI,
                                            "AI Player " + std::to_string(ai_owner_id));
    }

    int team_id = 0;
    if (ai_setup.team_id.has_value()) {
      team_id = ai_setup.team_id.value();
    } else {
      team_id = default_team_id++;
    }

    owner_registry.set_owner_team(ai_owner_id, team_id);

    const auto ai_nation_id = resolve_nation_id(ai_setup.nation);
    nation_registry.set_player_nation(ai_owner_id, ai_nation_id);
    apply_owner_color(ai_owner_id, ai_setup.color);

    const QString ai_commander_troop =
        App::Core::resolve_commander_troop(ai_setup.nation, ai_setup.commander_troop);
    const auto ai_commander_pos =
        App::Core::resolve_commander_position(ai_setup.starting_units,
                                              ai_setup.starting_buildings,
                                              existing_owner_spawn_anchors(ai_owner_id),
                                              {132.0F, 80.0F});
    spawn_commander_for_owner(
        ai_owner_id, ai_nation_id, ai_commander_troop, ai_commander_pos);
    spawn_units_for_owner(ai_owner_id, ai_nation_id, ai_setup.starting_units);
    spawn_buildings_for_owner(ai_owner_id, ai_nation_id, ai_setup.starting_buildings);

    for (const auto& wave : ai_setup.waves) {
      PendingMissionWave pending_wave;
      pending_wave.owner_id = ai_owner_id;
      pending_wave.nation_id = ai_nation_id;
      pending_wave.ai_id = ai_setup.id;
      pending_wave.trigger_time = std::max(0.0F, wave.timing);
      pending_wave.entry_world_position = mission_position_to_world(wave.entry_point);
      pending_wave.composition = wave.composition;
      m_pending_mission_waves.push_back(std::move(pending_wave));
    }

    ai_owner_id++;
  }

  auto entities = m_world->get_entities_with<Engine::Core::UnitComponent>();
  for (auto* entity : entities) {
    if (entity == nullptr) {
      continue;
    }
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      continue;
    }
    apply_team_color(entity, unit->owner_id);
  }

  if (auto* ai_system = m_world->get_system<Game::Systems::AISystem>()) {
    ai_system->reinitialize();

    int ai_id = 2;
    for (const auto& ai_setup : mission.ai_setups) {
      Game::Systems::AI::AIStrategy strategy = Game::Systems::AI::AIStrategy::Balanced;

      if (ai_setup.strategy.has_value()) {
        strategy = Game::Systems::AI::AIStrategyFactory::parse_strategy(
            ai_setup.strategy.value());
      }

      ai_system->set_ai_strategy(ai_id,
                                 strategy,
                                 ai_setup.personality.aggression,
                                 ai_setup.personality.defense,
                                 ai_setup.personality.harassment,
                                 ai_setup.difficulty);
      ai_id++;
    }
  }

  int const prev_selected_player = m_selected_player_id;
  GameStateRestorer::rebuild_registries_after_load(
      m_world.get(), m_selected_player_id, m_level, m_runtime.local_owner_id);
  GameStateRestorer::rebuild_entity_cache(
      m_world.get(), m_entity_cache, m_runtime.local_owner_id);

  if (m_selected_player_id != prev_selected_player) {
    emit selected_player_id_changed();
  }

  center_camera_on_local_forces();
  emit troop_count_changed();
  emit owner_info_changed();
}

void GameEngine::configure_mission_victory_conditions() {
  if (!m_campaign_manager || !m_victory_service) {
    return;
  }

  m_campaign_manager->configure_mission_victory_conditions(m_victory_service.get(),
                                                           m_runtime.local_owner_id);

  m_victory_service->set_victory_callback([this](const QString& state) {
    if (m_runtime.victory_state != state) {
      ensure_result_audio_ready(state);
      m_runtime.victory_state = state;
      emit victory_state_changed();

      if (state == "victory" && !m_campaign_manager->current_campaign_id().isEmpty()) {
        mark_current_mission_completed();
      }
    }
  });
}

void GameEngine::configure_rain_system() {
  if (m_rain_manager) {
    m_rain_manager->configure(m_level.rain, m_level.biome_seed);
  }

  if (!m_rain) {
    return;
  }

  const float world_width = static_cast<float>(m_level.grid_width) * m_level.tile_size;
  const float world_height =
      static_cast<float>(m_level.grid_height) * m_level.tile_size;
  m_rain->configure(world_width, world_height, m_level.biome_seed, m_level.rain.type);
  m_rain->set_enabled(m_level.rain.enabled);
  m_rain->set_wind_strength(m_level.rain.wind_strength);

  const float initial_intensity =
      m_rain_manager ? m_rain_manager->get_intensity()
                     : (m_level.rain.enabled ? m_level.rain.intensity : 0.0F);
  m_rain->set_intensity(initial_intensity);
}

void GameEngine::reset_preload_interaction_state() {
  m_saved_rts_selection_ids.clear();
  if (m_command_controller) {
    m_command_controller->reset_transient_state();
  }

  if (m_production_manager) {
    m_production_manager->reset_transient_state();
  }

  if (m_world) {
    if (auto* selection_system =
            m_world->get_system<Game::Systems::SelectionSystem>()) {
      selection_system->clear_selection();
    }
  }

  if (m_renderer) {
    m_renderer->set_selected_entities({});
    m_renderer->set_hovered_entity_id(0);
  }

  if (m_hover_tracker && m_world && (m_camera != nullptr)) {
    m_hover_tracker->update_hover(-1, -1, *m_world, *m_camera, 0, 0);
  }

  if (m_cursor_manager && m_cursor_manager->mode() != CursorMode::Normal) {
    set_cursor_mode(CursorMode::Normal);
  }

  m_follow_selection_enabled = false;
  m_runtime.selection_refresh_counter = 0;

  emit selected_units_changed();
}

void GameEngine::reset_mission_runtime_state() {
  m_campaign_mission_elapsed = 0.0F;
  m_pending_mission_waves.clear();
  Game::Systems::PlayerResourceRegistry::instance().clear();
  sync_selected_player_state();
  stop_mission_ambience();
  AudioSystem::get_instance().stop_music();
  AudioResourceLoader::unload_audio_resources(AudioLoadPolicy::Mission);
  AudioResourceLoader::unload_audio_resources(AudioLoadPolicy::Lazy);
}

void GameEngine::update_mission_waves(float dt) {
  if (dt <= 0.0F || !m_world || m_pending_mission_waves.empty() ||
      !m_runtime.victory_state.isEmpty()) {
    return;
  }
  if (!m_campaign_manager ||
      !m_campaign_manager->current_mission_context().is_campaign()) {
    return;
  }

  m_campaign_mission_elapsed += dt;

  bool spawned_any = false;
  for (auto& wave : m_pending_mission_waves) {
    if (wave.spawned || m_campaign_mission_elapsed < wave.trigger_time) {
      continue;
    }
    spawn_mission_wave(wave);
    wave.spawned = true;
    spawned_any = true;
  }

  if (spawned_any) {
    emit owner_info_changed();
  }
}

void GameEngine::apply_skirmish_commander_setup(const QVariantList& player_configs) {
  if (!m_world || player_configs.isEmpty()) {
    return;
  }
  if (m_campaign_manager &&
      m_campaign_manager->current_mission_context().is_campaign()) {
    return;
  }

  auto reg = Game::Map::MapTransformer::get_factory_registry();
  if (!reg) {
    qWarning() << "Skirmish commander setup skipped: unit factory registry missing";
    return;
  }

  auto& owner_registry = Game::Systems::OwnerRegistry::instance();
  auto& nation_registry = Game::Systems::NationRegistry::instance();

  Game::Map::MapDefinition map_def;
  QString map_error;
  const QString resolved_map_path =
      Utils::Resources::resolve_resource_path(m_level.map_path);
  const bool map_loaded =
      Game::Map::MapLoader::load_from_json_file(resolved_map_path, map_def, &map_error);
  if (!map_loaded) {
    qWarning() << "Skirmish commander setup: failed to load map definition for"
               << m_level.map_path << "-" << map_error;
  }

  auto apply_team_color = [&](Engine::Core::Entity* entity, int owner_id) {
    if (entity == nullptr) {
      return;
    }
    auto* renderable = entity->get_component<Engine::Core::RenderableComponent>();
    if (renderable == nullptr) {
      return;
    }
    const QVector3D team_color = Game::Visuals::team_colorForOwner(owner_id);
    renderable->color[0] = team_color.x();
    renderable->color[1] = team_color.y();
    renderable->color[2] = team_color.z();
  };

  auto existing_owner_spawn_anchors = [&](int owner_id) {
    std::vector<App::Core::ExistingOwnerSpawnAnchor> anchors;
    auto entities = m_world->get_entities_with<Engine::Core::UnitComponent>();
    anchors.reserve(entities.size());
    for (auto* entity : entities) {
      if (entity == nullptr) {
        continue;
      }
      const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
      const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
      if (unit == nullptr || transform == nullptr || unit->owner_id != owner_id ||
          unit->health <= 0) {
        continue;
      }
      anchors.push_back(
          {.position = {transform->position.x, transform->position.z},
           .is_building = Game::Units::is_building_spawn(unit->spawn_type)});
    }
    return anchors;
  };

  auto map_spawn_fallback =
      [&](int owner_id) -> std::optional<Game::Mission::Position> {
    if (!map_loaded) {
      return std::nullopt;
    }
    for (const auto& spawn : map_def.spawns) {
      if (spawn.player_id != owner_id) {
        continue;
      }
      float world_x = spawn.x;
      float world_z = spawn.z;
      if (map_def.coordSystem == Game::Map::CoordSystem::Grid) {
        const float tile = std::max(0.0001F, map_def.grid.tile_size);
        world_x = (spawn.x - (map_def.grid.width * 0.5F - 0.5F)) * tile;
        world_z = (spawn.z - (map_def.grid.height * 0.5F - 0.5F)) * tile;
      }
      return Game::Mission::Position{world_x, world_z};
    }
    return std::nullopt;
  };

  auto owner_has_living_commander = [&](int owner_id) {
    for (auto* entity :
         m_world->get_entities_with<Engine::Core::CommanderComponent>()) {
      if (entity == nullptr) {
        continue;
      }
      const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
      if (unit != nullptr && unit->owner_id == owner_id && unit->health > 0) {
        return true;
      }
    }
    return false;
  };

  QSet<int> processed_owner_ids;
  for (const QVariant& config_var : player_configs) {
    const QVariantMap config = config_var.toMap();
    if (config.isEmpty()) {
      continue;
    }

    int owner_id = config.value("player_id", -1).toInt();
    if (config.value("isHuman", false).toBool()) {
      owner_id = m_runtime.local_owner_id;
    }
    if (owner_id < 0 || processed_owner_ids.contains(owner_id) ||
        owner_has_living_commander(owner_id)) {
      continue;
    }
    processed_owner_ids.insert(owner_id);

    const auto* assigned_nation = nation_registry.get_nation_for_player(owner_id);
    const auto nation_id = assigned_nation != nullptr
                               ? assigned_nation->id
                               : nation_registry.default_nation_id();
    QString nation_key = config.value("nationId").toString();
    if (nation_key.isEmpty()) {
      nation_key =
          QString::fromStdString(Game::Systems::nation_id_to_string(nation_id));
    }
    const auto configured_commander =
        config.contains("commanderTroop")
            ? std::optional<QString>(config.value("commanderTroop").toString())
            : std::nullopt;
    const QString commander_troop =
        App::Core::resolve_commander_troop(nation_key, configured_commander);
    if (commander_troop.isEmpty()) {
      continue;
    }

    const auto spawn_type =
        Game::Units::spawn_typeFromString(commander_troop.trimmed().toStdString());
    if (!spawn_type.has_value()) {
      qWarning() << "Skirmish commander setup: unknown commander troop"
                 << commander_troop;
      continue;
    }
    const auto troop_type = Game::Units::spawn_typeToTroopType(*spawn_type);
    if (!troop_type.has_value() || !Game::Units::is_commander_troop(*troop_type)) {
      qWarning() << "Skirmish commander setup: invalid commander troop"
                 << commander_troop;
      continue;
    }

    App::Core::ResolvedCommanderPosition commander_position;
    const auto anchors = existing_owner_spawn_anchors(owner_id);
    if (!anchors.empty()) {
      commander_position =
          App::Core::resolve_commander_position({}, {}, anchors, {0.0F, 0.0F});
    } else if (const auto fallback = map_spawn_fallback(owner_id);
               fallback.has_value()) {
      commander_position = {.position = fallback.value(),
                            .space = App::Core::CommanderPositionSpace::World};
    } else {
      commander_position = {.position = {0.0F, 0.0F},
                            .space = App::Core::CommanderPositionSpace::World};
    }

    Game::Units::SpawnParams params;
    params.position =
        QVector3D(commander_position.position.x, 0.0F, commander_position.position.z);
    params.player_id = owner_id;
    params.spawn_type = *spawn_type;
    params.ai_controlled = owner_registry.is_ai(owner_id);
    params.nation_id = nation_id;
    auto unit = reg->create(params.spawn_type, *m_world, params);
    if (!unit) {
      qWarning() << "Skirmish commander setup: failed to spawn commander"
                 << commander_troop << "for owner" << owner_id;
      continue;
    }
    apply_team_color(m_world->get_entity(unit->id()), owner_id);
  }
}

void GameEngine::spawn_mission_wave(const PendingMissionWave& wave) {
  if (!m_world) {
    return;
  }

  auto reg = Game::Map::MapTransformer::get_factory_registry();
  if (!reg) {
    qWarning() << "Mission wave spawn skipped: unit factory registry missing";
    return;
  }

  auto& owner_registry = Game::Systems::OwnerRegistry::instance();
  if (owner_registry.get_owner_type(wave.owner_id) ==
      Game::Systems::OwnerType::Neutral) {
    owner_registry.register_owner_with_id(wave.owner_id,
                                          Game::Systems::OwnerType::AI,
                                          "AI Wave " + std::to_string(wave.owner_id));
  }

  const bool ai_controlled = owner_registry.is_ai(wave.owner_id);
  if (wave.composition.empty()) {
    qWarning() << "Mission wave has empty composition for AI" << wave.ai_id;
    return;
  }
  const float spacing = std::max(0.5F, m_level.tile_size * 1.2F);
  const int composition_count = static_cast<int>(wave.composition.size());
  constexpr float k_group_radius_multiplier = 3.0F;
  constexpr float k_two_pi = 6.28318530718F;

  int spawned_units = 0;

  for (int comp_index = 0; comp_index < composition_count; ++comp_index) {
    const auto& comp = wave.composition[static_cast<std::size_t>(comp_index)];
    const auto spawn_type = Game::Units::spawn_typeFromString(comp.type.toStdString());
    if (!spawn_type.has_value()) {
      qWarning() << "Mission wave: unknown unit type" << comp.type;
      continue;
    }

    const int count = std::max(1, comp.count);
    const int grid = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count))));
    const float angle = (k_two_pi * static_cast<float>(comp_index)) /
                        static_cast<float>(composition_count);
    const QVector3D group_center =
        wave.entry_world_position + QVector3D(std::cos(angle), 0.0F, std::sin(angle)) *
                                        (spacing * k_group_radius_multiplier);

    for (int i = 0; i < count; ++i) {
      const int row = i / grid;
      const int col = i % grid;
      const float offset_x = (float(col) - (grid - 1) * 0.5F) * spacing;
      const float offset_z = (float(row) - (grid - 1) * 0.5F) * spacing;
      const QVector3D pos = QVector3D(
          group_center.x() + offset_x, group_center.y(), group_center.z() + offset_z);

      Game::Units::SpawnParams sp;
      sp.position = pos;
      sp.player_id = wave.owner_id;
      sp.spawn_type = spawn_type.value();
      sp.ai_controlled = ai_controlled;
      sp.nation_id = wave.nation_id;

      auto unit = reg->create(sp.spawn_type, *m_world, sp);
      if (!unit) {
        qWarning() << "Mission wave: failed to spawn unit" << comp.type << "for owner"
                   << wave.owner_id;
        continue;
      }

      auto* entity = m_world->get_entity(unit->id());
      if (entity != nullptr) {
        auto* renderable = entity->get_component<Engine::Core::RenderableComponent>();
        if (renderable != nullptr) {
          const QVector3D team_color = Game::Visuals::team_colorForOwner(wave.owner_id);
          renderable->color[0] = team_color.x();
          renderable->color[1] = team_color.y();
          renderable->color[2] = team_color.z();
        }
      }
      spawned_units++;
    }
  }

  if (spawned_units > 0) {
    qInfo() << "Mission wave spawned for AI" << wave.ai_id << "(" << wave.owner_id
            << "):" << spawned_units << "units at t=" << m_campaign_mission_elapsed;

    QString wave_name = wave.ai_id;
    wave_name.replace('_', ' ');
    if (wave_name.isEmpty()) {
      wave_name = QStringLiteral("Enemy");
    }

    const QString direction = classify_wave_direction(wave.entry_world_position);
    const QString announcement = QStringLiteral("%1 wave from the %2 (%3 units)")
                                     .arg(wave_name, direction)
                                     .arg(spawned_units);
    emit mission_announcement(announcement);
  }
}

void GameEngine::center_camera_on_local_forces() {
  if (!m_world || (m_camera == nullptr)) {
    return;
  }

  QVector3D troops_sum(0.0F, 0.0F, 0.0F);
  QVector3D structures_sum(0.0F, 0.0F, 0.0F);
  int troops_count = 0;
  int structures_count = 0;

  auto entities = m_world->get_entities_with<Engine::Core::UnitComponent>();
  for (auto* entity : entities) {
    if (entity == nullptr) {
      continue;
    }

    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (unit == nullptr || transform == nullptr || unit->health <= 0 ||
        unit->owner_id != m_runtime.local_owner_id) {
      continue;
    }

    const QVector3D pos(
        transform->position.x, transform->position.y, transform->position.z);
    if (Game::Units::is_troop_spawn(unit->spawn_type)) {
      troops_sum += pos;
      troops_count++;
    } else {
      structures_sum += pos;
      structures_count++;
    }
  }

  QVector3D focus;
  if (troops_count > 0) {
    focus = troops_sum / static_cast<float>(troops_count);
  } else if (structures_count > 0) {
    focus = structures_sum / static_cast<float>(structures_count);
  } else {
    return;
  }

  const QVector3D current_target = m_camera->get_target();
  const QVector3D current_position = m_camera->get_position();
  const QVector3D offset = current_position - current_target;

  if (offset.lengthSquared() < 1e-6F) {
    const auto& cam_config = Game::GameConfig::instance().camera();
    m_camera->set_rts_view(focus,
                           cam_config.default_distance,
                           cam_config.default_pitch,
                           cam_config.default_yaw);
    return;
  }

  m_camera->look_at(focus + offset, focus, m_camera->get_up_vector());
}

void GameEngine::finalize_skirmish_load() {
  m_runtime.loading = false;
  m_loading_overlay_wait_for_first_frame.store(true, std::memory_order_release);
  m_loading_overlay_frames_remaining = 5;
  m_loading_overlay_min_duration_ms = 1000;
  m_loading_overlay_timer.restart();
  m_finalize_progress_after_overlay = true;

  m_show_objectives_after_loading = is_campaign_mission();

  emit is_loading_changed();

  GameStateRestorer::rebuild_entity_cache(
      m_world.get(), m_entity_cache, m_runtime.local_owner_id);
  emit troop_count_changed();
  sync_scatter_world_props();
  sync_selected_player_state();

  m_ambient_state_manager = std::make_unique<AmbientStateManager>();

  Engine::Core::EventManager::instance().publish(Engine::Core::AmbientStateChangedEvent(
      Engine::Core::AmbientState::PEACEFUL, Engine::Core::AmbientState::PEACEFUL));

  if (m_input_handler) {
    m_input_handler->set_spectator_mode(m_level.is_spectator_mode);
  }

  emit owner_info_changed();
  emit spectator_mode_changed();
}

void GameEngine::open_settings() {
  if (m_save_load_service) {
    m_save_load_service->open_settings();
  }
}

void GameEngine::load_save() {
  load_from_slot("savegame");
}

void GameEngine::save_game(const QString& filename) {
  save_to_slot(filename, filename);
}

void GameEngine::save_game_to_slot(const QString& slot_name) {
  save_to_slot(slot_name, slot_name);
}

void GameEngine::load_game_from_slot(const QString& slot_name) {
  load_from_slot(slot_name);
}

auto GameEngine::load_from_slot(const QString& slot) -> bool {
  if (!m_save_load_service || !m_world) {
    set_error("Load: not initialized");
    return false;
  }

  if (m_control_mode == PlayerControlMode::Commander) {
    exit_commander_control_mode();
  }

  reset_preload_interaction_state();
  reset_mission_runtime_state();

  m_finalize_progress_after_overlay = false;
  m_loading_overlay_active = true;
  m_runtime.loading = true;
  emit is_loading_changed();

  if (!m_save_load_service->load_game_from_slot(*m_world, slot)) {
    set_error(m_save_load_service->get_last_error());
    m_runtime.loading = false;
    m_loading_overlay_active = false;
    m_loading_overlay_wait_for_first_frame.store(false, std::memory_order_release);
    m_finalize_progress_after_overlay = false;
    m_show_objectives_after_loading = false;
    emit is_loading_changed();
    return false;
  }

  const QJsonObject meta = m_save_load_service->get_last_metadata();

  if (m_campaign_manager && meta.contains("mission_mode")) {
    Game::Mission::MissionContext mission_context;
    mission_context.mode = meta["mission_mode"].toString();
    mission_context.campaign_id = meta["mission_campaign_id"].toString();
    mission_context.mission_id = meta["mission_id"].toString();
    mission_context.difficulty = meta["mission_difficulty"].toString();
    m_campaign_manager->set_mission_context(mission_context);
  }

  Game::Systems::GameStateSerializer::restore_level_from_metadata(meta, m_level);
  Game::Systems::GameStateSerializer::restore_camera_from_metadata(
      meta, m_camera, m_viewport.width, m_viewport.height);

  Game::Systems::RuntimeSnapshot runtime_snap = to_runtime_snapshot();
  Game::Systems::GameStateSerializer::restore_runtime_from_metadata(meta, runtime_snap);
  apply_runtime_snapshot(runtime_snap);

  GameStateRestorer::restore_environment_from_metadata(meta,
                                                       scene_context(),
                                                       m_level,
                                                       m_runtime.local_owner_id,
                                                       m_minimap_manager.get(),
                                                       m_visibility_coordinator.get());

  auto unit_reg = std::make_shared<Game::Units::UnitFactoryRegistry>();
  Game::Units::register_built_in_units(*unit_reg);
  Game::Map::MapTransformer::setFactoryRegistry(unit_reg);
  qInfo() << "Factory registry reinitialized after loading saved game";

  GameStateRestorer::rebuild_registries_after_load(
      m_world.get(), m_selected_player_id, m_level, m_runtime.local_owner_id);
  GameStateRestorer::rebuild_entity_cache(
      m_world.get(), m_entity_cache, m_runtime.local_owner_id);
  AudioResourceLoader::load_audio_resources(AudioLoadPolicy::Mission);
  configure_audio_manifest_mappings();

  emit troop_count_changed();

  if (auto* ai_system = m_world->get_system<Game::Systems::AISystem>()) {
    qInfo() << "Reinitializing AI system after loading saved game";
    ai_system->reinitialize();
  }

  if (m_victory_service) {
    m_victory_service->configure(Game::Map::VictoryConfig(), m_runtime.local_owner_id);
  }

  m_runtime.loading = false;
  m_loading_overlay_wait_for_first_frame.store(true, std::memory_order_release);
  m_loading_overlay_frames_remaining = 5;
  m_loading_overlay_min_duration_ms = 1000;
  m_loading_overlay_timer.restart();
  m_finalize_progress_after_overlay = true;
  emit is_loading_changed();
  qInfo() << "Game load complete, victory/defeat checks re-enabled";

  emit selected_units_changed();
  emit owner_info_changed();
  return true;
}

auto GameEngine::save_to_slot(const QString& slot, const QString& title) -> bool {
  if (!m_save_load_service || !m_world) {
    set_error("Save: not initialized");
    return false;
  }

  if (m_control_mode == PlayerControlMode::Commander) {
    exit_commander_control_mode();
  }
  Game::Systems::RuntimeSnapshot const runtime_snap = to_runtime_snapshot();
  QJsonObject meta = Game::Systems::GameStateSerializer::build_metadata(
      *m_world, m_camera, m_level, runtime_snap);
  meta["title"] = title;

  if (m_campaign_manager) {
    const auto& mission_context = m_campaign_manager->current_mission_context();
    meta["mission_mode"] = mission_context.mode;
    meta["mission_campaign_id"] = mission_context.campaign_id;
    meta["mission_id"] = mission_context.mission_id;
    meta["mission_difficulty"] = mission_context.difficulty;
  }

  const QByteArray screenshot = capture_screenshot();
  if (!m_save_load_service->save_game_to_slot(
          *m_world, slot, title, m_level.map_name, meta, screenshot)) {
    set_error(m_save_load_service->get_last_error());
    return false;
  }
  emit save_slots_changed();
  return true;
}

auto GameEngine::get_save_slots() const -> QVariantList {
  if (!m_save_load_service) {
    qWarning() << "Cannot get save slots: service not initialized";
    return {};
  }

  return m_save_load_service->get_save_slots();
}

void GameEngine::refresh_save_slots() {
  emit save_slots_changed();
}

auto GameEngine::delete_save_slot(const QString& slot_name) -> bool {
  if (!m_save_load_service) {
    qWarning() << "Cannot delete save slot: service not initialized";
    return false;
  }

  bool const success = m_save_load_service->delete_save_slot(slot_name);

  if (!success) {
    QString const error = m_save_load_service->get_last_error();
    qWarning() << "Failed to delete save slot:" << error;
    set_error(error);
  } else {
    emit save_slots_changed();
  }

  return success;
}

auto GameEngine::to_runtime_snapshot() const -> Game::Systems::RuntimeSnapshot {
  Game::Systems::RuntimeSnapshot snapshot;
  snapshot.paused = m_runtime.paused;
  snapshot.time_scale = m_runtime.time_scale;
  snapshot.local_owner_id = m_runtime.local_owner_id;
  snapshot.victory_state = m_runtime.victory_state;
  snapshot.cursor_mode = static_cast<int>(m_runtime.cursor_mode);
  snapshot.selected_player_id = m_selected_player_id;
  snapshot.follow_selection = m_follow_selection_enabled;
  snapshot.resources_by_owner =
      Game::Systems::PlayerResourceRegistry::instance().snapshot();
  return snapshot;
}

void GameEngine::apply_runtime_snapshot(
    const Game::Systems::RuntimeSnapshot& snapshot) {
  m_runtime.paused = snapshot.paused;
  m_runtime.time_scale = snapshot.time_scale;
  m_runtime.local_owner_id = snapshot.local_owner_id;
  m_runtime.victory_state = snapshot.victory_state;
  m_selected_player_id = snapshot.selected_player_id;
  m_follow_selection_enabled = snapshot.follow_selection;
  Game::Systems::PlayerResourceRegistry::instance().restore(
      snapshot.resources_by_owner);

  m_runtime.cursor_mode = static_cast<CursorMode>(snapshot.cursor_mode);
  if (m_cursor_manager) {
    m_cursor_manager->set_mode(m_runtime.cursor_mode);
  }
  sync_selected_player_state();
}

void GameEngine::sync_selected_player_state() {
  int const owner_id =
      m_selected_player_id > 0 ? m_selected_player_id : m_runtime.local_owner_id;
  QVariantMap const next_state =
      build_player_state_map(owner_id, m_level.max_troops_per_player);
  if (m_selected_player_state == next_state) {
    return;
  }
  m_selected_player_state = next_state;
  emit selected_player_state_changed();
  emit owner_info_changed();
}

void GameEngine::sync_scatter_world_props() {
  auto& terrain_service = Game::Map::TerrainService::instance();
  if (m_scatter == nullptr || !terrain_service.is_initialized() ||
      terrain_service.get_height_map() == nullptr) {
    return;
  }

  auto const revision = terrain_service.world_props_revision();
  if (revision == m_last_world_props_revision) {
    return;
  }

  m_scatter->configure(*terrain_service.get_height_map(),
                       terrain_service.biome_settings(),
                       terrain_service.world_props(),
                       true);
  m_last_world_props_revision = revision;
}

void GameEngine::initialize_player_resources() {
  auto& resources = Game::Systems::PlayerResourceRegistry::instance();
  resources.clear();

  for (const auto& owner : Game::Systems::OwnerRegistry::instance().get_all_owners()) {
    resources.ensure_owner(owner.owner_id);
  }

  if (m_campaign_manager &&
      m_campaign_manager->current_mission_definition().has_value()) {
    auto const& mission_resources = m_campaign_manager->current_mission_definition()
                                        ->player_setup.starting_resources;
    for (Game::Systems::ResourceType const type : Game::Systems::k_all_resource_types) {
      resources.set(m_runtime.local_owner_id, type, mission_resources.get(type));
    }
  }

  sync_selected_player_state();
}

auto GameEngine::capture_screenshot() const -> QByteArray {
  return {};
}

void GameEngine::exit_game() {
  if (m_save_load_service) {
    m_save_load_service->exit_game();
  }
}

auto GameEngine::get_owner_info() const -> QVariantList {
  QVariantList result;
  const auto& owner_registry = Game::Systems::OwnerRegistry::instance();
  const auto& owners = owner_registry.get_all_owners();

  for (const auto& owner : owners) {
    QVariantMap owner_map;
    owner_map["id"] = owner.owner_id;
    owner_map["name"] = QString::fromStdString(owner.name);
    owner_map["team_id"] = owner.team_id;

    QString type_str;
    switch (owner.type) {
    case Game::Systems::OwnerType::Player:
      type_str = "Player";
      break;
    case Game::Systems::OwnerType::AI:
      type_str = "AI";
      break;
    case Game::Systems::OwnerType::Neutral:
      type_str = "Neutral";
      break;
    }
    owner_map["type"] = type_str;
    owner_map["isLocal"] = (owner.owner_id == m_runtime.local_owner_id);
    owner_map["state"] =
        build_player_state_map(owner.owner_id, m_level.max_troops_per_player);

    result.append(owner_map);
  }

  return result;
}

void GameEngine::get_selected_unit_ids(std::vector<Engine::Core::EntityID>& out) const {
  out.clear();
  if (!m_selection_controller) {
    return;
  }
  m_selection_controller->get_selected_unit_ids(out);
}

auto GameEngine::get_unit_type_key(Engine::Core::EntityID id,
                                   QString& type_key) const -> bool {
  type_key.clear();
  if (!m_world) {
    return false;
  }
  auto* e = m_world->get_entity(id);
  if (e == nullptr) {
    return false;
  }
  if (auto* u = e->get_component<Engine::Core::UnitComponent>()) {
    type_key = Game::Units::spawn_typeToQString(u->spawn_type);
    return true;
  }
  return false;
}

auto GameEngine::get_unit_info(Engine::Core::EntityID id,
                               QString& name,
                               int& health,
                               int& max_health,
                               bool& is_building,
                               bool& alive,
                               QString& nation) const -> bool {
  if (!m_world) {
    return false;
  }
  auto* e = m_world->get_entity(id);
  if (e == nullptr) {
    return false;
  }
  is_building = e->has_component<Engine::Core::BuildingComponent>();
  if (auto* u = e->get_component<Engine::Core::UnitComponent>()) {

    auto troop_type_opt = Game::Units::spawn_typeToTroopType(u->spawn_type);
    if (troop_type_opt.has_value()) {
      auto profile = Game::Systems::TroopProfileService::instance().get_profile(
          u->nation_id, *troop_type_opt);
      name = QString::fromStdString(profile.display_name);
    } else {

      name = QString::fromStdString(Game::Units::spawn_typeToString(u->spawn_type));
    }
    health = u->health;
    max_health = u->max_health;
    alive = (u->health > 0);
    nation = Game::Systems::nation_id_to_qstring(u->nation_id);
    return true;
  }
  name = QStringLiteral("Entity");
  health = max_health = 0;
  alive = true;
  nation = QStringLiteral("");
  return true;
}

auto GameEngine::get_unit_stamina_info(Engine::Core::EntityID id,
                                       float& stamina_ratio,
                                       bool& is_running,
                                       bool& can_run) const -> bool {
  stamina_ratio = 1.0F;
  is_running = false;
  can_run = false;

  if (!m_world) {
    return false;
  }
  auto* e = m_world->get_entity(id);
  if (e == nullptr) {
    return false;
  }

  auto* unit = e->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return false;
  }

  can_run = Game::Units::can_use_run_mode(unit->spawn_type);

  auto* stamina = e->get_component<Engine::Core::StaminaComponent>();
  if (stamina != nullptr) {
    stamina_ratio = stamina->get_stamina_ratio();
    is_running = stamina->is_running;
  }

  return true;
}

void GameEngine::on_unit_spawned(const Engine::Core::UnitSpawnedEvent& event) {
  auto& owners = Game::Systems::OwnerRegistry::instance();

  if (event.owner_id == m_runtime.local_owner_id) {
    if (event.spawn_type == Game::Units::SpawnType::Barracks) {
      m_entity_cache.player_barracks_alive = true;
    } else {
      int const production_cost =
          Game::Units::TroopConfig::instance().get_production_cost(event.spawn_type);
      m_entity_cache.player_troop_count += production_cost;
    }
  } else if (owners.is_ai(event.owner_id)) {
    if (event.spawn_type == Game::Units::SpawnType::Barracks) {
      m_entity_cache.enemy_barracks_count++;
      m_entity_cache.enemy_barracks_alive = true;
    }
  }

  auto emit_if_changed = [&] {
    if (m_entity_cache.player_troop_count != m_runtime.last_troop_count) {
      m_runtime.last_troop_count = m_entity_cache.player_troop_count;
      emit troop_count_changed();
    }
  };
  emit_if_changed();
  if (event.owner_id == m_runtime.local_owner_id) {
    const auto troop_type = Game::Units::spawn_typeToTroopType(event.spawn_type);
    if (troop_type.has_value() && Game::Units::is_commander_troop(*troop_type)) {
      emit commander_control_available_changed();
    }
  }
}

void GameEngine::on_unit_died(const Engine::Core::UnitDiedEvent& event) {
  auto& owners = Game::Systems::OwnerRegistry::instance();

  if (event.owner_id == m_runtime.local_owner_id) {
    if (event.spawn_type == Game::Units::SpawnType::Barracks) {
      m_entity_cache.player_barracks_alive = false;
    } else {
      int const production_cost =
          Game::Units::TroopConfig::instance().get_production_cost(event.spawn_type);
      m_entity_cache.player_troop_count -= production_cost;
      m_entity_cache.player_troop_count =
          std::max(0, m_entity_cache.player_troop_count);
    }
  } else if (owners.is_ai(event.owner_id)) {
    if (event.spawn_type == Game::Units::SpawnType::Barracks) {
      m_entity_cache.enemy_barracks_count--;
      m_entity_cache.enemy_barracks_count =
          std::max(0, m_entity_cache.enemy_barracks_count);
      m_entity_cache.enemy_barracks_alive = (m_entity_cache.enemy_barracks_count > 0);
    }
  }
  if (event.owner_id == m_runtime.local_owner_id) {
    const auto troop_type = Game::Units::spawn_typeToTroopType(event.spawn_type);
    if (troop_type.has_value() && Game::Units::is_commander_troop(*troop_type)) {
      if (m_controlled_commander_id == event.unit_id) {
        exit_commander_control_mode();
      }
      emit commander_control_available_changed();
    }
  }
}

auto GameEngine::minimap_image() const -> QImage {
  if (m_minimap_manager) {
    return m_minimap_manager->get_image();
  }
  return {};
}

auto GameEngine::generate_map_preview(
    const QString& map_path, const QVariantList& player_configs) const -> QImage {
  Game::Map::Minimap::MapPreviewGenerator generator;
  return generator.generate_preview(map_path, player_configs);
}

float GameEngine::loading_progress() const {
  if (m_loading_progress_tracker) {
    return m_loading_progress_tracker->progress();
  }
  return 0.0F;
}

QString GameEngine::loading_stage_text() const {
  if (m_loading_progress_tracker) {
    auto stage = m_loading_progress_tracker->current_stage();
    auto stage_name = m_loading_progress_tracker->stage_name(stage);
    auto detail = m_loading_progress_tracker->current_detail();
    if (!detail.isEmpty()) {
      return stage_name + " - " + detail;
    }
    return stage_name;
  }
  return {};
}
