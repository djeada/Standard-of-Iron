#pragma once

#include <QElapsedTimer>
#include <QJsonObject>
#include <QList>
#include <QMatrix4x4>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QStringList>
#include <QVariant>
#include <QVector3D>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "../models/cursor_manager.h"
#include "../models/cursor_mode.h"
#include "../models/hover_tracker.h"
#include "../models/selected_units_model.h"
#include "../utils/engine_view_helpers.h"
#include "../utils/movement_utils.h"
#include "../utils/selection_utils.h"
#include "ambient_state_manager.h"
#include "app_scene_context.h"
#include "camera_controller.h"
#include "commander_control_controller.h"
#include "commander_input_adapter.h"
#include "entity_cache.h"
#include "game/audio/audio_event_handler.h"
#include "game/core/event_manager.h"
#include "game/map/mission_definition.h"
#include "game/systems/game_state_serializer.h"
#include "input_command_handler.h"
#include "minimap_manager.h"
#include "render/entity/combat_dust_renderer.h"
#include "renderer_bootstrap.h"
#include "runtime_frame_orchestrator.h"

class ProductionManager;
class CampaignManager;
class SelectionQueryService;
class VisibilityCoordinator;

namespace Engine::Core {
class World;
using EntityID = unsigned int;
struct MovementComponent;
struct TransformComponent;
struct RenderableComponent;
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
class MapCatalog;
struct MapDefinition;
} // namespace Map
} // namespace Game

namespace App {
namespace Controllers {
class CommandController;
}
namespace Models {
class AudioSystemProxy;
}
} // namespace App

class QQuickWindow;
class LoadingProgressTracker;

class GameEngine : public QObject {
  Q_OBJECT
public:
  explicit GameEngine(QObject* parent = nullptr);
  ~GameEngine() override;

  void cleanup_opengl_resources();

  Q_INVOKABLE void load_campaigns();

  Q_PROPERTY(QAbstractItemModel* selected_units_model READ selected_units_model NOTIFY
                 selected_units_changed)
  Q_PROPERTY(bool paused READ paused WRITE set_paused)
  Q_PROPERTY(float time_scale READ time_scale WRITE set_game_speed)
  Q_PROPERTY(QString victory_state READ victory_state NOTIFY victory_state_changed)
  Q_PROPERTY(QString cursor_mode READ cursor_mode WRITE set_cursor_mode NOTIFY
                 cursor_mode_changed)
  Q_PROPERTY(qreal global_cursor_x READ global_cursor_x NOTIFY global_cursor_changed)
  Q_PROPERTY(qreal global_cursor_y READ global_cursor_y NOTIFY global_cursor_changed)
  Q_PROPERTY(
      bool has_units_selected READ has_units_selected NOTIFY selected_units_changed)
  Q_PROPERTY(int player_troop_count READ player_troop_count NOTIFY troop_count_changed)
  Q_PROPERTY(
      int max_troops_per_player READ max_troops_per_player NOTIFY troop_count_changed)
  Q_PROPERTY(QVariantMap selected_player_state READ selected_player_state NOTIFY
                 selected_player_state_changed)
  Q_PROPERTY(
      QVariantList available_maps READ available_maps NOTIFY available_maps_changed)
  Q_PROPERTY(bool maps_loading READ maps_loading NOTIFY maps_loading_changed)
  Q_PROPERTY(QVariantList available_nations READ available_nations CONSTANT)
  Q_PROPERTY(QVariantList available_campaigns READ available_campaigns NOTIFY
                 available_campaigns_changed)
  Q_PROPERTY(int enemy_troops_defeated READ enemy_troops_defeated NOTIFY
                 enemy_troops_defeated_changed)
  Q_PROPERTY(QVariantList owner_info READ get_owner_info NOTIFY owner_info_changed)
  Q_PROPERTY(int selected_player_id READ selected_player_id WRITE set_selected_player_id
                 NOTIFY selected_player_id_changed)
  Q_PROPERTY(QString last_error READ last_error NOTIFY last_error_changed)
  Q_PROPERTY(QObject* audio_system READ audio_system CONSTANT)
  Q_PROPERTY(QImage minimap_image READ minimap_image NOTIFY minimap_image_changed)
  Q_PROPERTY(
      bool is_spectator_mode READ is_spectator_mode NOTIFY spectator_mode_changed)
  Q_PROPERTY(bool is_loading READ is_loading NOTIFY is_loading_changed)
  Q_PROPERTY(
      float loading_progress READ loading_progress NOTIFY loading_progress_changed)
  Q_PROPERTY(
      QString loading_stage_text READ loading_stage_text NOTIFY loading_stage_changed)
  Q_PROPERTY(bool is_placing_formation READ is_placing_formation NOTIFY
                 placing_formation_changed)
  Q_PROPERTY(bool is_placing_construction READ is_placing_construction NOTIFY
                 placing_construction_changed)
  Q_PROPERTY(QString pending_builder_construction_type READ
                 pending_builder_construction_type NOTIFY placing_construction_changed)
  Q_PROPERTY(bool construction_preview_active READ construction_preview_active NOTIFY
                 construction_preview_active_changed)
  Q_PROPERTY(bool construction_preview_valid READ construction_preview_valid NOTIFY
                 construction_preview_valid_changed)
  Q_PROPERTY(
      int construction_preview_segment_count READ construction_preview_segment_count
          NOTIFY construction_preview_summary_changed)
  Q_PROPERTY(int construction_preview_valid_segment_count READ
                 construction_preview_valid_segment_count NOTIFY
                     construction_preview_summary_changed)
  Q_PROPERTY(int construction_preview_total_cost READ construction_preview_total_cost
                 NOTIFY construction_preview_summary_changed)
  Q_PROPERTY(
      bool is_campaign_mission READ is_campaign_mission NOTIFY campaign_mission_changed)
  Q_PROPERTY(bool civilian_delivery_available READ civilian_delivery_available NOTIFY
                 civilian_delivery_available_changed)
  Q_PROPERTY(QString control_mode READ control_mode NOTIFY control_mode_changed)
  Q_PROPERTY(QString game_mode READ game_mode NOTIFY game_mode_changed)
  Q_PROPERTY(bool commander_control_available READ commander_control_available NOTIFY
                 commander_control_available_changed)
  Q_PROPERTY(QObject* commander_input READ commander_input CONSTANT)

