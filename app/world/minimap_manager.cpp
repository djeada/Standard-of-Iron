#include "app/world/minimap_manager.h"

#include <QDebug>
#include <QPainter>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_loader.h"
#include "game/map/render_visibility_rules.h"
#include "game/map/visibility_service.h"
#include "game/render_bridge/minimap/camera_viewport_layer.h"
#include "game/render_bridge/minimap/minimap_generator.h"
#include "game/render_bridge/minimap/minimap_utils.h"
#include "game/render_bridge/minimap/unit_layer.h"
#include "game/session/session_context.h"
#include "game/systems/selection_system.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"
#include "scene/camera.h"

namespace {
[[nodiscard]] auto hash_combine(std::uint64_t seed,
                                std::uint64_t value) noexcept -> std::uint64_t {
  seed ^= value + 0x9E3779B97F4A7C15ULL + (seed << 6U) + (seed >> 2U);
  return seed;
}

[[nodiscard]] auto quantize(float value, float scale) noexcept -> std::uint64_t {
  return static_cast<std::uint64_t>(
      static_cast<std::int64_t>(std::lround(value * scale)));
}

[[nodiscard]] auto classify_marker(Game::Units::SpawnType type) noexcept
    -> Game::Map::Minimap::MarkerClass {
  using Game::Map::Minimap::MarkerClass;
  switch (type) {
  case Game::Units::SpawnType::Barracks:
    return MarkerClass::Stronghold;
  case Game::Units::SpawnType::DefenseTower:
    return MarkerClass::Tower;
  case Game::Units::SpawnType::Temple:
  case Game::Units::SpawnType::Marketplace:
    return MarkerClass::Landmark;
  case Game::Units::SpawnType::Home:
  case Game::Units::SpawnType::Farm:
  case Game::Units::SpawnType::WallSegment:
  case Game::Units::SpawnType::WallGate:
    return MarkerClass::MinorStructure;
  default:
    return MarkerClass::Troop;
  }
}

[[nodiscard]] auto
capture_step_for(const Engine::Core::CaptureComponent& capture) -> std::uint8_t {
  if (!capture.is_being_captured || capture.capture_progress <= 0.0F) {
    return 0;
  }
  const float required = std::max(capture.required_time, 0.0001F);
  const float ratio = std::clamp(capture.capture_progress / required, 0.0F, 1.0F);
  const auto step = static_cast<int>(
      std::ceil(ratio * static_cast<float>(Game::Map::Minimap::k_capture_steps)));
  return static_cast<std::uint8_t>(
      std::clamp(step, 1, static_cast<int>(Game::Map::Minimap::k_capture_steps)));
}

constexpr float k_destination_cluster_grid = 2.0F;
constexpr float k_origin_hash_steps = 96.0F;
constexpr std::size_t k_max_pending_capture_alerts = 8;
constexpr std::size_t k_max_destinations = 8;
} // namespace

MinimapManager::MinimapManager() = default;

MinimapManager::~MinimapManager() = default;

bool MinimapManager::consume_dirty_flag() {
  bool const was_dirty = m_dirty;
  m_dirty = false;
  return was_dirty;
}

namespace {

constexpr int k_hud_minimap_max_dimension = 320;

} // namespace

void MinimapManager::generate_for_map(const Game::Map::MapDefinition& map_def) {

  Game::Map::Minimap::MinimapOrientation::instance().set_yaw_degrees(
      map_def.camera.yaw_deg);

  Game::Map::Minimap::MinimapGenerator::Config config;
  config.structure_bake =
      Game::Map::Minimap::MinimapGenerator::StructureBake::LandmarksOnly;
  config.max_image_dimension = k_hud_minimap_max_dimension;
  Game::Map::Minimap::MinimapGenerator generator(config);
  m_minimap_base_image = generator.generate(map_def);
  if (!m_minimap_base_image.isNull() &&
      m_minimap_base_image.format() != QImage::Format_ARGB32) {
    m_minimap_base_image = m_minimap_base_image.convertToFormat(QImage::Format_ARGB32);
  }

  if (!m_minimap_base_image.isNull()) {

    m_world_width = static_cast<float>(map_def.grid.width);
    m_world_height = static_cast<float>(map_def.grid.height);
    m_tile_size = map_def.grid.tile_size;

    m_fog_compositor.reset();
    m_minimap_fog_image = m_minimap_base_image.copy();
    m_minimap_units_image = m_minimap_fog_image;
    m_minimap_image = m_minimap_units_image;

    const float inv_tile =
        1.0F / std::max(m_tile_size, Game::Map::Minimap::Constants::k_min_tile_size);
    m_hash_position_scale =
        (static_cast<float>(m_minimap_base_image.width() - 1) / m_world_width) *
        inv_tile;

    m_capture_watch.clear();
    m_capture_alerts.clear();
    m_destinations.clear();
    m_destination_hash = 0;
    m_destinations_dirty = true;

    m_unit_layer = std::make_unique<Game::Map::Minimap::UnitLayer>();
    m_unit_layer->init(m_minimap_base_image.width(),
                       m_minimap_base_image.height(),
                       m_world_width,
                       m_world_height,
                       m_tile_size);

    m_camera_viewport_layer =
        std::make_unique<Game::Map::Minimap::CameraViewportLayer>();
    m_camera_viewport_layer->init(m_minimap_base_image.width(),
                                  m_minimap_base_image.height(),
                                  m_world_width,
                                  m_world_height);

    m_last_fog_composite_version = std::numeric_limits<std::uint64_t>::max();
    m_last_unit_hash = 0;
    m_camera_viewport_valid = false;
    m_viewport_composite_dirty = true;
    mark_dirty();
  } else {
    qWarning() << "MinimapManager: Failed to generate minimap";
  }
}

