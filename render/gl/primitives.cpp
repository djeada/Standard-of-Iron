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

float simpleHash(float seed) {
  float x = std::sin(seed * 12.9898f) * 43758.5453f;
  return x - std::floor(x);
}

Mesh *createUnitTorsoMesh(int radialSegments, int heightSegments) {
  const float halfH = 0.5f;
  const float TWO_PI = 6.28318530718f;

  const bool invertProfile = true;

  auto clampf = [](float x, float a, float b) {
    return x < a ? a : (x > b ? b : x);
  };
  auto smoothstep01 = [&](float x) {
    x = clampf(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
  };
  auto smoothBand = [&](float t, float a, float b) {
    float enter = smoothstep01((t - a) / (b - a + 1e-6f));
    float exit = smoothstep01((t - b) / (a - b - 1e-6f));
    float v = enter < exit ? enter : exit;
    return clampf(v, 0.0f, 1.0f);
  };

  struct Axes {
    float ax;
    float az;
  };
  struct Key {
    float t;
    Axes A;
  };

  const Key keys[] = {
      {0.10f, {0.98f, 0.92f}}, {0.20f, {1.02f, 0.96f}}, {0.45f, {0.82f, 0.78f}},
      {0.65f, {1.20f, 1.04f}}, {0.85f, {1.42f, 1.18f}}, {1.02f, {1.60f, 1.06f}},
      {1.10f, {1.20f, 0.96f}},
  };
  constexpr int KEY_COUNT = sizeof(keys) / sizeof(keys[0]);

  auto catRom = [](float p0, float p1, float p2, float p3, float u) {
    return 0.5f * ((2.0f * p1) + (-p0 + p2) * u +
                   (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * u * u +
                   (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * u * u * u);
  };

  auto sampleAxes = [&](float t) -> Axes {
    t = clampf(t, 0.0f, 1.0f);
    int i = 0;
    while (i + 1 < KEY_COUNT && t > keys[i + 1].t)
      ++i;
    int i0 = i > 0 ? i - 1 : 0;
    int i1 = i;
    int i2 = (i + 1 < KEY_COUNT) ? i + 1 : KEY_COUNT - 1;
    int i3 = (i + 2 < KEY_COUNT) ? i + 2 : KEY_COUNT - 1;

    float denom = (keys[i2].t - keys[i1].t);
    float u = denom > 1e-6f ? (t - keys[i1].t) / denom : 0.0f;
    u = clampf(u, 0.0f, 1.0f);

    float ax =
        catRom(keys[i0].A.ax, keys[i1].A.ax, keys[i2].A.ax, keys[i3].A.ax, u);
    float az =
        catRom(keys[i0].A.az, keys[i1].A.az, keys[i2].A.az, keys[i3].A.az, u);
    return {ax, az};
  };

  auto ellipseRadius = [](float a, float b, float ang) {
    float c = std::cos(ang), s = std::sin(ang);
    float denom = std::sqrt((b * b * c * c) + (a * a * s * s));
    return (a * b) / (denom + 1e-8f);
  };

  auto xOffsetAt = [&](float t) {
    return 0.02f * smoothBand(t, 0.6f, 0.95f) -
           0.01f * smoothBand(t, 0.0f, 0.2f);
  };
  auto zOffsetAt = [&](float t) {
    float lordosis = -0.03f * smoothBand(t, 0.15f, 0.40f);
    float chestFwd = 0.035f * smoothBand(t, 0.65f, 0.85f);
    float neckBack = -0.015f * smoothBand(t, 0.90f, 1.00f);
    return lordosis + chestFwd + neckBack;
  };
  auto twistAt = [&](float t) { return 0.10f * smoothBand(t, 0.55f, 0.95f); };

  auto thetaScale = [&](float t, float ang) {
    float s = 0.0f;
    float sinA = std::sin(ang), cosA = std::cos(ang), cos2 = cosA * cosA;
    s += 0.07f * smoothBand(t, 0.68f, 0.88f) * std::max(0.0f, sinA);
    s += -0.03f * smoothBand(t, 0.65f, 0.90f) * std::max(0.0f, -sinA);
    s += 0.06f * smoothBand(t, 0.55f, 0.75f) * cos2;
    s += -0.02f * smoothBand(t, 0.40f, 0.55f) * cos2;
    s += 0.015f * smoothBand(t, 0.70f, 0.95f) * cosA;
    return 1.0f + s;
  };

  auto micro = [](float s) {
    float f = std::sin(s * 12.9898f) * 43758.5453f;
    return f - std::floor(f);
  };

  auto samplePos = [&](float t, float ang) -> QVector3D {
    float ts = invertProfile ? (1.0f - t) : t;

    Axes A = sampleAxes(ts);
    float twist = twistAt(ts);
    float th = ang + twist;

    float R = ellipseRadius(A.ax, A.az, th);
    float S = thetaScale(ts, th);
    float r = R * S;

    float px = r * std::cos(th);
    float pz = r * std::sin(th);

    px += xOffsetAt(ts);
    pz += zOffsetAt(ts);

    float py = -halfH + t * (2.0f * halfH);

    float s = (t * 37.0f) + (ang * 3.0f);
    px += (micro(s) - 0.5f) * 0.004f;
    pz += (micro(s + 1.23f) - 0.5f) * 0.004f;

    return QVector3D(px, py, pz);
  };

  std::vector<Vertex> v;
  std::vector<unsigned int> idx;
  v.reserve((radialSegments + 1) * (heightSegments + 1) +
            (radialSegments + 1) * 2 + 2);
  idx.reserve(radialSegments * heightSegments * 6 + radialSegments * 6);

  for (int y = 0; y <= heightSegments; ++y) {
    float t = float(y) / float(heightSegments);
    float dt = 1.0f / float(heightSegments);
    float vCoord = t;

    for (int i = 0; i <= radialSegments; ++i) {
      float u = float(i) / float(radialSegments);
      float ang = u * TWO_PI;
      float da = TWO_PI / float(radialSegments);

      QVector3D p = samplePos(t, ang);
      QVector3D pu = samplePos(t, ang + da);
      QVector3D pv = samplePos(clampf(t + dt, 0.0f, 1.0f), ang);

      QVector3D du = pu - p;
      QVector3D dv = pv - p;

      QVector3D n = QVector3D::crossProduct(du, dv);
      if (n.lengthSquared() > 0.0f)
        n.normalize();

      v.push_back({{p.x(), p.y(), p.z()}, {n.x(), n.y(), n.z()}, {u, vCoord}});
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

  {

    int baseTop = (int)v.size();
    float tTop = 1.0f;
    float tTopS = invertProfile ? (1.0f - tTop) : tTop;
    QVector3D cTop(xOffsetAt(tTopS), halfH, zOffsetAt(tTopS));
    v.push_back({{cTop.x(), cTop.y(), cTop.z()}, {0, 1, 0}, {0.5f, 0.5f}});
    for (int i = 0; i <= radialSegments; ++i) {
      float u = float(i) / float(radialSegments);
      float ang = u * TWO_PI;
      QVector3D p = samplePos(tTop, ang);
      v.push_back({{p.x(), p.y(), p.z()},
                   {0, 1, 0},
                   {0.5f + 0.5f * std::cos(ang), 0.5f + 0.5f * std::sin(ang)}});
    }
    for (int i = 1; i <= radialSegments; ++i) {
      idx.push_back(baseTop);
      idx.push_back(baseTop + i);
      idx.push_back(baseTop + i + 1);
    }
  }
  {

    int baseBot = (int)v.size();
    float tBot = 0.0f;
    float tBotS = invertProfile ? (1.0f - tBot) : tBot;
    QVector3D cBot(xOffsetAt(tBotS), -halfH, zOffsetAt(tBotS));
    v.push_back({{cBot.x(), cBot.y(), cBot.z()}, {0, -1, 0}, {0.5f, 0.5f}});
    for (int i = 0; i <= radialSegments; ++i) {
      float u = float(i) / float(radialSegments);
      float ang = u * TWO_PI;
      QVector3D p = samplePos(tBot, ang);
      v.push_back({{p.x(), p.y(), p.z()},
                   {0, -1, 0},
                   {0.5f + 0.5f * std::cos(ang), 0.5f + 0.5f * std::sin(ang)}});
    }
    for (int i = 1; i <= radialSegments; ++i) {
      idx.push_back(baseBot);
      idx.push_back(baseBot + i + 1);
      idx.push_back(baseBot + i);
    }
  }

  return new Mesh(v, idx);
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

Mesh *getUnitTorso(int radialSegments, int heightSegments) {
  static std::unique_ptr<Mesh> s_mesh(
      createUnitTorsoMesh(radialSegments, heightSegments));
  return s_mesh.get();
}

} // namespace Render::GL