  Q_INVOKABLE void on_map_clicked(qreal sx, qreal sy);
  Q_INVOKABLE void on_right_click(qreal sx, qreal sy);
  Q_INVOKABLE void on_right_double_click(qreal sx, qreal sy);
  Q_INVOKABLE [[nodiscard]] bool on_right_press(qreal sx, qreal sy);
  Q_INVOKABLE void on_right_move(qreal sx, qreal sy);
  Q_INVOKABLE void on_right_release(qreal sx, qreal sy);
  Q_INVOKABLE void on_right_drag_orient(qreal sx, qreal sy);
  Q_INVOKABLE void on_click_select(qreal sx, qreal sy, bool additive = false);
  Q_INVOKABLE void
  on_area_selected(qreal x1, qreal y1, qreal x2, qreal y2, bool additive = false);
  Q_INVOKABLE void select_all_troops();
  Q_INVOKABLE void select_unit_by_id(int unit_id);
  Q_INVOKABLE void set_hover_at_screen(qreal sx, qreal sy);
  Q_INVOKABLE void on_attack_click(qreal sx, qreal sy);
  Q_INVOKABLE void on_stop_command();
  Q_INVOKABLE void on_hold_command();
  Q_INVOKABLE void on_guard_command();
  Q_INVOKABLE void on_formation_command();
  Q_INVOKABLE void on_run_command();
  Q_INVOKABLE void on_heal_command();
  Q_INVOKABLE void on_build_command();
  Q_INVOKABLE void on_guard_click(qreal sx, qreal sy);
  Q_INVOKABLE void on_civilian_delivery_click(qreal sx, qreal sy);
  Q_INVOKABLE [[nodiscard]] bool any_selected_in_hold_mode() const;
  Q_INVOKABLE [[nodiscard]] bool any_selected_in_guard_mode() const;
  Q_INVOKABLE [[nodiscard]] bool any_selected_in_formation_mode() const;
  Q_INVOKABLE [[nodiscard]] bool any_selected_in_run_mode() const;
  Q_INVOKABLE [[nodiscard]] bool is_placing_formation() const;
  Q_INVOKABLE [[nodiscard]] bool is_placing_construction() const;
  Q_INVOKABLE [[nodiscard]] QString pending_builder_construction_type() const;
  Q_INVOKABLE [[nodiscard]] bool construction_preview_active() const;
  Q_INVOKABLE [[nodiscard]] bool construction_preview_valid() const;
  Q_INVOKABLE [[nodiscard]] bool construction_preview_rotatable() const;
  Q_INVOKABLE [[nodiscard]] int construction_preview_segment_count() const;
  Q_INVOKABLE [[nodiscard]] int construction_preview_valid_segment_count() const;
  Q_INVOKABLE [[nodiscard]] int construction_preview_total_cost() const;
  Q_INVOKABLE void on_formation_mouse_move(qreal sx, qreal sy);
  Q_INVOKABLE void on_formation_scroll(float delta);
  Q_INVOKABLE void on_formation_confirm();
  Q_INVOKABLE void on_formation_cancel();
  Q_INVOKABLE void on_construction_mouse_move(qreal sx, qreal sy);
  Q_INVOKABLE void on_construction_pointer_pressed(qreal sx, qreal sy);
  Q_INVOKABLE void on_construction_pointer_released(qreal sx, qreal sy);
  Q_INVOKABLE void on_construction_scroll(float delta);
  Q_INVOKABLE void on_construction_confirm();
  Q_INVOKABLE void on_construction_cancel();
  Q_INVOKABLE void on_patrol_click(qreal sx, qreal sy);
  Q_INVOKABLE void toggle_commander_control_mode();
  Q_INVOKABLE void commander_key_down(int key, int modifiers = 0);
  Q_INVOKABLE void commander_key_up(int key, int modifiers = 0);
  Q_INVOKABLE void commander_primary_action();
  Q_INVOKABLE void commander_primary_action_down();
  Q_INVOKABLE void commander_primary_action_up();
  Q_INVOKABLE void commander_secondary_action_down();
  Q_INVOKABLE void commander_secondary_action_up();
  Q_INVOKABLE void commander_trigger_rally();
  Q_INVOKABLE void begin_commander_flag_rally();
  Q_INVOKABLE void confirm_commander_flag_rally(qreal sx, qreal sy);
  Q_INVOKABLE void cancel_commander_flag_rally();
  Q_INVOKABLE void begin_barracks_rally_placement();
  Q_INVOKABLE void confirm_barracks_rally_placement(qreal sx, qreal sy);
  Q_INVOKABLE void cancel_barracks_rally_placement();
  [[nodiscard]] bool is_placing_commander_rally() const;
  [[nodiscard]] bool has_commander_rally_preview() const;
  [[nodiscard]] QVector3D get_commander_rally_preview() const;
  [[nodiscard]] bool has_commander_rally_flag() const;
  [[nodiscard]] QVector3D get_commander_rally_flag_position() const;
  Q_INVOKABLE void commander_dodge();
  Q_INVOKABLE void commander_jump();
  Q_INVOKABLE void commander_cycle_lock_on();
  Q_INVOKABLE void commander_special_action();
  Q_INVOKABLE void commander_vanguard_rush();
  Q_INVOKABLE void commander_second_wind();
  Q_INVOKABLE void commander_toggle_camera_mode();
  Q_INVOKABLE void commander_mouse_move(qreal dx, qreal dy);
  Q_INVOKABLE void
  commander_mouse_look_at(qreal sx, qreal sy, qreal center_sx, qreal center_sy);
  Q_INVOKABLE void commander_center_mouse(qreal center_sx, qreal center_sy);

