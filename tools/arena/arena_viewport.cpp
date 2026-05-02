#include "arena_viewport.h"

#include "app/core/renderer_bootstrap.h"
#include "app/utils/movement_utils.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/game_config.h"
#include "game/map/terrain.h"
#include "game/map/terrain_service.h"
#include "game/map/world_bootstrap.h"
#include "game/systems/ai_system.h"
#include "game/systems/camera_service.h"
#include "game/systems/command_service.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/picking_service.h"
#include "game/systems/selection_system.h"
#include "game/systems/troop_count_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_config.h"
#include "game/units/troop_type.h"
#include "game/units/unit.h"
#include "render/gl/camera.h"
#include "render/ground/fog_renderer.h"
#include "render/ground/rain_renderer.h"
#include "render/ground/terrain_feature_manager.h"
#include "render/ground/terrain_scatter_manager.h"
#include "render/ground/terrain_surface_manager.h"
#include "render/scene_renderer.h"
#include "render/terrain_scene_proxy.h"

#include <QDebug>
#include <QEvent>
#include <QFocusEvent>
#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMap>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QVector3D>
#include <QWheelEvent>
#include <QtMath>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace {

constexpr int k_local_owner_id = 1;
constexpr int k_enemy_owner_id = 2;
constexpr int k_all_owners_filter = 0;
constexpr int k_terrain_width = 96;
constexpr int k_terrain_height = 96;
constexpr float k_terrain_tile_size = 1.0F;
constexpr int k_selection_drag_threshold = 6;

constexpr float k_unit_spawn_clearance = 3.25F;
constexpr float k_building_spawn_clearance = 5.0F;
constexpr float k_spawn_search_step_factor = 1.1F;
constexpr int k_max_spawn_search_ring = 20;

auto local_controllable_selection(
    Engine::Core::World *world,
    const std::vector<Engine::Core::EntityID> &selected_ids)
    -> std::vector<Engine::Core::EntityID> {
  std::vector<Engine::Core::EntityID> controllable_ids;
  if (world == nullptr) {
    return controllable_ids;
  }

  controllable_ids.reserve(selected_ids.size());
  for (auto entity_id : selected_ids) {
    auto *entity = world->get_entity(entity_id);
    auto *unit_component =
        entity != nullptr ? entity->get_component<Engine::Core::UnitComponent>()
                          : nullptr;
    if ((unit_component == nullptr) ||
        (unit_component->owner_id != k_local_owner_id) ||
        (unit_component->health <= 0) ||
        (entity != nullptr &&
         entity->has_component<Engine::Core::PendingRemovalComponent>())) {
      continue;
    }
    controllable_ids.push_back(entity_id);
  }
  return controllable_ids;
}

inline auto clamp_octaves(int octaves) -> int {
  return std::clamp(octaves, 1, 8);
}

inline auto hash_coords(int x, int z, std::uint32_t seed) -> std::uint32_t {
  std::uint32_t const ux = static_cast<std::uint32_t>(x) * 73856093U;
  std::uint32_t const uz = static_cast<std::uint32_t>(z) * 19349663U;
  return ux ^ uz ^ (seed * 83492791U + 0x9e3779b9U);
}

inline auto hash_to_unit(std::uint32_t h) -> float {
  h ^= h >> 17;
  h *= 0xed5ad4bbU;
  h ^= h >> 11;
  h *= 0xac4c1b51U;
  h ^= h >> 15;
  h *= 0x31848babU;
  h ^= h >> 14;
  return static_cast<float>(h & 0x00FFFFFFU) / static_cast<float>(0x01000000U);
}

inline auto smooth_lerp(float a, float b, float t) -> float {
  float const smooth = t * t * (3.0F - 2.0F * t);
  return a + (b - a) * smooth;
}

auto value_noise(float x, float z, std::uint32_t seed) -> float {
  int const ix0 = static_cast<int>(std::floor(x));
  int const iz0 = static_cast<int>(std::floor(z));
  int const ix1 = ix0 + 1;
  int const iz1 = iz0 + 1;

  float const tx = x - static_cast<float>(ix0);
  float const tz = z - static_cast<float>(iz0);

  float const n00 = hash_to_unit(hash_coords(ix0, iz0, seed));
  float const n10 = hash_to_unit(hash_coords(ix1, iz0, seed));
  float const n01 = hash_to_unit(hash_coords(ix0, iz1, seed));
  float const n11 = hash_to_unit(hash_coords(ix1, iz1, seed));

  float const nx0 = smooth_lerp(n00, n10, tx);
  float const nx1 = smooth_lerp(n01, n11, tx);
  return smooth_lerp(nx0, nx1, tz);
}

auto fbm_noise(float x, float z, std::uint32_t seed, int octaves) -> float {
  float sum = 0.0F;
  float amplitude = 1.0F;
  float frequency = 1.0F;
  float normalization = 0.0F;

  for (int i = 0; i < clamp_octaves(octaves); ++i) {
    sum +=
        value_noise(x * frequency, z * frequency, seed + i * 97U) * amplitude;
    normalization += amplitude;
    amplitude *= 0.5F;
    frequency *= 2.0F;
  }

  return normalization > 0.0F ? (sum / normalization) : 0.0F;
}

auto sample_grid(const Game::Map::TerrainField &field, int x, int z) -> float {
  if (field.heights.empty() || field.width <= 0 || field.height <= 0) {
    return 0.0F;
  }
  x = std::clamp(x, 0, field.width - 1);
  z = std::clamp(z, 0, field.height - 1);
  return field.heights[static_cast<size_t>(z * field.width + x)];
}

auto terrain_world_position(const Game::Map::TerrainField &field, int x,
                            int z) -> QVector3D {
  float const half_width = static_cast<float>(field.width) * 0.5F - 0.5F;
  float const half_height = static_cast<float>(field.height) * 0.5F - 0.5F;
  float const world_x = (static_cast<float>(x) - half_width) * field.tile_size;
  float const world_z = (static_cast<float>(z) - half_height) * field.tile_size;
  float const world_y = sample_grid(field, x, z);
  return {world_x, world_y, world_z};
}

auto classify_terrain(float normalized_height) -> Game::Map::TerrainType {
  if (normalized_height > 0.72F) {
    return Game::Map::TerrainType::Mountain;
  }
  if (normalized_height > 0.28F) {
    return Game::Map::TerrainType::Hill;
  }
  return Game::Map::TerrainType::Flat;
}

auto prettify_identifier(const QString &value) -> QString {
  QString label = value;
  label.replace(QLatin1Char('_'), QLatin1Char(' '));
  QStringList parts = label.split(QLatin1Char(' '), Qt::SkipEmptyParts);
  for (QString &part : parts) {
    if (!part.isEmpty()) {
      part[0] = part[0].toUpper();
    }
  }
  return parts.join(QLatin1Char(' '));
}

auto is_mounted_spawn_type(Game::Units::SpawnType spawn_type) -> bool {
  using Game::Units::SpawnType;
  return spawn_type == SpawnType::MountedKnight ||
         spawn_type == SpawnType::HorseArcher ||
         spawn_type == SpawnType::HorseSpearman;
}

auto resolved_individuals_per_unit(const Engine::Core::UnitComponent &unit)
    -> int {
  if (unit.render_individuals_per_unit_override > 0) {
    return unit.render_individuals_per_unit_override;
  }
  return Game::Units::TroopConfig::instance().get_individuals_per_unit(
      unit.spawn_type);
}

auto entity_spawn_clearance(const Engine::Core::Entity &entity) -> float {
  return entity.has_component<Engine::Core::BuildingComponent>()
             ? k_building_spawn_clearance
             : k_unit_spawn_clearance;
}

} // namespace

ArenaViewport::ArenaViewport(QWidget *parent)
    : QOpenGLWidget(parent),
      m_spawn_nation_id(Game::Systems::NationID::RomanRepublic),
      m_spawn_unit_type(Game::Units::TroopType::Swordsman) {
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  setMinimumSize(800, 600);

  auto rendering = RendererBootstrap::initialize_rendering();
  m_world = std::make_unique<Engine::Core::World>();
  m_renderer = std::move(rendering.renderer);
  m_camera = std::move(rendering.camera);
  m_terrain_scene = std::move(rendering.terrain_scene);
  m_surface = std::move(rendering.surface);
  m_features = std::move(rendering.features);
  m_scatter = std::move(rendering.scatter);
  m_fog = std::move(rendering.fog);
  m_rain = std::move(rendering.rain);
  m_camera_service = std::make_unique<Game::Systems::CameraService>();
  m_picking_service = std::make_unique<Game::Systems::PickingService>();
  if (m_renderer != nullptr) {
    m_renderer->set_force_full_creature_lod(true);
  }

  RendererBootstrap::initialize_world_systems(*m_world);
  configure_runtime();
  regenerate_terrain();
  reset_camera();

  m_frame_clock.start();
  m_frame_timer.setInterval(16);
  connect(&m_frame_timer, &QTimer::timeout, this,
          qOverload<>(&ArenaViewport::update));
  m_frame_timer.start();
}

