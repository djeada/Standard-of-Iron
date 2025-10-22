#include "resources.h"
#include <QVector3D>
#include <cmath>

namespace Render::GL {

bool ResourceManager::initialize() {
  initializeOpenGLFunctions();

  m_quadMesh.reset(createQuadMesh());
  m_groundMesh.reset(createPlaneMesh(1.0f, 1.0f, 64));
  m_unitMesh.reset(createCubeMesh());

  m_whiteTexture = std::make_unique<Texture>();
  m_whiteTexture->createEmpty(1, 1, Texture::Format::RGBA);
  unsigned char whitePixel[4] = {255, 255, 255, 255};
  m_whiteTexture->bind();
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                  whitePixel);
  return true;
}

} // namespace Render::GL