  Q_INVOKABLE void camera_move(float dx, float dz);
  Q_INVOKABLE void camera_elevate(float dy);
  Q_INVOKABLE void reset_camera();
  Q_INVOKABLE void camera_zoom(float delta);
  Q_INVOKABLE [[nodiscard]] float camera_distance() const;
  Q_INVOKABLE void camera_yaw(float degrees);
  Q_INVOKABLE void camera_orbit(float yaw_deg, float pitch_deg);
  Q_INVOKABLE void camera_orbit_direction(int direction, bool shift);
  Q_INVOKABLE void camera_follow_selection(bool enable);
  Q_INVOKABLE void camera_set_follow_lerp(float alpha);
  Q_INVOKABLE void
  on_minimap_left_click(qreal mx, qreal my, qreal minimap_width, qreal minimap_height);
  Q_INVOKABLE void
  on_minimap_right_click(qreal mx, qreal my, qreal minimap_width, qreal minimap_height);
  Q_INVOKABLE void start_loading_maps();
  Q_INVOKABLE void set_audio_frontend_context(const QString& context);

  Q_INVOKABLE void set_paused(bool paused) { m_runtime.paused = paused; }
  Q_INVOKABLE void set_game_speed(float speed) {
    m_runtime.time_scale = std::max(0.0F, speed);
  }
  [[nodiscard]] bool paused() const { return m_runtime.paused; }
  [[nodiscard]] float time_scale() const { return m_runtime.time_scale; }
  [[nodiscard]] QString victory_state() const { return m_runtime.victory_state; }
  [[nodiscard]] QString cursor_mode() const;
  void set_cursor_mode(CursorMode mode);
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