ArenaViewport::~ArenaViewport() {
  m_frame_timer.stop();
  m_units.clear();
  Game::Map::TerrainService::instance().clear();

  if (context() != nullptr) {
    makeCurrent();
    m_terrain_scene.reset();
    m_scatter.reset();
    m_features.reset();
    m_surface.reset();
    m_fog.reset();
    m_rain.reset();
    if (m_renderer != nullptr) {
      m_renderer->shutdown();
      m_renderer.reset();
    }
    doneCurrent();
  }
}

void ArenaViewport::configure_runtime() {
  Game::Systems::NationRegistry::instance().initialize_defaults();
  Game::Systems::TroopCountRegistry::instance().initialize();
  Game::Systems::CommandService::initialize(k_terrain_width, k_terrain_height);

  m_unit_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
  Game::Units::registerBuiltInUnits(*m_unit_factory);

  setup_default_players();
  sync_spawn_selection_defaults();
}

void ArenaViewport::setup_default_players() {
  auto &owners = Game::Systems::OwnerRegistry::instance();
  owners.clear();
  owners.register_owner_with_id(
      k_local_owner_id, Game::Systems::OwnerType::Player, "Arena Player");
  owners.register_owner_with_id(k_enemy_owner_id, Game::Systems::OwnerType::AI,
                                "Arena Opponent");
  owners.set_owner_team(k_local_owner_id, 1);
  owners.set_owner_team(k_enemy_owner_id, 2);
  owners.set_local_player_id(k_local_owner_id);

  auto &nations = Game::Systems::NationRegistry::instance();
  nations.clear_player_assignments();
  nations.set_player_nation(k_local_owner_id,
                            Game::Systems::NationID::RomanRepublic);
  nations.set_player_nation(k_enemy_owner_id,
                            Game::Systems::NationID::Carthage);

  if (m_world != nullptr) {
    if (auto *ai_system = m_world->get_system<Game::Systems::AISystem>()) {
      ai_system->reinitialize();
    }
  }
}

void ArenaViewport::initializeGL() {
  if (m_gl_initialized || m_renderer == nullptr || m_camera == nullptr) {
    return;
  }

  QString error;
  m_gl_initialized = Game::Map::WorldBootstrap::initialize(
      *m_renderer, *m_camera,
      m_terrain_scene != nullptr ? m_terrain_scene->ground() : nullptr, &error);

  if (!m_gl_initialized) {
    qWarning() << "ArenaViewport:" << error;
    return;
  }

  configure_rendering_from_terrain();
  set_wireframe_enabled(m_wireframe_enabled);
  m_frame_clock.restart();
}

void ArenaViewport::resizeGL(int width, int height) {
  if (m_renderer != nullptr && width > 0 && height > 0) {
    m_renderer->set_viewport(width, height);
  }
}

void ArenaViewport::paintGL() {
  if (!m_gl_initialized || m_renderer == nullptr || m_camera == nullptr ||
      m_world == nullptr) {
    return;
  }

  float real_dt = 1.0F / 60.0F;
  if (m_frame_clock.isValid()) {
    real_dt = std::clamp(static_cast<float>(m_frame_clock.restart()) / 1000.0F,
                         0.0F, 0.1F);
  }
  m_fps = real_dt > 0.0F ? (m_fps * 0.9F + (1.0F / real_dt) * 0.1F) : m_fps;
  float const simulation_dt = m_paused ? 0.0F : real_dt;

  sanitize_selection();
  apply_keyboard_camera_controls(real_dt);
  m_camera->update(real_dt);

  if (!m_paused) {
    m_world->update(simulation_dt);
  }

  std::erase_if(
      m_units, [this](const std::unique_ptr<Game::Units::Unit> &unit) {
        return unit == nullptr || m_world->get_entity(unit->id()) == nullptr;
      });

  if (width() > 0 && height() > 0) {
    m_renderer->set_viewport(width(), height());
  }

  update_selected_entities();
  sync_selection_summary();
  m_renderer->set_hovered_entity_id(m_hovered_entity_id);
  m_renderer->set_local_owner_id(k_local_owner_id);
  m_renderer->update_animation_time(simulation_dt);
  m_renderer->begin_frame();
  if (m_terrain_scene != nullptr) {
    m_terrain_scene->submit(*m_renderer, m_renderer->resources());
  }
  m_renderer->render_world(m_world.get());
  m_renderer->end_frame();

  {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    draw_debug_overlay(painter);
  }

  {
    QPainter stats_painter(this);
    stats_painter.setRenderHint(QPainter::Antialiasing, false);
    draw_stats_overlay(stats_painter);
    if (m_controls_overlay_visible) {
      draw_controls_overlay(stats_painter);
    }
  }
}

void ArenaViewport::keyPressEvent(QKeyEvent *event) {
  if (event == nullptr) {
    return;
  }

  switch (event->key()) {
  case Qt::Key_Up:
    m_pan_up_pressed = true;
    event->accept();
    return;
  case Qt::Key_Down:
    m_pan_down_pressed = true;
    event->accept();
    return;
  case Qt::Key_Left:
    m_pan_left_pressed = true;
    event->accept();
    return;
  case Qt::Key_Right:
    m_pan_right_pressed = true;
    event->accept();
    return;
  case Qt::Key_Q:
    if (m_camera_service != nullptr && m_camera != nullptr) {
      float const yaw_step =
          (event->modifiers() & Qt::ShiftModifier) != 0 ? 8.0F : 4.0F;
      m_camera_service->yaw(*m_camera, -yaw_step);
      update();
    }
    event->accept();
    return;
  case Qt::Key_E:
    if (m_camera_service != nullptr && m_camera != nullptr) {
      float const yaw_step =
          (event->modifiers() & Qt::ShiftModifier) != 0 ? 8.0F : 4.0F;
      m_camera_service->yaw(*m_camera, yaw_step);
      update();
    }
    event->accept();
    return;
  case Qt::Key_R:
    if (m_camera_service != nullptr && m_camera != nullptr) {
      m_camera_service->orbit_direction(
          *m_camera, 1, (event->modifiers() & Qt::ShiftModifier) != 0);
      update();
    }
    event->accept();
    return;
  case Qt::Key_F:
    if (m_camera_service != nullptr && m_camera != nullptr) {
      m_camera_service->orbit_direction(
          *m_camera, -1, (event->modifiers() & Qt::ShiftModifier) != 0);
      update();
    }
    event->accept();
    return;
  case Qt::Key_X:
    if (!event->isAutoRepeat()) {
      select_all_local_units();
    }
    event->accept();
    return;
  case Qt::Key_Space:
    if (!event->isAutoRepeat()) {
      pause_simulation(!m_paused);
    }
    event->accept();
    return;
  case Qt::Key_F1:
  case Qt::Key_Question:
    if (!event->isAutoRepeat()) {
      m_controls_overlay_visible = !m_controls_overlay_visible;
      update();
    }
    event->accept();
    return;
  default:
    break;
  }

  QOpenGLWidget::keyPressEvent(event);
}

void ArenaViewport::keyReleaseEvent(QKeyEvent *event) {
  if (event == nullptr) {
    return;
  }

  switch (event->key()) {
  case Qt::Key_Up:
    m_pan_up_pressed = false;
    event->accept();
    return;
  case Qt::Key_Down:
    m_pan_down_pressed = false;
    event->accept();
    return;
  case Qt::Key_Left:
    m_pan_left_pressed = false;
    event->accept();
    return;
  case Qt::Key_Right:
    m_pan_right_pressed = false;
    event->accept();
    return;
  default:
    break;
  }

  QOpenGLWidget::keyReleaseEvent(event);
}

void ArenaViewport::focusOutEvent(QFocusEvent *event) {
  clear_camera_key_state();
  QOpenGLWidget::focusOutEvent(event);
}

void ArenaViewport::mousePressEvent(QMouseEvent *event) {
  setFocus(Qt::MouseFocusReason);
  m_last_mouse_pos = event->pos();
  m_last_mouse_pos_valid = true;
  if (event->button() == Qt::LeftButton) {
    m_selection_anchor = event->pos();
    m_selection_current = event->pos();
    m_selection_drag_active = true;
  } else if (event->button() == Qt::RightButton &&
             (event->modifiers() & Qt::AltModifier) == 0) {
    issue_move_order(event->position());
  }
  QOpenGLWidget::mousePressEvent(event);
}

