#include "primitives.h"
#include <QVector3D>
#include <cmath>
#include <memory>
#include <vector>

namespace Render::GL {

namespace {

Mesh *createUnitCylinderMesh(int radialSegments) {
  const float radius = 1.0f;
  const float halfH = 0.5f;

  std::vector<Vertex> v;
  std::vector<unsigned int> idx;

  for (int y = 0; y <= 1; ++y) {
    float py = y ? halfH : -halfH;
    float vCoord = float(y);
    for (int i = 0; i <= radialSegments; ++i) {
      float u = float(i) / float(radialSegments);
      float ang = u * 6.28318530718f;
      float px = radius * std::cos(ang);
      float pz = radius * std::sin(ang);
      QVector3D n(px, 0.0f, pz);
      n.normalize();
      v.push_back({{px, py, pz}, {n.x(), n.y(), n.z()}, {u, vCoord}});
    }
  }
  int row = radialSegments + 1;
  for (int i = 0; i < radialSegments; ++i) {
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
  for (int i = 0; i <= radialSegments; ++i) {
    float u = float(i) / float(radialSegments);
    float ang = u * 6.28318530718f;
    float px = radius * std::cos(ang);
    float pz = radius * std::sin(ang);
    v.push_back({{px, halfH, pz},
                 {0.0f, 1.0f, 0.0f},
                 {0.5f + 0.5f * std::cos(ang), 0.5f + 0.5f * std::sin(ang)}});
  }
  for (int i = 1; i <= radialSegments; ++i) {
    idx.push_back(baseTop);
    idx.push_back(baseTop + i);
    idx.push_back(baseTop + i + 1);
  }

  int baseBot = (int)v.size();
  v.push_back({{0.0f, -halfH, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.5f, 0.5f}});
  for (int i = 0; i <= radialSegments; ++i) {
    float u = float(i) / float(radialSegments);
    float ang = u * 6.28318530718f;
    float px = radius * std::cos(ang);
    float pz = radius * std::sin(ang);
    v.push_back({{px, -halfH, pz},
                 {0.0f, -1.0f, 0.0f},
                 {0.5f + 0.5f * std::cos(ang), 0.5f + 0.5f * std::sin(ang)}});
  }
  for (int i = 1; i <= radialSegments; ++i) {
    idx.push_back(baseBot);
    idx.push_back(baseBot + i + 1);
    idx.push_back(baseBot + i);
  }

  return new Mesh(v, idx);
}

Mesh *createUnitSphereMesh(int latSegments, int lonSegments) {
  const float r = 1.0f;
  std::vector<Vertex> v;
  std::vector<unsigned int> idx;

  for (int y = 0; y <= latSegments; ++y) {
    float vy = float(y) / float(latSegments);
    float phi = vy * 3.14159265358979323846f;
    float py = r * std::cos(phi);
    float pr = r * std::sin(phi);

    for (int x = 0; x <= lonSegments; ++x) {
      float vx = float(x) / float(lonSegments);
      float theta = vx * 6.28318530717958647692f;
      float px = pr * std::cos(theta);
      float pz = pr * std::sin(theta);

      QVector3D n(px, py, pz);
      n.normalize();
      v.push_back({{px, py, pz}, {n.x(), n.y(), n.z()}, {vx, vy}});
    }
  }

  int row = lonSegments + 1;
  for (int y = 0; y < latSegments; ++y) {
    for (int x = 0; x < lonSegments; ++x) {
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

Mesh *createUnitConeMesh(int radialSegments) {
  const float baseR = 1.0f;
  const float halfH = 0.5f;

  std::vector<Vertex> v;
  std::vector<unsigned int> idx;

  int apexIdx = 0;
  v.push_back({{0.0f, +halfH, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.5f, 1.0f}});

  for (int i = 0; i <= radialSegments; ++i) {
    float u = float(i) / float(radialSegments);
    float ang = u * 6.28318530718f;
    float px = baseR * std::cos(ang);
    float pz = baseR * std::sin(ang);
    QVector3D n(px, baseR, pz);
    n.normalize();
    v.push_back({{px, -halfH, pz}, {n.x(), n.y(), n.z()}, {u, 0.0f}});
  }

  for (int i = 1; i <= radialSegments; ++i) {
    idx.push_back(apexIdx);
    idx.push_back(i);
    idx.push_back(i + 1);
  }

  int baseCenter = (int)v.size();
  v.push_back({{0.0f, -halfH, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.5f, 0.5f}});
  int baseStart = (int)v.size();
  for (int i = 0; i <= radialSegments; ++i) {
    float u = float(i) / float(radialSegments);
    float ang = u * 6.28318530718f;
    float px = baseR * std::cos(ang);
    float pz = baseR * std::sin(ang);
    v.push_back({{px, -halfH, pz},
                 {0.0f, -1.0f, 0.0f},
                 {0.5f + 0.5f * std::cos(ang), 0.5f + 0.5f * std::sin(ang)}});
  }
  for (int i = 0; i < radialSegments; ++i) {
    idx.push_back(baseCenter);
    idx.push_back(baseStart + i + 1);
    idx.push_back(baseStart + i);
  }

  return new Mesh(v, idx);
}

Mesh *createCapsuleMesh(int radialSegments, int heightSegments) {
  const float radius = 0.25f;
  const float halfH = 0.5f;

  std::vector<Vertex> verts;
  std::vector<unsigned int> idx;

  for (int y = 0; y <= heightSegments; ++y) {
    float v = float(y) / float(heightSegments);
    float py = -halfH + v * (2.0f * halfH);
    for (int i = 0; i <= radialSegments; ++i) {
      float u = float(i) / float(radialSegments);
      float ang = u * 6.2831853f;
      float px = radius * std::cos(ang);
      float pz = radius * std::sin(ang);
      QVector3D n(px, 0.0f, pz);
      n.normalize();
      verts.push_back({{px, py, pz}, {n.x(), n.y(), n.z()}, {u, v}});
    }
  }

  int row = radialSegments + 1;
  for (int y = 0; y < heightSegments; ++y) {
    for (int i = 0; i < radialSegments; ++i) {
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
  for (int i = 0; i <= radialSegments; ++i) {
    float u = float(i) / float(radialSegments);
    float ang = u * 6.2831853f;
    float px = radius * std::cos(ang);
    float pz = radius * std::sin(ang);
    verts.push_back(
        {{px, halfH, pz},
         {0.0f, 1.0f, 0.0f},
         {0.5f + 0.5f * std::cos(ang), 0.5f + 0.5f * std::sin(ang)}});
  }
  for (int i = 1; i <= radialSegments; ++i) {
    idx.push_back(baseTop);
    idx.push_back(baseTop + i);
    idx.push_back(baseTop + i + 1);
  }

  int baseBot = (int)verts.size();
  verts.push_back({{0.0f, -halfH, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.5f, 0.5f}});
  for (int i = 0; i <= radialSegments; ++i) {
    float u = float(i) / float(radialSegments);
    float ang = u * 6.2831853f;
    float px = radius * std::cos(ang);
    float pz = radius * std::sin(ang);
    verts.push_back(
        {{px, -halfH, pz},
         {0.0f, -1.0f, 0.0f},
         {0.5f + 0.5f * std::cos(ang), 0.5f + 0.5f * std::sin(ang)}});
  }
  for (int i = 1; i <= radialSegments; ++i) {
    idx.push_back(baseBot);
    idx.push_back(baseBot + i + 1);
    idx.push_back(baseBot + i);
  }

  return new Mesh(verts, idx);
}

} // namespace

Mesh *getUnitCylinder(int radialSegments) {
  static std::unique_ptr<Mesh> s_mesh(createUnitCylinderMesh(radialSegments));
  return s_mesh.get();
}

Mesh *getUnitSphere(int latSegments, int lonSegments) {
  static std::unique_ptr<Mesh> s_mesh(
      createUnitSphereMesh(latSegments, lonSegments));
  return s_mesh.get();
}

Mesh *getUnitCone(int radialSegments) {
  static std::unique_ptr<Mesh> s_mesh(createUnitConeMesh(radialSegments));
  return s_mesh.get();
}

Mesh *getUnitCapsule(int radialSegments, int heightSegments) {
  static std::unique_ptr<Mesh> s_mesh(
      createCapsuleMesh(radialSegments, heightSegments));
  return s_mesh.get();
}

} // namespace Render::GL
