#include "app/viewmodels/minimap_view_model.h"

#include <QColor>

#include <algorithm>
#include <cmath>

#include "app/core/client_context.h"
#include "app/input/input_command_handler.h"
#include "app/orders/order_markers.h"
#include "app/viewmodels/camera_view_model.h"
#include "app/world/minimap_manager.h"
#include "app/world/visibility_coordinator.h"
#include "game/map/render_visibility_rules.h"
#include "game/render_bridge/minimap/minimap_utils.h"
#include "game/session/session_context.h"
#include "game/systems/owner_registry.h"
#include "scene/camera.h"

namespace App::ViewModels {

namespace {

constexpr qint64 k_consider_interval_ms = 60;
constexpr int k_alert_grid = 12;

struct AlertStyle {
  const char* kind;
  qint64 cooldown_ms;
  float lifetime_seconds;
};

auto style_for(MinimapAlert alert) -> AlertStyle {
  switch (alert) {
  case MinimapAlert::TroopsAttacked:
    return {"troops_attacked", 2500, 2.0F};
  case MinimapAlert::StructureAttacked:
    return {"structure_attacked", 2000, 2.6F};
  case MinimapAlert::CaptureStarted:
    return {"capture_started", 4000, 3.0F};
  case MinimapAlert::CaptureContested:
    return {"capture_contested", 4000, 3.0F};
  case MinimapAlert::CaptureFinished:
    return {"capture_finished", 0, 3.6F};
  case MinimapAlert::ShrineStirred:
    return {"shrine", 0, 3.6F};
  }
  return {"troops_attacked", 2500, 2.0F};
}

auto cell_key(MinimapAlert alert, float nx, float ny) -> std::uint32_t {
  const auto cx = static_cast<std::uint32_t>(
      std::clamp(static_cast<int>(nx * k_alert_grid), 0, k_alert_grid - 1));
  const auto cy = static_cast<std::uint32_t>(
      std::clamp(static_cast<int>(ny * k_alert_grid), 0, k_alert_grid - 1));
  return (static_cast<std::uint32_t>(alert) << 16U) | (cx << 8U) | cy;
}

} // namespace

MinimapViewModel::MinimapViewModel(const App::Core::ClientContext& context,
                                   App::Core::ClientHost& host,
                                   CameraViewModel& camera,
                                   QObject* parent)
    : QObject(parent)
    , m_context(context)
    , m_host(host)
    , m_camera(camera) {
  m_alert_clock.start();
}

auto MinimapViewModel::image() const -> QImage {
  if (auto* minimap = m_context.minimap) {
    return minimap->get_image();
  }
  return {};
}

auto MinimapViewModel::world_at(qreal mx,
                                qreal my,
                                qreal minimap_width,
                                qreal minimap_height) const
    -> std::optional<QVector3D> {
  auto* minimap = m_context.minimap;
  if ((minimap == nullptr) || !minimap->has_minimap() || minimap_width <= 0.0 ||
      minimap_height <= 0.0) {
    return std::nullopt;
  }

  const QImage& image = minimap->get_image();
  if (image.isNull()) {
    return std::nullopt;
  }

  const auto image_width = static_cast<float>(image.width());
  const auto image_height = static_cast<float>(image.height());
  const float px =
      (static_cast<float>(mx) / static_cast<float>(minimap_width)) * image_width;
  const float py =
      (static_cast<float>(my) / static_cast<float>(minimap_height)) * image_height;

  const auto [world_x, world_z] =
      Game::Map::Minimap::pixel_to_world(px,
                                         py,
                                         minimap->get_world_width(),
                                         minimap->get_world_height(),
                                         image_width,
                                         image_height,
                                         minimap->get_tile_size());
  return QVector3D(world_x, 0.0F, world_z);
}

void MinimapViewModel::on_left_click(qreal mx,
                                     qreal my,
                                     qreal minimap_width,
                                     qreal minimap_height) {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  auto* camera = m_context.active_camera;
  if (camera == nullptr) {
    return;
  }
  const auto target = world_at(mx, my, minimap_width, minimap_height);
  if (!target) {
    return;
  }

  const QVector3D offset = camera->get_position() - camera->get_target();
  camera->look_at(*target + offset, *target, camera->get_up_vector());

  m_camera.set_following_selection(false);
}

void MinimapViewModel::on_right_click(qreal mx,
                                      qreal my,
                                      qreal minimap_width,
                                      qreal minimap_height) {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  const auto target = world_at(mx, my, minimap_width, minimap_height);
  if (!target) {
    return;
  }
  if (auto* input = m_context.input) {
    input->on_minimap_right_click(*target, m_context.local_owner_id);
  }
}

void MinimapViewModel::note_order_marker(const App::Core::OrderMarker& marker) {

  auto* minimap = m_context.minimap;
  if (minimap == nullptr || !minimap->has_minimap()) {
    return;
  }

  const auto [nx, ny] = Game::Map::Minimap::world_to_pixel(marker.position.x(),
                                                           marker.position.z(),
                                                           minimap->get_world_width(),
                                                           minimap->get_world_height(),
                                                           1.0F,
                                                           1.0F);

  const QVector3D color = App::Core::order_marker_color(marker.kind, marker.rejected);
  emit order_ping(std::clamp(nx, 0.0F, 1.0F),
                  std::clamp(ny, 0.0F, 1.0F),
                  QColor::fromRgbF(color.x(), color.y(), color.z()).name(),
                  marker.rejected,
                  marker.lifetime);
}

auto MinimapViewModel::relation_for(int owner_id) const -> QString {
  const int local = m_context.local_owner_id;
  if (owner_id <= 0) {
    return QStringLiteral("neutral");
  }
  if (owner_id == local) {
    return QStringLiteral("self");
  }
  if (m_context.session != nullptr &&
      m_context.session->owners().are_allies(owner_id, local)) {
    return QStringLiteral("ally");
  }
  return QStringLiteral("enemy");
}

auto MinimapViewModel::consume_alert_budget() -> bool {
  const qint64 now = m_alert_clock.elapsed();
  if (now - m_last_considered_ms < k_consider_interval_ms) {
    return false;
  }
  m_last_considered_ms = now;
  return true;
}

auto MinimapViewModel::accept_alert(MinimapAlert kind, float nx, float ny) -> bool {
  const AlertStyle style = style_for(kind);
  if (style.cooldown_ms <= 0) {
    return true;
  }

  const std::uint32_t key = cell_key(kind, nx, ny);
  const qint64 now = m_alert_clock.elapsed();
  AlertSlot& slot = m_alert_slots[key % k_alert_slot_count];
  if (slot.used && slot.key == key && now - slot.last_ms < style.cooldown_ms) {
    return false;
  }
  slot.used = true;
  slot.key = key;
  slot.last_ms = now;
  return true;
}

void MinimapViewModel::note_alert(MinimapAlert kind,
                                  float world_x,
                                  float world_z,
                                  int subject_owner_id,
                                  int actor_owner_id) {
  auto* minimap = m_context.minimap;
  if (minimap == nullptr) {
    return;
  }

  float nx = 0.0F;
  float ny = 0.0F;
  if (!minimap->world_to_normalized(world_x, world_z, nx, ny)) {
    return;
  }

  const QString subject_relation = relation_for(subject_owner_id);
  const QString actor_relation = relation_for(actor_owner_id);

  QString relation;
  if (subject_relation == QStringLiteral("self")) {
    relation = QStringLiteral("self");
  } else if (subject_relation == QStringLiteral("ally")) {
    relation = QStringLiteral("ally");
  } else if (actor_relation == QStringLiteral("self") ||
             actor_relation == QStringLiteral("ally")) {
    relation = QStringLiteral("friendly");
  } else {
    relation = QStringLiteral("enemy");
  }

  if (relation == QStringLiteral("enemy") && m_context.visibility != nullptr) {
    const auto snapshot = m_context.visibility->current_snapshot();
    if (snapshot != nullptr && snapshot->initialized &&
        !Game::Map::should_render_non_local_unit(*snapshot, world_x, world_z)) {
      return;
    }
  }

  if (!accept_alert(kind, nx, ny)) {
    return;
  }

  const AlertStyle style = style_for(kind);
  emit event_blip(nx,
                  ny,
                  QString::fromLatin1(style.kind),
                  relation,
                  static_cast<qreal>(style.lifetime_seconds));
}

void MinimapViewModel::set_destinations(QVariantList destinations) {
  if (m_destinations == destinations) {
    return;
  }
  m_destinations = std::move(destinations);
  emit destinations_changed();
}

void MinimapViewModel::set_landmarks(QVariantList landmarks) {
  if (m_landmarks == landmarks) {
    return;
  }
  m_landmarks = std::move(landmarks);
  emit landmarks_changed();
}

void MinimapViewModel::clear_overlays() {
  set_destinations({});
  set_landmarks({});
  m_alert_slots = {};
}

} // namespace App::ViewModels
