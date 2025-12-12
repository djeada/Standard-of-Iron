#include "minimap_texture_manager.h"
#include "minimap_generator.h"
#include <QDebug>

namespace Game::Map::Minimap {

MinimapTextureManager::MinimapTextureManager()
    : m_generator(std::make_unique<MinimapGenerator>()),
      m_texture(std::make_unique<Render::GL::Texture>()) {}

MinimapTextureManager::~MinimapTextureManager() = default;

auto MinimapTextureManager::generateForMap(const MapDefinition &mapDef)
    -> bool {
  // Generate the minimap image
  m_image = m_generator->generate(mapDef);

  if (m_image.isNull()) {
    qWarning() << "MinimapTextureManager: Failed to generate minimap image";
    return false;
  }

  qDebug() << "MinimapTextureManager: Generated minimap of size"
           << m_image.width() << "x" << m_image.height();

  // Note: OpenGL texture upload would happen here when an OpenGL context is available
  // For now, we just store the image. The texture upload would be:
  // 1. Convert QImage to appropriate format (RGBA8888)
  // 2. Upload to OpenGL via m_texture->loadFromData() or similar
  // This is left as a TODO because it requires an active OpenGL context

  return true;
}

auto MinimapTextureManager::getTexture() const -> Render::GL::Texture * {
  return m_texture.get();
}

auto MinimapTextureManager::getImage() const -> const QImage & {
  return m_image;
}

void MinimapTextureManager::clear() {
  m_image = QImage();
  // Clear OpenGL texture if needed
}

} // namespace Game::Map::Minimap
