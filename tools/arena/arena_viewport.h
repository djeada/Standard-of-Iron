#pragma once

#include <QElapsedTimer>
#include <QOpenGLWidget>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "arena_scenario.h"
#include "game/core/component.h"
#include "game/map/map_definition.h"
#include "game/map/terrain.h"
#include "game/systems/nation_id.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"

namespace Engine::Core {
class World;
using EntityID = unsigned int;
} // namespace Engine::Core

namespace Game::Systems {
class CameraService;
class PickingService;
class SelectionSystem;
} // namespace Game::Systems

namespace Game::Units {
class Unit;
class UnitFactoryRegistry;
} // namespace Game::Units

namespace Arena {
class ArenaScenarioRunner;
struct ArenaScenarioReport;
} // namespace Arena

namespace Render::Profiling {
class FrameContinuityAnalyzer;
} // namespace Render::Profiling

namespace Render::GL {
class Renderer;
class Camera;
class TerrainSceneProxy;
class TerrainSurfaceManager;
class TerrainFeatureManager;
class TerrainScatterManager;
class FogRenderer;
class MapBoundaryFogRenderer;
class AmbientFogRenderer;
class RainRenderer;
} // namespace Render::GL

class QMouseEvent;
class QKeyEvent;
class QWheelEvent;
class QPainter;
class QPointF;
class QFocusEvent;

class ArenaViewport : public QOpenGLWidget {
  Q_OBJECT

public:
  explicit ArenaViewport(QWidget* parent = nullptr);
  ~ArenaViewport() override;

public slots:
  void regenerate_terrain();
  void set_terrain_seed(int seed);
  void set_terrain_height_scale(float value);
  void set_terrain_octaves(int value);
  void set_terrain_frequency(float value);
  void set_wireframe_enabled(bool enabled);
  void set_normals_overlay_enabled(bool enabled);
  void set_ground_type(const QString& ground_type);
  void set_rain_enabled(bool enabled);
  void set_rain_intensity(float intensity);
  void set_time_of_day(Game::Map::TimeOfDay time_of_day);

  [[nodiscard]] auto time_of_day() const noexcept -> Game::Map::TimeOfDay {
    return m_time_of_day;
  }
  [[nodiscard]] auto lighting_summary() const -> QString;

  void set_spawn_owner(int owner_id);
  void set_spawn_nation(const QString& nation_id);
  void set_spawn_unit_type(const QString& unit_type);
  void set_spawn_individuals_per_unit(int count);
  void set_spawn_rider_visible(bool visible);
  void spawn_unit();
  void spawn_units(int count);
  void spawn_opposing_batch(int count);
  void spawn_mirror_match(int count);
  void clear_units();

  void set_spawn_building_owner(int owner_id);
  void set_spawn_building_nation(const QString& nation_id);
  void set_spawn_building_type(const QString& building_type);
  void spawn_buildings(int count);
  void clear_buildings();
  void set_spawn_world_prop_type(const QString& prop_type);
  void set_spawn_world_prop_scale(float value);
  void set_spawn_world_prop_rotation_degrees(float value);
  void set_spawn_fire_camp_intensity(float value);
  void set_spawn_fire_camp_radius(float value);
  void spawn_world_prop();
  void clear_world_props();
  void clear_world_props_of_type();
  void reset_arena();
  void load_scenario(const QString& scenario_id);
  void set_terrain_review_content_enabled(bool enabled);
  [[nodiscard]] auto load_terrain_review_map(const QString& map_path,
                                             QString* error = nullptr) -> bool;
  void set_terrain_review_overview_camera();
  void set_terrain_review_gameplay_camera();
  [[nodiscard]] auto
  terrain_review_definition() const -> const Game::Map::MapDefinition*;
  void apply_visual_overrides_to_selection();
  void set_animation_name(const QString& animation_name);
  void play_selected_animation();
  void play_idle_animation();
  void play_walk_animation();
  void play_attack_animation();
  void play_death_animation();
  void move_selected_unit_forward();
  void set_movement_speed(float speed);
  void set_skeleton_debug_enabled(bool enabled);
  void set_combat_debug_enabled(bool enabled);
  void set_attack_scrub_enabled(bool enabled);
  void set_attack_scrub_phase(float phase);

  void pause_simulation(bool paused);
  void reset_camera();

  void set_batch_fixed_step(float seconds);
  void set_scenario_duration_override(float seconds);
  [[nodiscard]] auto active_scenario_finished() const -> bool;
  [[nodiscard]] auto
  active_scenario_report() const -> const Arena::ArenaScenarioReport*;
  [[nodiscard]] auto write_scenario_artifacts(const QString& directory,
                                              QString* error = nullptr) const -> bool;

signals:
  void paused_changed(bool paused);
  void selection_summary_changed(const QString& summary);
  void scenario_issue_detected(const QString& scenario_id,
                               const QString& issue_summary);
  void
  scenario_finished(const QString& scenario_id, bool passed, const QString& summary);
  void frame_rendered();
  void lighting_changed(const QString& summary);

protected:
  void initializeGL() override;
  void resizeGL(int width, int height) override;
  void paintGL() override;

