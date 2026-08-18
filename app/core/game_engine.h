#pragma once

#include <QElapsedTimer>
#include <QJsonObject>
#include <QList>
#include <QMatrix4x4>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QStringList>
#include <QTimer>
#include <QVariant>
#include <QVector3D>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "app/core/app_scene_context.h"
#include "app/core/client_context.h"
#include "app/core/entity_cache.h"
#include "app/core/match_presentation_sync.h"
#include "app/core/runtime_frame_orchestrator.h"
#include "app/economy/economy_overview.h"
#include "app/input/cursor_manager.h"
#include "app/input/cursor_mode.h"
#include "app/input/hover_tracker.h"
#include "app/input/input_command_handler.h"
#include "app/input/rts_camera_controller.h"
#include "app/mission/mission_wave_runtime.h"
#include "app/mission/tutorial_observation.h"
#include "app/models/selected_units_model.h"
#include "app/orders/movement_utils.h"
#include "app/orders/order_feedback.h"
#include "app/orders/order_markers.h"
#include "app/persistence/save_load_coordinator.h"
#include "app/session/renderer_bootstrap.h"
#include "app/utils/engine_view_helpers.h"
#include "app/viewmodels/activity_view_model.h"
#include "app/viewmodels/camera_view_model.h"
#include "app/viewmodels/commander_message_view_model.h"
#include "app/viewmodels/commander_view_model.h"
#include "app/viewmodels/economy_view_model.h"
#include "app/viewmodels/match_setup_view_model.h"
#include "app/viewmodels/minimap_view_model.h"
#include "app/viewmodels/mission_view_model.h"
#include "app/viewmodels/orders_view_model.h"
#include "app/viewmodels/placement_view_model.h"
#include "app/viewmodels/production_view_model.h"
#include "app/viewmodels/wave_view_model.h"
#include "app/world/ambient_state_manager.h"
#include "app/world/focus_target.h"
#include "app/world/minimap_manager.h"
#include "game/audio/audio_event_handler.h"
#include "game/command/command.h"
#include "game/command/replay.h"
#include "game/core/event_manager.h"
#include "game/map/mission_definition.h"
#include "game/map/mission_stage_tracker.h"
#include "game/mission/commander_message_director.h"
#include "game/mission/mission_setup_coordinator.h"
#include "game/mission/mission_wave_director.h"
#include "game/mission/mission_waves.h"
#include "game/mission/tutorial_director.h"
#include "game/render_bridge/selection_controller.h"
#include "game/session/session_context.h"
#include "game/systems/attack_range.h"
#include "game/systems/attack_targeting.h"
#include "game/systems/interaction_targeting.h"
#include "game/systems/match_snapshot.h"
#include "game/systems/save_format.h"
#include "game/systems/target_focus.h"
#include "game/systems/unit_activity.h"
#include "game/util/selection_utils.h"

class ProductionManager;
class CampaignManager;
class SelectionQueryService;
class VisibilityCoordinator;
class AudioCoordinator;

namespace Engine::Core {
class World;
using EntityID = std::uint64_t;
class MovementComponent;
class TransformComponent;
class RenderableComponent;
} // namespace Engine::Core

namespace Render::GL {
class Renderer;
class Camera;
class TerrainSceneProxy;
class TerrainSurfaceManager;
class TerrainFeatureManager;
class TerrainScatterManager;
class ResourceManager;
class FogRenderer;
class MapBoundaryFogRenderer;
class AmbientFogRenderer;
class RainRenderer;
} // namespace Render::GL

namespace Game {
namespace Map::Minimap {
class UnitLayer;
}
namespace Systems {
class SelectionSystem;
class SelectionController;
class ArrowSystem;
class PickingService;
class VictoryService;
class CameraService;
class SaveLoadService;
class RainManager;
} // namespace Systems
namespace Map {
class EnvironmentClock;
class MapCatalog;
struct MapDefinition;
} // namespace Map
} // namespace Game

namespace App {
namespace ViewModels {
class SaveSlotsViewModel;
}
namespace Controllers {
class CommandController;
}
namespace Core {
class SkirmishRuntimeCoordinator;
class WeatherAudio;
} // namespace Core
namespace Models {
class AudioSystemProxy;
}
} // namespace App

class QQuickWindow;
class LoadingProgressTracker;

