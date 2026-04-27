#include "arena_viewport.h"

#include "app/core/renderer_bootstrap.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/terrain.h"
#include "game/map/terrain_service.h"
#include "game/map/world_bootstrap.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/picking_service.h"
#include "game/systems/selection_system.h"
#include "game/systems/command_service.h"
#include "game/systems/troop_count_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"
#include "game/units/unit.h"
#include "render/gl/camera.h"
#include "render/ground/terrain_feature_manager.h"
#include "render/ground/terrain_scatter_manager.h"
#include "render/ground/terrain_surface_manager.h"
#include "render/scene_renderer.h"
#include "render/terrain_scene_proxy.h"

#include <QEvent>
#include <QDebug>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QWheelEvent>
#include <QtMath>
#include <QVector3D>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace {

constexpr int k_local_owner_id = 1;
constexpr int k_enemy_owner_id = 2;
constexpr int k_terrain_width = 96;
constexpr int k_terrain_height = 96;
constexpr float k_terrain_tile_size = 1.0F;
constexpr int k_selection_drag_threshold = 6;

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
    sum += value_noise(x * frequency, z * frequency, seed + i * 97U) * amplitude;
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

auto terrain_world_position(const Game::Map::TerrainField &field, int x, int z)
    -> QVector3D {
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
  m_picking_service = std::make_unique<Game::Systems::PickingService>();

  RendererBootstrap::initialize_world_systems(*m_world);
  configure_runtime();
  regenerateTerrain();
  resetCamera();

  m_frameClock.start();
  m_frameTimer.setInterval(16);
  connect(&m_frameTimer, &QTimer::timeout, this,
          qOverload<>(&ArenaViewport::update));
  m_frameTimer.start();
}

ArenaViewport::~ArenaViewport() {
  m_frameTimer.stop();
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
  owners.register_owner_with_id(k_local_owner_id, Game::Systems::OwnerType::Player,
                                "Arena Player");
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
  setWireframeEnabled(m_wireframe_enabled);
  m_frameClock.restart();
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
  if (m_frameClock.isValid()) {
    real_dt =
        std::clamp(static_cast<float>(m_frameClock.restart()) / 1000.0F, 0.0F, 0.1F);
  }
  float const simulation_dt = m_paused ? 0.0F : real_dt;

  sanitize_selection();
  m_camera->update(real_dt);

  if (!m_paused) {
    m_world->update(simulation_dt);
  }

  std::erase_if(m_units, [this](const std::unique_ptr<Game::Units::Unit> &unit) {
    return unit == nullptr || m_world->get_entity(unit->id()) == nullptr;
  });

  if (width() > 0 && height() > 0) {
    m_renderer->set_viewport(width(), height());
  }

  update_selected_entities();
  m_renderer->set_hovered_entity_id(m_hovered_entity_id);
  m_renderer->set_local_owner_id(k_local_owner_id);
  m_renderer->update_animation_time(simulation_dt);
  m_renderer->begin_frame();
  if (m_terrain_scene != nullptr) {
    m_terrain_scene->submit(*m_renderer, m_renderer->resources());
  }
  m_renderer->render_world(m_world.get());
  m_renderer->end_frame();

  if (m_normals_overlay_enabled || m_pose_overlay_enabled ||
      m_selection_drag_active) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    draw_debug_overlay(painter);
  }
}

void ArenaViewport::mousePressEvent(QMouseEvent *event) {
  m_last_mouse_pos = event->pos();
  if (event->button() == Qt::LeftButton) {
    m_selection_anchor = event->pos();
    m_selection_current = event->pos();
    m_selection_drag_active = true;
  }
  QOpenGLWidget::mousePressEvent(event);
}

void ArenaViewport::mouseMoveEvent(QMouseEvent *event) {
  QPoint const delta = event->pos() - m_last_mouse_pos;
  m_last_mouse_pos = event->pos();

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
  } else if ((event->buttons() & Qt::LeftButton) != 0 && m_selection_drag_active) {
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
    if ((m_selection_current - m_selection_anchor).manhattanLength() >=
        k_selection_drag_threshold) {
      select_entities_in_rect(QRect(m_selection_anchor, m_selection_current),
                              additive);
    } else {
      QPointF const click_pos = event->position();
      Engine::Core::EntityID const picked =
          Game::Systems::PickingService::pick_unit_first(
              static_cast<float>(click_pos.x()),
              static_cast<float>(click_pos.y()), *m_world, *m_camera, width(),
              height(), k_local_owner_id);
      select_entity(picked, additive);
    }
    m_selection_drag_active = false;
    update();
  } else if (event->button() == Qt::RightButton &&
             (event->modifiers() & Qt::AltModifier) == 0) {
    issue_move_order(event->position());
  }

  QOpenGLWidget::mouseReleaseEvent(event);
}

