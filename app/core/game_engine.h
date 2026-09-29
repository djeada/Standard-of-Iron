#pragma once

#include <QAbstractItemModel>
#include <QJsonObject>
#include <QObject>
#include <QPointF>
#include <QStringList>
#include <QVariant>
#include <QVector3D>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "app/core/app_scene_context.h"
#include "app/core/client_context.h"
#include "app/core/entity_cache.h"
#include "app/core/runtime_frame_orchestrator.h"
#include "app/core/simulation_lifecycle.h"
#include "app/input/cursor_mode.h"
#include "app/input/input_command_handler.h"
#include "app/session/loading_overlay.h"
#include "game/command/command_validator.h"
#include "game/core/event_manager.h"
#include "game/systems/match_snapshot.h"
#include "game/systems/persistence/save_format.h"
#include "scene/camera.h"

class ProductionManager;
class CampaignManager;
class SelectionQueryService;
class VisibilityCoordinator;
class MinimapManager;
class CursorManager;
class HoverTracker;
class RtsCameraController;
class LoadingProgressTracker;
class SelectedUnitsModel;
class QQuickWindow;

namespace Engine::Core {
class World;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Game {
namespace Command {
struct Command;
}
namespace Session {
class SessionContext;
class ScopedSession;
} // namespace Session
namespace Systems {
class PickingService;
class VictoryService;
class CameraService;
class SaveLoadService;
class SelectionController;
} // namespace Systems
namespace Map {
class MapCatalog;
}
namespace Mission {
struct MissionDefinition;
} // namespace Mission
} // namespace Game

namespace Render::GL {
class Renderer;
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

namespace App {
namespace Core {
struct PresentationFrame;
} // namespace Core
namespace ViewModels {
class CameraViewModel;
class MatchSetupViewModel;
class ProductionViewModel;
class OrdersViewModel;
class MinimapViewModel;
class CommanderViewModel;
class SaveSlotsViewModel;
class PlacementViewModel;
class WaveViewModel;
class CommanderMessageViewModel;
class MissionViewModel;
class ActivityViewModel;
class EconomyViewModel;
} // namespace ViewModels
namespace Controllers {
class CommandController;
}
namespace Core {
class AudioServices;
class EconomyReadModel;
class OrderFeedbackPresenter;
class SaveSlotController;
class SaveLoadCoordinator;
class SkirmishRuntimeCoordinator;
struct LoadFromSlotContext;
struct LoadFromSlotEffects;
struct PerformSkirmishLoadEffects;
struct OrderOutcome;
struct SaveToSlotEffects;
} // namespace Core
namespace Mission {
class CommanderMessageRuntime;
class MissionRuntime;
class TutorialRuntime;
struct CommanderMessageBinding;
struct MissionBinding;
} // namespace Mission
namespace Session {
class EnvironmentRuntime;
class ReplayCoordinator;
} // namespace Session
namespace World {
class AllyAnnouncementPresenter;
class BattleStats;
class FocusTracker;
class MinimapEvents;
class TargetingPresentation;
struct FocusInputs;
struct TargetingInputs;
} // namespace World
} // namespace App

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
  Q_PROPERTY(QString defeat_reason READ defeat_reason NOTIFY victory_state_changed)
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

  [[nodiscard]] QString defeat_reason() const { return m_runtime.defeat_reason; }
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

  Q_INVOKABLE [[nodiscard]] QVariantMap get_player_stats(int owner_id);

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

  void set_replay_verify_exit(bool enabled);
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
    return m_runtime.loading || m_loading_overlay.active();
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
  [[nodiscard]] auto lock_frame() -> std::unique_lock<std::recursive_mutex> override {
    return m_lifecycle.lock_frame();
  }

  using FrameLockStats = App::Core::FrameLockStats;

  [[nodiscard]] auto simulation_profile_report() -> QJsonObject;

  [[nodiscard]] auto frame_lock_stats() const -> const FrameLockStats& {
    return m_lifecycle.stats();
  }
  [[nodiscard]] bool renderer_initialized() const { return m_runtime.initialized; }
  [[nodiscard]] auto commander_message_speakers() const -> const QStringList&;
  void set_release_self_test_mode(bool enabled) noexcept {
    m_release_self_test_mode = enabled;
  }
  void simulate(float dt);

