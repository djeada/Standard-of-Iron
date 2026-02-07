#include "minimap_manager.h"

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/map_loader.h"
#include "game/map/minimap/camera_viewport_layer.h"
#include "game/map/minimap/minimap_generator.h"
#include "game/map/minimap/minimap_utils.h"
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

bool MinimapManager::consume_dirty_flag() {
  bool was_dirty = m_dirty;
  m_dirty = false;
  return was_dirty;
}

void MinimapManager::generate_for_map(const Game::Map::MapDefinition &map_def) {

  Game::Map::Minimap::MinimapOrientation::instance().set_yaw_degrees(
      map_def.camera.yaw_deg);

  Game::Map::Minimap::MinimapGenerator generator;
  m_minimap_base_image = generator.generate(map_def);

  if (!m_minimap_base_image.isNull()) {
    qDebug() << "MinimapManager: Generated minimap of size"
             << m_minimap_base_image.width() << "x"
             << m_minimap_base_image.height();

    m_world_width = static_cast<float>(map_def.grid.width);
    m_world_height = static_cast<float>(map_def.grid.height);
    m_tile_size = map_def.grid.tile_size;

    m_minimap_fog_image = m_minimap_base_image.copy();
    m_minimap_image = m_minimap_fog_image.copy();
    m_fog_lookup_entries.clear();
    m_fog_lookup_vis_width = 0;
    m_fog_lookup_vis_height = 0;
    m_fog_lookup_img_width = 0;
    m_fog_lookup_img_height = 0;

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
    mark_dirty();
  } else {
    qWarning() << "MinimapManager: Failed to generate minimap";
  }
}

void MinimapManager::rebuild_fog_lookup(int vis_width, int vis_height) {
  if (m_minimap_base_image.isNull() || vis_width <= 0 || vis_height <= 0) {
    m_fog_lookup_entries.clear();
    m_fog_lookup_vis_width = 0;
    m_fog_lookup_vis_height = 0;
    m_fog_lookup_img_width = 0;
    m_fog_lookup_img_height = 0;
    return;
  }

  m_fog_lookup_vis_width = vis_width;
  m_fog_lookup_vis_height = vis_height;
  m_fog_lookup_img_width = m_minimap_base_image.width();
  m_fog_lookup_img_height = m_minimap_base_image.height();

  const int img_width = m_fog_lookup_img_width;
  const int img_height = m_fog_lookup_img_height;
  m_fog_lookup_entries.resize(static_cast<std::size_t>(img_width * img_height));

  const auto &orient = Game::Map::Minimap::MinimapOrientation::instance();
  const float inv_cos = orient.cos_yaw();
  const float inv_sin = -orient.sin_yaw();

  const float scale_x =
      static_cast<float>(vis_width) / static_cast<float>(img_width);
  const float scale_y =
      static_cast<float>(vis_height) / static_cast<float>(img_height);
  const float half_img_w = static_cast<float>(img_width) * 0.5F;
  const float half_img_h = static_cast<float>(img_height) * 0.5F;
  const float half_vis_w = static_cast<float>(vis_width) * 0.5F;
  const float half_vis_h = static_cast<float>(vis_height) * 0.5F;

  std::size_t lookup_idx = 0;
  for (int y = 0; y < img_height; ++y) {
    const float centered_y = static_cast<float>(y) - half_img_h;
    for (int x = 0; x < img_width; ++x) {
      const float centered_x = static_cast<float>(x) - half_img_w;

      const float world_x = centered_x * inv_cos - centered_y * inv_sin;
      const float world_y = centered_x * inv_sin + centered_y * inv_cos;

      const float vis_x = (world_x * scale_x) + half_vis_w;
      const float vis_y = (world_y * scale_y) + half_vis_h;

      const int vx0 = std::clamp(static_cast<int>(vis_x), 0, vis_width - 1);
      const int vx1 = std::clamp(vx0 + 1, 0, vis_width - 1);
      const int vy0 = std::clamp(static_cast<int>(vis_y), 0, vis_height - 1);
      const int vy1 = std::clamp(vy0 + 1, 0, vis_height - 1);

      FogLookupEntry &entry = m_fog_lookup_entries[lookup_idx++];
      entry.idx00 = vy0 * vis_width + vx0;
      entry.idx10 = vy0 * vis_width + vx1;
      entry.idx01 = vy1 * vis_width + vx0;
      entry.idx11 = vy1 * vis_width + vx1;
      entry.fx = vis_x - static_cast<float>(vx0);
      entry.fy = vis_y - static_cast<float>(vy0);
    }
  }
}