void ArenaViewport::wheelEvent(QWheelEvent *event) {
  if (m_camera != nullptr) {
    m_camera->zoom_distance(static_cast<float>(event->angleDelta().y()) / 1200.0F);
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
  if (m_picking_service == nullptr || m_world == nullptr || m_camera == nullptr ||
      width() <= 0 || height() <= 0) {
    m_hovered_entity_id = 0;
    return;
  }

  m_hovered_entity_id = m_picking_service->update_hover(
      static_cast<float>(pos.x()), static_cast<float>(pos.y()), *m_world,
      *m_camera, width(), height());
}

auto ArenaViewport::selection_system() const -> Game::Systems::SelectionSystem * {
  return m_world != nullptr ? m_world->get_system<Game::Systems::SelectionSystem>()
                            : nullptr;
}

void ArenaViewport::select_entity(Engine::Core::EntityID entity_id, bool additive) {
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
      static_cast<float>(normalized.left()), static_cast<float>(normalized.top()),
      static_cast<float>(normalized.right()),
      static_cast<float>(normalized.bottom()), *m_world, *m_camera, width(),
      height(), k_local_owner_id);
  for (auto entity_id : picked) {
    selection->select_unit(entity_id);
  }
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

  QVector3D target{};
  if (!Game::Systems::PickingService::screen_to_ground(
          *m_camera, width(), height(), screen_pos, target)) {
    return;
  }

  target.setY(Game::Map::TerrainService::instance().resolve_surface_world_y(
      target.x(), target.z(), 0.0F, target.y()));

  std::vector<Engine::Core::EntityID> const ids = selection->get_selected_units();
  std::vector<QVector3D> targets(ids.size(), target);
  clear_forced_animation_state(ids);

  Game::Systems::CommandService::MoveOptions options;
  options.group_move = ids.size() > 1;
  Game::Systems::CommandService::move_units(*m_world, ids, targets, options);
  update();
}