  void film_step(float dt);
  void update_presentation(float dt);
  void publish_presentation_frame();
  void publish_frame_snapshots();
  void capture_render_selection();
  void update(float dt);
  void render(int pixel_width, int pixel_height);
  void set_input_viewport_size(qreal width, qreal height);

  [[nodiscard]] auto try_begin_render_frame() -> bool;
  void end_render_frame();

  void start_simulation_thread();
  void stop_simulation_thread();
  [[nodiscard]] auto simulation_thread_running() const -> bool {
    return m_lifecycle.running();
  }
  [[nodiscard]] auto take_simulation_tick_us() -> std::uint64_t {
    return m_lifecycle.take_tick_us();
  }
  [[nodiscard]] auto try_begin_simulation_tick() -> bool;
  void end_simulation_tick();

  class WorldFreeze {
  public:
    explicit WorldFreeze(GameEngine& engine);
    ~WorldFreeze();

    WorldFreeze(const WorldFreeze&) = delete;
    auto operator=(const WorldFreeze&) -> WorldFreeze& = delete;
    WorldFreeze(WorldFreeze&&) = delete;
    auto operator=(WorldFreeze&&) -> WorldFreeze& = delete;

    [[nodiscard]] auto acquired() const noexcept -> bool { return m_acquired; }
    explicit operator bool() const noexcept { return m_acquired; }

  private:
    GameEngine& m_engine;
    bool m_acquired = false;
  };

  [[nodiscard]] auto world_freeze_refusals() const noexcept -> int {
    return m_lifecycle.barrier().refusals();
  }

  [[nodiscard]] auto player_feedback() -> App::Core::PlayerFeedbackBus& {
    return m_player_feedback;
  }

  [[nodiscard]] auto last_save_capture_us() const -> std::uint64_t;

private:
  struct RuntimeState {
    bool initialized = false;
    bool paused = false;
    bool loading = false;
    float time_scale = 1.0F;
    int local_owner_id = 1;
    QString victory_state = "";
    QString defeat_reason = "";
    CursorMode cursor_mode{CursorMode::Normal};
    QString last_error = "";
    Qt::CursorShape current_cursor = Qt::ArrowCursor;
    int last_troop_count = 0;
    qreal last_cursor_x = -1.0;
    qreal last_cursor_y = -1.0;
    int selection_refresh_counter = 0;
    float minimap_unit_update_accumulator = 0.0F;
  };

  void build_client_and_view_models();
  void build_services_and_controllers();

  void create_session();
  void create_view_models();
  void create_feature_runtimes();
  void create_render_services();
  void wire_view_models();
  void wire_victory_service();
  void on_match_outcome(const QString& state);
  void route_combat_hit(const Engine::Core::CombatHitEvent& event);
  void wire_loading_and_saves();
  void wire_world_services();
  void wire_map_catalog();
  void wire_audio();
  void wire_production_and_placement();
  void wire_command_controller();
  void wire_selection_and_cursor();
  void wire_event_subscriptions();
  [[nodiscard]] auto can_inspect_entity(Engine::Core::EntityID id) const -> bool;

  void run_simulation_tick(float dt);
  void note_dropped_simulation_ticks(std::uint64_t dropped, float real_dt);
  void update_active_runtime_simulation(float dt);
  void advance_frame_orchestrator(float dt);
  void update_control_presentation(float dt);
  void sync_render_camera();
  [[nodiscard]] static auto world_freeze_refused_message() -> QString;

  bool screen_to_ground(const QPointF& screen_pt, QVector3D& out_world);
  void sync_selection_flags();
  void sync_target_presentation(float dt);
  [[nodiscard]] auto targeting_inputs() -> App::World::TargetingInputs;
  [[nodiscard]] auto focus_inputs() -> App::World::FocusInputs;

  void apply_presentation_camera(const App::Core::PresentationFrame& presentation);
  void prewarm_overlay_gpu_resources();
  void render_effects_pass(const App::Core::PresentationFrame& presentation);

  void handle_order_feedback(const App::Core::OrderOutcome& outcome);
  void report_late_command_rejection(const Game::Command::Command& command,
                                     Game::Command::Rejection reason);
  void announce_player_defeats(float dt);