void ArenaViewport::mouseMoveEvent(QMouseEvent *event) {
  QPoint const delta = event->pos() - m_last_mouse_pos;
  m_last_mouse_pos = event->pos();
  m_last_mouse_pos_valid = true;

  if (m_camera != nullptr && (event->buttons() & Qt::MiddleButton) != 0) {
    if ((event->modifiers() & Qt::ShiftModifier) != 0) {
      m_camera->pan(-static_cast<float>(delta.x()) * 0.05F,
                    static_cast<float>(delta.y()) * 0.05F);
    } else {
      m_camera->orbit(-static_cast<float>(delta.x()) * 0.45F,
                      -static_cast<float>(delta.y()) * 0.25F);
    }
    update();
  } else if (m_camera != nullptr && (event->buttons() & Qt::RightButton) != 0 &&
             (event->modifiers() & Qt::AltModifier) != 0) {
    m_camera->pan(-static_cast<float>(delta.x()) * 0.05F,
                  static_cast<float>(delta.y()) * 0.05F);
    update();
  } else if ((event->buttons() & Qt::LeftButton) != 0 &&
             m_selection_drag_active) {
    m_selection_current = event->pos();
    update();
  } else {
    update_hover(event->pos());
  }

  QOpenGLWidget::mouseMoveEvent(event);
}

void ArenaViewport::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton && m_camera != nullptr &&
      m_world != nullptr && width() > 0 && height() > 0) {
    m_selection_current = event->pos();
    bool const additive = (event->modifiers() & Qt::ShiftModifier) != 0;
    bool const dragged =
        (m_selection_current - m_selection_anchor).manhattanLength() >=
        k_selection_drag_threshold;
    if (dragged) {
      select_entities_in_rect(QRect(m_selection_anchor, m_selection_current),
                              additive);
    } else {
      QPointF const click_pos = event->position();
      update_spawn_anchor_from_click(click_pos);
      Engine::Core::EntityID const picked =
          Game::Systems::PickingService::pick_single(
              static_cast<float>(click_pos.x()),
              static_cast<float>(click_pos.y()), *m_world, *m_camera, width(),
              height(), k_local_owner_id, true);
      select_entity(picked, additive);
    }
    m_selection_drag_active = false;
    update();
  }

  QOpenGLWidget::mouseReleaseEvent(event);
}

void ArenaViewport::wheelEvent(QWheelEvent *event) {
  if (m_camera != nullptr) {
    m_camera->zoom_distance(static_cast<float>(event->angleDelta().y()) /
                            1200.0F);
    update();
  }
  QOpenGLWidget::wheelEvent(event);
}

void ArenaViewport::leaveEvent(QEvent *event) {
  m_hovered_entity_id = 0;
  m_selection_drag_active = false;
  QOpenGLWidget::leaveEvent(event);
}

void ArenaViewport::update_hover(const QPoint &pos) {
  if (m_picking_service == nullptr || m_world == nullptr ||
      m_camera == nullptr || width() <= 0 || height() <= 0) {
    m_hovered_entity_id = 0;
    return;
  }

  m_hovered_entity_id = m_picking_service->update_hover(
      static_cast<float>(pos.x()), static_cast<float>(pos.y()), *m_world,
      *m_camera, width(), height());
}

auto ArenaViewport::selection_system() const
    -> Game::Systems::SelectionSystem * {
  return m_world != nullptr
             ? m_world->get_system<Game::Systems::SelectionSystem>()
             : nullptr;
}

void ArenaViewport::select_entity(Engine::Core::EntityID entity_id,
                                  bool additive) {
  auto *selection = selection_system();
  if (selection == nullptr) {
    return;
  }

  if (!additive) {
    selection->clear_selection();
  }
  if (entity_id != 0U) {
    selection->select_unit(entity_id);
  }
  update();
}

void ArenaViewport::select_entities_in_rect(const QRect &selection_rect,
                                            bool additive) {
  auto *selection = selection_system();
  if (selection == nullptr || m_world == nullptr || m_camera == nullptr ||
      width() <= 0 || height() <= 0) {
    return;
  }

  if (!additive) {
    selection->clear_selection();
  }

  QRect const normalized = selection_rect.normalized();
  auto picked = Game::Systems::PickingService::pick_in_rect(
      static_cast<float>(normalized.left()),
      static_cast<float>(normalized.top()),
      static_cast<float>(normalized.right()),
      static_cast<float>(normalized.bottom()), *m_world, *m_camera, width(),
      height(), k_local_owner_id);
  for (auto entity_id : picked) {
    selection->select_unit(entity_id);
  }
}

void ArenaViewport::select_all_local_units() {
  auto *selection = selection_system();
  if (selection == nullptr || m_world == nullptr) {
    return;
  }

  selection->clear_selection();
  for (const auto &unit : m_units) {
    if (unit == nullptr) {
      continue;
    }

    auto *entity = m_world->get_entity(unit->id());
    auto *unit_component =
        entity != nullptr ? entity->get_component<Engine::Core::UnitComponent>()
                          : nullptr;
    if ((unit_component == nullptr) ||
        (unit_component->owner_id != k_local_owner_id) ||
        (unit_component->health <= 0) ||
        (entity != nullptr &&
         entity->has_component<Engine::Core::PendingRemovalComponent>())) {
      continue;
    }

    selection->select_unit(entity->get_id());
  }
  update();
}

void ArenaViewport::issue_move_order(const QPointF &screen_pos) {
  if (m_world == nullptr || m_camera == nullptr || width() <= 0 ||
      height() <= 0) {
    return;
  }

  sanitize_selection();
  auto *selection = selection_system();
  if (selection == nullptr || selection->get_selected_units().empty()) {
    return;
  }

  std::vector<Engine::Core::EntityID> const ids = local_controllable_selection(
      m_world.get(), selection->get_selected_units());
  if (ids.empty()) {
    return;
  }
  clear_forced_animation_state(ids);
  App::Utils::issue_move_or_attack_command(
      m_world.get(), ids, m_picking_service.get(), m_camera.get(),
      screen_pos.x(), screen_pos.y(), width(), height(), k_local_owner_id);
  update();
}

void ArenaViewport::update_spawn_anchor_from_click(const QPointF &screen_pos) {
  if (m_camera == nullptr || width() <= 0 || height() <= 0) {
    return;
  }

  QVector3D spawn_anchor;
  if (!Game::Systems::PickingService::screen_to_ground(
          *m_camera, width(), height(), screen_pos, spawn_anchor)) {
    return;
  }

  m_spawn_anchor_world = App::Utils::snap_to_walkable_ground(spawn_anchor);
  m_spawn_anchor_world_valid = true;
}

auto ArenaViewport::resolve_spawn_anchor_world() const -> QVector3D {
  if (m_spawn_anchor_world_valid) {
    return App::Utils::snap_to_walkable_ground(m_spawn_anchor_world);
  }

  return App::Utils::snap_to_walkable_ground(
      Game::Map::TerrainService::instance().resolve_surface_world_position(
          0.0F, 0.0F, 0.0F, 0.0F));
}