class GameEngine : public QObject, private App::Core::ClientHost {
  Q_OBJECT
public:
  explicit GameEngine(QObject* parent = nullptr);
  ~GameEngine() override;

  void cleanup_opengl_resources();

  Q_PROPERTY(QAbstractItemModel* selected_units_model READ selected_units_model NOTIFY
                 selected_units_changed)
  Q_PROPERTY(bool paused READ paused WRITE set_paused)
  Q_PROPERTY(
      float time_scale READ time_scale WRITE set_game_speed NOTIFY time_scale_changed)
  Q_PROPERTY(QString victory_state READ victory_state NOTIFY victory_state_changed)
  Q_PROPERTY(QString cursor_mode READ cursor_mode WRITE set_cursor_mode NOTIFY
                 cursor_mode_changed)
  Q_PROPERTY(qreal global_cursor_x READ global_cursor_x NOTIFY global_cursor_changed)
  Q_PROPERTY(qreal global_cursor_y READ global_cursor_y NOTIFY global_cursor_changed)
  Q_PROPERTY(
      bool has_units_selected READ has_units_selected NOTIFY selected_units_changed)
  Q_PROPERTY(
      int max_troops_per_player READ max_troops_per_player NOTIFY troop_count_changed)
  Q_PROPERTY(QVariantMap selected_player_state READ selected_player_state NOTIFY
                 selected_player_state_changed)
  Q_PROPERTY(int enemy_troops_defeated READ enemy_troops_defeated NOTIFY
                 enemy_troops_defeated_changed)
  Q_PROPERTY(QVariantList owner_info READ get_owner_info NOTIFY owner_info_changed)

  Q_PROPERTY(
      QString local_player_nation READ local_player_nation NOTIFY owner_info_changed)
  Q_PROPERTY(int selected_player_id READ selected_player_id WRITE set_selected_player_id
                 NOTIFY selected_player_id_changed)
  Q_PROPERTY(QString last_error READ last_error NOTIFY last_error_changed)
  Q_PROPERTY(QObject* audio_system READ audio_system CONSTANT)
  Q_PROPERTY(
      bool is_spectator_mode READ is_spectator_mode NOTIFY spectator_mode_changed)
  Q_PROPERTY(bool is_loading READ is_loading NOTIFY is_loading_changed)
  Q_PROPERTY(
      float loading_progress READ loading_progress NOTIFY loading_progress_changed)
  Q_PROPERTY(
      QString loading_stage_text READ loading_stage_text NOTIFY loading_stage_changed)
  Q_PROPERTY(QObject* camera READ camera_view_model CONSTANT)
  Q_PROPERTY(QObject* setup READ match_setup_view_model CONSTANT)
  Q_PROPERTY(QObject* production READ production_view_model CONSTANT)
  Q_PROPERTY(QObject* orders READ orders_view_model CONSTANT)
  Q_PROPERTY(QObject* commander READ commander_view_model CONSTANT)
  Q_PROPERTY(QObject* minimap READ minimap_view_model CONSTANT)
  Q_PROPERTY(QObject* saves READ save_slots_view_model CONSTANT)
  Q_PROPERTY(QObject* placement READ placement_view_model CONSTANT)
  Q_PROPERTY(QObject* waves READ wave_view_model CONSTANT)
  Q_PROPERTY(QObject* mission READ mission_view_model CONSTANT)
  Q_PROPERTY(QObject* commander_message READ commander_message_view_model CONSTANT)
  Q_PROPERTY(QObject* activity READ activity_view_model CONSTANT)
  Q_PROPERTY(QObject* economy READ economy_view_model CONSTANT)
  Q_PROPERTY(QObject* tutorial READ tutorial_view_model CONSTANT)

  Q_INVOKABLE void set_audio_frontend_context(const QString& context);

  Q_INVOKABLE void set_paused(bool paused);
  Q_INVOKABLE void set_game_speed(float speed);
  [[nodiscard]] bool paused() const { return m_runtime.paused; }
  [[nodiscard]] float time_scale() const { return m_runtime.time_scale; }
  [[nodiscard]] auto dropped_simulation_ticks() const -> std::uint64_t {
    return m_dropped_simulation_ticks;
  }
  [[nodiscard]] QString victory_state() const { return m_runtime.victory_state; }
  [[nodiscard]] QString cursor_mode() const;
  void set_cursor_mode(const QString& mode);
  [[nodiscard]] qreal global_cursor_x() const;
  [[nodiscard]] qreal global_cursor_y() const;
  [[nodiscard]] bool has_units_selected() const;
  [[nodiscard]] int player_troop_count() const;
  [[nodiscard]] int max_troops_per_player() const {
    return m_level.max_troops_per_player;
  }
  [[nodiscard]] int enemy_troops_defeated() const;
  [[nodiscard]] QVariantMap selected_player_state() const;