void ArenaViewport::sanitize_selection() {
  auto *selection = selection_system();
  if (selection == nullptr || m_world == nullptr) {
    return;
  }

  auto ids = selection->get_selected_units();
  for (auto entity_id : ids) {
    auto *entity = m_world->get_entity(entity_id);
    auto *unit = entity != nullptr ? entity->get_component<Engine::Core::UnitComponent>()
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

void ArenaViewport::regenerateTerrain() {
  std::vector<float> heights(static_cast<size_t>(k_terrain_width * k_terrain_height),
                             0.0F);
  std::vector<Game::Map::TerrainType> terrain_types(heights.size(),
                                                    Game::Map::TerrainType::Flat);

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
      float const primary = fbm_noise(world_x * m_terrain_settings.frequency,
                                      world_z * m_terrain_settings.frequency,
                                      static_cast<std::uint32_t>(
                                          std::max(0, m_terrain_settings.seed)),
                                      m_terrain_settings.octaves);
      float const detail = fbm_noise(world_x * m_terrain_settings.frequency * 2.4F,
                                     world_z * m_terrain_settings.frequency * 2.4F,
                                     static_cast<std::uint32_t>(
                                         std::max(0, m_terrain_settings.seed + 31)),
                                     std::max(1, m_terrain_settings.octaves - 1));
      float const ridge = 1.0F - std::abs(primary * 2.0F - 1.0F);
      float const radial =
          std::clamp(1.0F -
                         QVector3D(world_x, 0.0F, world_z).length() /
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

  Game::Map::BiomeSettings biome;
  Game::Map::apply_ground_type_defaults(biome, Game::Map::GroundType::ForestMud);
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

  m_surface->ground()->configure(height_map->getTileSize(), height_map->getWidth(),
                                 height_map->getHeight());
  m_surface->ground()->set_biome(terrain_service.biome_settings());
  m_surface->terrain()->configure(*height_map, terrain_service.biome_settings());
  m_features->configure(*height_map, terrain_service.road_segments());
  m_scatter->configure(*height_map, terrain_service.biome_settings(),
                       terrain_service.fire_camps());
  setWireframeEnabled(m_wireframe_enabled);
}

void ArenaViewport::setTerrainSeed(int seed) {
  m_terrain_settings.seed = std::max(0, seed);
}

void ArenaViewport::setTerrainHeightScale(float value) {
  m_terrain_settings.height_scale = std::max(0.0F, value);
}

void ArenaViewport::setTerrainOctaves(int value) {
  m_terrain_settings.octaves = clamp_octaves(value);
}

void ArenaViewport::setTerrainFrequency(float value) {
  m_terrain_settings.frequency = std::clamp(value, 0.01F, 2.0F);
}

void ArenaViewport::setWireframeEnabled(bool enabled) {
  m_wireframe_enabled = enabled;
  if (m_terrain_scene != nullptr && m_terrain_scene->terrain() != nullptr) {
    m_terrain_scene->terrain()->set_wireframe(enabled);
  }
  update();
}

void ArenaViewport::setNormalsOverlayEnabled(bool enabled) {
  m_normals_overlay_enabled = enabled;
  update();
}

void ArenaViewport::setSpawnNation(const QString &nationId) {
  Game::Systems::NationID parsed{};
  if (!Game::Systems::try_parse_nation_id(nationId, parsed)) {
    return;
  }
  m_spawn_nation_id = parsed;
  sync_spawn_selection_defaults();
}

void ArenaViewport::setSpawnUnitType(const QString &unitType) {
  Game::Units::TroopType parsed{};
  if (!Game::Units::tryParseTroopType(unitType, parsed)) {
    return;
  }
  m_spawn_unit_type = parsed;
}

void ArenaViewport::spawnUnit() {
  if (m_unit_factory == nullptr || m_world == nullptr) {
    return;
  }

  float const column = static_cast<float>(m_units.size() % 4);
  float const row = static_cast<float>(m_units.size() / 4);
  float const world_x = (column - 1.5F) * 3.5F;
  float const world_z = row * 4.0F - 6.0F;
  float const world_y = Game::Map::TerrainService::instance().resolve_surface_world_y(
      world_x, world_z, 0.0F, 0.0F);

  Game::Units::SpawnParams params;
  params.position = {world_x, world_y, world_z};
  params.player_id = k_local_owner_id;
  params.spawn_type = Game::Units::spawn_typeFromTroopType(m_spawn_unit_type);
  params.ai_controlled = false;
  params.nation_id = m_spawn_nation_id;

  auto unit = m_unit_factory->create(m_spawn_unit_type, *m_world, params);
  if (unit == nullptr) {
    return;
  }

  auto *entity = m_world->get_entity(unit->id());
  auto *unit_component =
      entity != nullptr ? entity->get_component<Engine::Core::UnitComponent>() : nullptr;
  if (unit_component != nullptr) {
    unit_component->speed = m_default_unit_speed;
  }

  Engine::Core::EntityID const entity_id = unit->id();
  m_units.push_back(std::move(unit));
  select_entity(entity_id);
  update();
}

void ArenaViewport::clearUnits() {
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

void ArenaViewport::setAnimationName(const QString &animationName) {
  m_animation_name = animationName.trimmed();
  if (m_animation_name.isEmpty()) {
    m_animation_name = QStringLiteral("Idle");
  }
}

void ArenaViewport::playSelectedAnimation() {
  if (m_animation_name.compare(QStringLiteral("Walk"), Qt::CaseInsensitive) == 0) {
    playWalkAnimation();
    return;
  }
  if (m_animation_name.compare(QStringLiteral("Attack"), Qt::CaseInsensitive) == 0) {
    playAttackAnimation();
    return;
  }
  if (m_animation_name.compare(QStringLiteral("Death"), Qt::CaseInsensitive) == 0) {
    playDeathAnimation();
    return;
  }
  playIdleAnimation();
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
         (m_units.back() == nullptr || m_world->get_entity(m_units.back()->id()) == nullptr)) {
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
    if (auto *movement = entity->get_component<Engine::Core::MovementComponent>()) {
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

void ArenaViewport::playIdleAnimation() {
  auto ids = selected_unit_ids_or_fallback();
  clear_forced_animation_state(ids);
  update();
}

void ArenaViewport::playWalkAnimation() {
  auto ids = selected_unit_ids_or_fallback();
  if (ids.empty() || m_world == nullptr) {
    return;
  }

  clear_forced_animation_state(ids);

  for (auto entity_id : ids) {
    auto *entity = m_world->get_entity(entity_id);
    auto *transform =
        entity != nullptr ? entity->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
    auto *movement =
        entity != nullptr ? entity->get_component<Engine::Core::MovementComponent>()
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

void ArenaViewport::playAttackAnimation() {
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
      combat_state = entity->add_component<Engine::Core::CombatStateComponent>();
    }
    if (combat_state == nullptr) {
      continue;
    }

    combat_state->animation_state = Engine::Core::CombatAnimationState::Advance;
    combat_state->state_time = 0.0F;
    combat_state->state_duration =
        Engine::Core::CombatStateComponent::kAdvanceDuration;
    combat_state->attack_variant = 0;
  }

  update();
}

void ArenaViewport::playDeathAnimation() {
  auto ids = selected_unit_ids_or_fallback();
  if (ids.empty() || m_world == nullptr) {
    return;
  }

  for (auto entity_id : ids) {
    auto *entity = m_world->get_entity(entity_id);
    if (entity == nullptr) {
      continue;
    }

    if (auto *movement = entity->get_component<Engine::Core::MovementComponent>()) {
      movement->has_target = false;
      movement->vx = 0.0F;
      movement->vz = 0.0F;
      movement->clear_path();
      movement->path_pending = false;
    }
    if (auto *renderable = entity->get_component<Engine::Core::RenderableComponent>()) {
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

void ArenaViewport::moveSelectedUnitForward() {
  auto ids = selected_unit_ids_or_fallback();
  if (ids.empty() || m_world == nullptr) {
    return;
  }

  clear_forced_animation_state(ids);

  for (auto entity_id : ids) {
    auto *entity = m_world->get_entity(entity_id);
    auto *transform =
        entity != nullptr ? entity->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
    auto *movement =
        entity != nullptr ? entity->get_component<Engine::Core::MovementComponent>()
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

void ArenaViewport::setMovementSpeed(float speed) {
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

void ArenaViewport::setSkeletonDebugEnabled(bool enabled) {
  m_pose_overlay_enabled = enabled;
  update();
}

void ArenaViewport::pauseSimulation(bool paused) {
  if (m_paused == paused) {
    return;
  }
  m_paused = paused;
  emit pausedChanged(m_paused);
  update();
}

void ArenaViewport::resetCamera() {
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
        entity != nullptr ? entity->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
    if (transform == nullptr) {
      continue;
    }
    transform->position.y = Game::Map::TerrainService::instance().resolve_surface_world_y(
        transform->position.x, transform->position.z, 0.0F, transform->position.y);
  }
}

void ArenaViewport::draw_debug_overlay(QPainter &painter) {
  draw_selection_marquee(painter);
  if (m_normals_overlay_enabled) {
    draw_terrain_normals(painter);
  }
  if (m_pose_overlay_enabled) {
    draw_pose_overlay(painter);
  }
}

void ArenaViewport::draw_selection_marquee(QPainter &painter) {
  if (!m_selection_drag_active) {
    return;
  }

  QRect const rect = QRect(m_selection_anchor, m_selection_current).normalized();
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
  if (m_camera == nullptr || m_world == nullptr || width() <= 0 || height() <= 0) {
    return;
  }

  auto ids = selected_unit_ids_or_fallback();
  if (ids.empty()) {
    return;
  }

  for (auto entity_id : ids) {
    auto *entity = m_world->get_entity(entity_id);
    auto *transform =
        entity != nullptr ? entity->get_component<Engine::Core::TransformComponent>()
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

  auto it = std::find_if(
      nation->available_troops.begin(), nation->available_troops.end(),
      [this](const Game::Systems::TroopType &troop) {
        return troop.unit_type == m_spawn_unit_type;
      });
  if (it == nation->available_troops.end()) {
    m_spawn_unit_type = nation->available_troops.front().unit_type;
  }
}
