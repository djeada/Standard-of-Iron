#include "app/viewmodels/minimap_view_model.h"

#include "app/core/client_context.h"
#include "app/input/input_command_handler.h"
#include "app/viewmodels/camera_view_model.h"
#include "app/world/minimap_manager.h"
#include "game/render_bridge/minimap/minimap_utils.h"
#include "scene/camera.h"

namespace App::ViewModels {

MinimapViewModel::MinimapViewModel(const App::Core::ClientContext& context,
                                   App::Core::ClientHost& host,
                                   CameraViewModel& camera,
                                   QObject* parent)
    : QObject(parent)
    , m_context(context)
    , m_host(host)
    , m_camera(camera) {
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
  const auto target = world_at(mx, my, minimap_width, minimap_height);
  if (!target) {
    return;
  }
  if (auto* input = m_context.input) {
    input->on_minimap_right_click(*target, m_context.local_owner_id);
  }
}

} // namespace App::ViewModels
