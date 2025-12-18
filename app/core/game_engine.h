#pragma once

#include "../models/cursor_manager.h"
#include "../models/cursor_mode.h"
#include "../models/hover_tracker.h"
#include "../models/selected_units_model.h"
#include "../utils/engine_view_helpers.h"
#include "../utils/movement_utils.h"
#include "../utils/selection_utils.h"
#include "ambient_state_manager.h"
#include "camera_controller.h"
#include "game/audio/AudioEventHandler.h"
#include "game/core/event_manager.h"
#include "game/map/mission_definition.h"
#include "game/systems/game_state_serializer.h"
#include "input_command_handler.h"
#include "minimap_manager.h"
#include "renderer_bootstrap.h"
#include <QElapsedTimer>
#include <QJsonObject>
#include <QList>
#include <QMatrix4x4>
#include <QObject>
#include <QPointF>
#include <QStringList>
#include <QVariant>
#include <QVector3D>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

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
class ResourceManager;
class GroundRenderer;
class TerrainRenderer;
class BiomeRenderer;
class RiverRenderer;
class RoadRenderer;
class RiverbankRenderer;
class BridgeRenderer;
class FogRenderer;
class StoneRenderer;
class PlantRenderer;
class PineRenderer;
class OliveRenderer;
class FireCampRenderer;
class RainRenderer;
struct IRenderPass;
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

struct EntityCache {
  int player_troop_count = 0;
  bool player_barracks_alive = false;
  bool enemy_barracks_alive = false;
  int enemy_barracks_count = 0;

  void reset() {
    player_troop_count = 0;
    player_barracks_alive = false;
    enemy_barracks_alive = false;
    enemy_barracks_count = 0;
  }
};

class GameEngine : public QObject {
  Q_OBJECT
public:
  explicit GameEngine(QObject *parent = nullptr);
  ~GameEngine() override;

  void cleanup_opengl_resources();

  Q_PROPERTY(QAbstractItemModel *selected_units_model READ selected_units_model
                 NOTIFY selected_units_changed)
  Q_PROPERTY(bool paused READ paused WRITE set_paused)
  Q_PROPERTY(float time_scale READ time_scale WRITE set_game_speed)
  Q_PROPERTY(
      QString victory_state READ victory_state NOTIFY victory_state_changed)
  Q_PROPERTY(QString cursor_mode READ cursor_mode WRITE set_cursor_mode NOTIFY
                 cursor_mode_changed)
  Q_PROPERTY(
      qreal global_cursor_x READ global_cursor_x NOTIFY global_cursor_changed)
  Q_PROPERTY(
      qreal global_cursor_y READ global_cursor_y NOTIFY global_cursor_changed)
  Q_PROPERTY(bool has_units_selected READ has_units_selected NOTIFY
                 selected_units_changed)
  Q_PROPERTY(
      int player_troop_count READ player_troop_count NOTIFY troop_count_changed)
  Q_PROPERTY(int max_troops_per_player READ max_troops_per_player NOTIFY
                 troop_count_changed)
  Q_PROPERTY(QVariantList available_maps READ available_maps NOTIFY
                 available_maps_changed)
  Q_PROPERTY(bool maps_loading READ maps_loading NOTIFY maps_loading_changed)
  Q_PROPERTY(QVariantList available_nations READ available_nations CONSTANT)
  Q_PROPERTY(QVariantList available_campaigns READ available_campaigns NOTIFY
                 available_campaigns_changed)
  Q_PROPERTY(int enemy_troops_defeated READ enemy_troops_defeated NOTIFY
                 enemy_troops_defeated_changed)
  Q_PROPERTY(
      QVariantList owner_info READ get_owner_info NOTIFY owner_info_changed)
  Q_PROPERTY(int selected_player_id READ selected_player_id WRITE
                 set_selected_player_id NOTIFY selected_player_id_changed)
  Q_PROPERTY(QString last_error READ last_error NOTIFY last_error_changed)
  Q_PROPERTY(QObject *audio_system READ audio_system CONSTANT)
  Q_PROPERTY(
      QImage minimap_image READ minimap_image NOTIFY minimap_image_changed)
  Q_PROPERTY(bool is_spectator_mode READ is_spectator_mode NOTIFY
                 spectator_mode_changed)
  Q_PROPERTY(bool is_loading READ is_loading NOTIFY is_loading_changed)
  Q_PROPERTY(float loading_progress READ loading_progress NOTIFY
                 loading_progress_changed)
  Q_PROPERTY(QString loading_stage_text READ loading_stage_text NOTIFY
                 loading_stage_changed)
  Q_PROPERTY(bool is_placing_formation READ is_placing_formation NOTIFY
                 placing_formation_changed)