  Q_INVOKABLE [[nodiscard]] static QVariantMap get_player_stats(int owner_id);

  [[nodiscard]] int selected_player_id() const { return m_selected_player_id; }
  void set_selected_player_id(int id);
  [[nodiscard]] QString last_error() const { return m_runtime.last_error; }
  Q_INVOKABLE void clear_error() {
    if (!m_runtime.last_error.isEmpty()) {
      m_runtime.last_error = "";
      emit last_error_changed();
    }
  }

  void set_replay_record_path(const QString& path);
  auto start_replay(const QString& path) -> bool;
  [[nodiscard]] auto replay_playing() const -> bool;

  void set_replay_verify_exit(bool enabled) { m_replay_verify_exit = enabled; }
  Q_INVOKABLE void open_settings();
  [[nodiscard]] QObject* camera_view_model() const;
  [[nodiscard]] QObject* match_setup_view_model() const;
  [[nodiscard]] QObject* production_view_model() const;
  [[nodiscard]] QObject* orders_view_model() const;
  void launch_match(const App::Core::MatchLaunch& launch);

  [[nodiscard]] App::ViewModels::MatchSetupViewModel* match_setup() const {
    return m_match_setup_view_model.get();
  }
  [[nodiscard]] App::ViewModels::MinimapViewModel* minimap_model() const {
    return m_minimap_view_model.get();
  }
  [[nodiscard]] QObject* commander_view_model() const;
  [[nodiscard]] QObject* minimap_view_model() const;
  [[nodiscard]] QObject* save_slots_view_model() const;
  [[nodiscard]] QObject* placement_view_model() const;
  [[nodiscard]] QObject* wave_view_model() const;
  [[nodiscard]] QObject* commander_message_view_model() const;
  [[nodiscard]] QObject* mission_view_model() const;
  [[nodiscard]] QObject* tutorial_view_model() const;
  [[nodiscard]] QObject* activity_view_model() const;
  [[nodiscard]] QObject* economy_view_model() const;

  Q_INVOKABLE void exit_game();
  Q_INVOKABLE [[nodiscard]] QVariantList get_owner_info() const;
  [[nodiscard]] QString local_player_nation() const;
  [[nodiscard]] bool is_spectator_mode() const { return m_level.is_spectator_mode; }

  [[nodiscard]] bool is_loading() const {
    return m_runtime.loading || m_loading_overlay_active;
  }

  [[nodiscard]] float loading_progress() const;
  [[nodiscard]] QString loading_stage_text() const;

  [[nodiscard]] bool release_self_test_mission_ready() const;

  [[nodiscard]] QString release_self_test_pending_reason() const;

  QObject* audio_system();

  void setWindow(QQuickWindow* w) {
    m_window = w;
    publish_client_context();
  }

  [[nodiscard]] bool consume_screenshot_request();
  void submit_frame_image(const QImage& image);

  void ensure_initialized() override;
  [[nodiscard]] bool renderer_initialized() const { return m_runtime.initialized; }
  void set_release_self_test_mode(bool enabled) noexcept {
    m_release_self_test_mode = enabled;
  }
  void update(float dt);
  void render(int pixel_width, int pixel_height);
  void set_input_viewport_size(qreal width, qreal height);

  [[nodiscard]] auto try_begin_render_frame() -> bool;
  void end_render_frame();

  class WorldFreeze {
  public:
    explicit WorldFreeze(GameEngine& engine);
    ~WorldFreeze();

    WorldFreeze(const WorldFreeze&) = delete;
    auto operator=(const WorldFreeze&) -> WorldFreeze& = delete;
    WorldFreeze(WorldFreeze&&) = delete;
    auto operator=(WorldFreeze&&) -> WorldFreeze& = delete;