  Q_INVOKABLE [[nodiscard]] bool has_selected_type(const QString& type) const;
  Q_INVOKABLE void recruit_near_selected(const QString& unit_type);
  Q_INVOKABLE void start_building_placement(const QString& building_type);
  Q_INVOKABLE void place_building_at_screen(qreal sx, qreal sy);
  Q_INVOKABLE void cancel_building_placement();
  Q_INVOKABLE [[nodiscard]] QString pending_building_type() const;
  Q_INVOKABLE [[nodiscard]] QVariantMap get_selected_production_state() const;
  Q_INVOKABLE [[nodiscard]] QVariantMap get_selected_home_production_state() const;
  Q_INVOKABLE [[nodiscard]] QVariantMap get_selected_builder_production_state() const;
  Q_INVOKABLE [[nodiscard]] QVariantMap get_controlled_commander_status() const;
  Q_INVOKABLE QVariantList pop_rpg_damage_events();
  Q_INVOKABLE QVariantMap rpg_project_world(float x, float y, float z) const;
  Q_INVOKABLE void start_builder_construction(const QString& item_type);
  Q_INVOKABLE [[nodiscard]] QVariantMap
  get_unit_production_info(const QString& unit_type, const QString& nation_id) const;
  Q_INVOKABLE [[nodiscard]] QVariantMap get_hud_action_states() const;
  Q_INVOKABLE [[nodiscard]] QString get_selected_units_command_mode() const;
  Q_INVOKABLE [[nodiscard]] QString
  get_selected_units_toggle_state(const QString& mode) const;
  Q_INVOKABLE [[nodiscard]] QVariantMap get_selected_units_mode_availability() const;
  Q_INVOKABLE void set_rally_at_screen(qreal sx, qreal sy);
  Q_INVOKABLE [[nodiscard]] QVariantList available_maps() const;
  [[nodiscard]] QVariantList available_nations() const;
  Q_INVOKABLE [[nodiscard]] QVariantList
  available_commanders(const QString& nation_id) const;
  [[nodiscard]] QVariantList available_campaigns() const;
  [[nodiscard]] bool maps_loading() const { return m_maps_loading; }
  Q_INVOKABLE void start_skirmish(const QString& map_path,
                                  const QVariantList& player_configs = QVariantList());
  Q_INVOKABLE void start_campaign_mission(const QString& campaign_id);
  Q_INVOKABLE void mark_current_mission_completed();
  Q_INVOKABLE [[nodiscard]] QVariantMap get_current_mission_objectives() const;
  Q_INVOKABLE [[nodiscard]] QVariantMap
  get_mission_definition(const QString& mission_id) const;
  Q_INVOKABLE void open_settings();
  Q_INVOKABLE void load_save();
  Q_INVOKABLE void save_game(const QString& filename = "savegame.json");
  Q_INVOKABLE void save_game_to_slot(const QString& slot_name);
  Q_INVOKABLE void load_game_from_slot(const QString& slot_name);
  Q_INVOKABLE [[nodiscard]] QVariantList get_save_slots() const;
  Q_INVOKABLE void refresh_save_slots();
  Q_INVOKABLE bool delete_save_slot(const QString& slot_name);
  Q_INVOKABLE void exit_game();
  Q_INVOKABLE [[nodiscard]] QVariantList get_owner_info() const;
  Q_INVOKABLE [[nodiscard]] QImage
  generate_map_preview(const QString& map_path,
                       const QVariantList& player_configs) const;

