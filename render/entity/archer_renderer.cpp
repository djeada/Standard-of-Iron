#include "archer_renderer.h"
#include "../../game/core/component.h"
#include "../../game/core/entity.h"
#include "../../game/visuals/team_colors.h"
#include "../geom/selection_ring.h"
#include "../gl/mesh.h"
#include "../gl/texture.h"
#include "registry.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace Render::GL {

static Mesh *createUnitCylinderMesh() {
  const int radial = 24;
  const float radius = 1.0f;
  const float halfH = 0.5f;
  std::vector<Vertex> v;
  std::vector<unsigned int> idx;

  for (int y = 0; y <= 1; ++y) {
    float py = y ? halfH : -halfH;
    float vCoord = float(y);
    for (int i = 0; i <= radial; ++i) {
      float u = float(i) / float(radial);
      float ang = u * 6.28318530718f;
      float px = radius * std::cos(ang);
      float pz = radius * std::sin(ang);
      QVector3D n(px, 0.0f, pz);
      n.normalize();
      v.push_back({{px, py, pz}, {n.x(), n.y(), n.z()}, {u, vCoord}});
    }
  }
  int row = radial + 1;

  for (int i = 0; i < radial; ++i) {
    int a = 0 * row + i;
    int b = 0 * row + i + 1;
    int c = 1 * row + i + 1;
    int d = 1 * row + i;
    idx.push_back(a);
    idx.push_back(b);
    idx.push_back(c);
    idx.push_back(c);
    idx.push_back(d);
    idx.push_back(a);
  }

  int baseTop = (int)v.size();
  v.push_back({{0.0f, halfH, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.5f, 0.5f}});
  for (int i = 0; i <= radial; ++i) {
    float u = float(i) / float(radial);
    float ang = u * 6.28318530718f;
    float px = radius * std::cos(ang);
    float pz = radius * std::sin(ang);
    v.push_back({{px, halfH, pz},
                 {0.0f, 1.0f, 0.0f},
                 {0.5f + 0.5f * std::cos(ang), 0.5f + 0.5f * std::sin(ang)}});
  }
  for (int i = 1; i <= radial; ++i) {
    idx.push_back(baseTop);
    idx.push_back(baseTop + i);
    idx.push_back(baseTop + i + 1);
  }

  int baseBot = (int)v.size();
  v.push_back({{0.0f, -halfH, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.5f, 0.5f}});
  for (int i = 0; i <= radial; ++i) {
    float u = float(i) / float(radial);
    float ang = u * 6.28318530718f;
    float px = radius * std::cos(ang);
    float pz = radius * std::sin(ang);
    v.push_back({{px, -halfH, pz},
                 {0.0f, -1.0f, 0.0f},
                 {0.5f + 0.5f * std::cos(ang), 0.5f + 0.5f * std::sin(ang)}});
  }
  for (int i = 1; i <= radial; ++i) {
    idx.push_back(baseBot);
    idx.push_back(baseBot + i + 1);
    idx.push_back(baseBot + i);
  }
  return new Mesh(v, idx);
}

static Mesh *createUnitSphereMesh() {
  const int lat = 12;
  const int lon = 24;
  const float r = 1.0f;
  std::vector<Vertex> v;
  std::vector<unsigned int> idx;

  for (int y = 0; y <= lat; ++y) {
    float vy = float(y) / float(lat);
    float phi = vy * 3.1415926535f;
    float py = r * std::cos(phi - 1.57079632679f);
    float pr = r * std::sin(phi);
    for (int x = 0; x <= lon; ++x) {
      float vx = float(x) / float(lon);
      float theta = vx * 6.28318530718f;
      float px = pr * std::cos(theta);
      float pz = pr * std::sin(theta);
      QVector3D n(px, py, pz);
      n.normalize();
      v.push_back({{px, py, pz}, {n.x(), n.y(), n.z()}, {vx, vy}});
    }
  }
  int row = lon + 1;
  for (int y = 0; y < lat; ++y) {
    for (int x = 0; x < lon; ++x) {
      int a = y * row + x;
      int b = a + 1;
      int c = (y + 1) * row + x + 1;
      int d = (y + 1) * row + x;
      idx.push_back(a);
      idx.push_back(b);
      idx.push_back(c);
      idx.push_back(c);
      idx.push_back(d);
      idx.push_back(a);
    }
  }
  return new Mesh(v, idx);
}