  private:
    GameEngine& m_engine;
  };

private:
  struct RuntimeState {
    bool initialized = false;
    bool paused = false;
    bool loading = false;
    float time_scale = 1.0F;
    int local_owner_id = 1;
    QString victory_state = "";
    CursorMode cursor_mode{CursorMode::Normal};
    QString last_error = "";
    Qt::CursorShape current_cursor = Qt::ArrowCursor;
    int last_troop_count = 0;
    qreal last_cursor_x = -1.0;
    qreal last_cursor_y = -1.0;
    int selection_refresh_counter = 0;
    float minimap_unit_update_accumulator = 0.0F;
  };
  using PendingMissionWave = Game::Mission::PendingMissionWave;
  using PendingMissionEvent = Game::Mission::PendingMissionEvent;
  using MissionWaveDirector = Game::Mission::MissionWaveDirector;
  bool screen_to_ground(const QPointF& screen_pt, QVector3D& out_world);
  bool world_to_screen(const QVector3D& world, QPointF& out_screen) const;
  void note_dropped_simulation_ticks(std::uint64_t dropped, float real_dt);
  void update_active_runtime_simulation(float dt);
  void sync_selection_flags();
  void sync_attack_targeting();
  void sync_interaction_targeting(float delta_time);
  void sync_attack_range_rings();
  [[nodiscard]] auto
  attack_sync_context() const -> App::Core::PresentationSync::SelectionAttackContext;
  void handle_order_feedback(const App::Core::OrderOutcome& outcome);
  void sync_selected_player_state();
  void sync_economy_state();
  [[nodiscard]] auto
  mission_objective_resources() const -> Game::Systems::ResourceAmounts;
  void reset_economy_coach();
  void sync_focus_targets();
  void sync_target_focus_markers();
  [[nodiscard]] auto
  describe_focus_entity(Engine::Core::EntityID id) const -> App::Core::FocusTargetInfo;
  void sync_scatter_world_props();
  QAbstractItemModel* selected_units_model();
  void on_unit_spawned(const Engine::Core::UnitSpawnedEvent& event);
  void on_unit_died(const Engine::Core::UnitDiedEvent& event);

  void build_client_and_view_models();
  void build_services_and_controllers();
  void update_cursor(Qt::CursorShape new_cursor);
  void set_error(const QString& error_message);
  [[nodiscard]] Game::Systems::RuntimeSnapshot to_runtime_snapshot() const;
  void apply_runtime_snapshot(const Game::Systems::RuntimeSnapshot& snapshot);
  [[nodiscard]] AppSceneContext scene_context() const;
  struct ReplayLaunch {
    QString kind;
    QString reference;
    QVariantList player_configs;
  };
  void arm_replay_for_started_match();
  void finish_replay_verification_if_done();
  bool m_replay_verify_exit = false;
  ReplayLaunch m_replay_launch;
  QString m_replay_record_path;
  std::optional<Game::Command::ReplayFile> m_pending_replay;

  void start_skirmish_internal(const QString& map_path,
                               const QVariantList& player_configs,
                               bool set_skirmish_context);
  void apply_skirmish_commander_setup(const QVariantList& player_configs);
  void apply_mission_setup();
  void configure_mission_victory_conditions();
  void configure_rain_system();
  void reset_preload_interaction_state();
  void reset_mission_runtime_state();
  void update_mission_waves(float dt);
  [[nodiscard]] auto mission_wave_binding() -> App::Mission::MissionWaveBinding;
  void publish_wave_status();
  void configure_mission_stages();
  void configure_commander_messages();
  void update_commander_messages(float delta_time);
  void publish_commander_message();
  void publish_mission_stages();
  void update_mission_stages(float delta_time);
  void restore_mission_stages(const QJsonObject& stage_state);
  void restore_mission_waves(const QJsonObject& wave_state);
  void update_tutorial(float real_dt);
  void publish_tutorial_focus_points(const QVariantMap& wave_status);
  void activate_tutorial_if_configured();
  void update_loading_overlay();
  void update_cursor_position();
  void on_frame_image_captured(const QImage& image);
  void begin_save(const QString& slot_name,
                  Game::Systems::Save::SlotKind kind,
                  int autosave_retention);
  void connect_save_service_signals();
  void save_game_to_slot(const QString& slot_name);
  void quicksave();
  void autosave();
  void cancel_active_save();
  void load_game_from_slot(const QString& slot_name);
  void restart_autosave_timer();

