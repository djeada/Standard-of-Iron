#include "minimap_texture_manager.h"
#include "minimap_generator.h"
#include <QDebug>

namespace Game::Map::Minimap {

MinimapTextureManager::MinimapTextureManager()
    : m_generator(std::make_unique<MinimapGenerator>()),
      m_texture(std::make_unique<Render::GL::Texture>()) {}

MinimapTextureManager::~MinimapTextureManager() = default;

auto MinimapTextureManager::generate_for_map(const MapDefinition &map_def)
    -> bool {

  m_image = m_generator->generate(map_def);

  if (m_image.isNull()) {
    qWarning() << "MinimapTextureManager: Failed to generate minimap image";
    return false;
  }

  qDebug() << "MinimapTextureManager: Generated minimap of size"
           << m_image.width() << "x" << m_image.height();

  return true;
}

auto MinimapTextureManager::get_texture() const -> Render::GL::Texture * {
  return m_texture.get();
}

auto MinimapTextureManager::get_image() const -> const QImage & {
  return m_image;
}

void MinimapTextureManager::clear() { m_image = QImage(); }

} // namespace Game::Map::Minimap
