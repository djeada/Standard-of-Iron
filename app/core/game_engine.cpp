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
#include "audio_coordinator.h"
#include "audio_event_handler.h"
#include "audio_resource_loader.h"
#include "camera_controller.h"
#include "campaign_manager.h"
#include "commander_mode_coordinator.h"
#include "commander_status_builder.h"
#include "core/system.h"
#include "frame_ui_coordinator.h"
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
#include "game/systems/building_collision_registry.h"
#include "game/systems/camera_service.h"
#include "game/systems/camera_visibility_service.h"
#include "game/systems/capture_system.h"
#include "game/systems/cleanup_system.h"
#include "game/systems/combat_rules.h"
#include "game/systems/combat_system.h"
#include "game/systems/command_service.h"
#include "game/systems/formation_planner.h"
#include "game/systems/game_state_serializer.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/guard_system.h"
#include "game/systems/healing_system.h"
#include "game/systems/marketplace_system.h"
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
#include "game/systems/rain_manager.h"
#include "game/systems/rpg_combat_system/rpg_combat_processor.h"
#include "game/systems/save_load_service.h"
#include "game/systems/selection_system.h"
#include "game/systems/terrain_alignment_system.h"
#include "game/systems/troop_count_registry.h"
#include "game/systems/troop_profile_service.h"
#include "game/systems/undead_awakening_system.h"
#include "game/systems/victory_service.h"
#include "game/units/commander_catalog.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_config.h"
#include "game/units/troop_type.h"
#include "game/visuals/team_colors.h"
#include "game_state_restorer.h"
#include "input_command_handler.h"
#include "loading_progress_tracker.h"
#include "minimap_manager.h"
#include "mission_commander_setup.h"
#include "mission_definition_view.h"
#include "mission_setup_coordinator.h"
#include "production_manager.h"
#include "render/geom/stone.h"
#include "render/gl/bootstrap.h"
#include "render/gl/camera.h"
#include "render/ground/ambient_fog_renderer.h"
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
#include "rts_action_model.h"
#include "save_load_coordinator.h"
#include "selection_query_service.h"
#include "skirmish_runtime_coordinator.h"
#include "utils/resource_utils.h"
#include "visibility_coordinator.h"