void MinimapManager::update_fog(float dt, int local_owner_id) {
  Q_UNUSED(local_owner_id);

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
    if (m_minimap_fog_image.isNull() ||
        m_minimap_fog_image.size() != m_minimap_base_image.size()) {
      m_minimap_fog_image = m_minimap_base_image.copy();
    }
    return;
  }

  const auto current_version = visibility_service.version();
  if (current_version == m_minimap_fog_version &&
      !m_minimap_fog_image.isNull()) {
    return;
  }
  m_minimap_fog_version = current_version;
  mark_dirty();

  const auto visibility_snapshot = visibility_service.snapshot();
  const int vis_width = visibility_snapshot.width;
  const int vis_height = visibility_snapshot.height;
  const auto &cells = visibility_snapshot.cells;

  if (cells.empty() || vis_width <= 0 || vis_height <= 0) {
    m_minimap_fog_image = m_minimap_base_image.copy();
    return;
  }

  const int img_width = m_minimap_base_image.width();
  const int img_height = m_minimap_base_image.height();

  if (m_fog_lookup_vis_width != vis_width ||
      m_fog_lookup_vis_height != vis_height ||
      m_fog_lookup_img_width != img_width ||
      m_fog_lookup_img_height != img_height ||
      m_fog_lookup_entries.size() !=
          static_cast<std::size_t>(img_width * img_height)) {
    rebuild_fog_lookup(vis_width, vis_height);
  }

  if (m_minimap_fog_image.isNull() || m_minimap_fog_image.size() !=
                                         m_minimap_base_image.size() ||
      m_minimap_fog_image.format() != m_minimap_base_image.format()) {
    m_minimap_fog_image = m_minimap_base_image.copy();
  }

  constexpr int FOG_R = 45;
  constexpr int FOG_G = 38;
  constexpr int FOG_B = 30;
  constexpr int ALPHA_UNSEEN = 180;
  constexpr int ALPHA_EXPLORED = 60;
  constexpr int ALPHA_VISIBLE = 0;
  constexpr float ALPHA_THRESHOLD = 0.5F;
  constexpr float ALPHA_SCALE = 1.0F / 255.0F;
  constexpr std::uint8_t k_visible =
      static_cast<std::uint8_t>(Game::Map::VisibilityState::Visible);
  constexpr std::uint8_t k_explored =
      static_cast<std::uint8_t>(Game::Map::VisibilityState::Explored);

  auto alpha_from_cell = [&](std::uint8_t state) -> float {
    if (state == k_visible) {
      return static_cast<float>(ALPHA_VISIBLE);
    }
    if (state == k_explored) {
      return static_cast<float>(ALPHA_EXPLORED);
    }
    return static_cast<float>(ALPHA_UNSEEN);
  };

  std::size_t lookup_idx = 0;
  for (int y = 0; y < img_height; ++y) {
    const auto *base_scanline =
        reinterpret_cast<const QRgb *>(m_minimap_base_image.constScanLine(y));
    auto *scanline = reinterpret_cast<QRgb *>(m_minimap_fog_image.scanLine(y));

    for (int x = 0; x < img_width; ++x) {
      const FogLookupEntry &sample = m_fog_lookup_entries[lookup_idx++];
      const float a00 =
          alpha_from_cell(cells[static_cast<std::size_t>(sample.idx00)]);
      const float a10 =
          alpha_from_cell(cells[static_cast<std::size_t>(sample.idx10)]);
      const float a01 =
          alpha_from_cell(cells[static_cast<std::size_t>(sample.idx01)]);
      const float a11 =
          alpha_from_cell(cells[static_cast<std::size_t>(sample.idx11)]);

      const float alpha_top = a00 + (a10 - a00) * sample.fx;
      const float alpha_bot = a01 + (a11 - a01) * sample.fx;
      const float fog_alpha = alpha_top + (alpha_bot - alpha_top) * sample.fy;

      if (fog_alpha > ALPHA_THRESHOLD) {
        const QRgb original = base_scanline[x];
        const int orig_r = qRed(original);
        const int orig_g = qGreen(original);
        const int orig_b = qBlue(original);

        const float blend = fog_alpha * ALPHA_SCALE;
        const float inv_blend = 1.0F - blend;

        const int new_r = static_cast<int>(orig_r * inv_blend + FOG_R * blend);
        const int new_g = static_cast<int>(orig_g * inv_blend + FOG_G * blend);
        const int new_b = static_cast<int>(orig_b * inv_blend + FOG_B * blend);

        scanline[x] = qRgba(new_r, new_g, new_b, 255);
      } else {
        scanline[x] = base_scanline[x];
      }
    }
  }
}

