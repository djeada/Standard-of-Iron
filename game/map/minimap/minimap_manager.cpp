#include "minimap_manager.h"
#include <QPainter>

namespace Game::Map::Minimap {

MinimapManager::MinimapManager(const MapDefinition &map_def,
                               const MinimapManagerConfig &config)
    : m_config(config), m_grid(map_def.grid) {

  m_generator = std::make_unique<MinimapGenerator>(config.generator_config);
  m_base_image = m_generator->generate(map_def);

  if (config.fog_enabled) {
    m_fog_mask = std::make_unique<FogOfWarMask>(
        map_def.grid.width, map_def.grid.height, map_def.grid.tile_size,
        config.fog_config);
  }

  m_composite_dirty = true;
}

MinimapManager::~MinimapManager() = default;

MinimapManager::MinimapManager(MinimapManager &&) noexcept = default;
auto MinimapManager::operator=(MinimapManager &&) noexcept -> MinimapManager & =
                                                                  default;

void MinimapManager::tick(const std::vector<VisionSource> &vision_sources,
                          int player_id) {
  if (m_fog_mask && m_config.fog_enabled) {
    if (m_fog_mask->tick(vision_sources, player_id)) {
      m_composite_dirty = true;
    }
  }
}

void MinimapManager::force_fog_update(
    const std::vector<VisionSource> &vision_sources, int player_id) {
  if (m_fog_mask && m_config.fog_enabled) {
    m_fog_mask->update_vision(vision_sources, player_id);
    m_composite_dirty = true;
  }
}

auto MinimapManager::get_base_image() const -> const QImage & {
  return m_base_image;
}

auto MinimapManager::get_fog_mask() const -> QImage {
  if (!m_fog_mask || !m_config.fog_enabled) {

    QImage empty(m_base_image.size(), QImage::Format_RGBA8888);
    empty.fill(Qt::transparent);
    return empty;
  }

  return m_fog_mask->generate_mask(m_base_image.width(), m_base_image.height());
}

auto MinimapManager::get_composite_image() const -> QImage {
  if (m_composite_dirty) {
    regenerate_composite();
  }
  return m_composite_image;
}

auto MinimapManager::get_composite_image(int width,
                                         int height) const -> QImage {
  QImage composite = get_composite_image();

  if (composite.width() != width || composite.height() != height) {
    return composite.scaled(width, height, Qt::KeepAspectRatio,
                            Qt::SmoothTransformation);
  }

  return composite;
}

void MinimapManager::regenerate_composite() const {

  m_composite_image = m_base_image.copy();

  if (m_fog_mask && m_config.fog_enabled) {
    QImage fog = m_fog_mask->generate_mask(m_composite_image.width(),
                                           m_composite_image.height());

    QPainter painter(&m_composite_image);

    if (m_config.fog_multiply_blend) {

      painter.setCompositionMode(QPainter::CompositionMode_Multiply);
    } else {

      painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

    painter.drawImage(0, 0, fog);
    painter.end();
  }

  m_composite_dirty = false;

  if (m_fog_mask) {
    m_fog_mask->clear_dirty();
  }
}

auto MinimapManager::is_position_visible(float world_x,
                                         float world_z) const -> bool {
  if (!m_fog_mask || !m_config.fog_enabled) {
    return true;
  }
  return m_fog_mask->is_visible(world_x, world_z);
}

auto MinimapManager::is_position_revealed(float world_x,
                                          float world_z) const -> bool {
  if (!m_fog_mask || !m_config.fog_enabled) {
    return true;
  }
  return m_fog_mask->is_revealed(world_x, world_z);
}

void MinimapManager::reset_fog() {
  if (m_fog_mask) {
    m_fog_mask->reset();
    m_composite_dirty = true;
  }
}

void MinimapManager::reveal_all() {
  if (m_fog_mask) {
    m_fog_mask->reveal_all();
    m_composite_dirty = true;
  }
}

void MinimapManager::set_fog_enabled(bool enabled) {
  if (m_config.fog_enabled != enabled) {
    m_config.fog_enabled = enabled;
    m_composite_dirty = true;

    if (enabled && !m_fog_mask) {
      m_fog_mask = std::make_unique<FogOfWarMask>(
          m_grid.width, m_grid.height, m_grid.tile_size, m_config.fog_config);
    }
  }
}

auto MinimapManager::is_fog_enabled() const -> bool {
  return m_config.fog_enabled;
}

auto MinimapManager::memory_usage() const -> size_t {
  size_t total = sizeof(*this);
  total += static_cast<size_t>(m_base_image.sizeInBytes());
  total += static_cast<size_t>(m_composite_image.sizeInBytes());

  if (m_fog_mask) {
    total += m_fog_mask->memory_usage();
  }

  return total;
}

void MinimapManager::regenerate_base(const MapDefinition &map_def) {
  m_grid = map_def.grid;
  m_base_image = m_generator->generate(map_def);

  if (m_fog_mask) {
    m_fog_mask = std::make_unique<FogOfWarMask>(
        map_def.grid.width, map_def.grid.height, map_def.grid.tile_size,
        m_config.fog_config);
  }

  m_composite_dirty = true;
}

} // namespace Game::Map::Minimap