  Q_INVOKABLE void on_map_clicked(qreal sx, qreal sy);
  Q_INVOKABLE void on_right_click(qreal sx, qreal sy);
  Q_INVOKABLE void on_right_double_click(qreal sx, qreal sy);
  Q_INVOKABLE void on_click_select(qreal sx, qreal sy, bool additive = false);
  Q_INVOKABLE void on_area_selected(qreal x1, qreal y1, qreal x2, qreal y2,
                                    bool additive = false);
  Q_INVOKABLE void select_all_troops();
  Q_INVOKABLE void select_unit_by_id(int unitId);
  Q_INVOKABLE void set_hover_at_screen(qreal sx, qreal sy);
  Q_INVOKABLE void on_attack_click(qreal sx, qreal sy);
  Q_INVOKABLE void on_stop_command();
  Q_INVOKABLE void on_hold_command();
  Q_INVOKABLE void on_guard_command();
  Q_INVOKABLE void on_formation_command();
  Q_INVOKABLE void on_run_command();
  Q_INVOKABLE void on_guard_click(qreal sx, qreal sy);
  Q_INVOKABLE [[nodiscard]] bool any_selected_in_hold_mode() const;
  Q_INVOKABLE [[nodiscard]] bool any_selected_in_guard_mode() const;
  Q_INVOKABLE [[nodiscard]] bool any_selected_in_formation_mode() const;
  Q_INVOKABLE [[nodiscard]] bool any_selected_in_run_mode() const;
  Q_INVOKABLE [[nodiscard]] bool is_placing_formation() const;
  Q_INVOKABLE void on_formation_mouse_move(qreal sx, qreal sy);
  Q_INVOKABLE void on_formation_scroll(float delta);
  Q_INVOKABLE void on_formation_confirm();
  Q_INVOKABLE void on_formation_cancel();
  Q_INVOKABLE void on_patrol_click(qreal sx, qreal sy);

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
  Q_INVOKABLE void start_loading_maps();

  Q_INVOKABLE void set_paused(bool paused) { m_runtime.paused = paused; }
  Q_INVOKABLE void set_game_speed(float speed) {
    m_runtime.time_scale = std::max(0.0F, speed);
  }
  [[nodiscard]] bool paused() const { return m_runtime.paused; }
  [[nodiscard]] float time_scale() const { return m_runtime.time_scale; }
  [[nodiscard]] QString victory_state() const {
    return m_runtime.victory_state;
  }
  [[nodiscard]] QString cursor_mode() const;
  void set_cursor_mode(CursorMode mode);
  void set_cursor_mode(const QString &mode);
  [[nodiscard]] qreal global_cursor_x() const;
  [[nodiscard]] qreal global_cursor_y() const;
  [[nodiscard]] bool has_units_selected() const;
  [[nodiscard]] int player_troop_count() const;
  [[nodiscard]] int max_troops_per_player() const {
    return m_level.max_troops_per_player;
  }
  [[nodiscard]] int enemy_troops_defeated() const;

  Q_INVOKABLE [[nodiscard]] static QVariantMap get_player_stats(int owner_id);

  [[nodiscard]] int selected_player_id() const { return m_selected_player_id; }
  void set_selected_player_id(int id) {
    if (m_selected_player_id != id) {
      m_selected_player_id = id;
      emit selected_player_id_changed();
    }
  }
  [[nodiscard]] QString last_error() const { return m_runtime.last_error; }
  Q_INVOKABLE void clear_error() {
    if (!m_runtime.last_error.isEmpty()) {
      m_runtime.last_error = "";
      emit last_error_changed();
    }
  }