static Mesh *createUnitConeMesh() {
  const int radial = 24;
  const float baseR = 1.0f;
  const float halfH = 0.5f;
  std::vector<Vertex> v;
  std::vector<unsigned int> idx;

  int apexIdx = 0;
  v.push_back({{0.0f, +halfH, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.5f, 1.0f}});

  for (int i = 0; i <= radial; ++i) {
    float u = float(i) / float(radial);
    float ang = u * 6.28318530718f;
    float px = baseR * std::cos(ang);
    float pz = baseR * std::sin(ang);

    QVector3D n(px, baseR, pz);
    n.normalize();
    v.push_back({{px, -halfH, pz}, {n.x(), n.y(), n.z()}, {u, 0.0f}});
  }

  for (int i = 1; i <= radial; ++i) {
    int a = apexIdx;
    int b = i;
    int c = i + 1;
    idx.push_back(a);
    idx.push_back(b);
    idx.push_back(c);
  }

  int baseCenter = (int)v.size();
  v.push_back({{0.0f, -halfH, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.5f, 0.5f}});

  int baseStart = (int)v.size();
  for (int i = 0; i <= radial; ++i) {
    float u = float(i) / float(radial);
    float ang = u * 6.28318530718f;
    float px = baseR * std::cos(ang);
    float pz = baseR * std::sin(ang);
    v.push_back({{px, -halfH, pz},
                 {0.0f, -1.0f, 0.0f},
                 {0.5f + 0.5f * std::cos(ang), 0.5f + 0.5f * std::sin(ang)}});
  }
  for (int i = 0; i < radial; ++i) {
    idx.push_back(baseCenter);
    idx.push_back(baseStart + i + 1);
    idx.push_back(baseStart + i);
  }
  return new Mesh(v, idx);
}

static Mesh *createCapsuleMesh() {
  const int radial = 24;
  const int heightSegments = 1;
  const float radius = 0.25f;
  const float halfH = 0.5f;
  std::vector<Vertex> verts;
  std::vector<unsigned int> idx;

  for (int y = 0; y <= heightSegments; ++y) {
    float v = float(y) / float(heightSegments);
    float py = -halfH + v * (2.0f * halfH);
    for (int i = 0; i <= radial; ++i) {
      float u = float(i) / float(radial);
      float ang = u * 6.2831853f;
      float px = radius * std::cos(ang);
      float pz = radius * std::sin(ang);
      QVector3D n(px, 0.0f, pz);
      n.normalize();
      verts.push_back({{px, py, pz}, {n.x(), n.y(), n.z()}, {u, v}});
    }
  }
  int row = radial + 1;
  for (int y = 0; y < heightSegments; ++y) {
    for (int i = 0; i < radial; ++i) {
      int a = y * row + i;
      int b = y * row + i + 1;
      int c = (y + 1) * row + i + 1;
      int d = (y + 1) * row + i;
      idx.push_back(a);
      idx.push_back(b);
      idx.push_back(c);
      idx.push_back(c);
      idx.push_back(d);
      idx.push_back(a);
    }
  }

  int baseTop = (int)verts.size();
  verts.push_back({{0.0f, halfH, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.5f, 0.5f}});
  for (int i = 0; i <= radial; ++i) {
    float u = float(i) / float(radial);
    float ang = u * 6.2831853f;
    float px = radius * std::cos(ang);
    float pz = radius * std::sin(ang);
    verts.push_back(
        {{px, halfH, pz},
         {0.0f, 1.0f, 0.0f},
         {0.5f + 0.5f * std::cos(ang), 0.5f + 0.5f * std::sin(ang)}});
  }
  for (int i = 1; i <= radial; ++i) {
    idx.push_back(baseTop);
    idx.push_back(baseTop + i);
    idx.push_back(baseTop + i + 1);
  }

  int baseBot = (int)verts.size();
  verts.push_back({{0.0f, -halfH, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.5f, 0.5f}});
  for (int i = 0; i <= radial; ++i) {
    float u = float(i) / float(radial);
    float ang = u * 6.2831853f;
    float px = radius * std::cos(ang);
    float pz = radius * std::sin(ang);
    verts.push_back(
        {{px, -halfH, pz},
         {0.0f, -1.0f, 0.0f},
         {0.5f + 0.5f * std::cos(ang), 0.5f + 0.5f * std::sin(ang)}});
  }
  for (int i = 1; i <= radial; ++i) {
    idx.push_back(baseBot);
    idx.push_back(baseBot + i + 1);
    idx.push_back(baseBot + i);
  }
  return new Mesh(verts, idx);
}

