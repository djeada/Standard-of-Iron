#include "minimap_manager.h"

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_loader.h"
#include "game/map/minimap/camera_viewport_layer.h"
#include "game/map/minimap/minimap_generator.h"
#include "game/map/minimap/unit_layer.h"
#include "game/map/visibility_service.h"
#include "game/systems/selection_system.h"
#include "game/units/troop_type.h"
#include "render/gl/camera.h"
#include <QDebug>
#include <QPainter>
#include <algorithm>
#include <cmath>
#include <unordered_set>

MinimapManager::MinimapManager() = default;

MinimapManager::~MinimapManager() = default;

void MinimapManager::generate_for_map(const Game::Map::MapDefinition &map_def) {
  Game::Map::Minimap::MinimapGenerator generator;
  m_minimap_base_image = generator.generate(map_def);

  if (!m_minimap_base_image.isNull()) {
    qDebug() << "MinimapManager: Generated minimap of size"
             << m_minimap_base_image.width() << "x"
             << m_minimap_base_image.height();

    m_world_width = static_cast<float>(map_def.grid.width);
    m_world_height = static_cast<float>(map_def.grid.height);
    m_tile_size = map_def.grid.tile_size;

    m_unit_layer = std::make_unique<Game::Map::Minimap::UnitLayer>();
    m_unit_layer->init(m_minimap_base_image.width(),
                       m_minimap_base_image.height(), m_world_width,
                       m_world_height);
    qDebug() << "MinimapManager: Initialized unit layer for world"
             << m_world_width << "x" << m_world_height;

    m_camera_viewport_layer =
        std::make_unique<Game::Map::Minimap::CameraViewportLayer>();
    m_camera_viewport_layer->init(m_minimap_base_image.width(),
                                  m_minimap_base_image.height(), m_world_width,
                                  m_world_height);

    m_minimap_fog_version = 0;
    m_minimap_update_timer = MINIMAP_UPDATE_INTERVAL;
    update_fog(0.0F, 1);
  } else {
    qWarning() << "MinimapManager: Failed to generate minimap";
  }
}

void MinimapManager::update_fog(float dt, int local_owner_id) {
  if (m_minimap_base_image.isNull()) {
    return;
  }

  m_minimap_update_timer += dt;
  if (m_minimap_update_timer < MINIMAP_UPDATE_INTERVAL) {
    return;
  }
  m_minimap_update_timer = 0.0F;

  auto &visibility_service = Game::Map::VisibilityService::instance();
  if (!visibility_service.is_initialized()) {
    if (m_minimap_image != m_minimap_base_image) {
      m_minimap_image = m_minimap_base_image;
    }
    return;
  }

  const auto current_version = visibility_service.version();
  if (current_version == m_minimap_fog_version && !m_minimap_image.isNull()) {
    return;
  }
  m_minimap_fog_version = current_version;

  const int vis_width = visibility_service.getWidth();
  const int vis_height = visibility_service.getHeight();
  const auto cells = visibility_service.snapshotCells();

  if (cells.empty() || vis_width <= 0 || vis_height <= 0) {
    m_minimap_image = m_minimap_base_image;
    return;
  }

  m_minimap_image = m_minimap_base_image.copy();

  const int img_width = m_minimap_image.width();
  const int img_height = m_minimap_image.height();

  constexpr float k_inv_cos = -0.70710678118F;
  constexpr float k_inv_sin = 0.70710678118F;

  const float scale_x =
      static_cast<float>(vis_width) / static_cast<float>(img_width);
  const float scale_y =
      static_cast<float>(vis_height) / static_cast<float>(img_height);

  constexpr int FOG_R = 45;
  constexpr int FOG_G = 38;
  constexpr int FOG_B = 30;
  constexpr int ALPHA_UNSEEN = 180;
  constexpr int ALPHA_EXPLORED = 60;
  constexpr int ALPHA_VISIBLE = 0;
  constexpr float ALPHA_THRESHOLD = 0.5F;
  constexpr float ALPHA_SCALE = 1.0F / 255.0F;

  auto get_alpha = [&cells, vis_width, ALPHA_VISIBLE, ALPHA_EXPLORED,
                    ALPHA_UNSEEN](int vx, int vy) -> float {
    const size_t idx = static_cast<size_t>(vy * vis_width + vx);
    if (idx >= cells.size()) {
      return static_cast<float>(ALPHA_UNSEEN);
    }
    const auto state = static_cast<Game::Map::VisibilityState>(cells[idx]);
    switch (state) {
    case Game::Map::VisibilityState::Visible:
      return static_cast<float>(ALPHA_VISIBLE);
    case Game::Map::VisibilityState::Explored:
      return static_cast<float>(ALPHA_EXPLORED);
    default:
      return static_cast<float>(ALPHA_UNSEEN);
    }
  };

  const float half_img_w = static_cast<float>(img_width) * 0.5F;
  const float half_img_h = static_cast<float>(img_height) * 0.5F;
  const float half_vis_w = static_cast<float>(vis_width) * 0.5F;
  const float half_vis_h = static_cast<float>(vis_height) * 0.5F;

  for (int y = 0; y < img_height; ++y) {
    auto *scanline = reinterpret_cast<QRgb *>(m_minimap_image.scanLine(y));

    for (int x = 0; x < img_width; ++x) {
      const float centered_x = static_cast<float>(x) - half_img_w;
      const float centered_y = static_cast<float>(y) - half_img_h;

      const float world_x = centered_x * k_inv_cos - centered_y * k_inv_sin;
      const float world_y = centered_x * k_inv_sin + centered_y * k_inv_cos;

      const float vis_x = (world_x * scale_x) + half_vis_w;
      const float vis_y = (world_y * scale_y) + half_vis_h;

      const int vx0 = std::clamp(static_cast<int>(vis_x), 0, vis_width - 1);
      const int vx1 = std::clamp(vx0 + 1, 0, vis_width - 1);
      const float fx = vis_x - static_cast<float>(vx0);

      const int vy0 = std::clamp(static_cast<int>(vis_y), 0, vis_height - 1);
      const int vy1 = std::clamp(vy0 + 1, 0, vis_height - 1);
      const float fy = vis_y - static_cast<float>(vy0);

      const float a00 = get_alpha(vx0, vy0);
      const float a10 = get_alpha(vx1, vy0);
      const float a01 = get_alpha(vx0, vy1);
      const float a11 = get_alpha(vx1, vy1);

      const float alpha_top = a00 + (a10 - a00) * fx;
      const float alpha_bot = a01 + (a11 - a01) * fx;
      const float fog_alpha = alpha_top + (alpha_bot - alpha_top) * fy;

      if (fog_alpha > ALPHA_THRESHOLD) {
        const QRgb original = scanline[x];
        const int orig_r = qRed(original);
        const int orig_g = qGreen(original);
        const int orig_b = qBlue(original);

        const float blend = fog_alpha * ALPHA_SCALE;
        const float inv_blend = 1.0F - blend;

        const int new_r = static_cast<int>(orig_r * inv_blend + FOG_R * blend);
        const int new_g = static_cast<int>(orig_g * inv_blend + FOG_G * blend);
        const int new_b = static_cast<int>(orig_b * inv_blend + FOG_B * blend);

        scanline[x] = qRgba(new_r, new_g, new_b, 255);
      }
    }
  }
}

