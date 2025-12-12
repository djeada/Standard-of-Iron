#pragma once

#include "../map_definition.h"
#include "render/gl/texture.h"
#include <QImage>
#include <memory>

namespace Game::Map::Minimap {

class MinimapGenerator;

/**
 * @brief Manages minimap texture lifecycle and integration with the map system.
 *
 * This class provides the glue between the map loading system and the minimap
 * generator. It generates the static minimap texture when a map is loaded and
 * manages the OpenGL texture for rendering.
 */
class MinimapTextureManager {
public:
  MinimapTextureManager();
  ~MinimapTextureManager();

  /**
   * @brief Generates and uploads a minimap texture for the given map definition
   * @param mapDef The map definition to generate the minimap from
   * @return true if successful, false otherwise
   */
  auto generateForMap(const MapDefinition &mapDef) -> bool;

  /**
   * @brief Gets the OpenGL texture for rendering
   * @return Pointer to the texture, or nullptr if not generated
   */
  [[nodiscard]] auto getTexture() const -> Render::GL::Texture *;

  /**
   * @brief Gets the generated minimap image (for debugging/saving)
   * @return The minimap image
   */
  [[nodiscard]] auto getImage() const -> const QImage &;

  /**
   * @brief Clears the current minimap texture
   */
  void clear();

private:
  std::unique_ptr<MinimapGenerator> m_generator;
  std::unique_ptr<Render::GL::Texture> m_texture;
  QImage m_image;
};

} // namespace Game::Map::Minimap