static Mesh *getUnitCylinder() {
  static std::unique_ptr<Mesh> m(createUnitCylinderMesh());
  return m.get();
}
static Mesh *getUnitSphere() {
  static std::unique_ptr<Mesh> m(createUnitSphereMesh());
  return m.get();
}
static Mesh *getUnitCone() {
  static std::unique_ptr<Mesh> m(createUnitConeMesh());
  return m.get();
}
static Mesh *getArcherCapsule() {
  static std::unique_ptr<Mesh> m(createCapsuleMesh());
  return m.get();
}

static inline float clamp01(float x) {
  return std::max(0.0f, std::min(1.0f, x));
}

static QMatrix4x4 cylinderBetween(const QVector3D &a, const QVector3D &b,
                                  float radius) {
  QVector3D mid = (a + b) * 0.5f;
  QVector3D dir = b - a;
  float len = dir.length();
  QMatrix4x4 M;
  M.translate(mid);
  if (len > 1e-6f) {
    QVector3D yAxis(0, 1, 0);
    QVector3D d = dir / len;
    float dot = std::clamp(QVector3D::dotProduct(yAxis, d), -1.0f, 1.0f);
    float angleDeg = std::acos(dot) * 57.2957795131f;
    QVector3D axis = QVector3D::crossProduct(yAxis, d);
    if (axis.lengthSquared() < 1e-6f) {
      if (dot < 0.0f)
        M.rotate(180.0f, 1.0f, 0.0f, 0.0f);
    } else {
      axis.normalize();
      M.rotate(angleDeg, axis);
    }
    M.scale(radius, len, radius);
  } else {
    M.scale(radius, 1.0f, radius);
  }
  return M;
}

static QMatrix4x4 sphereAt(const QVector3D &pos, float radius) {
  QMatrix4x4 M;
  M.translate(pos);
  M.scale(radius, radius, radius);
  return M;
}

static QMatrix4x4 coneFromTo(const QVector3D &baseCenter, const QVector3D &apex,
                             float baseRadius) {
  return cylinderBetween(baseCenter, apex, baseRadius);
}