  Q_INVOKABLE [[nodiscard]] bool has_selected_type(const QString &type) const;
  Q_INVOKABLE void recruit_near_selected(const QString &unit_type);
  Q_INVOKABLE void start_building_placement(const QString &building_type);
  Q_INVOKABLE void place_building_at_screen(qreal sx, qreal sy);
  Q_INVOKABLE void cancel_building_placement();
  Q_INVOKABLE [[nodiscard]] QString pending_building_type() const;
  Q_INVOKABLE [[nodiscard]] QVariantMap get_selected_production_state() const;
  Q_INVOKABLE [[nodiscard]] QVariantMap
  get_selected_builder_production_state() const;
  Q_INVOKABLE void start_builder_construction(const QString &item_type);
  Q_INVOKABLE [[nodiscard]] QVariantMap
  get_unit_production_info(const QString &unit_type) const;
  Q_INVOKABLE [[nodiscard]] QString get_selected_units_command_mode() const;
  Q_INVOKABLE [[nodiscard]] QVariantMap
  get_selected_units_mode_availability() const;
  Q_INVOKABLE void set_rally_at_screen(qreal sx, qreal sy);
  Q_INVOKABLE [[nodiscard]] QVariantList available_maps() const;
  [[nodiscard]] QVariantList available_nations() const;
  [[nodiscard]] QVariantList available_campaigns() const;
  [[nodiscard]] bool maps_loading() const { return m_maps_loading; }
  Q_INVOKABLE void
  start_skirmish(const QString &map_path,
                 const QVariantList &playerConfigs = QVariantList());
  Q_INVOKABLE void start_campaign_mission(const QString &campaign_id);
  Q_INVOKABLE void mark_current_mission_completed();
  Q_INVOKABLE void open_settings();
  Q_INVOKABLE void load_save();
  Q_INVOKABLE void save_game(const QString &filename = "savegame.json");
  Q_INVOKABLE void save_game_to_slot(const QString &slot_name);
  Q_INVOKABLE void load_game_from_slot(const QString &slot_name);
  Q_INVOKABLE [[nodiscard]] QVariantList get_save_slots() const;
  Q_INVOKABLE void refresh_save_slots();
  Q_INVOKABLE bool delete_save_slot(const QString &slot_name);
  Q_INVOKABLE void exit_game();
  Q_INVOKABLE [[nodiscard]] QVariantList get_owner_info() const;
  Q_INVOKABLE [[nodiscard]] QImage
  generate_map_preview(const QString &map_path,
                       const QVariantList &player_configs) const;

  [[nodiscard]] QImage minimap_image() const;

  [[nodiscard]] bool is_spectator_mode() const {
    return m_level.is_spectator_mode;
  }

  [[nodiscard]] bool is_loading() const {
    return m_runtime.loading || m_loading_overlay_active;
  }

  [[nodiscard]] float loading_progress() const;
  [[nodiscard]] QString loading_stage_text() const;

  QObject *audio_system();

  void setWindow(QQuickWindow *w) { m_window = w; }

  void ensure_initialized();
  void update(float dt);
  void render(int pixelWidth, int pixelHeight);

