#include "arrow.h"
#include "../../game/systems/arrow_system.h"
#include "../entity/registry.h"
#include "../gl/mesh.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <vector>

namespace Render {
namespace Geom {

static GL::Mesh *createArrowMesh() {
  using GL::Vertex;
  std::vector<GL::Vertex> verts;
  std::vector<unsigned int> idx;

  const int radial = 12;
  const float shaftRadius = 0.05f;
  const float shaftLen = 0.85f;
  const float tipLen = 0.15f;
  const float tipStartZ = shaftLen;
  const float tipEndZ = shaftLen + tipLen;

  int baseIndex = 0;
  for (int ring = 0; ring < 2; ++ring) {
    float z = (ring == 0) ? 0.0f : shaftLen;
    for (int i = 0; i < radial; ++i) {
      float a = (float(i) / radial) * 6.2831853f;
      float x = std::cos(a) * shaftRadius;
      float y = std::sin(a) * shaftRadius;
      QVector3D n(x, y, 0.0f);
      n.normalize();
      verts.push_back(
          {{x, y, z}, {n.x(), n.y(), n.z()}, {float(i) / radial, z}});
    }
  }

  for (int i = 0; i < radial; ++i) {
    int next = (i + 1) % radial;
    int a = baseIndex + i;
    int b = baseIndex + next;
    int c = baseIndex + radial + next;
    int d = baseIndex + radial + i;
    idx.push_back(a);
    idx.push_back(b);
    idx.push_back(c);
    idx.push_back(c);
    idx.push_back(d);
    idx.push_back(a);
  }

  int ringStart = verts.size();
  for (int i = 0; i < radial; ++i) {
    float a = (float(i) / radial) * 6.2831853f;
    float x = std::cos(a) * shaftRadius * 1.4f;
    float y = std::sin(a) * shaftRadius * 1.4f;
    QVector3D n(x, y, 0.2f);
    n.normalize();
    verts.push_back(
        {{x, y, tipStartZ}, {n.x(), n.y(), n.z()}, {float(i) / radial, 0.0f}});
  }

  int apexIndex = verts.size();
  verts.push_back({{0.0f, 0.0f, tipEndZ}, {0.0f, 0.0f, 1.0f}, {0.5f, 1.0f}});
  for (int i = 0; i < radial; ++i) {
    int next = (i + 1) % radial;
    idx.push_back(ringStart + i);
    idx.push_back(apexIndex);
    idx.push_back(ringStart + next);
  }

  return new GL::Mesh(verts, idx);
}

GL::Mesh *Arrow::get() {
  static GL::Mesh *mesh = createArrowMesh();
  return mesh;
}

} // namespace Geom

namespace GL {

void renderArrows(Renderer *renderer, ResourceManager *resources,
                  const Game::Systems::ArrowSystem &arrowSystem) {
  if (!renderer || !resources)
    return;
  auto *arrowMesh = resources->arrow();
  if (!arrowMesh)
    return;

  const auto &arrows = arrowSystem.arrows();
  for (const auto &arrow : arrows) {
    if (!arrow.active)
      continue;

    const QVector3D delta = arrow.end - arrow.start;
    const float dist = std::max(0.001f, delta.length());
    QVector3D pos = arrow.start + delta * arrow.t;
    float h = arrow.arcHeight * 4.0f * arrow.t * (1.0f - arrow.t);
    pos.setY(pos.y() + h);

    QMatrix4x4 model;
    model.translate(pos.x(), pos.y(), pos.z());

    QVector3D dir = delta.normalized();
    float yawDeg = std::atan2(dir.x(), dir.z()) * 180.0f / 3.14159265f;
    model.rotate(yawDeg, QVector3D(0, 1, 0));

    float vy = (arrow.end.y() - arrow.start.y()) / dist;
    float pitchDeg =
        -std::atan2(vy - (8.0f * arrow.arcHeight * (arrow.t - 0.5f) / dist),
                    1.0f) *
        180.0f / 3.14159265f;
    model.rotate(pitchDeg, QVector3D(1, 0, 0));

    constexpr float ARROW_Z_SCALE = 0.40f;
    constexpr float ARROW_XY_SCALE = 0.26f;
    constexpr float ARROW_Z_TRANSLATE_FACTOR = 0.5f;
    model.translate(0.0f, 0.0f, -ARROW_Z_SCALE * ARROW_Z_TRANSLATE_FACTOR);
    model.scale(ARROW_XY_SCALE, ARROW_XY_SCALE, ARROW_Z_SCALE);

    renderer->mesh(arrowMesh, model, arrow.color, nullptr, 1.0f);
  }
}

} // namespace GL
} // namespace Render