void registerArcherRenderer(EntityRendererRegistry &registry) {
  registry.registerRenderer("archer", [](const DrawContext &p,
                                         ISubmitter &out) {
    QVector3D tunic(0.8f, 0.9f, 1.0f);
    Engine::Core::UnitComponent *unit = nullptr;
    Engine::Core::RenderableComponent *rc = nullptr;
    if (p.entity) {
      unit = p.entity->getComponent<Engine::Core::UnitComponent>();
      rc = p.entity->getComponent<Engine::Core::RenderableComponent>();
    }
    if (unit && unit->ownerId > 0) {
      tunic = Game::Visuals::teamColorForOwner(unit->ownerId);
    } else if (rc) {
      tunic = QVector3D(rc->color[0], rc->color[1], rc->color[2]);
    }
    auto tint = [&](float k) -> QVector3D {
      return QVector3D(clamp01(tunic.x() * k), clamp01(tunic.y() * k),
                       clamp01(tunic.z() * k));
    };

    const QVector3D skin(0.96f, 0.80f, 0.69f);
    const QVector3D leather(0.35f, 0.22f, 0.12f);
    const QVector3D wood(0.16f, 0.10f, 0.05f);
    const QVector3D metal(0.65f, 0.66f, 0.70f);
    const QVector3D fletch = tint(0.9f);

    const QVector3D headPos(0.0f, 0.82f, 0.0f);
    const QVector3D hipL(-0.12f, -0.45f, 0.03f);
    const QVector3D hipR(0.12f, -0.45f, -0.03f);
    const QVector3D footL(-0.14f, -1.00f, 0.06f);
    const QVector3D footR(0.14f, -1.00f, -0.06f);

    const QVector3D shoulderL(-0.18f, 0.35f, 0.00f);
    const QVector3D shoulderR(0.18f, 0.35f, 0.00f);

    const QVector3D elbowL(-0.45f, 0.27f, 0.02f);
    const QVector3D handL(-0.60f, 0.20f, 0.05f);

    const QVector3D elbowR(0.36f, 0.35f, 0.10f);
    const QVector3D handR(0.25f, 0.45f, 0.15f);

    const float bowTopY = 0.65f;
    const float bowBotY = -0.15f;
    const float bowX = -0.60f;
    const float bowZMid = 0.05f;
    const float bowDepth = 0.10f;
    const float bowRodR = 0.02f;
    const float stringR = 0.006f;

    out.mesh(getArcherCapsule(), p.model, tint(0.8f), nullptr, 1.0f);

    {
      QMatrix4x4 M = p.model * sphereAt(headPos, 0.18f);
      out.mesh(getUnitSphere(), M, skin, nullptr, 1.0f);
    }

    {
      QMatrix4x4 ML = p.model * cylinderBetween(hipL, footL, 0.08f);
      QMatrix4x4 MR = p.model * cylinderBetween(hipR, footR, 0.08f);
      out.mesh(getUnitCylinder(), ML, leather, nullptr, 1.0f);
      out.mesh(getUnitCylinder(), MR, leather, nullptr, 1.0f);
    }

    {

      QMatrix4x4 M1 = p.model * cylinderBetween(shoulderL, elbowL, 0.06f);
      QMatrix4x4 M2 = p.model * cylinderBetween(elbowL, handL, 0.055f);
      out.mesh(getUnitCylinder(), M1, leather, nullptr, 1.0f);
      out.mesh(getUnitCylinder(), M2, leather, nullptr, 1.0f);

      QMatrix4x4 M3 = p.model * cylinderBetween(shoulderR, elbowR, 0.06f);
      QMatrix4x4 M4 = p.model * cylinderBetween(elbowR, handR, 0.055f);
      out.mesh(getUnitCylinder(), M3, leather, nullptr, 1.0f);
      out.mesh(getUnitCylinder(), M4, leather, nullptr, 1.0f);

      out.mesh(getUnitSphere(), p.model * sphereAt(handL, 0.07f), skin, nullptr,
               1.0f);
      out.mesh(getUnitSphere(), p.model * sphereAt(handR, 0.07f), skin, nullptr,
               1.0f);
    }

    {
      QVector3D qTop(-0.05f, 0.85f, -0.28f);
      QVector3D qBase(-0.10f, 0.25f, -0.22f);
      QMatrix4x4 MQ = p.model * cylinderBetween(qBase, qTop, 0.06f);
      out.mesh(getUnitCylinder(), MQ, leather, nullptr, 1.0f);

      QVector3D a1 = qTop + QVector3D(0.00f, 0.06f, 0.00f);
      QMatrix4x4 Mshaft1 = p.model * cylinderBetween(qTop, a1, 0.01f);
      out.mesh(getUnitCylinder(), Mshaft1, wood, nullptr, 1.0f);
      out.mesh(getUnitCone(),
               p.model *
                   coneFromTo(a1, a1 + QVector3D(0.0f, 0.06f, 0.0f), 0.03f),
               fletch, nullptr, 1.0f);

      QVector3D a2 = qTop + QVector3D(0.02f, 0.05f, 0.02f);
      QMatrix4x4 Mshaft2 = p.model * cylinderBetween(qTop, a2, 0.01f);
      out.mesh(getUnitCylinder(), Mshaft2, wood, nullptr, 1.0f);
      out.mesh(getUnitCone(),
               p.model *
                   coneFromTo(a2, a2 + QVector3D(0.0f, 0.06f, 0.0f), 0.03f),
               fletch, nullptr, 1.0f);
    }

    {
      const int segs = 12;
      std::vector<QVector3D> bowPts;
      bowPts.reserve(segs + 1);
      for (int i = 0; i <= segs; ++i) {
        float t = float(i) / float(segs);
        float y = bowBotY + t * (bowTopY - bowBotY);

        float z = bowZMid + bowDepth * std::sin((t - 0.5f) * 3.14159265f);
        bowPts.push_back(QVector3D(bowX, y, z));
      }
      for (int i = 0; i < segs; ++i) {
        QMatrix4x4 Mb =
            p.model * cylinderBetween(bowPts[i], bowPts[i + 1], bowRodR);
        out.mesh(getUnitCylinder(), Mb, wood, nullptr, 1.0f);
      }

      QVector3D topEnd = bowPts.back();
      QVector3D botEnd = bowPts.front();
      QMatrix4x4 Ms1 = p.model * cylinderBetween(topEnd, handR, stringR);
      QMatrix4x4 Ms2 = p.model * cylinderBetween(handR, botEnd, stringR);
      out.mesh(getUnitCylinder(), Ms1, metal, nullptr, 1.0f);
      out.mesh(getUnitCylinder(), Ms2, metal, nullptr, 1.0f);
    }

    {
      QVector3D tail = handR + QVector3D(-0.05f, 0.0f, 0.0f);
      QVector3D tip = tail + QVector3D(0.0f, 0.0f, 0.70f);
      QMatrix4x4 Mshaft = p.model * cylinderBetween(tail, tip, 0.01f);
      out.mesh(getUnitCylinder(), Mshaft, wood, nullptr, 1.0f);

      float headLen = 0.08f;
      QVector3D headBase = tip - QVector3D(0.0f, 0.0f, headLen);
      QMatrix4x4 Mhead = p.model * coneFromTo(headBase, tip, 0.03f);
      out.mesh(getUnitCone(), Mhead, metal, nullptr, 1.0f);

      QVector3D f1b = tail - QVector3D(0.0f, 0.0f, 0.02f);
      QVector3D f1a = f1b - QVector3D(0.0f, 0.0f, 0.05f);
      out.mesh(getUnitCone(), p.model * coneFromTo(f1b, f1a, 0.025f), fletch,
               nullptr, 1.0f);

      QVector3D f2b = tail + QVector3D(0.0f, 0.0f, 0.02f);
      QVector3D f2a = f2b + QVector3D(0.0f, 0.0f, 0.05f);
      out.mesh(getUnitCone(), p.model * coneFromTo(f2a, f2b, 0.025f), fletch,
               nullptr, 1.0f);
    }

    if (p.selected) {
      QMatrix4x4 ringM;
      QVector3D pos = p.model.column(3).toVector3D();
      ringM.translate(pos.x(), 0.01f, pos.z());
      ringM.scale(0.5f, 1.0f, 0.5f);
      out.selectionRing(ringM, 0.6f, 0.25f, QVector3D(0.2f, 0.8f, 0.2f));
    }
    if (p.hovered && !p.selected) {
      QMatrix4x4 ringM;
      QVector3D pos = p.model.column(3).toVector3D();
      ringM.translate(pos.x(), 0.01f, pos.z());
      ringM.scale(0.5f, 1.0f, 0.5f);
      out.selectionRing(ringM, 0.35f, 0.15f, QVector3D(0.90f, 0.90f, 0.25f));
    }
  });
}

} // namespace Render::GL