auto ArenaViewport::is_spawn_position_available(const QVector3D &position,
                                                float clearance) const -> bool {
  if (Game::Map::TerrainService::instance().is_forbidden_world(position.x(),
                                                               position.z())) {
    return false;
  }

  if (m_world == nullptr) {
    return true;
  }

  for (const auto &unit : m_units) {
    if (unit == nullptr) {
      continue;
    }

    auto *entity = m_world->get_entity(unit->id());
    auto *transform =
        entity != nullptr
            ? entity->get_component<Engine::Core::TransformComponent>()
            : nullptr;
    auto *unit_component =
        entity != nullptr ? entity->get_component<Engine::Core::UnitComponent>()
                          : nullptr;
    if (entity == nullptr || transform == nullptr ||
        unit_component == nullptr || unit_component->health <= 0 ||
        entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    float const dx = transform->position.x - position.x();
    float const dz = transform->position.z - position.z();
    float const min_distance =
        std::max(clearance, entity_spawn_clearance(*entity));
    if ((dx * dx + dz * dz) < (min_distance * min_distance)) {
      return false;
    }
  }

  return true;
}

auto ArenaViewport::find_available_spawn_position(
    const QVector3D &anchor, float clearance) const -> QVector3D {
  auto resolve_candidate = [anchor](float world_x, float world_z) {
    return App::Utils::snap_to_walkable_ground(
        Game::Map::TerrainService::instance().resolve_surface_world_position(
            world_x, world_z, 0.0F, anchor.y()));
  };

  QVector3D const snapped_anchor = resolve_candidate(anchor.x(), anchor.z());
  if (is_spawn_position_available(snapped_anchor, clearance)) {
    return snapped_anchor;
  }

  float const step = std::max(1.0F, clearance * k_spawn_search_step_factor);
  for (int ring = 1; ring <= k_max_spawn_search_ring; ++ring) {
    for (int grid_z = -ring; grid_z <= ring; ++grid_z) {
      for (int grid_x = -ring; grid_x <= ring; ++grid_x) {
        if (std::max(std::abs(grid_x), std::abs(grid_z)) != ring) {
          continue;
        }

        QVector3D const candidate =
            resolve_candidate(anchor.x() + static_cast<float>(grid_x) * step,
                              anchor.z() + static_cast<float>(grid_z) * step);
        if (is_spawn_position_available(candidate, clearance)) {
          return candidate;
        }
      }
    }
  }

  return snapped_anchor;
}

void ArenaViewport::apply_keyboard_camera_controls(float real_dt) {
  if (m_camera_service == nullptr || m_camera == nullptr) {
    return;
  }

  int dx = 0;
  int dz = 0;
  if (m_pan_up_pressed) {
    dz += 1;
  }
  if (m_pan_down_pressed) {
    dz -= 1;
  }
  if (m_pan_left_pressed) {
    dx -= 1;
  }
  if (m_pan_right_pressed) {
    dx += 1;
  }
  if (dx == 0 && dz == 0) {
    return;
  }

  float const step =
      ((QGuiApplication::keyboardModifiers() & Qt::ShiftModifier) != 0) ? 2.0F
                                                                        : 1.0F;
  float const frame_scale = std::clamp(real_dt * 60.0F, 0.5F, 2.0F);
  m_camera_service->move(*m_camera, static_cast<float>(dx) * step * frame_scale,
                         static_cast<float>(dz) * step * frame_scale);
}

void ArenaViewport::clear_camera_key_state() {
  m_pan_up_pressed = false;
  m_pan_down_pressed = false;
  m_pan_left_pressed = false;
  m_pan_right_pressed = false;
}

auto ArenaViewport::owner_display_name(int owner_id) const -> QString {
  std::string const owner_name =
      Game::Systems::OwnerRegistry::instance().get_owner_name(owner_id);
  return owner_name.empty() ? QStringLiteral("Owner %1").arg(owner_id)
                            : QString::fromStdString(owner_name);
}

auto ArenaViewport::nation_display_name(Game::Systems::NationID nation_id) const
    -> QString {
  const auto *nation =
      Game::Systems::NationRegistry::instance().get_nation(nation_id);
  if (nation == nullptr) {
    return prettify_identifier(Game::Systems::nation_id_to_qstring(nation_id));
  }

  QString const label = QString::fromStdString(nation->display_name).trimmed();
  return label.isEmpty() ? prettify_identifier(
                               Game::Systems::nation_id_to_qstring(nation_id))
                         : label;
}

auto ArenaViewport::troop_display_name(Game::Systems::NationID nation_id,
                                       Game::Units::SpawnType spawnType) const
    -> QString {
  auto troop_type = Game::Units::spawn_typeToTroopType(spawnType);
  if (!troop_type.has_value()) {
    return QStringLiteral("Unknown");
  }

  const auto *nation =
      Game::Systems::NationRegistry::instance().get_nation(nation_id);
  if (nation != nullptr) {
    auto it = std::find_if(
        nation->available_troops.begin(), nation->available_troops.end(),
        [&troop_type](const Game::Systems::TroopType &entry) {
          return entry.unit_type == troop_type.value();
        });
    if (it != nation->available_troops.end()) {
      QString const label = QString::fromStdString(it->display_name).trimmed();
      if (!label.isEmpty()) {
        return label;
      }
    }
  }

  return prettify_identifier(Game::Units::troop_typeToQString(*troop_type));
}

void ArenaViewport::sanitize_selection() {
  auto *selection = selection_system();
  if (selection == nullptr || m_world == nullptr) {
    return;
  }

  auto ids = selection->get_selected_units();
  for (auto entity_id : ids) {
    auto *entity = m_world->get_entity(entity_id);
    auto *unit = entity != nullptr
                     ? entity->get_component<Engine::Core::UnitComponent>()
                     : nullptr;
    if (entity == nullptr || unit == nullptr || unit->health <= 0 ||
        entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      selection->deselect_unit(entity_id);
    }
  }
}

void ArenaViewport::update_selected_entities() {
  auto *selection = selection_system();
  if (selection == nullptr || m_renderer == nullptr) {
    return;
  }
  const auto &selected = selection->get_selected_units();
  std::vector<unsigned int> ids(selected.begin(), selected.end());
  m_renderer->set_selected_entities(ids);
}

void ArenaViewport::sync_selection_summary() {
  QString const summary = build_selection_summary();
  if (summary == m_last_selection_summary) {
    return;
  }
  m_last_selection_summary = summary;
  emit selection_summary_changed(summary);
}

auto ArenaViewport::build_selection_summary() const -> QString {
  auto *selection = selection_system();
  if (selection == nullptr || m_world == nullptr) {
    return QStringLiteral("No units selected.");
  }

  const auto &selected = selection->get_selected_units();
  if (selected.empty()) {
    return QStringLiteral("No units selected.");
  }

  int living_count = 0;
  int total_individuals = 0;
  int health_sum = 0;
  int max_health_sum = 0;
  int riderless_mounts = 0;
  float center_x = 0.0F;
  float center_z = 0.0F;
  QMap<QString, int> owner_counts;
  QMap<QString, int> composition_counts;

  for (auto entity_id : selected) {
    auto *entity = m_world->get_entity(entity_id);
    auto *unit = entity != nullptr
                     ? entity->get_component<Engine::Core::UnitComponent>()
                     : nullptr;
    auto *transform =
        entity != nullptr
            ? entity->get_component<Engine::Core::TransformComponent>()
            : nullptr;
    if (entity == nullptr || unit == nullptr || transform == nullptr ||
        unit->health <= 0 ||
        entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    ++living_count;
    total_individuals += resolved_individuals_per_unit(*unit);
    health_sum += std::max(0, unit->health);
    max_health_sum += std::max(1, unit->max_health);
    center_x += transform->position.x;
    center_z += transform->position.z;
    if (is_mounted_spawn_type(unit->spawn_type) && !unit->render_rider) {
      ++riderless_mounts;
    }
    owner_counts[owner_display_name(unit->owner_id)] += 1;
    composition_counts[QStringLiteral("%1 %2").arg(
        nation_display_name(unit->nation_id),
        troop_display_name(unit->nation_id, unit->spawn_type))] += 1;
  }

  if (living_count == 0) {
    return QStringLiteral("No units selected.");
  }

  QStringList owner_parts;
  for (auto it = owner_counts.cbegin(); it != owner_counts.cend(); ++it) {
    owner_parts << QStringLiteral("%1 x%2").arg(it.key()).arg(it.value());
  }

  QStringList composition_parts;
  for (auto it = composition_counts.cbegin(); it != composition_counts.cend();
       ++it) {
    composition_parts << QStringLiteral("%1 x%2").arg(it.key()).arg(it.value());
  }
  if (composition_parts.size() > 4) {
    int const hidden_count = composition_parts.size() - 4;
    composition_parts = composition_parts.mid(0, 4);
    composition_parts << QStringLiteral("+%1 more").arg(hidden_count);
  }

  center_x /= static_cast<float>(living_count);
  center_z /= static_cast<float>(living_count);
  int const health_percent =
      max_health_sum > 0 ? (health_sum * 100) / max_health_sum : 0;

  QStringList lines;
  lines << QStringLiteral("Selected: %1").arg(living_count);
  lines << QStringLiteral("Sides: %1")
               .arg(owner_parts.join(QStringLiteral(", ")));
  lines << QStringLiteral("Health: %1%").arg(health_percent);
  lines << QStringLiteral("Members: %1 total").arg(total_individuals);
  lines << QStringLiteral("Center: (%1, %2)")
               .arg(center_x, 0, 'f', 1)
               .arg(center_z, 0, 'f', 1);
  if (riderless_mounts > 0) {
    lines << QStringLiteral("Riderless mounts: %1").arg(riderless_mounts);
  }
  lines << QStringLiteral("Units: %1")
               .arg(composition_parts.join(QStringLiteral(", ")));
  return lines.join(QLatin1Char('\n'));
}

void ArenaViewport::regenerate_terrain() {
  std::vector<float> heights(
      static_cast<size_t>(k_terrain_width * k_terrain_height), 0.0F);
  std::vector<Game::Map::TerrainType> terrain_types(
      heights.size(), Game::Map::TerrainType::Flat);

  float const half_width = static_cast<float>(k_terrain_width) * 0.5F - 0.5F;
  float const half_height = static_cast<float>(k_terrain_height) * 0.5F - 0.5F;
  float const safe_scale = std::max(0.0F, m_terrain_settings.height_scale);
  float max_height = 0.0F;

  for (int z = 0; z < k_terrain_height; ++z) {
    for (int x = 0; x < k_terrain_width; ++x) {
      float const world_x =
          (static_cast<float>(x) - half_width) * k_terrain_tile_size;
      float const world_z =
          (static_cast<float>(z) - half_height) * k_terrain_tile_size;
      float const primary = fbm_noise(
          world_x * m_terrain_settings.frequency,
          world_z * m_terrain_settings.frequency,
          static_cast<std::uint32_t>(std::max(0, m_terrain_settings.seed)),
          m_terrain_settings.octaves);
      float const detail = fbm_noise(
          world_x * m_terrain_settings.frequency * 2.4F,
          world_z * m_terrain_settings.frequency * 2.4F,
          static_cast<std::uint32_t>(std::max(0, m_terrain_settings.seed + 31)),
          std::max(1, m_terrain_settings.octaves - 1));
      float const ridge = 1.0F - std::abs(primary * 2.0F - 1.0F);
      float const radial =
          std::clamp(1.0F - QVector3D(world_x, 0.0F, world_z).length() /
                                (static_cast<float>(k_terrain_width) * 0.72F),
                     0.35F, 1.0F);
      float const blended = std::clamp(
          (primary * 0.62F) + (detail * 0.23F) + (ridge * 0.15F), 0.0F, 1.0F);
      float const height = safe_scale * blended * radial;
      size_t const index = static_cast<size_t>(z * k_terrain_width + x);
      heights[index] = height;
      max_height = std::max(max_height, height);
    }
  }

  float const normalization = std::max(0.001F, max_height);
  for (size_t i = 0; i < heights.size(); ++i) {
    terrain_types[i] = classify_terrain(heights[i] / normalization);
  }

  float const arena_floor_height =
      std::min(safe_scale * 0.18F, std::max(0.0F, max_height * 0.25F));
  constexpr float k_arena_half_width = 18.0F;
  constexpr float k_arena_half_depth = 18.0F;
  for (int z = 0; z < k_terrain_height; ++z) {
    for (int x = 0; x < k_terrain_width; ++x) {
      float const world_x =
          (static_cast<float>(x) - half_width) * k_terrain_tile_size;
      float const world_z =
          (static_cast<float>(z) - half_height) * k_terrain_tile_size;
      if (std::abs(world_x) > k_arena_half_width ||
          std::abs(world_z) > k_arena_half_depth) {
        continue;
      }

      size_t const index = static_cast<size_t>(z * k_terrain_width + x);
      heights[index] = arena_floor_height;
      terrain_types[index] = Game::Map::TerrainType::Flat;
    }
  }

  Game::Map::BiomeSettings biome;
  Game::Map::apply_ground_type_defaults(biome, m_ground_type);
  biome.seed = static_cast<std::uint32_t>(std::max(0, m_terrain_settings.seed));
  biome.height_noise_frequency = m_terrain_settings.frequency;
  biome.height_noise_amplitude =
      std::clamp(m_terrain_settings.height_scale * 0.05F, 0.05F, 1.25F);

  Game::Map::TerrainService::instance().restore_from_serialized(
      k_terrain_width, k_terrain_height, k_terrain_tile_size, heights,
      terrain_types, {}, {}, {}, biome);
  Game::Systems::CommandService::initialize(k_terrain_width, k_terrain_height);

  align_units_to_terrain();
  if (m_gl_initialized) {
    configure_rendering_from_terrain();
  }
  update();
}

void ArenaViewport::configure_rendering_from_terrain() {
  auto &terrain_service = Game::Map::TerrainService::instance();
  auto *height_map = terrain_service.get_height_map();
  if (height_map == nullptr || m_surface == nullptr || m_features == nullptr ||
      m_scatter == nullptr) {
    return;
  }

  m_surface->ground()->configure(height_map->getTileSize(),
                                 height_map->getWidth(),
                                 height_map->getHeight());
  m_surface->ground()->set_biome(terrain_service.biome_settings());
  m_surface->terrain()->configure(*height_map,
                                  terrain_service.biome_settings());
  m_features->configure(*height_map, terrain_service.road_segments());
  m_scatter->configure(*height_map, terrain_service.biome_settings(),
                       terrain_service.fire_camps());
  if (m_rain != nullptr) {
    float const world_width =
        static_cast<float>(height_map->getWidth()) * height_map->getTileSize();
    float const world_height =
        static_cast<float>(height_map->getHeight()) * height_map->getTileSize();
    m_rain->configure(
        world_width, world_height,
        static_cast<std::uint32_t>(std::max(0, m_terrain_settings.seed)));
  }
  set_wireframe_enabled(m_wireframe_enabled);
}

void ArenaViewport::set_terrain_seed(int seed) {
  m_terrain_settings.seed = std::max(0, seed);
}

void ArenaViewport::set_terrain_height_scale(float value) {
  m_terrain_settings.height_scale = std::max(0.0F, value);
}

void ArenaViewport::set_terrain_octaves(int value) {
  m_terrain_settings.octaves = clamp_octaves(value);
}

void ArenaViewport::set_terrain_frequency(float value) {
  m_terrain_settings.frequency = std::clamp(value, 0.01F, 2.0F);
}

void ArenaViewport::set_wireframe_enabled(bool enabled) {
  m_wireframe_enabled = enabled;
  if (m_terrain_scene != nullptr && m_terrain_scene->terrain() != nullptr) {
    m_terrain_scene->terrain()->set_wireframe(enabled);
  }
  update();
}

void ArenaViewport::set_normals_overlay_enabled(bool enabled) {
  m_normals_overlay_enabled = enabled;
  update();
}

void ArenaViewport::set_ground_type(const QString &ground_type) {
  Game::Map::GroundType parsed = Game::Map::GroundType::ForestMud;
  if (!Game::Map::try_parse_ground_type(ground_type, parsed)) {
    qWarning() << "ArenaViewport: unknown ground type" << ground_type
               << "- defaulting to ForestMud";
  }
  if (m_ground_type == parsed) {
    return;
  }
  m_ground_type = parsed;
  regenerate_terrain();
}

void ArenaViewport::set_rain_enabled(bool enabled) {
  if (m_rain != nullptr) {
    m_rain->set_enabled(enabled);
  }
  update();
}

void ArenaViewport::set_rain_intensity(float intensity) {
  if (m_rain != nullptr) {
    m_rain->set_intensity(intensity);
  }
  update();
}

void ArenaViewport::set_spawn_owner(int owner_id) {
  if (owner_id == k_enemy_owner_id) {
    m_spawn_owner_id = k_enemy_owner_id;
    return;
  }
  m_spawn_owner_id = k_local_owner_id;
}

void ArenaViewport::set_spawn_nation(const QString &nation_id) {
  Game::Systems::NationID parsed{};
  if (!Game::Systems::try_parse_nation_id(nation_id, parsed)) {
    return;
  }
  m_spawn_nation_id = parsed;
  sync_spawn_selection_defaults();
}

void ArenaViewport::set_spawn_unit_type(const QString &unit_type) {
  Game::Units::TroopType parsed{};
  if (!Game::Units::tryParseTroopType(unit_type, parsed)) {
    return;
  }
  m_spawn_unit_type = parsed;
}

void ArenaViewport::set_spawn_individuals_per_unit(int count) {
  m_spawn_individuals_per_unit_override = std::max(0, count);
  auto *selection = selection_system();
  if (selection != nullptr && !selection->get_selected_units().empty()) {
    apply_visual_overrides_to_selection();
  }
}

void ArenaViewport::set_spawn_rider_visible(bool visible) {
  m_spawn_rider_visible = visible;
  auto *selection = selection_system();
  if (selection != nullptr && !selection->get_selected_units().empty()) {
    apply_visual_overrides_to_selection();
  }
}

void ArenaViewport::spawn_unit() { spawn_units(1); }

void ArenaViewport::spawn_units(int count) {
  int const clamped_count = std::clamp(count, 1, 128);
  std::vector<Engine::Core::EntityID> spawned_ids;
  spawned_ids.reserve(static_cast<size_t>(clamped_count));

  for (int i = 0; i < clamped_count; ++i) {
    Engine::Core::EntityID const entity_id = spawn_single_unit(
        m_spawn_owner_id, m_spawn_nation_id, m_spawn_unit_type);
    if (entity_id != 0U) {
      spawned_ids.push_back(entity_id);
    }
  }

  if (spawned_ids.empty()) {
    return;
  }

  select_spawned_entities(spawned_ids);
  update();
}

void ArenaViewport::spawn_opposing_batch(int count) {
  int const clamped_count = std::clamp(count, 1, 128);
  int const owner_id = m_spawn_owner_id == k_enemy_owner_id ? k_local_owner_id
                                                            : k_enemy_owner_id;
  std::vector<Engine::Core::EntityID> spawned_ids;
  spawned_ids.reserve(static_cast<size_t>(clamped_count));

  for (int i = 0; i < clamped_count; ++i) {
    Engine::Core::EntityID const entity_id =
        spawn_single_unit(owner_id, m_spawn_nation_id, m_spawn_unit_type);
    if (entity_id != 0U) {
      spawned_ids.push_back(entity_id);
    }
  }

  if (spawned_ids.empty()) {
    return;
  }

  select_spawned_entities(spawned_ids);
  update();
}

void ArenaViewport::spawn_mirror_match(int count) {
  int const clamped_count = std::clamp(count, 1, 128);
  std::vector<Engine::Core::EntityID> spawned_ids;
  spawned_ids.reserve(static_cast<size_t>(clamped_count * 2));

  for (int owner_id : {k_local_owner_id, k_enemy_owner_id}) {
    for (int i = 0; i < clamped_count; ++i) {
      Engine::Core::EntityID const entity_id =
          spawn_single_unit(owner_id, m_spawn_nation_id, m_spawn_unit_type);
      if (entity_id != 0U) {
        spawned_ids.push_back(entity_id);
      }
    }
  }

  if (spawned_ids.empty()) {
    return;
  }

  select_spawned_entities(spawned_ids);
  update();
}

void ArenaViewport::apply_visual_overrides_to_selection() {
  sanitize_selection();
  auto *selection = selection_system();
  if (selection == nullptr || m_world == nullptr) {
    return;
  }

  for (auto entity_id : selection->get_selected_units()) {
    auto *entity = m_world->get_entity(entity_id);
    auto *unit_component =
        entity != nullptr ? entity->get_component<Engine::Core::UnitComponent>()
                          : nullptr;
    if (unit_component == nullptr || unit_component->health <= 0 ||
        entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }
    unit_component->render_individuals_per_unit_override =
        m_spawn_individuals_per_unit_override;
    unit_component->render_rider = m_spawn_rider_visible;
  }

  update();
}

void ArenaViewport::select_spawned_entities(
    const std::vector<Engine::Core::EntityID> &ids) {
  auto *selection = selection_system();
  if (selection == nullptr) {
    return;
  }

  selection->clear_selection();
  for (auto entity_id : ids) {
    selection->select_unit(entity_id);
  }
}

auto ArenaViewport::spawn_single_unit() -> Engine::Core::EntityID {
  return spawn_single_unit(m_spawn_owner_id, m_spawn_nation_id,
                           m_spawn_unit_type);
}

auto ArenaViewport::resolve_spawn_unit_type(
    Game::Systems::NationID nation_id,
    Game::Units::TroopType preferred) const -> Game::Units::TroopType {
  const auto *nation =
      Game::Systems::NationRegistry::instance().get_nation(nation_id);
  if (nation == nullptr || nation->available_troops.empty()) {
    return preferred;
  }

  auto it = std::find_if(nation->available_troops.begin(),
                         nation->available_troops.end(),
                         [preferred](const Game::Systems::TroopType &troop) {
                           return troop.unit_type == preferred;
                         });
  return it != nation->available_troops.end()
             ? preferred
             : nation->available_troops.front().unit_type;
}

auto ArenaViewport::spawn_single_unit(
    int owner_id, Game::Systems::NationID nation_id,
    Game::Units::TroopType unit_type) -> Engine::Core::EntityID {
  if (m_unit_factory == nullptr || m_world == nullptr) {
    return 0U;
  }

  Game::Units::TroopType const resolved_unit_type =
      resolve_spawn_unit_type(nation_id, unit_type);
  QVector3D const spawn_position = find_available_spawn_position(
      resolve_spawn_anchor_world(), k_unit_spawn_clearance);

  Game::Units::SpawnParams params;
  params.position = spawn_position;
  params.player_id = owner_id;
  params.spawn_type = Game::Units::spawn_typeFromTroopType(resolved_unit_type);
  params.ai_controlled = (owner_id == k_enemy_owner_id);
  params.nation_id = nation_id;

  auto unit = m_unit_factory->create(resolved_unit_type, *m_world, params);
  if (unit == nullptr) {
    return 0U;
  }

  auto *entity = m_world->get_entity(unit->id());
  auto *transform =
      entity != nullptr
          ? entity->get_component<Engine::Core::TransformComponent>()
          : nullptr;
  auto *unit_component =
      entity != nullptr ? entity->get_component<Engine::Core::UnitComponent>()
                        : nullptr;
  if (transform != nullptr && owner_id == k_enemy_owner_id) {
    transform->rotation.y = 180.0F;
    transform->desired_yaw = 180.0F;
    transform->has_desired_yaw = true;
  }
  if (unit_component != nullptr) {
    unit_component->speed = m_default_unit_speed;
    unit_component->render_individuals_per_unit_override =
        m_spawn_individuals_per_unit_override;
    unit_component->render_rider = m_spawn_rider_visible;
  }

  Engine::Core::EntityID const entity_id = unit->id();
  m_units.push_back(std::move(unit));
  return entity_id;
}

void ArenaViewport::clear_units() {
  if (m_world == nullptr) {
    return;
  }

  auto *selection = selection_system();
  if (selection != nullptr) {
    selection->clear_selection();
  }
  for (const auto &unit : m_units) {
    if (unit != nullptr) {
      m_world->destroy_entity(unit->id());
    }
  }
  m_units.clear();
  m_hovered_entity_id = 0;
  update();
}

void ArenaViewport::set_spawn_building_owner(int owner_id) {
  m_spawn_building_owner_id =
      (owner_id == k_enemy_owner_id) ? k_enemy_owner_id : k_local_owner_id;
}

void ArenaViewport::set_spawn_building_nation(const QString &nation_id) {
  Game::Systems::NationID parsed{};
  if (!Game::Systems::try_parse_nation_id(nation_id, parsed)) {
    return;
  }
  m_spawn_building_nation_id = parsed;
}

void ArenaViewport::set_spawn_building_type(const QString &building_type) {
  Game::Units::SpawnType parsed{};
  if (!Game::Units::tryParseSpawnType(building_type, parsed) ||
      !Game::Units::is_building_spawn(parsed)) {
    return;
  }
  m_spawn_building_type = parsed;
}

void ArenaViewport::spawn_buildings(int count) {
  int const clamped_count = std::clamp(count, 1, 16);
  std::vector<Engine::Core::EntityID> spawned_ids;
  spawned_ids.reserve(static_cast<size_t>(clamped_count));

  for (int i = 0; i < clamped_count; ++i) {
    Engine::Core::EntityID const entity_id = spawn_single_building(
        m_spawn_building_owner_id, m_spawn_building_nation_id,
        m_spawn_building_type);
    if (entity_id != 0U) {
      spawned_ids.push_back(entity_id);
    }
  }

  if (spawned_ids.empty()) {
    return;
  }

  select_spawned_entities(spawned_ids);
  update();
}

auto ArenaViewport::spawn_single_building(
    int owner_id, Game::Systems::NationID nation_id,
    Game::Units::SpawnType building_type) -> Engine::Core::EntityID {
  if (m_unit_factory == nullptr || m_world == nullptr) {
    return 0U;
  }
  QVector3D const spawn_position = find_available_spawn_position(
      resolve_spawn_anchor_world(), k_building_spawn_clearance);

  Game::Units::SpawnParams params;
  params.position = spawn_position;
  params.player_id = owner_id;
  params.spawn_type = building_type;
  params.ai_controlled = false;
  params.nation_id = nation_id;

  auto unit = m_unit_factory->create(building_type, *m_world, params);
  if (unit == nullptr) {
    return 0U;
  }

  Engine::Core::EntityID const entity_id = unit->id();
  m_units.push_back(std::move(unit));
  return entity_id;
}

void ArenaViewport::clear_buildings() {
  if (m_world == nullptr) {
    return;
  }

  auto *selection = selection_system();

  auto it = m_units.begin();
  while (it != m_units.end()) {
    if (*it == nullptr) {
      it = m_units.erase(it);
      continue;
    }
    auto *entity = m_world->get_entity((*it)->id());
    auto *unit_component =
        entity != nullptr ? entity->get_component<Engine::Core::UnitComponent>()
                          : nullptr;
    if (unit_component != nullptr &&
        Game::Units::is_building_spawn(unit_component->spawn_type)) {
      if (selection != nullptr) {
        selection->deselect_unit((*it)->id());
      }
      m_world->destroy_entity((*it)->id());
      it = m_units.erase(it);
    } else {
      ++it;
    }
  }

  m_hovered_entity_id = 0;
  update();
}

void ArenaViewport::reset_arena() {
  clear_units();
  pause_simulation(false);
  reset_camera();
}

void ArenaViewport::set_animation_name(const QString &animation_name) {
  m_animation_name = animation_name.trimmed();
  if (m_animation_name.isEmpty()) {
    m_animation_name = QStringLiteral("Idle");
  }
}

void ArenaViewport::play_selected_animation() {
  if (m_animation_name.compare(QStringLiteral("Walk"), Qt::CaseInsensitive) ==
      0) {
    play_walk_animation();
    return;
  }
  if (m_animation_name.compare(QStringLiteral("Attack"), Qt::CaseInsensitive) ==
      0) {
    play_attack_animation();
    return;
  }
  if (m_animation_name.compare(QStringLiteral("Death"), Qt::CaseInsensitive) ==
      0) {
    play_death_animation();
    return;
  }
  play_idle_animation();
}

auto ArenaViewport::selected_unit_ids_or_fallback()
    -> std::vector<Engine::Core::EntityID> {
  sanitize_selection();
  auto *selection = selection_system();
  if (selection == nullptr) {
    return {};
  }

  if (!selection->get_selected_units().empty()) {
    return selection->get_selected_units();
  }

  while (!m_units.empty() &&
         (m_units.back() == nullptr ||
          m_world->get_entity(m_units.back()->id()) == nullptr)) {
    m_units.pop_back();
  }

  if (!m_units.empty()) {
    Engine::Core::EntityID const fallback_id = m_units.back()->id();
    select_entity(fallback_id);
    return {fallback_id};
  }

  return {};
}

void ArenaViewport::clear_forced_animation_state(
    const std::vector<Engine::Core::EntityID> &ids) {
  if (m_world == nullptr) {
    return;
  }

  for (auto entity_id : ids) {
    auto *entity = m_world->get_entity(entity_id);
    if (entity == nullptr) {
      continue;
    }
    if (auto *movement =
            entity->get_component<Engine::Core::MovementComponent>()) {
      movement->has_target = false;
      movement->vx = 0.0F;
      movement->vz = 0.0F;
      movement->clear_path();
      movement->path_pending = false;
    }
    entity->remove_component<Engine::Core::AttackTargetComponent>();
    entity->remove_component<Engine::Core::CombatStateComponent>();
    entity->remove_component<Engine::Core::HitFeedbackComponent>();
  }
}

void ArenaViewport::play_idle_animation() {
  auto ids = selected_unit_ids_or_fallback();
  clear_forced_animation_state(ids);
  update();
}

void ArenaViewport::play_walk_animation() {
  auto ids = selected_unit_ids_or_fallback();
  if (ids.empty() || m_world == nullptr) {
    return;
  }

  clear_forced_animation_state(ids);

  for (auto entity_id : ids) {
    auto *entity = m_world->get_entity(entity_id);
    auto *transform =
        entity != nullptr
            ? entity->get_component<Engine::Core::TransformComponent>()
            : nullptr;
    auto *movement =
        entity != nullptr
            ? entity->get_component<Engine::Core::MovementComponent>()
            : nullptr;
    if (transform == nullptr || movement == nullptr) {
      continue;
    }

    float const yaw_rad = qDegreesToRadians(transform->rotation.y);
    float const direction_x = std::sin(yaw_rad);
    float const direction_z = std::cos(yaw_rad);
    float const distance = std::max(2.0F, m_default_unit_speed * 1.8F);
    float const target_x = transform->position.x + direction_x * distance;
    float const target_z = transform->position.z + direction_z * distance;

    movement->target_x = target_x;
    movement->target_y = target_z;
    movement->goal_x = target_x;
    movement->goal_y = target_z;
    movement->has_target = true;
    movement->clear_path();
    movement->path_pending = false;
  }

  update();
}

void ArenaViewport::play_attack_animation() {
  auto ids = selected_unit_ids_or_fallback();
  if (ids.empty() || m_world == nullptr) {
    return;
  }

  clear_forced_animation_state(ids);

  for (auto entity_id : ids) {
    auto *entity = m_world->get_entity(entity_id);
    if (entity == nullptr) {
      continue;
    }

    auto *combat_state =
        entity->get_component<Engine::Core::CombatStateComponent>();
    if (combat_state == nullptr) {
      combat_state =
          entity->add_component<Engine::Core::CombatStateComponent>();
    }
    if (combat_state == nullptr) {
      continue;
    }

    combat_state->animation_state = Engine::Core::CombatAnimationState::Advance;
    combat_state->state_time = 0.0F;
    combat_state->state_duration =
        Engine::Core::CombatStateComponent::k_advance_duration;
    combat_state->attack_variant = 0;
  }

  update();
}

void ArenaViewport::play_death_animation() {
  auto ids = selected_unit_ids_or_fallback();
  if (ids.empty() || m_world == nullptr) {
    return;
  }

  for (auto entity_id : ids) {
    auto *entity = m_world->get_entity(entity_id);
    if (entity == nullptr) {
      continue;
    }

    if (auto *movement =
            entity->get_component<Engine::Core::MovementComponent>()) {
      movement->has_target = false;
      movement->vx = 0.0F;
      movement->vz = 0.0F;
      movement->clear_path();
      movement->path_pending = false;
    }
    if (auto *renderable =
            entity->get_component<Engine::Core::RenderableComponent>()) {
      renderable->visible = false;
    }
    if (auto *unit = entity->get_component<Engine::Core::UnitComponent>()) {
      unit->health = 0;
    }
    if (!entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      entity->add_component<Engine::Core::PendingRemovalComponent>();
    }
  }

  auto *selection = selection_system();
  if (selection != nullptr) {
    selection->clear_selection();
  }
  m_hovered_entity_id = 0;
  update();
}

void ArenaViewport::move_selected_unit_forward() {
  auto ids = selected_unit_ids_or_fallback();
  if (ids.empty() || m_world == nullptr) {
    return;
  }

  clear_forced_animation_state(ids);

  for (auto entity_id : ids) {
    auto *entity = m_world->get_entity(entity_id);
    auto *transform =
        entity != nullptr
            ? entity->get_component<Engine::Core::TransformComponent>()
            : nullptr;
    auto *movement =
        entity != nullptr
            ? entity->get_component<Engine::Core::MovementComponent>()
            : nullptr;
    if (transform == nullptr || movement == nullptr) {
      continue;
    }

    float const yaw_rad = qDegreesToRadians(transform->rotation.y);
    float const target_x = transform->position.x + std::sin(yaw_rad) * 5.0F;
    float const target_z = transform->position.z + std::cos(yaw_rad) * 5.0F;

    movement->target_x = target_x;
    movement->target_y = target_z;
    movement->goal_x = target_x;
    movement->goal_y = target_z;
    movement->has_target = true;
    movement->clear_path();
    movement->path_pending = false;
  }

  update();
}

void ArenaViewport::set_movement_speed(float speed) {
  m_default_unit_speed = std::max(0.1F, speed);
  if (m_world == nullptr) {
    return;
  }

  for (const auto &unit : m_units) {
    if (unit == nullptr) {
      continue;
    }
    auto *entity = m_world->get_entity(unit->id());
    auto *unit_component =
        entity != nullptr ? entity->get_component<Engine::Core::UnitComponent>()
                          : nullptr;
    if (unit_component != nullptr) {
      unit_component->speed = m_default_unit_speed;
    }
  }
}

void ArenaViewport::set_skeleton_debug_enabled(bool enabled) {
  m_pose_overlay_enabled = enabled;
  update();
}

void ArenaViewport::pause_simulation(bool paused) {
  if (m_paused == paused) {
    return;
  }
  m_paused = paused;
  emit paused_changed(m_paused);
  update();
}

void ArenaViewport::reset_camera() {
  if (m_camera == nullptr) {
    return;
  }
  m_camera->set_rts_view({0.0F, 0.0F, 0.0F}, 42.0F, 45.0F, 225.0F);
  update();
}

void ArenaViewport::align_units_to_terrain() {
  if (m_world == nullptr) {
    return;
  }

  for (const auto &unit : m_units) {
    if (unit == nullptr) {
      continue;
    }
    auto *entity = m_world->get_entity(unit->id());
    auto *transform =
        entity != nullptr
            ? entity->get_component<Engine::Core::TransformComponent>()
            : nullptr;
    if (transform == nullptr) {
      continue;
    }
    QVector3D const snapped = App::Utils::snap_to_walkable_ground(
        {transform->position.x, transform->position.y, transform->position.z});
    transform->position.x = snapped.x();
    transform->position.y = snapped.y();
    transform->position.z = snapped.z();
  }
}

void ArenaViewport::draw_debug_overlay(QPainter &painter) {
  draw_spawn_anchor_marker(painter);
  draw_selection_marquee(painter);
  if (m_normals_overlay_enabled) {
    draw_terrain_normals(painter);
  }
  if (m_pose_overlay_enabled) {
    draw_pose_overlay(painter);
  }
}

void ArenaViewport::draw_spawn_anchor_marker(QPainter &painter) {
  if (m_camera == nullptr || width() <= 0 || height() <= 0) {
    return;
  }

  QPointF screen_anchor;
  if (!Game::Systems::PickingService::world_to_screen(
          *m_camera, width(), height(), resolve_spawn_anchor_world(),
          screen_anchor)) {
    return;
  }

  QColor const marker_color = m_spawn_anchor_world_valid
                                  ? QColor(255, 210, 96, 235)
                                  : QColor(190, 190, 190, 200);
  QString const label = m_spawn_anchor_world_valid
                            ? QStringLiteral("Spawn")
                            : QStringLiteral("Spawn (default)");

  painter.save();
  painter.setPen(QPen(marker_color, 2.0));
  painter.setBrush(Qt::NoBrush);
  painter.drawEllipse(screen_anchor, 10.0, 10.0);
  painter.drawLine(screen_anchor + QPointF(-15.0, 0.0),
                   screen_anchor + QPointF(-4.0, 0.0));
  painter.drawLine(screen_anchor + QPointF(4.0, 0.0),
                   screen_anchor + QPointF(15.0, 0.0));
  painter.drawLine(screen_anchor + QPointF(0.0, -15.0),
                   screen_anchor + QPointF(0.0, -4.0));
  painter.drawLine(screen_anchor + QPointF(0.0, 4.0),
                   screen_anchor + QPointF(0.0, 15.0));
  painter.setBrush(marker_color);
  painter.drawEllipse(screen_anchor, 3.0, 3.0);

  QRectF const label_box(screen_anchor.x() + 14.0, screen_anchor.y() - 25.0,
                         110.0, 20.0);
  painter.fillRect(label_box, QColor(0, 0, 0, 140));
  painter.setPen(QColor(245, 245, 245, 230));
  painter.drawText(label_box.adjusted(6.0, 0.0, -6.0, 0.0),
                   Qt::AlignVCenter | Qt::AlignLeft, label);
  painter.restore();
}

void ArenaViewport::draw_selection_marquee(QPainter &painter) {
  if (!m_selection_drag_active) {
    return;
  }

  QRect const rect =
      QRect(m_selection_anchor, m_selection_current).normalized();
  if (rect.width() < k_selection_drag_threshold &&
      rect.height() < k_selection_drag_threshold) {
    return;
  }

  painter.save();
  painter.setPen(QPen(QColor(86, 170, 255, 220), 1.5F, Qt::DashLine));
  painter.fillRect(rect, QColor(86, 170, 255, 45));
  painter.drawRect(rect);
  painter.restore();
}

void ArenaViewport::draw_terrain_normals(QPainter &painter) {
  if (m_camera == nullptr || width() <= 0 || height() <= 0) {
    return;
  }
  const auto &field = Game::Map::TerrainService::instance().terrain_field();
  if (field.empty()) {
    return;
  }

  painter.setPen(QPen(QColor(80, 230, 180, 180), 1.0));

  int const step = std::max(4, std::min(field.width, field.height) / 18);
  for (int z = 0; z < field.height; z += step) {
    for (int x = 0; x < field.width; x += step) {
      float const h_l = sample_grid(field, x - 1, z);
      float const h_r = sample_grid(field, x + 1, z);
      float const h_d = sample_grid(field, x, z - 1);
      float const h_u = sample_grid(field, x, z + 1);

      QVector3D normal(-(h_r - h_l) / (2.0F * field.tile_size), 1.0F,
                       -(h_u - h_d) / (2.0F * field.tile_size));
      if (normal.lengthSquared() <= std::numeric_limits<float>::epsilon()) {
        continue;
      }
      normal.normalize();

      QVector3D const start = terrain_world_position(field, x, z);
      QVector3D const end = start + normal * 1.5F;

      QPointF screen_start;
      QPointF screen_end;
      if (!m_camera->world_to_screen(start, width(), height(), screen_start) ||
          !m_camera->world_to_screen(end, width(), height(), screen_end)) {
        continue;
      }
      painter.drawLine(screen_start, screen_end);
    }
  }
}

void ArenaViewport::draw_pose_overlay(QPainter &painter) {
  if (m_camera == nullptr || m_world == nullptr || width() <= 0 ||
      height() <= 0) {
    return;
  }

  auto ids = selected_unit_ids_or_fallback();
  if (ids.empty()) {
    return;
  }

  for (auto entity_id : ids) {
    auto *entity = m_world->get_entity(entity_id);
    auto *transform =
        entity != nullptr
            ? entity->get_component<Engine::Core::TransformComponent>()
            : nullptr;
    if (transform == nullptr) {
      continue;
    }

    QVector3D const origin(transform->position.x, transform->position.y + 1.0F,
                           transform->position.z);
    float const yaw_rad = qDegreesToRadians(transform->rotation.y);
    float const axis_length = std::max(0.8F, transform->scale.y * 1.5F);
    QVector3D const forward(std::sin(yaw_rad), 0.0F, std::cos(yaw_rad));
    QVector3D const right(std::cos(yaw_rad), 0.0F, -std::sin(yaw_rad));
    QVector3D const up(0.0F, 1.0F, 0.0F);

    struct AxisLine {
      QVector3D end;
      QColor color;
    };

    std::array<AxisLine, 3> axes{
        AxisLine{origin + right * axis_length, QColor(255, 120, 120, 220)},
        AxisLine{origin + up * axis_length, QColor(120, 220, 255, 220)},
        AxisLine{origin + forward * axis_length, QColor(160, 255, 120, 220)}};

    QPointF screen_origin;
    if (!m_camera->world_to_screen(origin, width(), height(), screen_origin)) {
      continue;
    }

    for (const auto &axis : axes) {
      QPointF screen_end;
      if (!m_camera->world_to_screen(axis.end, width(), height(), screen_end)) {
        continue;
      }
      painter.setPen(QPen(axis.color, 2.0));
      painter.drawLine(screen_origin, screen_end);
    }
  }
}

void ArenaViewport::sync_spawn_selection_defaults() {
  const auto *nation =
      Game::Systems::NationRegistry::instance().get_nation(m_spawn_nation_id);
  if (nation == nullptr || nation->available_troops.empty()) {
    return;
  }

  auto it = std::find_if(nation->available_troops.begin(),
                         nation->available_troops.end(),
                         [this](const Game::Systems::TroopType &troop) {
                           return troop.unit_type == m_spawn_unit_type;
                         });
  if (it == nation->available_troops.end()) {
    m_spawn_unit_type = nation->available_troops.front().unit_type;
  }
}

void ArenaViewport::draw_stats_overlay(QPainter &painter) {
  if (width() <= 0 || height() <= 0) {
    return;
  }

  int player_count = 0;
  int enemy_count = 0;
  if (m_world != nullptr) {
    for (const auto &unit : m_units) {
      if (unit == nullptr) {
        continue;
      }
      auto *entity = m_world->get_entity(unit->id());
      auto *uc = entity != nullptr
                     ? entity->get_component<Engine::Core::UnitComponent>()
                     : nullptr;
      if (uc == nullptr || uc->health <= 0) {
        continue;
      }
      if (uc->owner_id == k_local_owner_id) {
        ++player_count;
      } else if (uc->owner_id == k_enemy_owner_id) {
        ++enemy_count;
      }
    }
  }

  QFont font = painter.font();
  font.setPixelSize(13);
  font.setBold(true);
  painter.setFont(font);

  QFontMetrics const fm(font);
  int const line_h = fm.height() + 2;
  int const pad = 6;

  QStringList lines;
  lines << QStringLiteral("FPS: %1").arg(static_cast<int>(m_fps + 0.5F));
  lines << QStringLiteral("Player: %1").arg(player_count);
  lines << QStringLiteral("Enemy:  %1").arg(enemy_count);
  if (m_paused) {
    lines << QStringLiteral("PAUSED");
  }

  int const box_w = [&]() {
    int max_w = 0;
    for (const auto &line : lines) {
      max_w = std::max(max_w, fm.horizontalAdvance(line));
    }
    return max_w + pad * 2;
  }();
  int const box_h = lines.size() * line_h + pad * 2;

  QRect const box(pad, pad, box_w, box_h);
  painter.fillRect(box, QColor(0, 0, 0, 140));

  painter.setPen(QColor(220, 220, 220, 220));
  for (int i = 0; i < lines.size(); ++i) {
    painter.drawText(pad * 2, pad + (i + 1) * line_h - 2, lines[i]);
  }
}

void ArenaViewport::draw_controls_overlay(QPainter &painter) {
  if (width() <= 0 || height() <= 0) {
    return;
  }

  QFont title_font = painter.font();
  title_font.setPixelSize(13);
  title_font.setBold(true);
  QFont body_font = title_font;
  body_font.setBold(false);

  QString const title = QStringLiteral("Arena Controls");
  QStringList const lines{
      QStringLiteral("LMB click: select + set spawn anchor"),
      QStringLiteral("LMB drag: box select"),
      QStringLiteral("Shift+LMB: add to selection"),
      QStringLiteral("RMB: move / attack like the main game"),
      QStringLiteral("Wheel: zoom"),
      QStringLiteral("Middle drag: orbit"),
      QStringLiteral("Shift+Middle drag: pan"),
      QStringLiteral("Arrow keys: pan camera"),
      QStringLiteral("Shift+Arrows: faster pan"),
      QStringLiteral("Q / E: yaw"),
      QStringLiteral("R / F: orbit pitch"),
      QStringLiteral("Home: reset camera"),
      QStringLiteral("X: select local army"),
      QStringLiteral("Space: pause"),
      QStringLiteral("F1 or ?: toggle this help"),
  };

  QFontMetrics const title_metrics(title_font);
  QFontMetrics const body_metrics(body_font);
  int const pad = 8;
  int const title_gap = 4;
  int const line_h = body_metrics.height() + 2;

  int box_w = title_metrics.horizontalAdvance(title);
  for (const auto &line : lines) {
    box_w = std::max(box_w, body_metrics.horizontalAdvance(line));
  }
  box_w += pad * 2;

  int const box_h = pad * 2 + title_metrics.height() + title_gap +
                    static_cast<int>(lines.size()) * line_h;
  QRect const box(width() - box_w - pad, pad, box_w, box_h);
  painter.fillRect(box, QColor(0, 0, 0, 150));
  painter.setPen(QColor(235, 235, 235, 230));

  int y = box.top() + pad + title_metrics.ascent();
  painter.setFont(title_font);
  painter.drawText(box.left() + pad, y, title);

  painter.setFont(body_font);
  y += title_gap + (title_metrics.descent() + line_h);
  for (const auto &line : lines) {
    painter.drawText(box.left() + pad, y, line);
    y += line_h;
  }
}