void MinimapManager::update_fog(
    const Game::Map::VisibilityService::Snapshot& snapshot) {
  if (m_minimap_base_image.isNull()) {
    return;
  }

  if (!snapshot.initialized || snapshot.cells.empty() || snapshot.width <= 0 ||
      snapshot.height <= 0) {
    clear_fog();
    return;
  }

  if (!m_fog_compositor.apply(m_minimap_base_image, snapshot, m_minimap_fog_image)) {
    return;
  }

  m_minimap_units_image = m_minimap_fog_image;
  m_minimap_image = m_minimap_units_image;
  m_viewport_composite_dirty = true;
  mark_dirty();
}

void MinimapManager::clear_fog() {
  if (m_minimap_base_image.isNull()) {
    return;
  }

  if (!m_fog_compositor.clear(m_minimap_base_image, m_minimap_fog_image)) {
    return;
  }

  m_last_fog_composite_version = std::numeric_limits<std::uint64_t>::max();
  m_minimap_units_image = m_minimap_fog_image;
  m_minimap_image = m_minimap_units_image;
  m_viewport_composite_dirty = true;
  mark_dirty();
}

bool MinimapManager::world_to_normalized(float world_x,
                                         float world_z,
                                         float& nx,
                                         float& ny) const {
  if (m_minimap_base_image.isNull() || m_world_width <= 0.0F ||
      m_world_height <= 0.0F) {
    return false;
  }
  const auto [normalized_x, normalized_y] = Game::Map::Minimap::world_to_normalized(
      world_x, world_z, m_world_width, m_world_height, m_tile_size);
  nx = normalized_x;
  ny = normalized_y;
  return true;
}

bool MinimapManager::consume_destinations_dirty() {
  const bool was_dirty = m_destinations_dirty;
  m_destinations_dirty = false;
  return was_dirty;
}

