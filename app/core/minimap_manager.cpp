#include "minimap_manager.h"

#include <QDebug>
#include <QPainter>

#include <algorithm>
#include <bit>
#include <cmath>
#include <unordered_set>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_loader.h"
#include "game/map/minimap/camera_viewport_layer.h"
#include "game/map/minimap/minimap_generator.h"
#include "game/map/minimap/minimap_utils.h"
#include "game/map/minimap/unit_layer.h"
#include "game/map/render_visibility_rules.h"
#include "game/map/visibility_service.h"
#include "game/systems/selection_system.h"
#include "game/units/troop_type.h"
#include "render/gl/camera.h"

namespace {
[[nodiscard]] auto hash_combine(std::uint64_t seed,
                                std::uint64_t value) noexcept -> std::uint64_t {
  seed ^= value + 0x9E3779B97F4A7C15ULL + (seed << 6U) + (seed >> 2U);
  return seed;
}

[[nodiscard]] auto hash_float(float value) noexcept -> std::uint64_t {
  return static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(value));
}
} // namespace

MinimapManager::MinimapManager() = default;

MinimapManager::~MinimapManager() = default;

bool MinimapManager::consume_dirty_flag() {
  bool was_dirty = m_dirty;
  m_dirty = false;
  return was_dirty;
}

void MinimapManager::generate_for_map(const Game::Map::MapDefinition& map_def) {

  Game::Map::Minimap::MinimapOrientation::instance().set_yaw_degrees(
      map_def.camera.yaw_deg);

  Game::Map::Minimap::MinimapGenerator generator;
  m_minimap_base_image = generator.generate(map_def);
  if (!m_minimap_base_image.isNull() &&
      m_minimap_base_image.format() != QImage::Format_ARGB32) {
    m_minimap_base_image = m_minimap_base_image.convertToFormat(QImage::Format_ARGB32);
  }

  if (!m_minimap_base_image.isNull()) {
    qDebug() << "MinimapManager: Generated minimap of size"
             << m_minimap_base_image.width() << "x" << m_minimap_base_image.height();

    m_world_width = static_cast<float>(map_def.grid.width);
    m_world_height = static_cast<float>(map_def.grid.height);
    m_tile_size = map_def.grid.tile_size;

    m_fog_compositor.reset();
    m_minimap_fog_image = m_minimap_base_image.copy();
    m_minimap_image = m_minimap_fog_image.copy();

    m_unit_layer = std::make_unique<Game::Map::Minimap::UnitLayer>();
    m_unit_layer->init(m_minimap_base_image.width(),
                       m_minimap_base_image.height(),
                       m_world_width,
                       m_world_height);
    qDebug() << "MinimapManager: Initialized unit layer for world" << m_world_width
             << "x" << m_world_height;

    m_camera_viewport_layer =
        std::make_unique<Game::Map::Minimap::CameraViewportLayer>();
    m_camera_viewport_layer->init(m_minimap_base_image.width(),
                                  m_minimap_base_image.height(),
                                  m_world_width,
                                  m_world_height);

    m_last_fog_composite_version = std::numeric_limits<std::uint64_t>::max();
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

  m_minimap_image = m_minimap_fog_image.copy();
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
  m_minimap_image = m_minimap_fog_image.copy();
  mark_dirty();
}

void MinimapManager::update_units(Engine::Core::World* world,
                                  Game::Systems::SelectionSystem* selection_system,
                                  int local_owner_id) {
  if (m_minimap_fog_image.isNull() || !m_unit_layer || !world) {
    return;
  }

  m_minimap_image = m_minimap_fog_image.copy();

  std::vector<Game::Map::Minimap::UnitMarker> markers;

  constexpr size_t EXPECTED_MAX_UNITS = 128;
  markers.reserve(EXPECTED_MAX_UNITS);

  std::unordered_set<Engine::Core::EntityID> selected_ids;
  if (selection_system) {
    const auto& sel = selection_system->get_selected_units();
    selected_ids.insert(sel.begin(), sel.end());
  }

  std::uint64_t unit_hash = hash_combine(0, static_cast<std::uint64_t>(local_owner_id));

  {
    const std::lock_guard<std::recursive_mutex> lock(world->get_entity_mutex());
    const auto& entities = world->get_entities();

    for (const auto& [entity_id, entity] : entities) {
      const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
      if (!unit) {
        continue;
      }

      if (unit->health <= 0) {
        continue;
      }

      const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
      if (!transform) {
        continue;
      }

      Game::Map::Minimap::UnitMarker marker;
      marker.world_x = transform->position.x;
      marker.world_z = transform->position.z;
      marker.owner_id = unit->owner_id;
      marker.is_selected = selected_ids.count(entity_id) > 0;
      marker.is_building = Game::Units::is_building_spawn(unit->spawn_type);

      markers.push_back(marker);

      unit_hash = hash_combine(unit_hash, static_cast<std::uint64_t>(entity_id));
      unit_hash = hash_combine(unit_hash, hash_float(marker.world_x));
      unit_hash = hash_combine(unit_hash, hash_float(marker.world_z));
      unit_hash = hash_combine(unit_hash, static_cast<std::uint64_t>(marker.owner_id));
      unit_hash = hash_combine(unit_hash, marker.is_selected ? 1ULL : 0ULL);
      unit_hash = hash_combine(unit_hash, marker.is_building ? 1ULL : 0ULL);
    }
  }
  unit_hash = hash_combine(unit_hash, static_cast<std::uint64_t>(markers.size()));

  const bool units_changed = (unit_hash != m_last_unit_hash);
  const bool fog_changed = (fog_version() != m_last_fog_composite_version);

  if (units_changed || fog_changed) {
    if (units_changed) {
      m_last_unit_hash = unit_hash;
    }
    m_last_fog_composite_version = fog_version();
    mark_dirty();

    auto& visibility_service = Game::Map::VisibilityService::instance();
    Game::Map::Minimap::VisibilityCheckFn visibility_check = nullptr;

    if (visibility_service.is_initialized()) {
      auto visibility_snapshot = visibility_service.snapshot_ptr();
      visibility_check = [visibility_snapshot](float world_x, float world_z) -> bool {
        return Game::Map::should_render_non_local_unit(
            *visibility_snapshot, world_x, world_z);
      };
    }

    m_unit_layer->update(markers, local_owner_id, visibility_check, nullptr);
  }

  const QImage& unit_overlay = m_unit_layer->get_image();
  if (!unit_overlay.isNull()) {
    QPainter painter(&m_minimap_image);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(0, 0, unit_overlay);
  }
}

void MinimapManager::update_camera_viewport(const Render::GL::Camera* camera,
                                            float screen_width,
                                            float screen_height) {
  if (m_minimap_image.isNull() || !m_camera_viewport_layer || !camera) {
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
  if (std::abs(camera_x - m_last_camera_x) > EPSILON ||
      std::abs(camera_z - m_last_camera_z) > EPSILON ||
      std::abs(viewport_width - m_last_viewport_w) > EPSILON ||
      std::abs(viewport_height - m_last_viewport_h) > EPSILON) {
    m_last_camera_x = camera_x;
    m_last_camera_z = camera_z;
    m_last_viewport_w = viewport_width;
    m_last_viewport_h = viewport_height;
    mark_dirty();
  }

  m_camera_viewport_layer->update(camera_x, camera_z, viewport_width, viewport_height);

  const QImage& viewport_overlay = m_camera_viewport_layer->get_image();
  if (!viewport_overlay.isNull()) {
    QPainter painter(&m_minimap_image);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(0, 0, viewport_overlay);
  }
}