  void get_selected_unit_ids(std::vector<Engine::Core::EntityID> &out) const;
  bool get_unit_info(Engine::Core::EntityID id, QString &name, int &health,
                     int &max_health, bool &isBuilding, bool &alive,
                     QString &nation) const;
  bool get_unit_stamina_info(Engine::Core::EntityID id, float &stamina_ratio,
                             bool &is_running, bool &can_run) const;

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
    std::uint64_t visibility_version = 0;
    float visibility_update_accumulator = 0.0F;
    qreal last_cursor_x = -1.0;
    qreal last_cursor_y = -1.0;
    int selection_refresh_counter = 0;
  };
  bool screen_to_ground(const QPointF &screenPt, QVector3D &outWorld);
  bool world_to_screen(const QVector3D &world, QPointF &outScreen) const;
  void sync_selection_flags();
  static void reset_movement(Engine::Core::Entity *entity);
  QAbstractItemModel *selected_units_model();
  void on_unit_spawned(const Engine::Core::UnitSpawnedEvent &event);
  void on_unit_died(const Engine::Core::UnitDiedEvent &event);
  void rebuild_entity_cache();
  void rebuild_registries_after_load();
  void rebuild_building_collisions();
  void restore_environment_from_metadata(const QJsonObject &metadata);
  void update_cursor(Qt::CursorShape newCursor);
  void set_error(const QString &errorMessage);
  bool load_from_slot(const QString &slot);
  bool save_to_slot(const QString &slot, const QString &title);
  [[nodiscard]] Game::Systems::RuntimeSnapshot to_runtime_snapshot() const;
  void apply_runtime_snapshot(const Game::Systems::RuntimeSnapshot &snapshot);
  [[nodiscard]] QByteArray capture_screenshot() const;
  void perform_skirmish_load(const QString &map_path,
                             const QVariantList &playerConfigs);

  std::unique_ptr<Engine::Core::World> m_world;
  std::unique_ptr<Render::GL::Renderer> m_renderer;
  std::unique_ptr<Render::GL::Camera> m_camera;
  std::shared_ptr<Render::GL::ResourceManager> m_resources;
  std::unique_ptr<Render::GL::GroundRenderer> m_ground;
  std::unique_ptr<Render::GL::TerrainRenderer> m_terrain;
  std::unique_ptr<Render::GL::BiomeRenderer> m_biome;
  std::unique_ptr<Render::GL::RiverRenderer> m_river;
  std::unique_ptr<Render::GL::RoadRenderer> m_road;
  std::unique_ptr<Render::GL::RiverbankRenderer> m_riverbank;
  std::unique_ptr<Render::GL::BridgeRenderer> m_bridge;
  std::unique_ptr<Render::GL::FogRenderer> m_fog;
  std::unique_ptr<Render::GL::StoneRenderer> m_stone;
  std::unique_ptr<Render::GL::PlantRenderer> m_plant;
  std::unique_ptr<Render::GL::PineRenderer> m_pine;
  std::unique_ptr<Render::GL::OliveRenderer> m_olive;
  std::unique_ptr<Render::GL::FireCampRenderer> m_firecamp;
  std::unique_ptr<Render::GL::RainRenderer> m_rain;
  std::unique_ptr<Game::Systems::RainManager> m_rainManager;
  std::vector<Render::GL::IRenderPass *> m_passes;
  std::unique_ptr<Game::Systems::PickingService> m_pickingService;
  std::unique_ptr<Game::Systems::VictoryService> m_victoryService;
  std::unique_ptr<Game::Systems::SaveLoadService> m_saveLoadService;
  std::unique_ptr<CursorManager> m_cursorManager;
  std::unique_ptr<HoverTracker> m_hoverTracker;
  std::unique_ptr<Game::Systems::CameraService> m_cameraService;
  std::unique_ptr<Game::Systems::SelectionController> m_selectionController;
  std::unique_ptr<App::Controllers::CommandController> m_commandController;
  std::unique_ptr<Game::Map::MapCatalog> m_mapCatalog;
  std::unique_ptr<Game::Audio::AudioEventHandler> m_audioEventHandler;
  std::unique_ptr<App::Models::AudioSystemProxy> m_audio_systemProxy;
  std::unique_ptr<MinimapManager> m_minimap_manager;
  std::unique_ptr<AmbientStateManager> m_ambient_state_manager;
  std::unique_ptr<InputCommandHandler> m_input_handler;
  std::unique_ptr<CameraController> m_camera_controller;
  std::unique_ptr<LoadingProgressTracker> m_loading_progress_tracker;
  QQuickWindow *m_window = nullptr;
  RuntimeState m_runtime;
  ViewportState m_viewport;
  bool m_followSelectionEnabled = false;
  Game::Systems::LevelSnapshot m_level;
  SelectedUnitsModel *m_selectedUnitsModel = nullptr;
  int m_enemyTroopsDefeated = 0;
  int m_selected_player_id = 1;
  QVariantList m_available_maps;
  QVariantList m_available_campaigns;
  bool m_maps_loading = false;
  QString m_current_campaign_id;
  QString m_pending_building_type;
  QString m_current_mission_id;
  std::optional<Game::Mission::MissionDefinition> m_current_mission_definition;
  bool m_loading_overlay_active = false;
  bool m_loading_overlay_wait_for_first_frame = false;
  int m_loading_overlay_frames_remaining = 0;
  qint64 m_loading_overlay_min_duration_ms = 0;
  QElapsedTimer m_loading_overlay_timer;
  bool m_finalize_progress_after_overlay = false;
  Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>
      m_unit_died_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::UnitSpawnedEvent>
      m_unit_spawned_subscription;
  EntityCache m_entity_cache;

  void load_campaigns();
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
};