void MinimapManager::update_units(Engine::Core::World* world,
                                  Game::Systems::SelectionSystem* selection_system,
                                  int local_owner_id) {
  if (m_minimap_fog_image.isNull() || !m_unit_layer || (world == nullptr)) {
    return;
  }

  auto& markers = m_marker_scratch;
  markers.clear();

  auto& selected_ids = m_selected_scratch;
  selected_ids.clear();
  if (selection_system != nullptr) {
    const auto& sel = selection_system->get_selected_units();
    selected_ids.assign(sel.begin(), sel.end());
    std::sort(selected_ids.begin(), selected_ids.end());
  }

  auto& capture_watch = m_capture_watch_scratch;
  capture_watch.clear();

  auto& destinations = m_destination_scratch;
  auto& destination_cells = m_destination_cell_scratch;
  destinations.clear();
  destination_cells.clear();
  std::uint64_t destination_hash = 0;
  float selected_sum_x = 0.0F;
  float selected_sum_z = 0.0F;
  int selected_count = 0;

  std::uint64_t unit_hash = hash_combine(0, static_cast<std::uint64_t>(local_owner_id));

  {
    const std::lock_guard<std::recursive_mutex> lock(world->get_entity_mutex());

    for (auto [entity_id, unit, transform] :
         world->view<Engine::Core::UnitComponent, Engine::Core::TransformComponent>()) {
      if (unit.health <= 0 || Game::Units::is_wildlife_spawn(unit.spawn_type)) {
        continue;
      }

      Game::Map::Minimap::UnitMarker marker;
      marker.world_x = transform.position.x;
      marker.world_z = transform.position.z;
      marker.owner_id = unit.owner_id;
      marker.is_selected =
          std::binary_search(selected_ids.begin(), selected_ids.end(), entity_id);
      marker.marker_class = classify_marker(unit.spawn_type);

      if (marker.marker_class == Game::Map::Minimap::MarkerClass::Stronghold) {
        if (const auto* capture =
                world->try_get<Engine::Core::CaptureComponent>(entity_id)) {
          marker.capture_step = capture_step_for(*capture);
          marker.capture_owner_id = capture->capturing_player_id;
          marker.contested = capture->capture_blocked;
          if (marker.capture_step > 0) {
            capture_watch.push_back(CaptureWatch{entity_id,
                                                 capture->capturing_player_id,
                                                 unit.owner_id,
                                                 marker.world_x,
                                                 marker.world_z,
                                                 capture->capture_blocked});
          }
        }
      }

      if (marker.is_selected &&
          marker.marker_class == Game::Map::Minimap::MarkerClass::Troop) {
        selected_sum_x += marker.world_x;
        selected_sum_z += marker.world_z;
        ++selected_count;
      }

      if (marker.is_selected &&
          marker.marker_class == Game::Map::Minimap::MarkerClass::Troop &&
          destinations.size() < k_max_destinations) {
        if (const auto* movement =
                world->try_get<Engine::Core::MovementComponent>(entity_id)) {
          if (movement->get_has_target()) {
            const float goal_x = movement->get_goal_x();
            const float goal_z = movement->get_goal_y();
            const auto cell_x = static_cast<std::int64_t>(
                std::lround(goal_x / k_destination_cluster_grid));
            const auto cell_z = static_cast<std::int64_t>(
                std::lround(goal_z / k_destination_cluster_grid));
            const std::uint64_t cell_key = hash_combine(
                static_cast<std::uint64_t>(cell_x), static_cast<std::uint64_t>(cell_z));
            const bool duplicate = std::any_of(
                destination_cells.begin(),
                destination_cells.end(),
                [cell_key](std::uint64_t known) { return known == cell_key; });
            float nx = 0.0F;
            float ny = 0.0F;
            if (!duplicate && world_to_normalized(goal_x, goal_z, nx, ny)) {
              destination_cells.push_back(cell_key);
              destinations.push_back(
                  DestinationMarker{.nx = nx, .ny = ny, .owner_id = unit.owner_id});
              destination_hash = hash_combine(destination_hash, cell_key);
            }
          }
        }
      }

      markers.push_back(marker);

      unit_hash = hash_combine(unit_hash, static_cast<std::uint64_t>(entity_id));
      unit_hash =
          hash_combine(unit_hash, quantize(marker.world_x, m_hash_position_scale));
      unit_hash =
          hash_combine(unit_hash, quantize(marker.world_z, m_hash_position_scale));
      unit_hash = hash_combine(unit_hash, static_cast<std::uint64_t>(marker.owner_id));
      unit_hash = hash_combine(unit_hash, marker.is_selected ? 1ULL : 0ULL);
      unit_hash =
          hash_combine(unit_hash, static_cast<std::uint64_t>(marker.marker_class));
      unit_hash = hash_combine(unit_hash,
                               static_cast<std::uint64_t>(marker.capture_step) |
                                   (marker.contested ? 0x100ULL : 0ULL));
    }
  }
  unit_hash = hash_combine(unit_hash, static_cast<std::uint64_t>(markers.size()));

  if (!destinations.empty() && selected_count > 0) {
    float origin_nx = 0.0F;
    float origin_ny = 0.0F;
    if (world_to_normalized(selected_sum_x / static_cast<float>(selected_count),
                            selected_sum_z / static_cast<float>(selected_count),
                            origin_nx,
                            origin_ny)) {
      for (auto& destination : destinations) {
        destination.origin_nx = origin_nx;
        destination.origin_ny = origin_ny;
      }
      destination_hash =
          hash_combine(destination_hash, quantize(origin_nx, k_origin_hash_steps));
      destination_hash =
          hash_combine(destination_hash, quantize(origin_ny, k_origin_hash_steps));
    }
  }

  collect_capture_alerts(capture_watch);

  if (destination_hash != m_destination_hash) {
    m_destination_hash = destination_hash;
    m_destinations = destinations;
    m_destinations_dirty = true;
  }

  const bool units_changed = (unit_hash != m_last_unit_hash);
  const bool fog_changed = (fog_version() != m_last_fog_composite_version);

  if (units_changed || fog_changed) {
    if (units_changed) {
      m_last_unit_hash = unit_hash;
    }
    m_last_fog_composite_version = fog_version();
    mark_dirty();

    auto& visibility_service = Game::Session::session_for(*world).visibility();
    Game::Map::Minimap::VisibilityCheckFn visibility_check = nullptr;

    Game::Map::VisibilityService::SnapshotPtr visibility_snapshot;
    if (visibility_service.is_initialized()) {
      visibility_snapshot = visibility_service.snapshot_ptr();
    }
    if (visibility_snapshot != nullptr) {

      const auto* snapshot = visibility_snapshot.get();
      visibility_check = [snapshot](float world_x, float world_z) -> bool {
        return Game::Map::should_render_non_local_unit(*snapshot, world_x, world_z);
      };
    }

    m_unit_layer->update(markers, local_owner_id, visibility_check, nullptr);

    m_minimap_units_image = m_minimap_fog_image;
    const QImage& unit_overlay = m_unit_layer->get_image();
    const QRect& overlay_rect = m_unit_layer->content_rect();
    if (!unit_overlay.isNull() && !overlay_rect.isEmpty()) {
      QPainter painter(&m_minimap_units_image);
      painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
      painter.drawImage(overlay_rect, unit_overlay, overlay_rect);
    }
    m_minimap_image = m_minimap_units_image;
    m_viewport_composite_dirty = true;
  }
}