  std::unique_ptr<App::ViewModels::CameraViewModel> m_camera_view_model;
  std::unique_ptr<App::ViewModels::MatchSetupViewModel> m_match_setup_view_model;
  std::unique_ptr<App::ViewModels::ProductionViewModel> m_production_view_model;
  std::unique_ptr<App::ViewModels::OrdersViewModel> m_orders_view_model;
  std::unique_ptr<App::ViewModels::MinimapViewModel> m_minimap_view_model;
  std::unique_ptr<App::ViewModels::CommanderViewModel> m_commander_view_model;
  std::unique_ptr<App::ViewModels::SaveSlotsViewModel> m_save_slots_view_model;
  std::unique_ptr<App::ViewModels::PlacementViewModel> m_placement_view_model;
  std::unique_ptr<App::ViewModels::WaveViewModel> m_wave_view_model;
  std::unique_ptr<App::ViewModels::CommanderMessageViewModel>
      m_commander_message_view_model;
  std::unique_ptr<App::ViewModels::MissionViewModel> m_mission_view_model;
  std::unique_ptr<App::ViewModels::ActivityViewModel> m_activity_view_model;
  std::unique_ptr<App::ViewModels::EconomyViewModel> m_economy_view_model;
  std::unique_ptr<Game::Mission::TutorialDirector> m_tutorial_director;

  void set_cursor_mode(CursorMode mode) override;

  void apply_game_mode_render_policy();
  void set_active_camera(Render::GL::Camera* camera);
  void get_selected_unit_ids(std::vector<Engine::Core::EntityID>& out) const;

  void publish_client_context();
  App::Core::ClientContext m_client;

  std::unique_ptr<Game::Session::SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_session_scope;
  Engine::Core::World* m_world = nullptr;
  std::unique_ptr<Render::GL::Renderer> m_renderer;
  std::unique_ptr<Render::GL::Camera> m_rts_camera;
  std::unique_ptr<Render::GL::Camera> m_commander_camera;
  Render::GL::Camera* m_camera = nullptr;
  std::unique_ptr<Render::GL::TerrainSceneProxy> m_terrain_scene;
  std::shared_ptr<Render::GL::ResourceManager> m_resources;
  std::unique_ptr<Render::GL::TerrainSurfaceManager> m_surface;
  std::unique_ptr<Render::GL::TerrainFeatureManager> m_features;
  std::unique_ptr<Render::GL::TerrainScatterManager> m_scatter;
  std::unique_ptr<Render::GL::FogRenderer> m_fog;
  std::unique_ptr<Render::GL::MapBoundaryFogRenderer> m_boundary_fog;
  std::unique_ptr<Render::GL::AmbientFogRenderer> m_ambient_fog;
  std::unique_ptr<Render::GL::RainRenderer> m_rain;
  std::unique_ptr<Game::Systems::RainManager> m_rain_manager;
  std::unique_ptr<App::Core::WeatherAudio> m_weather_audio;
  std::unique_ptr<Game::Map::EnvironmentClock> m_environment_clock;
  std::unique_ptr<Game::Systems::PickingService> m_picking_service;
  std::unique_ptr<Game::Systems::VictoryService> m_victory_service;
  Game::Systems::SaveLoadService* m_save_load_service = nullptr;
  std::unique_ptr<CursorManager> m_cursor_manager;
  std::unique_ptr<HoverTracker> m_hover_tracker;
  Game::Systems::AttackTargetingHighlights m_attack_targeting;
  Game::Systems::InteractionTargetingHighlights m_interaction_targeting;
  float m_interaction_targeting_accumulator = 0.0F;
  QVariantMap m_interaction_target_hint;
  std::vector<Game::Systems::AttackRangeRing> m_attack_range_rings;
  App::Core::OrderMarkerStore m_order_markers;
  std::vector<Game::Systems::TargetFocusMarker> m_target_focus;
  QVariantMap m_inspect_target;
  QVariantMap m_selection_target;
  QVariantMap m_attack_target_hint;
  std::unique_ptr<Game::Systems::CameraService> m_camera_service;
  std::unique_ptr<Game::Systems::SelectionController> m_selection_controller;
  std::unique_ptr<App::Controllers::CommandController> m_command_controller;
  std::unique_ptr<Game::Map::MapCatalog> m_map_catalog;
  std::unique_ptr<Game::Audio::AudioEventHandler> m_audio_event_handler;
  std::unique_ptr<AudioCoordinator> m_audio_coordinator;
  std::unique_ptr<Game::Mission::MissionSetupCoordinator> m_mission_setup;
  std::unique_ptr<App::Core::SaveLoadCoordinator> m_save_load_coordinator;
  std::unique_ptr<App::Core::SkirmishRuntimeCoordinator> m_skirmish_runtime;
  std::unique_ptr<App::Models::AudioSystemProxy> m_audio_systemProxy;
  QString m_audio_frontend_context;
  std::unique_ptr<MinimapManager> m_minimap_manager;
  std::unique_ptr<VisibilityCoordinator> m_visibility_coordinator;
  std::unique_ptr<AmbientStateManager> m_ambient_state_manager;
  std::unique_ptr<InputCommandHandler> m_input_handler;
  std::unique_ptr<RtsCameraController> m_camera_controller;
  std::unique_ptr<LoadingProgressTracker> m_loading_progress_tracker;
  std::unique_ptr<ProductionManager> m_production_manager;
  std::unique_ptr<CampaignManager> m_campaign_manager;
  std::unique_ptr<SelectionQueryService> m_selection_query_service;
  QVariantList m_catalogued_maps;
  QQuickWindow* m_window = nullptr;
  RuntimeState m_runtime;
  ViewportState m_viewport;
  bool m_release_self_test_mode = false;
  Game::Systems::LevelSnapshot m_level;
  SelectedUnitsModel* m_selected_units_model = nullptr;
  int m_enemy_troops_defeated = 0;
  int m_selected_player_id = 1;
  QVariantMap m_selected_player_state;
  QVariantList m_economy_resources;
  QVariantMap m_economy_help;
  QVariantMap m_economy_coach;
  App::Core::EconomyCoachBaseline m_economy_coach_baseline;
  bool m_economy_coach_available = false;
  QElapsedTimer m_economy_refresh_timer;
  std::uint64_t m_last_world_props_revision = 0;
  bool m_loading_overlay_active = false;
  std::atomic_bool m_loading_overlay_wait_for_first_frame{false};

