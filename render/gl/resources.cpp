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

  m_quad_mesh = create_quad_mesh();
  m_ground_mesh = create_plane_mesh(1.0F, 1.0F, ground_plane_subdivisions);
  m_unit_mesh = create_cube_mesh();

  m_white_texture = std::make_unique<Texture>();
  m_white_texture->create_empty(1, 1, Texture::Format::RGBA);
  unsigned char white_pixel[4] = {max_value, max_value, max_value, max_value};
  m_white_texture->bind();
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                  white_pixel);
  return true;
}

} // namespace Render::GL