  [[nodiscard]] QImage minimap_image() const;

  [[nodiscard]] bool is_spectator_mode() const { return m_level.is_spectator_mode; }

  [[nodiscard]] bool is_loading() const {
    return m_runtime.loading || m_loading_overlay_active;
  }

  [[nodiscard]] float loading_progress() const;
  [[nodiscard]] QString loading_stage_text() const;

  [[nodiscard]] bool is_campaign_mission() const;
  [[nodiscard]] bool civilian_delivery_available() const {
    return m_civilian_delivery_available;
  }
  [[nodiscard]] QString control_mode() const;
  [[nodiscard]] QString game_mode() const;
  [[nodiscard]] bool commander_control_available() const;
  [[nodiscard]] QObject* commander_input();

  QObject* audio_system();

  void setWindow(QQuickWindow* w) { m_window = w; }

  void ensure_initialized();
  void update(float dt);
  void render(int pixel_width, int pixel_height);
  void set_input_viewport_size(qreal width, qreal height);

  void get_selected_unit_ids(std::vector<Engine::Core::EntityID>& out) const;
  bool get_unit_type_key(Engine::Core::EntityID id, QString& type_key) const;
  bool get_unit_info(Engine::Core::EntityID id,
                     QString& name,
                     int& health,
                     int& max_health,
                     bool& is_building,
                     bool& alive,
                     QString& nation) const;
  bool get_unit_stamina_info(Engine::Core::EntityID id,
                             float& stamina_ratio,
                             bool& is_running,
                             bool& can_run) const;

