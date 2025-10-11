#include "selection_disc.h"
#include <QVector3D>
#include <cmath>
#include <vector>

namespace Render::Geom {

std::unique_ptr<Render::GL::Mesh> SelectionDisc::s_mesh;

static Render::GL::Mesh *createDiscMesh() {
  using namespace Render::GL;
  std::vector<Vertex> verts;
  std::vector<unsigned int> idx;
  const int seg = 72;

  verts.push_back({{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.5f, 0.5f}});
  for (int i = 0; i <= seg; ++i) {
    float a = (i / float(seg)) * 6.2831853f;
    float x = std::cos(a);
    float z = std::sin(a);
    verts.push_back(
        {{x, 0.0f, z}, {0.0f, 1.0f, 0.0f}, {0.5f + 0.5f * x, 0.5f + 0.5f * z}});
  }
  for (int i = 1; i <= seg; ++i) {
    idx.push_back(0);
    idx.push_back(i);
    idx.push_back(i + 1);
  }
  return new Mesh(verts, idx);
}

Render::GL::Mesh *SelectionDisc::get() {
  if (!s_mesh)
    s_mesh.reset(createDiscMesh());
  return s_mesh.get();
}

} // namespace Render::Geom