  std::atomic<int> m_world_freeze_depth{0};
  std::atomic<bool> m_render_frame_active{false};
  int m_loading_overlay_frames_remaining = 0;
  qint64 m_loading_overlay_last_frame_ms = 0;
  qint64 m_loading_overlay_min_duration_ms = 0;
  QElapsedTimer m_loading_overlay_timer;
  bool m_finalize_progress_after_overlay = false;
  bool m_show_objectives_after_loading = false;
  quint64 m_active_save_job = 0;
  std::atomic_bool m_screenshot_requested{false};
  QString m_screenshot_target_slot;
  QString m_save_progress_slot;
  QTimer m_autosave_timer;
  Game::Mission::MissionStageTracker m_mission_stage_tracker;
  Game::Mission::CommanderMessageDirector m_commander_message_director;
  float m_mission_stage_poll_accumulator = 0.0F;
  App::Mission::MissionWaveRuntime m_mission_waves;
  App::Mission::TutorialFrameNotes m_tutorial_notes;
  float m_tutorial_observe_accumulator = 0.0F;
  Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>
      m_unit_died_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::UnitSpawnedEvent>
      m_unit_spawned_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::CombatHitEvent>
      m_combat_hit_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::MissionAnnouncementEvent>
      m_mission_announcement_subscription;

  EntityCache m_entity_cache;
  RuntimeFrameOrchestrator m_frame_orchestrator;
  static constexpr float k_dropped_tick_report_interval = 5.0F;
  std::uint64_t m_dropped_simulation_ticks{0};
  float m_dropped_tick_report_cooldown{0.0F};

signals:
  void renderer_initialized_changed();
  void selected_units_changed();
  void selected_units_data_changed();
  void enemy_troops_defeated_changed();
  void victory_state_changed();
  void time_scale_changed();
  void cursor_mode_changed();
  void global_cursor_changed();
  void troop_count_changed();
  void owner_info_changed();
  void selected_player_id_changed();
  void selected_player_state_changed();
  void last_error_changed();
  void spectator_mode_changed();
  void is_loading_changed();
  void loading_progress_changed(float progress);
  void loading_stage_changed(QString stage_text);
  void control_mode_changed();
  void game_mode_changed();
  void commander_control_available_changed();
  void mission_announcement(QString text);
  void order_feedback(QString kind, bool accepted, QString message);
  void autosave_settings_changed();
};