namespace {

auto marketplace_trade_resource_from_key(QStringView key)
    -> std::optional<Game::Systems::ResourceType> {
  if (key == QLatin1String("wood")) {
    return Game::Systems::ResourceType::Wood;
  }
  if (key == QLatin1String("stone")) {
    return Game::Systems::ResourceType::Stone;
  }
  if (key == QLatin1String("iron")) {
    return Game::Systems::ResourceType::Iron;
  }
  return std::nullopt;
}

auto marketplace_trade_resource_label(QStringView key) -> QString {
  if (key == QLatin1String("wood")) {
    return QStringLiteral("wood");
  }
  if (key == QLatin1String("stone")) {
    return QStringLiteral("stone");
  }
  if (key == QLatin1String("iron")) {
    return QStringLiteral("iron");
  }
  return key.toString();
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
  m_ambient_fog = std::move(rendering.ambient_fog);
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
          &ProductionManager::construction_preview_active_changed,
          this,
          &GameEngine::construction_preview_active_changed);
  connect(m_production_manager.get(),
          &ProductionManager::construction_preview_valid_changed,
          this,
          &GameEngine::construction_preview_valid_changed);
  connect(m_production_manager.get(),
          &ProductionManager::construction_preview_summary_changed,
          this,
          &GameEngine::construction_preview_summary_changed);
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
  m_audio_coordinator = std::make_unique<AudioCoordinator>(m_audio_event_handler.get());
  m_commander_mode = std::make_unique<App::Core::CommanderModeCoordinator>();
  m_mission_setup = std::make_unique<App::Core::MissionSetupCoordinator>();
  m_save_load_coordinator = std::make_unique<App::Core::SaveLoadCoordinator>();
  m_skirmish_runtime = std::make_unique<App::Core::SkirmishRuntimeCoordinator>();
  if (m_audio_event_handler->initialize()) {
    qInfo() << "AudioEventHandler initialized successfully";
    AudioResourceLoader::load_audio_resources(AudioLoadPolicy::Screen);
    m_audio_coordinator->configure_audio_manifest_mappings(m_runtime.local_owner_id);

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
          &App::Controllers::CommandController::insufficient_resources,
          [this](const QString& message) {
            AudioSystem::get_instance().play_sound(
                "low_resources_click", 0.85F, false, 7, AudioCategory::SFX);
            set_error(message);
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
  m_ambient_fog.reset();
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
  if (!is_action_enabled(QStringLiteral("heal"))) {
    return;
  }
  m_cursor_manager->set_mode(CursorMode::Heal);
}

void GameEngine::on_build_command() {
  if (!m_cursor_manager) {
    return;
  }
  ensure_initialized();
  if (!is_action_enabled(QStringLiteral("build"))) {
    return;
  }
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

auto GameEngine::pending_builder_construction_type() const -> QString {
  return m_production_manager
             ? m_production_manager->pending_builder_construction_type()
             : QString();
}

auto GameEngine::construction_preview_active() const -> bool {
  return m_production_manager ? m_production_manager->construction_preview_active()
                              : false;
}

auto GameEngine::construction_preview_valid() const -> bool {
  return m_production_manager ? m_production_manager->construction_preview_valid()
                              : false;
}

auto GameEngine::construction_preview_rotatable() const -> bool {
  return m_production_manager ? m_production_manager->construction_preview_rotatable()
                              : false;
}

auto GameEngine::construction_preview_segment_count() const -> int {
  return m_production_manager
             ? m_production_manager->construction_preview_segment_count()
             : 0;
}

auto GameEngine::construction_preview_valid_segment_count() const -> int {
  return m_production_manager
             ? m_production_manager->construction_preview_valid_segment_count()
             : 0;
}

auto GameEngine::construction_preview_total_cost() const -> int {
  return m_production_manager ? m_production_manager->construction_preview_total_cost()
                              : 0;
}

void GameEngine::on_construction_mouse_move(qreal sx, qreal sy) {
  ensure_initialized();
  if (m_production_manager) {
    QPointF const viewport_point = map_input_to_viewport(sx, sy);
    m_production_manager->on_construction_mouse_move(
        viewport_point.x(), viewport_point.y(), m_viewport);
  }
}

void GameEngine::on_construction_pointer_pressed(qreal sx, qreal sy) {
  ensure_initialized();
  if (m_production_manager) {
    QPointF const viewport_point = map_input_to_viewport(sx, sy);
    m_production_manager->on_construction_pointer_pressed(
        viewport_point.x(), viewport_point.y(), m_viewport);
  }
}

void GameEngine::on_construction_pointer_released(qreal sx, qreal sy) {
  ensure_initialized();
  if (m_production_manager) {
    QPointF const viewport_point = map_input_to_viewport(sx, sy);
    m_production_manager->on_construction_pointer_released(
        viewport_point.x(), viewport_point.y(), m_viewport);
  }
  if (!is_placing_construction()) {
    set_cursor_mode(CursorMode::Normal);
  }
}

void GameEngine::on_construction_scroll(float delta) {
  ensure_initialized();
  if (m_production_manager) {
    m_production_manager->on_construction_scroll(delta);
  }
}

void GameEngine::on_construction_confirm() {
  ensure_initialized();
  if (m_production_manager) {
    m_production_manager->on_construction_confirm();
  }
  if (!is_placing_construction()) {
    set_cursor_mode(CursorMode::Normal);
  }
}

void GameEngine::on_construction_cancel() {
  if (m_production_manager) {
    m_production_manager->on_construction_cancel();
  }
  if (!is_placing_construction()) {
    set_cursor_mode(CursorMode::Normal);
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
  m_saved_rts_selection_ids =
      m_commander_mode
          ->store_rts_selection({.selection_controller = m_selection_controller.get()})
          .saved_rts_selection_ids;
}

void GameEngine::select_controlled_commander() {
  m_commander_mode->select_controlled_commander(
      {.selection_controller = m_selection_controller.get(),
       .controlled_commander_id = m_controlled_commander_id,
       .local_owner_id = m_runtime.local_owner_id});
}

void GameEngine::restore_rts_selection() {
  auto const effects = m_commander_mode->restore_rts_selection(
      {.world = m_world.get(),
       .local_owner_id = m_runtime.local_owner_id,
       .saved_rts_selection_ids = &m_saved_rts_selection_ids});
  if (effects.sync_selection_flags) {
    sync_selection_flags();
  }
  if (effects.emit_selected_units_changed) {
    emit selected_units_changed();
  }
  if (effects.clear_saved_rts_selection_ids) {
    m_saved_rts_selection_ids.clear();
  }
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

void GameEngine::request_enter_commander_control_mode() {
  ensure_initialized();
  auto* commander = find_local_commander();
  if (m_level.is_spectator_mode || commander == nullptr ||
      m_commander_camera == nullptr) {
    return;
  }

  if (is_placing_formation()) {
    on_formation_cancel();
  }
  if (is_placing_construction()) {
    on_construction_cancel();
  }
  set_cursor_mode(CursorMode::Normal);

  store_rts_selection();
  auto const effects = m_commander_mode->enter_commander_control_mode(
      {.world = m_world.get(),
       .commander = commander,
       .commander_camera = m_commander_camera.get(),
       .commander_control = &m_commander_control,
       .local_owner_id = m_runtime.local_owner_id,
       .is_spectator_mode = m_level.is_spectator_mode,
       .follow_selection_enabled = m_follow_selection_enabled});
  if (!effects.entered) {
    return;
  }

  if (effects.save_follow_selection_snapshot) {
    m_rts_camera_snapshot.follow_selection = effects.saved_follow_selection_enabled;
    m_rts_camera_snapshot.valid = true;
  }
  m_follow_selection_enabled = false;
  if (m_camera_controller) {
    m_camera_controller->follow_selection(false);
  }

  m_controlled_commander_id = effects.controlled_commander_id;
  if (effects.commander_view_yaw.has_value()) {
    m_commander_control.set_view_yaw(*effects.commander_view_yaw);
  }
  m_commander_control.set_view_pitch(0.0F);
  reset_commander_input();
  set_active_camera(m_commander_camera.get());

  enter_commander_runtime_mode();

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
}

void GameEngine::request_exit_commander_control_mode() {
  exit_commander_runtime_mode();
  reset_commander_input();
  auto const effects = m_commander_mode->exit_commander_control_mode(
      {.world = m_world.get(),
       .controlled_commander_id = m_controlled_commander_id,
       .rts_follow_selection_snapshot_valid = m_rts_camera_snapshot.valid,
       .rts_follow_selection_snapshot = m_rts_camera_snapshot.follow_selection});
  enter_rts_runtime_mode();
  m_controlled_commander_id = effects.controlled_commander_id;

  set_active_camera(m_rts_camera.get());
  if (effects.restored_follow_selection_enabled.has_value()) {
    m_follow_selection_enabled = *effects.restored_follow_selection_enabled;
    if (m_camera_controller) {
      m_camera_controller->follow_selection(m_follow_selection_enabled);
    }
  }
  restore_rts_selection();

  emit game_mode_changed();
  emit control_mode_changed();
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
    request_exit_commander_control_mode();
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
  if (m_cursor_manager == nullptr) {
    return;
  }

  auto const effects = m_commander_mode->begin_commander_flag_rally(
      {.world = m_world.get(),
       .local_commander = find_local_commander(),
       .controlled_commander = controlled_commander_entity(),
       .local_owner_id = m_runtime.local_owner_id,
       .commander_mode_active = m_control_mode == PlayerControlMode::Commander,
       .cursor_mode = m_cursor_manager->mode()});
  if (effects.should_exit_commander_mode) {
    request_exit_commander_control_mode();
    return;
  }
  if (effects.reset_commander_input) {
    reset_commander_input();
  }
  if (effects.clear_rally_preview) {
    m_commander_rally_preview_pos = std::nullopt;
  }
  if (effects.cursor_mode.has_value()) {
    set_cursor_mode(*effects.cursor_mode);
  }
  if (effects.seed_preview_from_view_center) {
    seed_commander_rally_preview_from_view_center();
  }
}

void GameEngine::confirm_commander_flag_rally(qreal sx, qreal sy) {
  if (m_cursor_manager == nullptr) {
    return;
  }

  auto const effects = m_commander_mode->confirm_commander_flag_rally(
      {.world = m_world.get(),
       .local_commander = find_local_commander(),
       .controlled_commander = controlled_commander_entity(),
       .picking_service = m_picking_service.get(),
       .camera = m_camera,
       .viewport_width = m_viewport.width,
       .viewport_height = m_viewport.height,
       .screen_x = sx,
       .screen_y = sy,
       .local_owner_id = m_runtime.local_owner_id,
       .commander_mode_active = m_control_mode == PlayerControlMode::Commander,
       .cursor_mode = m_cursor_manager->mode()});
  if (effects.reset_commander_input) {
    reset_commander_input();
  }
  if (effects.clear_rally_preview) {
    m_commander_rally_preview_pos = std::nullopt;
  }
  if (effects.cursor_mode.has_value()) {
    set_cursor_mode(*effects.cursor_mode);
  }
}

void GameEngine::cancel_commander_flag_rally() {
  auto const effects = m_commander_mode->cancel_commander_flag_rally(
      m_cursor_manager ? m_cursor_manager->mode() : CursorMode::Normal);
  if (effects.clear_rally_preview) {
    m_commander_rally_preview_pos = std::nullopt;
  }
  if (effects.cursor_mode.has_value()) {
    set_cursor_mode(*effects.cursor_mode);
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

void GameEngine::begin_barracks_rally_placement() {
  ensure_initialized();
  auto const effects = m_commander_mode->begin_barracks_rally_placement(
      {.world = m_world.get(), .local_owner_id = m_runtime.local_owner_id});
  if (effects.clear_rally_preview) {
    m_commander_rally_preview_pos = std::nullopt;
  }
  if (effects.cursor_mode.has_value()) {
    set_cursor_mode(*effects.cursor_mode);
  }
  if (effects.rally_preview.has_value()) {
    m_commander_rally_preview_pos = effects.rally_preview;
  }
}

void GameEngine::confirm_barracks_rally_placement(qreal sx, qreal sy) {
  auto const effects = m_commander_mode->confirm_barracks_rally_placement(
      {.world = m_world.get(),
       .production_manager = m_production_manager.get(),
       .viewport = &m_viewport,
       .local_owner_id = m_runtime.local_owner_id,
       .screen_x = sx,
       .screen_y = sy,
       .cursor_mode =
           m_cursor_manager ? m_cursor_manager->mode() : CursorMode::Normal});
  if (effects.clear_rally_preview) {
    m_commander_rally_preview_pos = std::nullopt;
  }
  if (effects.cursor_mode.has_value()) {
    set_cursor_mode(*effects.cursor_mode);
  }
}

void GameEngine::cancel_barracks_rally_placement() {
  auto const effects = m_commander_mode->cancel_barracks_rally_placement(
      m_cursor_manager ? m_cursor_manager->mode() : CursorMode::Normal);
  if (effects.clear_rally_preview) {
    m_commander_rally_preview_pos = std::nullopt;
  }
  if (effects.cursor_mode.has_value()) {
    set_cursor_mode(*effects.cursor_mode);
  }
}

auto GameEngine::has_commander_rally_flag() const -> bool {
  if (m_world == nullptr) {
    return false;
  }
  for (auto* entity : m_world->get_entities_with<Engine::Core::CommanderComponent>()) {
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->owner_id != m_runtime.local_owner_id) {
      continue;
    }
    auto* commander_data = entity->get_component<Engine::Core::CommanderComponent>();
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
  for (auto* entity : m_world->get_entities_with<Engine::Core::CommanderComponent>()) {
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->owner_id != m_runtime.local_owner_id) {
      continue;
    }
    auto* commander_data = entity->get_component<Engine::Core::CommanderComponent>();
    if (commander_data != nullptr && commander_data->flag_rally_flag_active) {
      return {
          commander_data->flag_rally_flag_x, 0.0F, commander_data->flag_rally_flag_z};
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
  if (!m_picking_service->screen_to_ground(
          QPointF(m_viewport.width * 0.5, m_viewport.height * 0.5),
          *m_camera,
          m_viewport.width,
          m_viewport.height,
          hit)) {
    return;
  }
  m_commander_rally_preview_pos =
      Game::Systems::CommandService::snap_to_walkable_ground(hit);
}

void GameEngine::seed_barracks_rally_preview_from_selection() {
  if (auto preview = m_commander_mode->seed_barracks_rally_preview_from_selection(
          {.world = m_world.get(), .local_owner_id = m_runtime.local_owner_id});
      preview.has_value()) {
    m_commander_rally_preview_pos = *preview;
  }
}

void GameEngine::restore_controlled_commander_direct_control_if_ready() {
  auto const effects =
      m_commander_mode->restore_controlled_commander_direct_control_if_ready(
          {.world = m_world.get(),
           .controlled_commander_id = m_controlled_commander_id,
           .commander_mode_active = m_control_mode == PlayerControlMode::Commander,
           .placing_commander_rally = is_placing_commander_rally()});
  if (effects.reset_commander_input) {
    reset_commander_input();
  }
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
  const bool civilian_delivery_available =
      App::Core::FrameUiCoordinator::civilian_delivery_available(
          {.world = m_world.get(),
           .hover_tracker = m_hover_tracker.get(),
           .local_owner_id = m_runtime.local_owner_id});
  if (m_civilian_delivery_available != civilian_delivery_available) {
    m_civilian_delivery_available = civilian_delivery_available;
    emit civilian_delivery_available_changed();
  }

  if (m_cursor_manager == nullptr || m_picking_service == nullptr ||
      m_camera == nullptr) {
    return;
  }

  QVector3D hit;
  if (m_cursor_manager->mode() == CursorMode::PlaceCommanderRally &&
      m_picking_service->screen_to_ground(
          QPointF(sx, sy), *m_camera, m_viewport.width, m_viewport.height, hit)) {
    m_commander_rally_preview_pos =
        Game::Systems::CommandService::snap_to_walkable_ground(hit);
  } else if (m_cursor_manager->mode() == CursorMode::PlaceBarracksRally &&
             m_picking_service->screen_to_surface(QPointF(sx, sy),
                                                  *m_camera,
                                                  m_viewport.width,
                                                  m_viewport.height,
                                                  hit)) {
    m_commander_rally_preview_pos = hit;
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
                         .ambient_fog = m_ambient_fog.get(),
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
  m_commander_mode->clear_controlled_commander_state(
      {.world = m_world.get(), .controlled_commander_id = m_controlled_commander_id});
}

void GameEngine::update_commander_control_mode(float dt) {
  poll_commander_mouse_look();
  auto const effects = m_commander_mode->update_commander_control_mode(
      {.world = m_world.get(),
       .commander_camera = m_commander_camera.get(),
       .commander_control = &m_commander_control,
       .controlled_commander_id = m_controlled_commander_id,
       .local_owner_id = m_runtime.local_owner_id,
       .dt = dt});
  if (effects.should_exit_commander_mode) {
    request_exit_commander_control_mode();
    return;
  }
  if (effects.hit_stop_duration.has_value()) {
    m_rpg_hit_stop_timer = *effects.hit_stop_duration;
    m_rpg_hit_stop_total = *effects.hit_stop_duration;
  }
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

      const float progress =
          1.0F - std::clamp(m_rpg_hit_stop_timer / m_rpg_hit_stop_total, 0.0F, 1.0F);
      const float time_scale =
          progress < 0.5F ? 0.04F : (0.04F + 0.96F * (progress - 0.5F) * 2.0F);
      dt *= time_scale;
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
  App::Core::FrameUiCoordinator::render_effects(
      {.renderer = m_renderer.get(),
       .world = m_world.get(),
       .command_controller = m_command_controller.get(),
       .local_owner_id = m_runtime.local_owner_id,
       .commander_rally_preview_pos = m_commander_rally_preview_pos},
      [this]() { render_runtime_mode_effects(); });
  m_renderer->end_frame();

  update_loading_overlay();
  update_cursor_position();
}

void GameEngine::set_input_viewport_size(qreal width, qreal height) {
  if (width > 0.0 && height > 0.0) {
    m_viewport.input_width = width;
    m_viewport.input_height = height;
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

auto GameEngine::map_input_to_viewport(qreal sx, qreal sy) const -> QPointF {
  if (m_viewport.width <= 0 || m_viewport.height <= 0 ||
      m_viewport.input_width <= 0.0 || m_viewport.input_height <= 0.0) {
    return {sx, sy};
  }

  qreal const scale_x = qreal(m_viewport.width) / m_viewport.input_width;
  qreal const scale_y = qreal(m_viewport.height) / m_viewport.input_height;
  return {sx * scale_x, sy * scale_y};
}

void GameEngine::sync_selection_flags() {
  if (!m_world) {
    return;
  }
  auto* selection_system = m_world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return;
  }

  App::Utils::sanitize_selection(m_world.get(), selection_system);
  const auto prune_effects =
      App::Core::FrameUiCoordinator::prune_selection_action_context(
          {.world = m_world.get(),
           .cursor_manager = m_cursor_manager.get(),
           .production_manager = m_production_manager.get(),
           .command_controller = m_command_controller.get(),
           .local_owner_id = m_runtime.local_owner_id,
           .hud_action_states = get_hud_action_states()});
  if (prune_effects.cancel_construction) {
    on_construction_cancel();
  }
  if (prune_effects.cancel_formation) {
    on_formation_cancel();
  }
  switch (prune_effects.cursor_resolution) {
  case App::Core::FrameUiCoordinator::CursorResolution::CancelBarracksRallyPlacement:
    cancel_barracks_rally_placement();
    break;
  case App::Core::FrameUiCoordinator::CursorResolution::CancelCommanderFlagRally:
    cancel_commander_flag_rally();
    break;
  case App::Core::FrameUiCoordinator::CursorResolution::ResetToNormal:
    if (prune_effects.clear_patrol_first_waypoint && m_command_controller) {
      m_command_controller->clear_patrol_first_waypoint();
    }
    set_cursor_mode(CursorMode::Normal);
    break;
  case App::Core::FrameUiCoordinator::CursorResolution::None:
    break;
  }

  emit hold_mode_changed(any_selected_in_hold_mode());
  emit guard_mode_changed(any_selected_in_guard_mode());
  emit formation_mode_changed(any_selected_in_formation_mode());
  emit run_mode_changed(any_selected_in_run_mode());
  const bool civilian_delivery_available =
      App::Core::FrameUiCoordinator::civilian_delivery_available(
          {.world = m_world.get(),
           .hover_tracker = m_hover_tracker.get(),
           .local_owner_id = m_runtime.local_owner_id});
  if (m_civilian_delivery_available != civilian_delivery_available) {
    m_civilian_delivery_available = civilian_delivery_available;
    emit civilian_delivery_available_changed();
  }
}

auto GameEngine::is_action_enabled(const QString& action_id) const -> bool {
  return get_hud_action_states()
      .value(action_id)
      .toMap()
      .value(QStringLiteral("enabled"))
      .toBool();
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
  if (!m_minimap_manager || !m_minimap_manager->has_minimap()) {
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

  if (m_input_handler) {
    m_input_handler->on_minimap_right_click(QVector3D(world_x, 0.0F, world_z));
  }
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
  m_audio_coordinator->apply_frontend_music_context(normalized);
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
    m_production_manager->start_building_placement(building_type,
                                                   m_runtime.local_owner_id);
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

auto GameEngine::get_construction_info(const QString& item_type) const -> QVariantMap {
  return m_production_manager ? m_production_manager->get_construction_info(item_type)
                              : QVariantMap();
}

auto GameEngine::get_selected_marketplace_state() const -> QVariantMap {
  return m_production_manager ? m_production_manager->get_selected_marketplace_state(
                                    m_runtime.local_owner_id)
                              : QVariantMap();
}

auto GameEngine::get_selected_builder_production_state() const -> QVariantMap {
  return m_production_manager
             ? m_production_manager->get_selected_builder_production_state()
             : QVariantMap();
}

bool GameEngine::marketplace_buy_resource(const QString& resource_key) {
  ensure_initialized();

  QVariantMap const market_state = get_selected_marketplace_state();
  if (!market_state.value("has_marketplace").toBool()) {
    set_error(QStringLiteral("Select your marketplace to trade."));
    return false;
  }

  auto const resource_type = marketplace_trade_resource_from_key(resource_key);
  if (!resource_type.has_value()) {
    set_error(QStringLiteral("Marketplace can trade only wood, stone, or iron."));
    return false;
  }

  auto& marketplace = Game::Systems::MarketplaceSystem::instance();
  if (!marketplace.can_buy(m_runtime.local_owner_id, *resource_type)) {
    set_error(QStringLiteral("Not enough gold to buy %1.")
                  .arg(marketplace_trade_resource_label(resource_key)));
    return false;
  }

  if (!marketplace.buy_resource(m_runtime.local_owner_id, *resource_type)) {
    set_error(QStringLiteral("Cannot buy %1 right now.")
                  .arg(marketplace_trade_resource_label(resource_key)));
    return false;
  }

  clear_error();
  sync_selected_player_state();
  return true;
}

bool GameEngine::marketplace_sell_resource(const QString& resource_key) {
  ensure_initialized();

  QVariantMap const market_state = get_selected_marketplace_state();
  if (!market_state.value("has_marketplace").toBool()) {
    set_error(QStringLiteral("Select your marketplace to trade."));
    return false;
  }

  auto const resource_type = marketplace_trade_resource_from_key(resource_key);
  if (!resource_type.has_value()) {
    set_error(QStringLiteral("Marketplace can trade only wood, stone, or iron."));
    return false;
  }

  auto& marketplace = Game::Systems::MarketplaceSystem::instance();
  if (!marketplace.can_sell(m_runtime.local_owner_id, *resource_type)) {
    set_error(QStringLiteral("Not enough %1 to sell.")
                  .arg(marketplace_trade_resource_label(resource_key)));
    return false;
  }

  if (!marketplace.sell_resource(m_runtime.local_owner_id, *resource_type)) {
    set_error(QStringLiteral("Cannot sell %1 right now.")
                  .arg(marketplace_trade_resource_label(resource_key)));
    return false;
  }

  clear_error();
  sync_selected_player_state();
  return true;
}

auto GameEngine::get_controlled_commander_status() const -> QVariantMap {
  App::Core::CommanderStatusInput input;
  input.world = m_world.get();
  input.controlled_commander_id = m_controlled_commander_id;
  input.dodge_active = m_commander_control.is_dodge_rolling();
  input.locked_target_id = m_commander_control.locked_target_id();
  input.rally_placing = is_placing_commander_rally();
  input.get_unit_info = [this](Engine::Core::EntityID id,
                               QString& name,
                               int& health,
                               int& max_health,
                               bool& is_building,
                               bool& alive,
                               QString& nation) {
    return get_unit_info(id, name, health, max_health, is_building, alive, nation);
  };
  input.get_unit_stamina_info = [this](Engine::Core::EntityID id,
                                       float& stamina_ratio,
                                       bool& is_running,
                                       bool& can_run) {
    return get_unit_stamina_info(id, stamina_ratio, is_running, can_run);
  };
  return App::Core::build_controlled_commander_status(input);
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
    if (m_production_manager->is_placing_construction()) {
      set_cursor_mode(item_type == QStringLiteral("collect") ? CursorMode::Collect
                                                             : CursorMode::Build);
    }
  }
}

auto GameEngine::get_selected_units_command_mode() const -> QString {
  App::Core::ActionContext context;
  context.world = m_world.get();
  context.cursor_mode =
      m_cursor_manager != nullptr ? m_cursor_manager->mode() : CursorMode::Normal;
  context.placing_construction = m_production_manager != nullptr &&
                                 m_production_manager->is_placing_construction();
  context.pending_builder_construction_type =
      m_production_manager != nullptr
          ? m_production_manager->pending_builder_construction_type()
          : QString();
  context.placing_formation =
      m_command_controller != nullptr && m_command_controller->is_placing_formation();
  context.has_patrol_first_waypoint = m_command_controller != nullptr &&
                                      m_command_controller->has_patrol_first_waypoint();
  return App::Core::get_current_action_mode(context);
}

auto GameEngine::get_selected_units_toggle_state(const QString& mode) const -> QString {
  return App::Core::get_toggle_state(m_world.get(), mode);
}

auto GameEngine::get_selected_units_mode_availability() const -> QVariantMap {
  return App::Core::get_mode_availability(m_world.get());
}

auto GameEngine::get_hud_action_states() const -> QVariantMap {
  App::Core::ActionContext context;
  context.world = m_world.get();
  context.cursor_mode =
      m_cursor_manager != nullptr ? m_cursor_manager->mode() : CursorMode::Normal;
  context.placing_construction = m_production_manager != nullptr &&
                                 m_production_manager->is_placing_construction();
  context.pending_builder_construction_type =
      m_production_manager != nullptr
          ? m_production_manager->pending_builder_construction_type()
          : QString();
  context.placing_formation =
      m_command_controller != nullptr && m_command_controller->is_placing_formation();
  context.has_patrol_first_waypoint = m_command_controller != nullptr &&
                                      m_command_controller->has_patrol_first_waypoint();
  return App::Core::get_action_states(context);
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
    if (!nation.playable || !nation.selectable_in_skirmish) {
      continue;
    }
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
  start_skirmish_internal(
      mission.map_path, build_campaign_player_configs(mission), false);
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
  if (!m_campaign_manager) {
    return {};
  }

  const auto& mission_def = m_campaign_manager->current_mission_definition();
  if (!mission_def.has_value()) {
    return {};
  }

  return build_mission_objectives_map(*mission_def);
}

QVariantMap GameEngine::get_mission_definition(const QString& mission_id) const {
  return load_mission_definition_map(mission_id);
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

  if (!m_world || !m_renderer || (m_camera == nullptr) || !m_skirmish_runtime) {
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
    if (!m_world || !m_renderer || (m_camera == nullptr) || !m_skirmish_runtime) {
      set_error("Cannot start skirmish: renderer not initialized");
      m_runtime.loading = false;
      emit is_loading_changed();
      return;
    }

    if (m_hover_tracker) {
      m_hover_tracker->update_hover(-1, -1, *m_world, *m_camera, 0, 0);
    }

    const bool allow_default_player_barracks =
        !m_campaign_manager ||
        !m_campaign_manager->current_mission_context().is_campaign();
    const auto load_effects =
        m_skirmish_runtime->perform_load({*m_world,
                                          m_level,
                                          m_entity_cache,
                                          map_path,
                                          player_configs,
                                          m_selected_player_id,
                                          scene_context(),
                                          m_victory_service.get(),
                                          m_minimap_manager.get(),
                                          m_visibility_coordinator.get(),
                                          allow_default_player_barracks,
                                          m_loading_progress_tracker.get(),
                                          [this]() {
                                            emit owner_info_changed();
                                          }});

    if (load_effects.selected_player_changed) {
      m_selected_player_id = load_effects.updated_player_id;
      emit selected_player_id_changed();
    }

    if (!load_effects.success) {
      set_error(load_effects.error);
      m_runtime.loading = false;
      m_loading_overlay_active = false;
      m_loading_overlay_wait_for_first_frame.store(false, std::memory_order_release);
      m_finalize_progress_after_overlay = false;
      m_show_objectives_after_loading = false;
      emit is_loading_changed();
      return;
    }

    m_runtime.local_owner_id = load_effects.updated_player_id;
    m_audio_coordinator->configure_audio_manifest_mappings(m_runtime.local_owner_id);
    const Game::Mission::MissionDefinition* mission_def = nullptr;
    if (m_campaign_manager &&
        m_campaign_manager->current_mission_definition().has_value()) {
      mission_def = &*m_campaign_manager->current_mission_definition();
    }
    const Game::Mission::MissionDefinition* campaign_mission_def = nullptr;
    if (m_campaign_manager &&
        m_campaign_manager->current_mission_context().is_campaign() &&
        m_campaign_manager->current_mission_definition().has_value()) {
      campaign_mission_def = &*m_campaign_manager->current_mission_definition();
    }
    m_audio_coordinator->apply_mission_ambience(
        mission_def, map_path, m_runtime.local_owner_id);

    apply_skirmish_commander_setup(player_configs);
    apply_mission_setup();
    m_skirmish_runtime->initialize_player_resources(
        {m_level, m_runtime.local_owner_id, campaign_mission_def});
    configure_mission_victory_conditions();
    configure_rain_system();

    const auto finalize_effects =
        m_skirmish_runtime->finalize_load({m_runtime.loading,
                                           m_loading_overlay_wait_for_first_frame,
                                           m_loading_overlay_frames_remaining,
                                           m_loading_overlay_min_duration_ms,
                                           m_loading_overlay_timer,
                                           m_finalize_progress_after_overlay,
                                           m_show_objectives_after_loading,
                                           is_campaign_mission()});

    if (finalize_effects.emit_is_loading_changed) {
      emit is_loading_changed();
    }
    if (finalize_effects.rebuild_entity_cache) {
      GameStateRestorer::rebuild_entity_cache(
          m_world.get(), m_entity_cache, m_runtime.local_owner_id);
    }
    if (finalize_effects.emit_troop_count_changed) {
      emit troop_count_changed();
    }
    if (finalize_effects.sync_scatter_world_props) {
      sync_scatter_world_props();
    }
    if (finalize_effects.sync_selected_player_state) {
      sync_selected_player_state();
    }
    if (finalize_effects.reset_ambient_state) {
      m_ambient_state_manager = std::make_unique<AmbientStateManager>();
      Engine::Core::EventManager::instance().publish(
          Engine::Core::AmbientStateChangedEvent(Engine::Core::AmbientState::PEACEFUL,
                                                 Engine::Core::AmbientState::PEACEFUL));
    }
    if (finalize_effects.apply_spectator_mode && m_input_handler) {
      m_input_handler->set_spectator_mode(m_level.is_spectator_mode);
    }
    if (finalize_effects.emit_owner_info_changed) {
      emit owner_info_changed();
    }
    if (finalize_effects.emit_spectator_mode_changed) {
      emit spectator_mode_changed();
    }
  });
}

void GameEngine::apply_mission_setup() {
  if (!m_world || !m_campaign_manager || !m_mission_setup || !m_skirmish_runtime) {
    return;
  }

  auto effects = m_mission_setup->apply_mission_setup({*m_world,
                                                       *m_campaign_manager,
                                                       m_level,
                                                       m_selected_player_id,
                                                       m_runtime.local_owner_id,
                                                       m_pending_mission_waves,
                                                       m_entity_cache});
  if (effects.selected_player_changed) {
    emit selected_player_id_changed();
  }
  if (effects.center_camera_on_local_forces) {
    m_skirmish_runtime->center_camera_on_local_forces(
        {m_world.get(), m_camera, m_runtime.local_owner_id});
  }
  if (effects.troop_count_changed) {
    emit troop_count_changed();
  }
  if (effects.owner_info_changed) {
    emit owner_info_changed();
  }
}

void GameEngine::configure_mission_victory_conditions() {
  if (!m_campaign_manager || !m_victory_service) {
    return;
  }

  m_campaign_manager->configure_mission_victory_conditions(m_victory_service.get(),
                                                           m_runtime.local_owner_id);

  m_victory_service->set_victory_callback([this](const QString& state) {
    if (m_runtime.victory_state != state) {
      m_audio_coordinator->ensure_result_audio_ready(state, m_runtime.local_owner_id);
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
  Game::Systems::MarketplaceSystem::instance().clear();
  sync_selected_player_state();
  m_audio_coordinator->stop_mission_ambience();
  AudioSystem::get_instance().stop_music();
  AudioResourceLoader::unload_audio_resources(AudioLoadPolicy::Mission);
  AudioResourceLoader::unload_audio_resources(AudioLoadPolicy::Lazy);
}

void GameEngine::update_mission_waves(float dt) {
  if (dt <= 0.0F || !m_world || !m_mission_setup || m_pending_mission_waves.empty() ||
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
    const auto effects = m_mission_setup->spawn_wave(
        {*m_world, m_level, m_campaign_mission_elapsed}, wave);
    for (const auto& announcement : effects.mission_announcements) {
      emit mission_announcement(announcement);
    }
    wave.spawned = true;
    spawned_any = true;
  }

  if (spawned_any) {
    emit owner_info_changed();
  }
}

void GameEngine::apply_skirmish_commander_setup(const QVariantList& player_configs) {
  if (!m_world || !m_mission_setup) {
    return;
  }

  const auto effects = m_mission_setup->apply_skirmish_commander_setup(
      {*m_world, m_campaign_manager.get(), m_level, m_runtime.local_owner_id},
      player_configs);
  for (const auto& announcement : effects.mission_announcements) {
    emit mission_announcement(announcement);
  }
}

void GameEngine::open_settings() {
  if (m_save_load_service) {
    m_save_load_service->open_settings();
  }
}

void GameEngine::load_save() {
  load_game_from_slot("savegame");
}

void GameEngine::save_game(const QString& filename) {
  save_game_to_slot(filename);
}

void GameEngine::save_game_to_slot(const QString& slot_name) {
  if (!m_save_load_service || !m_world) {
    set_error("Save: not initialized");
    return;
  }

  if (m_control_mode == PlayerControlMode::Commander) {
    request_exit_commander_control_mode();
  }
  const Game::Systems::RuntimeSnapshot runtime_snapshot = to_runtime_snapshot();
  const QByteArray screenshot = capture_screenshot();
  std::optional<Game::Mission::MissionContext> mission_context;
  if (m_campaign_manager) {
    mission_context = m_campaign_manager->current_mission_context();
  }

  const App::Core::SaveToSlotEffects effects = m_save_load_coordinator->save_to_slot(
      {.world = *m_world,
       .save_load_service = *m_save_load_service,
       .camera = m_camera,
       .level = m_level,
       .runtime_snapshot = runtime_snapshot,
       .slot = slot_name,
       .title = slot_name,
       .map_name = m_level.map_name,
       .screenshot = screenshot,
       .mission_context = std::move(mission_context)});
  if (!effects.success) {
    set_error(effects.error);
    return;
  }
  if (effects.emit_save_slots_changed) {
    emit save_slots_changed();
  }
}

void GameEngine::load_game_from_slot(const QString& slot_name) {
  if (!m_save_load_service || !m_world) {
    set_error("Load: not initialized");
    return;
  }

  if (m_control_mode == PlayerControlMode::Commander) {
    request_exit_commander_control_mode();
  }

  reset_preload_interaction_state();
  reset_mission_runtime_state();

  m_finalize_progress_after_overlay = false;
  m_loading_overlay_active = true;
  m_runtime.loading = true;
  emit is_loading_changed();

  Game::Systems::RuntimeSnapshot runtime_snapshot = to_runtime_snapshot();
  const App::Core::LoadFromSlotEffects effects =
      m_save_load_coordinator->load_from_slot(
          {.world = *m_world,
           .save_load_service = *m_save_load_service,
           .slot = slot_name,
           .campaign_manager = m_campaign_manager.get(),
           .level = m_level,
           .camera = m_camera,
           .viewport_width = m_viewport.width,
           .viewport_height = m_viewport.height,
           .runtime_snapshot = runtime_snapshot,
           .apply_runtime_snapshot =
               [this](const Game::Systems::RuntimeSnapshot& snapshot) {
                 apply_runtime_snapshot(snapshot);
               },
           .selected_player_id = m_selected_player_id,
           .scene = scene_context(),
           .entity_cache = m_entity_cache,
           .audio_coordinator = m_audio_coordinator.get(),
           .victory_service = m_victory_service.get(),
           .emit_troop_count_changed =
               [this]() {
                 emit troop_count_changed();
               }});
  if (!effects.success) {
    set_error(effects.error);
    m_runtime.loading = false;
    m_loading_overlay_active = false;
    m_loading_overlay_wait_for_first_frame.store(false, std::memory_order_release);
    m_finalize_progress_after_overlay = false;
    m_show_objectives_after_loading = false;
    emit is_loading_changed();
    return;
  }

  m_runtime.loading = false;
  m_loading_overlay_wait_for_first_frame.store(true, std::memory_order_release);
  m_loading_overlay_frames_remaining = 5;
  m_loading_overlay_min_duration_ms = 1000;
  m_loading_overlay_timer.restart();
  m_finalize_progress_after_overlay = true;
  emit is_loading_changed();
  qInfo() << "Game load complete, victory/defeat checks re-enabled";

  if (effects.emit_selected_units_changed) {
    emit selected_units_changed();
  }
  if (effects.emit_owner_info_changed) {
    emit owner_info_changed();
  }
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
  return m_save_load_coordinator->to_runtime_snapshot(
      {.paused = m_runtime.paused,
       .time_scale = m_runtime.time_scale,
       .local_owner_id = m_runtime.local_owner_id,
       .victory_state = m_runtime.victory_state,
       .cursor_mode = m_runtime.cursor_mode,
       .selected_player_id = m_selected_player_id,
       .follow_selection = m_follow_selection_enabled});
}

void GameEngine::apply_runtime_snapshot(
    const Game::Systems::RuntimeSnapshot& snapshot) {
  m_save_load_coordinator->apply_runtime_snapshot(
      snapshot,
      {.paused = m_runtime.paused,
       .time_scale = m_runtime.time_scale,
       .local_owner_id = m_runtime.local_owner_id,
       .victory_state = m_runtime.victory_state,
       .cursor_mode = m_runtime.cursor_mode,
       .selected_player_id = m_selected_player_id,
       .follow_selection = m_follow_selection_enabled});
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

  m_scatter->refresh_runtime_world_props(terrain_service.world_props());
  m_last_world_props_revision = revision;
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
        request_exit_commander_control_mode();
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
