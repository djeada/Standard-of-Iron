#include "arrow.h"
#include "../gl/mesh.h"
#include <vector>
#include <cmath>
#include <QVector3D>

namespace Render::Geom {

static Render::GL::Mesh* createArrowMesh() {
    using Render::GL::Vertex;
    std::vector<Vertex> verts;
    std::vector<unsigned int> idx;
    // Parameters for a simple arrow of length 1 along +Z
    const int radial = 12;
    const float shaftRadius = 0.05f;
    const float shaftLen = 0.85f;   // 85% shaft, 15% tip
    const float tipLen = 0.15f;
    const float tipStartZ = shaftLen;
    const float tipEndZ = shaftLen + tipLen; // should be 1.0
    // Shaft cylinder: two rings at z=0 and z=shaftLen
    int baseIndex = 0;
    for (int ring = 0; ring < 2; ++ring) {
        float z = (ring == 0) ? 0.0f : shaftLen;
        for (int i = 0; i < radial; ++i) {
            float a = (float(i) / radial) * 6.2831853f;
            float x = std::cos(a) * shaftRadius;
            float y = std::sin(a) * shaftRadius;
            QVector3D n(x, y, 0.0f);
            n.normalize();
            verts.push_back({{x, y, z}, {n.x(), n.y(), n.z()}, {float(i)/radial, z}});
        }
    }
    // Shaft indices (quads split into triangles)
    for (int i = 0; i < radial; ++i) {
        int next = (i + 1) % radial;
        int a = baseIndex + i;
        int b = baseIndex + next;
        int c = baseIndex + radial + next;
        int d = baseIndex + radial + i;
        idx.push_back(a); idx.push_back(b); idx.push_back(c);
        idx.push_back(c); idx.push_back(d); idx.push_back(a);
    }
    // Tip cone: triangle fan from tip apex to ring at tipStartZ
    int ringStart = verts.size();
    for (int i = 0; i < radial; ++i) {
        float a = (float(i) / radial) * 6.2831853f;
    float x = std::cos(a) * shaftRadius * 1.4f; // slightly larger base for tip
    float y = std::sin(a) * shaftRadius * 1.4f;
        QVector3D n(x, y, 0.2f);
        n.normalize();
        verts.push_back({{x, y, tipStartZ}, {n.x(), n.y(), n.z()}, {float(i)/radial, 0.0f}});
    }
    int apexIndex = verts.size();
    verts.push_back({{0.0f, 0.0f, tipEndZ}, {0.0f, 0.0f, 1.0f}, {0.5f, 1.0f}});
    for (int i = 0; i < radial; ++i) {
        int next = (i + 1) % radial;
        int a = ringStart + i;
        int b = ringStart + next;
        int apex = apexIndex;
        idx.push_back(a); idx.push_back(apex); idx.push_back(b);
    }
    return new Render::GL::Mesh(verts, idx);
}

Render::GL::Mesh* Arrow::get() {
    static Render::GL::Mesh* mesh = createArrowMesh();
    return mesh;
}

} // namespace Render::Geom