  [[nodiscard]] bool has_patrol_preview_waypoint() const;
  [[nodiscard]] QVector3D get_patrol_preview_waypoint() const;

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
  };
  struct PendingMissionWave {
    int owner_id = 0;
    Game::Systems::NationID nation_id = Game::Systems::NationID::RomanRepublic;
    QString ai_id;
    float trigger_time = 0.0F;
    QVector3D entry_world_position{0.0F, 0.0F, 0.0F};
    std::vector<Game::Mission::WaveComposition> composition;
    bool spawned = false;
  };
  enum class PlayerControlMode {
    Rts,
    Commander
  };
  enum class GameMode {
    Rts,
    Rpg
  };
  struct RightMouseGestureState {
    QPointF press_position;
    bool active = false;
    bool dragged = false;
    bool suppress_release_click = false;
    bool double_click_handled = false;
    bool placement_was_active_on_press = false;
    bool started_formation_placement = false;

    void reset() {
      press_position = QPointF();
      active = false;
      dragged = false;
      suppress_release_click = false;
      double_click_handled = false;
      placement_was_active_on_press = false;
      started_formation_placement = false;
    }
  };
  struct CameraSnapshot {
    QVector3D position{0.0F, 0.0F, 0.0F};
    QVector3D target{0.0F, 0.0F, 1.0F};
    QVector3D up{0.0F, 1.0F, 0.0F};
    bool follow_selection = false;
    bool valid = false;
  };
  bool screen_to_ground(const QPointF& screen_pt, QVector3D& out_world);
  bool world_to_screen(const QVector3D& world, QPointF& out_screen) const;
  [[nodiscard]] QPointF map_input_to_viewport(qreal sx, qreal sy) const;
  [[nodiscard]] Engine::Core::Entity* find_local_commander() const;
  bool enter_commander_control_mode();
  void exit_commander_control_mode();
  void request_enter_commander_control_mode();
  void request_exit_commander_control_mode();
  void enter_rts_runtime_mode();
  void enter_commander_runtime_mode();
  void exit_commander_runtime_mode();
  void set_active_camera(Render::GL::Camera* camera);
  [[nodiscard]] auto apply_runtime_time_effects(float dt) -> float;
  void update_active_runtime_simulation(float dt);
  [[nodiscard]] auto
  should_render_selected_entity(Engine::Core::EntityID id) const -> bool;
  void render_runtime_mode_effects();
  void update_rts_control_mode(float dt);
  void update_commander_control_mode(float dt);
  [[nodiscard]] Engine::Core::Entity* controlled_commander_entity();
  void store_rts_selection();
  void select_controlled_commander();
  void restore_rts_selection();
  void clear_controlled_commander_state();
  void poll_commander_mouse_look();
  void reset_commander_input();
  void sync_selection_flags();
  void prune_unavailable_action_context();
  [[nodiscard]] bool is_action_enabled(const QString& action_id) const;
  void update_civilian_delivery_availability();
  void sync_selected_player_state();
  void sync_scatter_world_props();
  void initialize_player_resources();
  static void reset_movement(Engine::Core::Entity* entity);
  QAbstractItemModel* selected_units_model();
  void apply_mission_ambience(const Game::Mission::MissionDefinition* mission,
                              const QString& map_path);
  void configure_audio_manifest_mappings();
  void configure_audio_voice_mappings();
  void configure_audio_ambient_mappings();
  void ensure_result_audio_ready(const QString& state);
  void apply_frontend_music_context(const QString& context);
  void stop_mission_ambience();
  void on_unit_spawned(const Engine::Core::UnitSpawnedEvent& event);
  void on_unit_died(const Engine::Core::UnitDiedEvent& event);
  void rebuild_entity_cache();
  void rebuild_registries_after_load();
  void rebuild_building_collisions();
  void restore_environment_from_metadata(const QJsonObject& metadata);
  void update_cursor(Qt::CursorShape new_cursor);
  void set_error(const QString& error_message);
  bool load_from_slot(const QString& slot);
  bool save_to_slot(const QString& slot, const QString& title);
  [[nodiscard]] Game::Systems::RuntimeSnapshot to_runtime_snapshot() const;
  void apply_runtime_snapshot(const Game::Systems::RuntimeSnapshot& snapshot);
  [[nodiscard]] QByteArray capture_screenshot() const;
  [[nodiscard]] AppSceneContext scene_context() const;
  void start_skirmish_internal(const QString& map_path,
                               const QVariantList& player_configs,
                               bool set_skirmish_context);
  void perform_skirmish_load(const QString& map_path,
                             const QVariantList& player_configs);
  void apply_skirmish_commander_setup(const QVariantList& player_configs);
  void apply_mission_setup();
  void configure_mission_victory_conditions();
  void configure_rain_system();
  void reset_preload_interaction_state();
  void reset_mission_runtime_state();
  void update_mission_waves(float dt);
  void spawn_mission_wave(const PendingMissionWave& wave);
  void center_camera_on_local_forces();
  void finalize_skirmish_load();
  void apply_game_mode_render_policy();
  void render_game_effects();
  void update_loading_overlay();
  void update_cursor_position();
  void restore_controlled_commander_direct_control_if_ready();
  void seed_commander_rally_preview_from_view_center();
  void seed_barracks_rally_preview_from_selection();
  [[nodiscard]] bool has_selected_local_barracks() const;

  std::unique_ptr<Engine::Core::World> m_world;
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
  std::unique_ptr<Render::GL::RainRenderer> m_rain;
  std::unique_ptr<Game::Systems::RainManager> m_rain_manager;
  std::unique_ptr<Game::Systems::PickingService> m_picking_service;
  std::unique_ptr<Game::Systems::VictoryService> m_victory_service;
  std::unique_ptr<Game::Systems::SaveLoadService> m_save_load_service;
  std::unique_ptr<CursorManager> m_cursor_manager;
  std::unique_ptr<HoverTracker> m_hover_tracker;
  bool m_civilian_delivery_available = false;
  std::unique_ptr<Game::Systems::CameraService> m_camera_service;
  std::unique_ptr<Game::Systems::SelectionController> m_selection_controller;
  std::unique_ptr<App::Controllers::CommandController> m_command_controller;
  std::unique_ptr<Game::Map::MapCatalog> m_map_catalog;
  std::unique_ptr<Game::Audio::AudioEventHandler> m_audio_event_handler;
  std::unique_ptr<App::Models::AudioSystemProxy> m_audio_systemProxy;
  QString m_audio_frontend_context;
  QString m_current_ambient_sound_id;
  std::unique_ptr<MinimapManager> m_minimap_manager;
  std::unique_ptr<VisibilityCoordinator> m_visibility_coordinator;
  std::unique_ptr<AmbientStateManager> m_ambient_state_manager;
  std::unique_ptr<InputCommandHandler> m_input_handler;
  std::unique_ptr<CameraController> m_camera_controller;
  std::unique_ptr<LoadingProgressTracker> m_loading_progress_tracker;
  std::unique_ptr<ProductionManager> m_production_manager;
  std::unique_ptr<CampaignManager> m_campaign_manager;
  std::unique_ptr<SelectionQueryService> m_selection_query_service;
  QQuickWindow* m_window = nullptr;
  RuntimeState m_runtime;
  ViewportState m_viewport;
  bool m_follow_selection_enabled = false;
  PlayerControlMode m_control_mode = PlayerControlMode::Rts;
  GameMode m_game_mode = GameMode::Rts;
  Engine::Core::EntityID m_controlled_commander_id = 0;
  std::vector<Engine::Core::EntityID> m_saved_rts_selection_ids;
  CameraSnapshot m_rts_camera_snapshot;
  RightMouseGestureState m_right_mouse_gesture;
  CommanderControlController m_commander_control;
  Render::GL::RpgTelegraphRenderer m_rpg_telegraphs;
  CommanderInputAdapter m_commander_input;
  Game::Systems::LevelSnapshot m_level;
  SelectedUnitsModel* m_selected_units_model = nullptr;
  int m_enemy_troops_defeated = 0;
  int m_selected_player_id = 1;
  QVariantMap m_selected_player_state;
  std::uint64_t m_last_world_props_revision = 0;
  QVariantList m_available_maps;
  bool m_maps_loading = false;
  bool m_loading_overlay_active = false;
  std::atomic_bool m_loading_overlay_wait_for_first_frame{false};
  int m_loading_overlay_frames_remaining = 0;
  qint64 m_loading_overlay_min_duration_ms = 0;
  QElapsedTimer m_loading_overlay_timer;
  bool m_finalize_progress_after_overlay = false;
  bool m_show_objectives_after_loading = false;
  float m_campaign_mission_elapsed = 0.0F;
  std::vector<PendingMissionWave> m_pending_mission_waves;
  Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>
      m_unit_died_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::UnitSpawnedEvent>
      m_unit_spawned_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::CombatHitEvent>
      m_combat_hit_subscription;

  struct RpgDamageEvent {
    float wx{0.0F};
    float wy{0.0F};
    float wz{0.0F};
    int damage{0};
    float damage_ratio{0.0F};
    int lane{0};
    bool killing_blow{false};
  };
  static constexpr int k_max_rpg_damage_events = 96;
  std::vector<RpgDamageEvent> m_rpg_damage_events;
  std::uint32_t m_rpg_damage_event_sequence{0};
  float m_rpg_hit_stop_timer{0.0F};
  EntityCache m_entity_cache;
  RuntimeFrameOrchestrator m_frame_orchestrator;
  std::optional<QVector3D> m_commander_rally_preview_pos;

signals:
  void selected_units_changed();
  void selected_units_data_changed();
  void enemy_troops_defeated_changed();
  void victory_state_changed();
  void cursor_mode_changed();
  void global_cursor_changed();
  void troop_count_changed();
  void available_maps_changed();
  void available_campaigns_changed();
  void owner_info_changed();
  void selected_player_id_changed();
  void selected_player_state_changed();
  void last_error_changed();
  void maps_loading_changed();
  void minimap_image_changed();
  void save_slots_changed();
  void hold_mode_changed(bool active);
  void guard_mode_changed(bool active);
  void formation_mode_changed(bool active);
  void run_mode_changed(bool active);
  void spectator_mode_changed();
  void is_loading_changed();
  void loading_progress_changed(float progress);
  void loading_stage_changed(QString stage_text);
  void placing_formation_changed();
  void placing_construction_changed();
  void construction_preview_active_changed();
  void construction_preview_valid_changed();
  void construction_preview_summary_changed();
  void campaign_mission_changed();
  void civilian_delivery_available_changed();
  void control_mode_changed();
  void game_mode_changed();
  void commander_control_available_changed();
  void mission_announcement(QString text);
};
