#pragma once

#include "game/map/terrain.h"
#include "game/systems/nation_id.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"

#include <QElapsedTimer>
#include <QOpenGLWidget>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <memory>
#include <vector>

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

namespace Render::GL {
class Renderer;
class Camera;
class TerrainSceneProxy;
class TerrainSurfaceManager;
class TerrainFeatureManager;
class TerrainScatterManager;
class FogRenderer;
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
  explicit ArenaViewport(QWidget *parent = nullptr);
  ~ArenaViewport() override;

public slots:
  void regenerateTerrain();
  void setTerrainSeed(int seed);
  void setTerrainHeightScale(float value);
  void setTerrainOctaves(int value);
  void setTerrainFrequency(float value);
  void setWireframeEnabled(bool enabled);
  void setNormalsOverlayEnabled(bool enabled);
  void setGroundType(const QString &groundType);
  void setRainEnabled(bool enabled);
  void setRainIntensity(float intensity);

  void setSpawnOwner(int ownerId);
  void setSpawnNation(const QString &nationId);
  void setSpawnUnitType(const QString &unitType);
  void setSpawnIndividualsPerUnit(int count);
  void setSpawnRiderVisible(bool visible);
  void spawnUnit();
  void spawnUnits(int count);
  void spawnOpposingBatch(int count);
  void spawnMirrorMatch(int count);
  void clearUnits();

  void setSpawnBuildingOwner(int ownerId);
  void setSpawnBuildingNation(const QString &nationId);
  void setSpawnBuildingType(const QString &buildingType);
  void spawnBuildings(int count);
  void resetArena();
  void applyVisualOverridesToSelection();
  void setAnimationName(const QString &animationName);
  void playSelectedAnimation();
  void playIdleAnimation();
  void playWalkAnimation();
  void playAttackAnimation();
  void playDeathAnimation();
  void moveSelectedUnitForward();
  void setMovementSpeed(float speed);
  void setSkeletonDebugEnabled(bool enabled);

  void pauseSimulation(bool paused);
  void resetCamera();

signals:
  void pausedChanged(bool paused);
  void selectionSummaryChanged(const QString &summary);

protected:
  void initializeGL() override;
  void resizeGL(int width, int height) override;
  void paintGL() override;

  void keyPressEvent(QKeyEvent *event) override;
  void keyReleaseEvent(QKeyEvent *event) override;
  void focusOutEvent(QFocusEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void leaveEvent(QEvent *event) override;

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
  void update_hover(const QPoint &pos);
  void select_entity(Engine::Core::EntityID entity_id, bool additive = false);
  void select_entities_in_rect(const QRect &selection_rect, bool additive);
  void select_all_local_units();
  void issue_move_order(const QPointF &screen_pos);
  void apply_keyboard_camera_controls(float real_dt);
  void clear_camera_key_state();
  void select_spawned_entities(const std::vector<Engine::Core::EntityID> &ids);
  auto spawn_single_unit() -> Engine::Core::EntityID;
  auto
  spawn_single_unit(int ownerId, Game::Systems::NationID nationId,
                    Game::Units::TroopType unitType) -> Engine::Core::EntityID;
  auto resolve_spawn_unit_type(Game::Systems::NationID nationId,
                               Game::Units::TroopType preferred) const
      -> Game::Units::TroopType;
  auto spawn_single_building(int ownerId, Game::Systems::NationID nationId,
                              Game::Units::SpawnType buildingType)
      -> Engine::Core::EntityID;
  auto owner_display_name(int ownerId) const -> QString;
  auto nation_display_name(Game::Systems::NationID nationId) const -> QString;
  auto troop_display_name(Game::Systems::NationID nationId,
                          Game::Units::SpawnType spawnType) const -> QString;
  auto selection_system() const -> Game::Systems::SelectionSystem *;
  auto selected_unit_ids_or_fallback() -> std::vector<Engine::Core::EntityID>;
  void sync_spawn_selection_defaults();
  void
  clear_forced_animation_state(const std::vector<Engine::Core::EntityID> &ids);
  void draw_debug_overlay(QPainter &painter);
  void draw_selection_marquee(QPainter &painter);
  void draw_terrain_normals(QPainter &painter);
  void draw_pose_overlay(QPainter &painter);
  void draw_stats_overlay(QPainter &painter);
  void draw_controls_overlay(QPainter &painter);

  QTimer m_frameTimer;
  QElapsedTimer m_frameClock;
  TerrainSettings m_terrain_settings;
  Game::Map::GroundType m_ground_type = Game::Map::GroundType::ForestMud;
  QString m_animation_name = QStringLiteral("Idle");

  std::unique_ptr<Engine::Core::World> m_world;
  std::unique_ptr<Render::GL::Renderer> m_renderer;
  std::unique_ptr<Render::GL::Camera> m_camera;
  std::unique_ptr<Render::GL::TerrainSceneProxy> m_terrain_scene;
  std::unique_ptr<Render::GL::TerrainSurfaceManager> m_surface;
  std::unique_ptr<Render::GL::TerrainFeatureManager> m_features;
  std::unique_ptr<Render::GL::TerrainScatterManager> m_scatter;
  std::unique_ptr<Render::GL::FogRenderer> m_fog;
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

  QPoint m_last_mouse_pos;
  QPoint m_selection_anchor;
  QPoint m_selection_current;
  Engine::Core::EntityID m_hovered_entity_id = 0;
  bool m_selection_drag_active = false;
  bool m_paused = false;
  bool m_wireframe_enabled = false;
  bool m_normals_overlay_enabled = false;
  bool m_pose_overlay_enabled = false;
  bool m_gl_initialized = false;
  bool m_controls_overlay_visible = true;
  bool m_pan_up_pressed = false;
  bool m_pan_down_pressed = false;
  bool m_pan_left_pressed = false;
  bool m_pan_right_pressed = false;
  QString m_last_selection_summary;
  float m_default_unit_speed = 2.2F;
  float m_fps = 0.0F;
};
