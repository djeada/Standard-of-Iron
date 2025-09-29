#include "selection_ring.h"
#include <QVector3D>

namespace Render::Geom {

std::unique_ptr<Render::GL::Mesh> SelectionRing::s_mesh;

static Render::GL::Mesh* createRingMesh() {
    using namespace Render::GL;
    std::vector<Vertex> verts;
    std::vector<unsigned int> idx;
    const int seg = 48;
    const float inner = 0.8f;
    const float outer = 1.0f;
    for (int i = 0; i < seg; ++i) {
        float a0 = (i / float(seg)) * 6.2831853f;
        float a1 = ((i + 1) / float(seg)) * 6.2831853f;
        QVector3D n(0, 1, 0);
        QVector3D v0i(inner * std::cos(a0), 0.0f, inner * std::sin(a0));
        QVector3D v0o(outer * std::cos(a0), 0.0f, outer * std::sin(a0));
        QVector3D v1o(outer * std::cos(a1), 0.0f, outer * std::sin(a1));
        QVector3D v1i(inner * std::cos(a1), 0.0f, inner * std::sin(a1));
        size_t base = verts.size();
        verts.push_back({{v0i.x(), 0.0f, v0i.z()}, {n.x(), n.y(), n.z()}, {0, 0}});
        verts.push_back({{v0o.x(), 0.0f, v0o.z()}, {n.x(), n.y(), n.z()}, {1, 0}});
        verts.push_back({{v1o.x(), 0.0f, v1o.z()}, {n.x(), n.y(), n.z()}, {1, 1}});
        verts.push_back({{v1i.x(), 0.0f, v1i.z()}, {n.x(), n.y(), n.z()}, {0, 1}});
        idx.push_back(base + 0); idx.push_back(base + 1); idx.push_back(base + 2);
        idx.push_back(base + 2); idx.push_back(base + 3); idx.push_back(base + 0);
    }
    return new Mesh(verts, idx);
}

Render::GL::Mesh* SelectionRing::get() {
    if (!s_mesh) s_mesh.reset(createRingMesh());
    return s_mesh.get();
}

} // namespace Render::Geom
