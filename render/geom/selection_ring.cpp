#include "selection_ring.h"
#include "gl/mesh.h"
#include <QVector3D>
#include <cmath>
#include <cstddef>
#include <memory>
#include <qvectornd.h>
#include <vector>

namespace Render::Geom {

std::unique_ptr<Render::GL::Mesh> SelectionRing::s_mesh;

static auto createRingMesh() -> std::unique_ptr<Render::GL::Mesh> {
  using namespace Render::GL;
  std::vector<Vertex> verts;
  std::vector<unsigned int> idx;
  constexpr int k_ring_segments = 48;
  const float inner = 0.94F;
  const float outer = 1.0F;
  for (int i = 0; i < k_ring_segments; ++i) {
    float const a0 = (i / float(k_ring_segments)) * 6.2831853F;
    float const a1 = ((i + 1) / float(k_ring_segments)) * 6.2831853F;
    QVector3D const n(0, 1, 0);
    QVector3D const v0i(inner * std::cos(a0), 0.0F, inner * std::sin(a0));
    QVector3D const v0o(outer * std::cos(a0), 0.0F, outer * std::sin(a0));
    QVector3D const v1o(outer * std::cos(a1), 0.0F, outer * std::sin(a1));
    QVector3D const v1i(inner * std::cos(a1), 0.0F, inner * std::sin(a1));
    size_t const base = verts.size();
    verts.push_back({{v0i.x(), 0.0F, v0i.z()}, {n.x(), n.y(), n.z()}, {0, 0}});
    verts.push_back({{v0o.x(), 0.0F, v0o.z()}, {n.x(), n.y(), n.z()}, {1, 0}});
    verts.push_back({{v1o.x(), 0.0F, v1o.z()}, {n.x(), n.y(), n.z()}, {1, 1}});
    verts.push_back({{v1i.x(), 0.0F, v1i.z()}, {n.x(), n.y(), n.z()}, {0, 1}});
    idx.push_back(base + 0);
    idx.push_back(base + 1);
    idx.push_back(base + 2);
    idx.push_back(base + 2);
    idx.push_back(base + 3);
    idx.push_back(base + 0);
  }
  return std::make_unique<Mesh>(verts, idx);
}

auto SelectionRing::get() -> Render::GL::Mesh * {
  if (!s_mesh) {
    s_mesh = createRingMesh();
  }
  return s_mesh.get();
}

} 