void MinimapManager::update_units(
    Engine::Core::World *world,
    Game::Systems::SelectionSystem *selection_system) {
  if (m_minimap_image.isNull() || !m_unit_layer || !world) {
    return;
  }

  std::vector<Game::Map::Minimap::UnitMarker> markers;

  constexpr size_t EXPECTED_MAX_UNITS = 128;
  markers.reserve(EXPECTED_MAX_UNITS);

  std::unordered_set<Engine::Core::EntityID> selected_ids;
  if (selection_system) {
    const auto &sel = selection_system->get_selected_units();
    selected_ids.insert(sel.begin(), sel.end());
  }

  {
    const std::lock_guard<std::recursive_mutex> lock(world->get_entity_mutex());
    const auto &entities = world->get_entities();

    for (const auto &[entity_id, entity] : entities) {
      const auto *unit = entity->get_component<Engine::Core::UnitComponent>();
      if (!unit) {
        continue;
      }

      // Skip dead units - they should not appear on the minimap
      if (unit->health <= 0) {
        continue;
      }

      const auto *transform =
          entity->get_component<Engine::Core::TransformComponent>();
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
    }
  }

  m_unit_layer->update(markers);

  const QImage &unit_overlay = m_unit_layer->get_image();
  if (!unit_overlay.isNull()) {
    QPainter painter(&m_minimap_image);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(0, 0, unit_overlay);
  }
}

void MinimapManager::update_camera_viewport(const Render::GL::Camera *camera,
                                            float screen_width,
                                            float screen_height) {
  if (m_minimap_image.isNull() || !m_camera_viewport_layer || !camera) {
    return;
  }

  // Get camera target position (where the camera is looking)
  const QVector3D &target = camera->get_target();

  // Estimate the visible world area based on camera distance and FOV
  const float distance = camera->get_distance();
  const float fov_rad =
      static_cast<float>(camera->get_fov() * M_PI / 180.0);
  const float aspect = screen_width / std::max(screen_height, 1.0F);

  // Calculate approximate viewport dimensions in world space
  // Based on the vertical FOV and camera distance
  const float viewport_half_height = distance * std::tan(fov_rad * 0.5F);
  const float viewport_half_width = viewport_half_height * aspect;

  // Convert from world units to tile units for the minimap
  const float viewport_width = viewport_half_width * 2.0F / m_tile_size;
  const float viewport_height = viewport_half_height * 2.0F / m_tile_size;

  // Update the camera viewport layer
  m_camera_viewport_layer->update(target.x() / m_tile_size,
                                  target.z() / m_tile_size, viewport_width,
                                  viewport_height);

  // Draw the camera viewport overlay on the minimap
  const QImage &viewport_overlay = m_camera_viewport_layer->get_image();
  if (!viewport_overlay.isNull()) {
    QPainter painter(&m_minimap_image);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(0, 0, viewport_overlay);
  }
}