void MinimapManager::update_units(
    Engine::Core::World *world,
    Game::Systems::SelectionSystem *selection_system, int local_owner_id) {
  if (m_minimap_fog_image.isNull() || !m_unit_layer || !world) {
    return;
  }

  m_minimap_image = m_minimap_fog_image.copy();

  std::vector<Game::Map::Minimap::UnitMarker> markers;

  constexpr size_t EXPECTED_MAX_UNITS = 128;
  markers.reserve(EXPECTED_MAX_UNITS);

  std::unordered_set<Engine::Core::EntityID> selected_ids;
  if (selection_system) {
    const auto &sel = selection_system->get_selected_units();
    selected_ids.insert(sel.begin(), sel.end());
  }

  std::uint64_t unit_hash = 0;

  {
    const std::lock_guard<std::recursive_mutex> lock(world->get_entity_mutex());
    const auto &entities = world->get_entities();

    for (const auto &[entity_id, entity] : entities) {
      const auto *unit = entity->get_component<Engine::Core::UnitComponent>();
      if (!unit) {
        continue;
      }

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

      unit_hash ^= static_cast<std::uint64_t>(entity_id);
      unit_hash ^=
          static_cast<std::uint64_t>(
              *reinterpret_cast<const std::uint32_t *>(&marker.world_x))
          << 1;
      unit_hash ^=
          static_cast<std::uint64_t>(
              *reinterpret_cast<const std::uint32_t *>(&marker.world_z))
          << 2;
      unit_hash ^= static_cast<std::uint64_t>(marker.is_selected) << 3;
    }
  }

  if (unit_hash != m_last_unit_hash) {
    m_last_unit_hash = unit_hash;
    mark_dirty();
  }

  auto &visibility_service = Game::Map::VisibilityService::instance();
  Game::Map::Minimap::VisibilityCheckFn visibility_check = nullptr;

  if (visibility_service.is_initialized()) {
    auto visibility_snapshot =
        std::make_shared<Game::Map::VisibilityService::Snapshot>(
            visibility_service.snapshot());
    visibility_check = [visibility_snapshot](float world_x,
                                             float world_z) -> bool {
      return visibility_snapshot->isVisibleWorld(world_x, world_z) ||
             visibility_snapshot->isExploredWorld(world_x, world_z);
    };
  }

  m_unit_layer->update(markers, local_owner_id, visibility_check, nullptr);

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

  const QVector3D &target = camera->get_target();

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

  m_camera_viewport_layer->update(camera_x, camera_z, viewport_width,
                                  viewport_height);

  const QImage &viewport_overlay = m_camera_viewport_layer->get_image();
  if (!viewport_overlay.isNull()) {
    QPainter painter(&m_minimap_image);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(0, 0, viewport_overlay);
  }
}
