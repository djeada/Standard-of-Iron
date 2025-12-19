#include "resources.h"
#include "gl/mesh.h"
#include "gl/texture.h"
#include "render_constants.h"
#include <GL/gl.h>
#include <QVector3D>
#include <cmath>
#include <memory>

namespace Render::GL {

using namespace Render::GL::Geometry;
using namespace Render::GL::RGBA;

auto ResourceManager::initialize() -> bool {
  initializeOpenGLFunctions();

  m_quadMesh = createQuadMesh();
  m_groundMesh = createPlaneMesh(1.0F, 1.0F, GroundPlaneSubdivisions);
  m_unitMesh = create_cube_mesh();

  m_whiteTexture = std::make_unique<Texture>();
  m_whiteTexture->create_empty(1, 1, Texture::Format::RGBA);
  unsigned char white_pixel[4] = {MaxValue, MaxValue, MaxValue, MaxValue};
  m_whiteTexture->bind();
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                  white_pixel);
  return true;
}

} // namespace Render::GL