  void sync_selected_player_state();
  void sync_economy_state();
  void sync_scatter_world_props();
  QAbstractItemModel* selected_units_model();
  void on_unit_spawned(const Engine::Core::UnitSpawnedEvent& event);
  void on_unit_died(const Engine::Core::UnitDiedEvent& event);

  void set_error(const QString& error_message);
  [[nodiscard]] Game::Systems::RuntimeSnapshot to_runtime_snapshot() const;
  void apply_runtime_snapshot(const Game::Systems::RuntimeSnapshot& snapshot);
  [[nodiscard]] AppSceneContext scene_context() const;

  void start_skirmish_internal(const QString& map_path,
                               const QVariantList& player_configs,
                               bool set_skirmish_context);
  void reset_match_outcome();
  void begin_match_loading(const QString& map_path);
  void complete_match_load(const QString& map_path, const QVariantList& player_configs);
  [[nodiscard]] auto load_match_world(const QString& map_path,
                                      const QVariantList& player_configs)
      -> App::Core::PerformSkirmishLoadEffects;
  void configure_loaded_match(const QString& map_path,
                              const QVariantList& player_configs,
                              const QVariantList& resolved_player_configs);
  void finalize_match_load();
  void fail_loading(const QString& error);
  void apply_mission_setup();
  void apply_skirmish_commander_setup(const QVariantList& player_configs);
  [[nodiscard]] auto mission_startup_pending_components() const -> QStringList;
  void configure_mission_victory_conditions();
  void reset_preload_interaction_state();
  void reset_mission_runtime_state();
  [[nodiscard]] auto
  current_mission_definition() const -> const Game::Mission::MissionDefinition*;
  [[nodiscard]] auto
  authored_mission_definition() const -> const Game::Mission::MissionDefinition*;

  [[nodiscard]] auto mission_binding() -> App::Mission::MissionBinding;
  [[nodiscard]] auto commander_binding() -> App::Mission::CommanderMessageBinding;
  void update_mission_waves(float dt);
  void update_mission_stages(float delta_time);
  void configure_mission_stages();
  void publish_wave_status();
  void update_commander_messages(float delta_time);
  void update_tutorial(float real_dt);

  void restore_mission_stages(const QJsonObject& stage_state);
  void restore_mission_waves(const QJsonObject& wave_state);
  void restore_commander_message_state(const QJsonObject& state);
  void restore_tutorial_state(const QJsonObject& state);
  void restore_battle_stats(const QJsonObject& state);

  void update_loading_overlay();
  void update_cursor_position();

  [[nodiscard]] auto
  capture_save_to_slot(const QString& slot_name,
                       Game::Systems::Save::SlotKind kind,
                       int autosave_retention) -> App::Core::SaveToSlotEffects;
  void load_game_from_slot(const QString& slot_name);
  [[nodiscard]] auto load_request(const QString& slot_name,
                                  Game::Systems::RuntimeSnapshot& runtime_snapshot)
      -> App::Core::LoadFromSlotContext;
  void begin_load_transition();
  void finish_successful_load(const QString& slot_name,
                              const App::Core::LoadFromSlotEffects& effects);
  void restore_environment_after_load();
  void release_loading_overlay_after_load();
  void end_match_after_failed_load();

  void set_cursor_mode(CursorMode mode) override;

  void apply_game_mode_render_policy();
  void set_active_camera(Render::GL::Camera* camera);
  void get_selected_unit_ids(std::vector<Engine::Core::EntityID>& out) const;

  void publish_client_context();
  App::Core::ClientContext m_client;

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

