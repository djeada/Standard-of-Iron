#pragma once

#include "../map_definition.h"
#include "render/gl/texture.h"
#include <QImage>
#include <memory>

namespace Game::Map::Minimap {

class MinimapGenerator;

class MinimapTextureManager {
public:
  MinimapTextureManager();
  ~MinimapTextureManager();

  auto generate_for_map(const MapDefinition &map_def) -> bool;

  [[nodiscard]] auto get_texture() const -> Render::GL::Texture *;

  [[nodiscard]] auto get_image() const -> const QImage &;

  void clear();

private:
  std::unique_ptr<MinimapGenerator> m_generator;
  std::unique_ptr<Render::GL::Texture> m_texture;
  QImage m_image;
};

} 
