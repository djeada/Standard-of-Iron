#include "selection_disc.h"
#include "gl/mesh.h"
#include <QVector3D>
#include <cmath>
#include <memory>
#include <vector>

namespace Render::Geom {

std::unique_ptr<Render::GL::Mesh> SelectionDisc::s_mesh;

static auto createDiscMesh() -> std::unique_ptr<Render::GL::Mesh> {
  using namespace Render::GL;
  std::vector<Vertex> verts;
  std::vector<unsigned int> idx;
  constexpr int k_disc_segments = 72;

  verts.push_back({{0.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}, {0.5F, 0.5F}});
  for (int i = 0; i <= k_disc_segments; ++i) {
    float const a = (i / float(k_disc_segments)) * 6.2831853F;
    float x = std::cos(a);
    float z = std::sin(a);
    verts.push_back(
        {{x, 0.0F, z}, {0.0F, 1.0F, 0.0F}, {0.5F + 0.5F * x, 0.5F + 0.5F * z}});
  }
  for (int i = 1; i <= k_disc_segments; ++i) {
    idx.push_back(0);
    idx.push_back(i);
    idx.push_back(i + 1);
  }
  return std::make_unique<Mesh>(verts, idx);
}

auto SelectionDisc::get() -> Render::GL::Mesh * {
  if (!s_mesh) {
    s_mesh = createDiscMesh();
  }
  return s_mesh.get();
}

} // namespace Render::Geom