  void keyPressEvent(QKeyEvent* event) override;
  void keyReleaseEvent(QKeyEvent* event) override;
  void focusOutEvent(QFocusEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void leaveEvent(QEvent* event) override;

private:
  struct TerrainSettings {
    int seed = 1337;
    float height_scale = 6.0F;
    int octaves = 4;
    float frequency = 0.07F;
  };

  void configure_runtime();
  void configure_rendering_from_terrain();
  void setup_default_players();
  void align_units_to_terrain();
  void sanitize_selection();
  void update_selected_entities();
  void sync_selection_summary();
  auto build_selection_summary() const -> QString;
  void update_hover(const QPoint& pos);
  void select_entity(Engine::Core::EntityID entity_id, bool additive = false);
  void select_entities_in_rect(const QRect& selection_rect, bool additive);
  void select_all_local_units();
  void issue_move_order(const QPointF& screen_pos);
  void update_spawn_anchor_from_click(const QPointF& screen_pos);
  [[nodiscard]] auto resolve_spawn_anchor_world() const -> QVector3D;
  [[nodiscard]] auto find_available_spawn_position(const QVector3D& anchor,
                                                   float clearance) const -> QVector3D;
  [[nodiscard]] auto is_spawn_position_available(const QVector3D& position,
                                                 float clearance) const -> bool;
  void apply_keyboard_camera_controls(float real_dt);
  void clear_camera_key_state();
  [[nodiscard]] auto terrain_review_max_camera_distance() const -> float;
  void update_active_scenario(float simulation_dt);
  void select_spawned_entities(const std::vector<Engine::Core::EntityID>& ids);
  auto spawn_single_unit() -> Engine::Core::EntityID;
  auto spawn_single_unit(int owner_id,
                         Game::Systems::NationID nation_id,
                         Game::Units::TroopType unit_type) -> Engine::Core::EntityID;
  auto spawn_single_unit(int owner_id,
                         Game::Systems::NationID nation_id,
                         Game::Units::TroopType unit_type,
                         const QVector3D& spawn_position,
                         bool ai_controlled) -> Engine::Core::EntityID;
  auto resolve_spawn_unit_type(Game::Systems::NationID nation_id,
                               Game::Units::TroopType preferred) const
      -> Game::Units::TroopType;
  auto find_unit_handle(Engine::Core::EntityID entity_id) const -> Game::Units::Unit*;
  void reconfigure_terrain_from_state();
  auto spawn_single_building(int owner_id,
                             Game::Systems::NationID nation_id,
                             Game::Units::SpawnType building_type,
                             std::optional<QVector3D> requested_position = std::nullopt,
                             bool ai_controlled = false) -> Engine::Core::EntityID;
  auto owner_display_name(int owner_id) const -> QString;
  auto nation_display_name(Game::Systems::NationID nation_id) const -> QString;
  auto troop_display_name(Game::Systems::NationID nation_id,
                          Game::Units::SpawnType spawn_type) const -> QString;
  auto selection_system() const -> Game::Systems::SelectionSystem*;
  auto selected_unit_ids_or_fallback() -> std::vector<Engine::Core::EntityID>;
  void sync_spawn_selection_defaults();
  void clear_forced_animation_state(const std::vector<Engine::Core::EntityID>& ids);
  void spawn_terrain_review_structures();
  void draw_debug_overlay(QPainter& painter);
  void draw_spawn_anchor_marker(QPainter& painter);
  void draw_selection_marquee(QPainter& painter);
  void draw_terrain_normals(QPainter& painter);
  void draw_pose_overlay(QPainter& painter);
  void draw_combat_animation_overlay(QPainter& painter);
  void draw_stats_overlay(QPainter& painter);
  void draw_controls_overlay(QPainter& painter);
  void capture_attack_scrub_anchor();
  void apply_attack_scrub_override();
  void set_force_full_creature_lod(bool enabled);
  void sample_frame_continuity();

  QTimer m_frame_timer;
  QElapsedTimer m_frame_clock;
  TerrainSettings m_terrain_settings;
  Game::Map::GroundType m_ground_type = Game::Map::GroundType::ForestMud;
  Game::Map::TimeOfDay m_time_of_day = Game::Map::TimeOfDay::Day;
  QString m_animation_name = QStringLiteral("Idle");

  std::unique_ptr<Engine::Core::World> m_world;
  std::unique_ptr<Render::GL::Renderer> m_renderer;
  std::unique_ptr<Render::GL::Camera> m_camera;
  std::unique_ptr<Render::GL::TerrainSceneProxy> m_terrain_scene;
  std::unique_ptr<Render::GL::TerrainSurfaceManager> m_surface;
  std::unique_ptr<Render::GL::TerrainFeatureManager> m_features;
  std::unique_ptr<Render::GL::TerrainScatterManager> m_scatter;
  std::unique_ptr<Render::GL::FogRenderer> m_fog;
  std::unique_ptr<Render::GL::MapBoundaryFogRenderer> m_boundary_fog;
  std::unique_ptr<Render::GL::AmbientFogRenderer> m_ambient_fog;
  std::unique_ptr<Render::GL::RainRenderer> m_rain;
  std::unique_ptr<Game::Systems::CameraService> m_camera_service;
  std::unique_ptr<Game::Systems::PickingService> m_picking_service;
  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_unit_factory;
  std::vector<std::unique_ptr<Game::Units::Unit>> m_units;
  int m_spawn_owner_id = 1;
  Game::Systems::NationID m_spawn_nation_id;
  Game::Units::TroopType m_spawn_unit_type;
  int m_spawn_individuals_per_unit_override = 0;
  bool m_spawn_rider_visible = true;

  int m_spawn_building_owner_id = 1;
  Game::Systems::NationID m_spawn_building_nation_id;
  Game::Units::SpawnType m_spawn_building_type = Game::Units::SpawnType::Barracks;
  std::vector<Game::Map::WorldProp> m_world_props;
  std::vector<Game::Map::RiverSegment> m_arena_rivers;
  std::vector<Game::Map::Lake> m_arena_lakes;
  std::vector<Game::Map::Bridge> m_arena_bridges;
  std::vector<Arena::ArenaScenarioElevationPatch> m_arena_elevation_patches;
  Game::Map::WorldProp::Type m_spawn_world_prop_type =
      Game::Map::WorldProp::Type::FireCamp;
  float m_spawn_world_prop_scale = 1.0F;
  float m_spawn_world_prop_rotation = 0.0F;
  float m_spawn_fire_camp_intensity = 1.0F;
  float m_spawn_fire_camp_radius = 3.0F;

  QPoint m_last_mouse_pos;
  bool m_last_mouse_pos_valid = false;
  QVector3D m_spawn_anchor_world;
  bool m_spawn_anchor_world_valid = false;
  QPoint m_selection_anchor;
  QPoint m_selection_current;
  Engine::Core::EntityID m_hovered_entity_id = 0;
  bool m_selection_drag_active = false;
  bool m_paused = false;
  bool m_wireframe_enabled = false;
  bool m_normals_overlay_enabled = false;
  bool m_pose_overlay_enabled = false;
  bool m_combat_debug_overlay_enabled = false;
  bool m_attack_scrub_enabled = false;
  bool m_gl_initialized = false;
  bool m_controls_overlay_visible = true;
  bool m_force_full_creature_lod = true;
  bool m_terrain_review_mode = false;
  bool m_terrain_review_content_enabled = false;
  bool m_batch_frame_in_progress = false;
  bool m_pan_up_pressed = false;
  bool m_pan_down_pressed = false;
  bool m_pan_left_pressed = false;
  bool m_pan_right_pressed = false;
  QString m_last_selection_summary;
  float m_default_unit_speed = 2.2F;
  float m_fps = 0.0F;
  float m_attack_scrub_phase = 0.5F;
  Engine::Core::EntityID m_attack_scrub_entity_id = 0;
  QVector3D m_attack_scrub_position{0.0F, 0.0F, 0.0F};
  QVector3D m_attack_scrub_rotation{0.0F, 0.0F, 0.0F};
  QVector3D m_attack_scrub_scale{1.0F, 1.0F, 1.0F};
  Engine::Core::CombatAttackFamily m_attack_scrub_family{
      Engine::Core::CombatAttackFamily::Sword};
  std::uint8_t m_attack_scrub_variant = 0U;
  float m_attack_scrub_offset = 0.0F;
  bool m_attack_scrub_finisher = false;
  std::unique_ptr<Arena::ArenaScenarioRunner> m_scenario_runner;
  std::unique_ptr<Render::Profiling::FrameContinuityAnalyzer>
      m_frame_continuity_analyzer;
  float m_batch_fixed_step = 0.0F;
  float m_scenario_duration_override = 0.0F;
  std::size_t m_last_scenario_issue_revision = 0U;
  bool m_scenario_finished_emitted = false;
  std::optional<Game::Map::MapDefinition> m_terrain_review_definition;
};