void MinimapManager::collect_capture_alerts(const std::vector<CaptureWatch>& current) {
  for (const auto& watch : current) {
    const auto previous = std::find_if(m_capture_watch.begin(),
                                       m_capture_watch.end(),
                                       [&watch](const CaptureWatch& known) {
                                         return known.entity_id == watch.entity_id;
                                       });
    const bool is_new = previous == m_capture_watch.end();
    const bool changed_hands =
        !is_new && previous->capturing_owner_id != watch.capturing_owner_id;
    const bool became_contested = !is_new && !previous->contested && watch.contested;
    if (!is_new && !changed_hands && !became_contested) {
      continue;
    }

    if (m_capture_alerts.size() >= k_max_pending_capture_alerts) {
      break;
    }
    m_capture_alerts.push_back(CaptureAlert{watch.world_x,
                                            watch.world_z,
                                            watch.site_owner_id,
                                            watch.capturing_owner_id,
                                            watch.contested});
  }

  m_capture_watch.assign(current.begin(), current.end());
}

void MinimapManager::update_camera_viewport(const Render::GL::Camera* camera,
                                            float screen_width,
                                            float screen_height) {
  if (m_minimap_image.isNull() || !m_camera_viewport_layer || (camera == nullptr)) {
    return;
  }

  const QVector3D& target = camera->get_target();

  const float distance = camera->get_distance();
  const float fov_rad =
      camera->get_fov() * Game::Map::Minimap::Constants::k_degrees_to_radians;
  const float aspect = screen_width / std::max(screen_height, 1.0F);

  const float viewport_half_height = distance * std::tan(fov_rad * 0.5F);
  const float viewport_half_width = viewport_half_height * aspect;

  const float viewport_width = viewport_half_width * 2.0F / m_tile_size;
  const float viewport_height = viewport_half_height * 2.0F / m_tile_size;

  const float camera_x = target.x() / m_tile_size;
  const float camera_z = target.z() / m_tile_size;

  constexpr float EPSILON = 0.01F;
  const bool camera_changed = !m_camera_viewport_valid ||
                              std::abs(camera_x - m_last_camera_x) > EPSILON ||
                              std::abs(camera_z - m_last_camera_z) > EPSILON ||
                              std::abs(viewport_width - m_last_viewport_w) > EPSILON ||
                              std::abs(viewport_height - m_last_viewport_h) > EPSILON;
  if (!camera_changed && !m_viewport_composite_dirty) {
    return;
  }

  if (camera_changed) {
    m_last_camera_x = camera_x;
    m_last_camera_z = camera_z;
    m_last_viewport_w = viewport_width;
    m_last_viewport_h = viewport_height;
    m_camera_viewport_valid = true;
    m_camera_viewport_layer->update(
        camera_x, camera_z, viewport_width, viewport_height);
  }

  m_minimap_image = m_minimap_units_image;

  const QImage& viewport_overlay = m_camera_viewport_layer->get_image();
  const QRect& overlay_rect = m_camera_viewport_layer->content_rect();
  if (!viewport_overlay.isNull() && !overlay_rect.isEmpty()) {
    QPainter painter(&m_minimap_image);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(overlay_rect, viewport_overlay, overlay_rect);
  }
  m_viewport_composite_dirty = false;
  mark_dirty();
}