  std::unique_ptr<Game::Session::SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_session_scope;
  Engine::Core::World* m_world = nullptr;
  std::vector<Engine::Core::EntityID> m_selected_render_ids;
  std::vector<Engine::Core::EntityID> m_scratch_selected_ids;
  std::unique_ptr<Render::GL::Renderer> m_renderer;
  std::unique_ptr<Render::GL::Camera> m_rts_camera;
  std::unique_ptr<Render::GL::Camera> m_commander_camera;
  Render::GL::Camera* m_camera = nullptr;
  Render::GL::Camera m_render_camera;
  std::shared_ptr<const App::Core::PresentationFrame> m_presentation_frame;
  std::vector<Engine::Core::EntityID> m_drawn_selected_ids;
  std::unique_ptr<Render::GL::TerrainSceneProxy> m_terrain_scene;
  std::shared_ptr<Render::GL::ResourceManager> m_resources;
  std::unique_ptr<Render::GL::TerrainSurfaceManager> m_surface;
  std::unique_ptr<Render::GL::TerrainFeatureManager> m_features;
  std::unique_ptr<Render::GL::TerrainScatterManager> m_scatter;
  std::unique_ptr<Render::GL::FogRenderer> m_fog;
  std::unique_ptr<Render::GL::MapBoundaryFogRenderer> m_boundary_fog;
  std::unique_ptr<Render::GL::AmbientFogRenderer> m_ambient_fog;
  std::unique_ptr<Render::GL::RainRenderer> m_rain;
  std::unique_ptr<App::Session::EnvironmentRuntime> m_environment;
  std::unique_ptr<Game::Systems::PickingService> m_picking_service;
  std::unique_ptr<Game::Systems::VictoryService> m_victory_service;
  Game::Systems::SaveLoadService* m_save_load_service = nullptr;
  std::unique_ptr<CursorManager> m_cursor_manager;
  std::unique_ptr<HoverTracker> m_hover_tracker;
  App::Core::PlayerFeedbackBus m_player_feedback;
  std::unique_ptr<App::Core::OrderFeedbackPresenter> m_order_feedback;
  std::unique_ptr<App::World::TargetingPresentation> m_targeting;
  std::unique_ptr<App::World::FocusTracker> m_focus;
  std::unique_ptr<Game::Systems::CameraService> m_camera_service;
  std::unique_ptr<Game::Systems::SelectionController> m_selection_controller;
  std::unique_ptr<App::Controllers::CommandController> m_command_controller;
  std::unique_ptr<Game::Map::MapCatalog> m_map_catalog;
  std::unique_ptr<App::Core::AudioServices> m_audio;
  std::unique_ptr<App::Core::SaveLoadCoordinator> m_save_load_coordinator;
  std::unique_ptr<App::Core::SaveSlotController> m_saves;
  std::unique_ptr<App::Core::SkirmishRuntimeCoordinator> m_skirmish_runtime;
  std::unique_ptr<MinimapManager> m_minimap_manager;
  std::unique_ptr<App::World::MinimapEvents> m_minimap_events;
  std::unique_ptr<VisibilityCoordinator> m_visibility_coordinator;
  std::unique_ptr<InputCommandHandler> m_input_handler;
  std::unique_ptr<RtsCameraController> m_camera_controller;
  std::unique_ptr<LoadingProgressTracker> m_loading_progress_tracker;
  std::unique_ptr<ProductionManager> m_production_manager;
  std::unique_ptr<CampaignManager> m_campaign_manager;
  std::unique_ptr<SelectionQueryService> m_selection_query_service;
  std::unique_ptr<App::Session::ReplayCoordinator> m_replay;
  std::unique_ptr<App::Mission::MissionRuntime> m_mission;
  std::unique_ptr<App::Mission::CommanderMessageRuntime> m_commander_messages;
  std::unique_ptr<App::Mission::TutorialRuntime> m_tutorial;
  std::unique_ptr<App::Core::EconomyReadModel> m_economy;
  std::unique_ptr<App::World::BattleStats> m_battle_stats;
  std::unique_ptr<App::World::AllyAnnouncementPresenter> m_ally_announcements;
  QQuickWindow* m_window = nullptr;
  RuntimeState m_runtime;
  ViewportState m_viewport;
  bool m_release_self_test_mode = false;
  Game::Systems::LevelSnapshot m_level;
  SelectedUnitsModel* m_selected_units_model = nullptr;
  int m_selected_player_id = 1;

  App::Session::LoadingOverlay m_loading_overlay;
  App::Core::SimulationLifecycle m_lifecycle;
  std::atomic<float> m_simulation_time_scale{0.0F};

  Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>
      m_unit_died_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::UnitSpawnedEvent>
      m_unit_spawned_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::CombatHitEvent>
      m_combat_hit_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::WorldFeedbackEvent>
      m_world_feedback_subscription;
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
  void player_defeated(QString text, bool ally, int owner_id);
  void ally_exchange(QString text, bool positive);
  void ally_appeal_opened(QVariantMap appeal);
  void ally_appeal_closed(quint32 appeal_id);
  void order_feedback(QString kind, bool accepted, QString message, QString failure);
  void autosave_settings_changed();

  void match_ended();
};
