#include "barracks_renderer.h"
#include "../../game/core/component.h"
#include "../geom/flag.h"
#include "../geom/math_utils.h"
#include "../geom/transforms.h"
#include "../gl/primitives.h"
#include "../gl/resources.h"
#include "registry.h"

#include <QMatrix4x4>
#include <QVector3D>

namespace Render::GL {
namespace {

using Render::Geom::clamp01;
using Render::Geom::clampVec01;
using Render::Geom::cylinderBetween;
using Render::Geom::sphereAt;

struct BuildingProportions {
  static constexpr float baseWidth = 2.4f;
  static constexpr float baseDepth = 2.0f;
  static constexpr float baseHeight = 1.8f;
  static constexpr float foundationHeight = 0.2f;
  static constexpr float wallThickness = 0.08f;
  static constexpr float beamThickness = 0.12f;
  static constexpr float cornerPostRadius = 0.08f;
  static constexpr float roofPitch = 0.8f;
  static constexpr float roofOverhang = 0.15f;
  static constexpr float thatchLayerHeight = 0.12f;
  static constexpr float annexWidth = 1.0f;
  static constexpr float annexDepth = 1.0f;
  static constexpr float annexHeight = 1.2f;
  static constexpr float annexRoofHeight = 0.5f;
  static constexpr float doorWidth = 0.5f;
  static constexpr float doorHeight = 0.8f;
  static constexpr float windowWidth = 0.4f;
  static constexpr float windowHeight = 0.5f;
  static constexpr float chimneyWidth = 0.25f;
  static constexpr float chimneyHeight = 1.0f;
  static constexpr float chimneyCapSize = 0.35f;
  static constexpr float bannerPoleHeight = 2.0f;
  static constexpr float bannerPoleRadius = 0.05f;
  static constexpr float bannerWidth = 0.5f;
  static constexpr float bannerHeight = 0.6f;
};

struct BarracksPalette {
  QVector3D plaster{0.92f, 0.88f, 0.78f};
  QVector3D plasterShade{0.78f, 0.74f, 0.64f};
  QVector3D timber{0.35f, 0.25f, 0.15f};
  QVector3D timberLight{0.50f, 0.38f, 0.22f};
  QVector3D woodDark{0.30f, 0.20f, 0.12f};
  QVector3D thatch{0.82f, 0.70f, 0.28f};
  QVector3D thatchDark{0.68f, 0.58f, 0.22f};
  QVector3D stone{0.55f, 0.54f, 0.52f};
  QVector3D stoneDark{0.42f, 0.41f, 0.39f};
  QVector3D door{0.28f, 0.20f, 0.12f};
  QVector3D window{0.35f, 0.42f, 0.48f};
  QVector3D path{0.62f, 0.60f, 0.54f};
  QVector3D crate{0.48f, 0.34f, 0.18f};
  QVector3D team{0.8f, 0.9f, 1.0f};
  QVector3D teamTrim{0.48f, 0.54f, 0.60f};
};

static inline BarracksPalette makePalette(const QVector3D &team) {
  BarracksPalette p;
  p.team = clampVec01(team);
  p.teamTrim =
      clampVec01(QVector3D(team.x() * 0.6f, team.y() * 0.6f, team.z() * 0.6f));
  return p;
}

static inline QVector3D lerp(const QVector3D &a, const QVector3D &b, float t) {
  return a * (1.0f - t) + b * t;
}

static inline void drawCylinder(ISubmitter &out, const QMatrix4x4 &model,
                                const QVector3D &a, const QVector3D &b,
                                float radius, const QVector3D &color,
                                Texture *white) {
  out.mesh(getUnitCylinder(), model * cylinderBetween(a, b, radius), color,
           white, 1.0f);
}

static inline void unitBox(ISubmitter &out, Mesh *unitMesh, Texture *white,
                           const QMatrix4x4 &model, const QVector3D &t,
                           const QVector3D &s, const QVector3D &color) {
  QMatrix4x4 M = model;
  M.translate(t);
  M.scale(s);
  out.mesh(unitMesh, M, color, white, 1.0f);
}

static inline void unitBox(ISubmitter &out, Mesh *unitMesh, Texture *white,
                           const QMatrix4x4 &model, const QVector3D &s,
                           const QVector3D &color) {
  QMatrix4x4 M = model;
  M.scale(s);
  out.mesh(unitMesh, M, color, white, 1.0f);
}

static inline void drawFoundation(const DrawContext &p, ISubmitter &out,
                                  Mesh *unit, Texture *white,
                                  const BarracksPalette &C) {
  constexpr float baseWidth = BuildingProportions::baseWidth;
  constexpr float baseDepth = BuildingProportions::baseDepth;
  constexpr float foundationHeight = BuildingProportions::foundationHeight;

  unitBox(out, unit, white, p.model,
          QVector3D(0.0f, -foundationHeight / 2, 0.0f),
          QVector3D(baseWidth / 2 + 0.1f, foundationHeight / 2,
                    baseDepth / 2 + 0.1f),
          C.stoneDark);

  const float stepH = 0.015f;
  const float stepW = 0.16f;
  const float stepD = 0.10f;
  const float frontZ = baseDepth * 0.5f + 0.12f;
  for (int i = 0; i < 5; ++i) {
    float t = i / 4.0f;
    float x = (i % 2 == 0) ? -0.18f : 0.18f;
    QVector3D c = lerp(C.path, C.stone, 0.25f * (i % 2));
    unitBox(out, unit, white, p.model,
            QVector3D(x, -foundationHeight + stepH, frontZ + t * 0.55f),
            QVector3D(stepW * (0.95f + 0.1f * (i % 2)), stepH, stepD), c);
  }

  QVector3D skirtColor = lerp(C.stoneDark, QVector3D(0.0f, 0.0f, 0.0f), 0.25f);
  unitBox(out, unit, white, p.model, QVector3D(0.0f, 0.02f, 0.0f),
          QVector3D(baseWidth * 0.50f, 0.01f, baseDepth * 0.50f), skirtColor);
}

static inline void drawWalls(const DrawContext &p, ISubmitter &out, Mesh *,
                             Texture *white, const BarracksPalette &C) {
  constexpr float W = BuildingProportions::baseWidth;
  constexpr float D = BuildingProportions::baseDepth;
  constexpr float H = BuildingProportions::baseHeight;

  const float r = 0.09f;
  const float notch = 0.07f;

  const float leftX = -W * 0.5f;
  const float rightX = W * 0.5f;
  const float backZ = -D * 0.5f;
  const float frontZ = D * 0.5f;

  const int courses = std::max(4, int(H / (2.0f * r)));
  const float y0 = r;

  auto logX = [&](float y, float z, float x0, float x1, const QVector3D &col) {
    drawCylinder(out, p.model, QVector3D(x0 - notch, y, z),
                 QVector3D(x1 + notch, y, z), r, col, white);
  };
  auto logZ = [&](float y, float x, float z0, float z1, const QVector3D &col) {
    drawCylinder(out, p.model, QVector3D(x, y, z0 - notch),
                 QVector3D(x, y, z1 + notch), r, col, white);
  };

  const float doorW = BuildingProportions::doorWidth;
  const float doorH = BuildingProportions::doorHeight;
  const float gapHalf = doorW * 0.5f;

  for (int i = 0; i < courses; ++i) {
    float y = y0 + i * (2.0f * r);
    QVector3D logCol = lerp(C.timber, C.timberLight, (i % 2) * 0.25f);

    if (y <= (doorH - 0.5f * r)) {
      logX(y, frontZ, leftX, -gapHalf, logCol);
      logX(y, frontZ, +gapHalf, rightX, logCol);
    } else {
      logX(y, frontZ, leftX, rightX, logCol);
    }

    logX(y, backZ, leftX, rightX, logCol);
    logZ(y, leftX, backZ, frontZ, logCol);
    logZ(y, rightX, backZ, frontZ, logCol);
  }

  QVector3D postCol = C.woodDark;
  drawCylinder(out, p.model, QVector3D(-gapHalf, y0, frontZ),
               QVector3D(-gapHalf, y0 + doorH, frontZ), r * 0.95f, postCol,
               white);
  drawCylinder(out, p.model, QVector3D(+gapHalf, y0, frontZ),
               QVector3D(+gapHalf, y0 + doorH, frontZ), r * 0.95f, postCol,
               white);
  drawCylinder(out, p.model, QVector3D(-gapHalf, y0 + doorH, frontZ),
               QVector3D(+gapHalf, y0 + doorH, frontZ), r, C.timberLight,
               white);

  float braceY0 = H * 0.35f;
  float braceY1 = H * 0.95f;
  drawCylinder(out, p.model, QVector3D(leftX + 0.08f, braceY0, backZ + 0.10f),
               QVector3D(leftX + 0.38f, braceY1, backZ + 0.10f), r * 0.6f,
               C.woodDark, white);
  drawCylinder(out, p.model, QVector3D(rightX - 0.08f, braceY0, backZ + 0.10f),
               QVector3D(rightX - 0.38f, braceY1, backZ + 0.10f), r * 0.6f,
               C.woodDark, white);
  drawCylinder(out, p.model, QVector3D(leftX + 0.08f, braceY0, frontZ - 0.10f),
               QVector3D(leftX + 0.38f, braceY1, frontZ - 0.10f), r * 0.6f,
               C.woodDark, white);
  drawCylinder(out, p.model, QVector3D(rightX - 0.08f, braceY0, frontZ - 0.10f),
               QVector3D(rightX - 0.38f, braceY1, frontZ - 0.10f), r * 0.6f,
               C.woodDark, white);
}

struct ChimneyInfo {
  float x;
  float z;
  float baseY;
  float topY;
  float gapRadius;
};

static inline ChimneyInfo drawChimney(const DrawContext &p, ISubmitter &out,
                                      Mesh *unit, Texture *white,
                                      const BarracksPalette &C) {
  constexpr float W = BuildingProportions::baseWidth;
  constexpr float D = BuildingProportions::baseDepth;
  constexpr float H = BuildingProportions::baseHeight;
  constexpr float rise = BuildingProportions::roofPitch;

  float x = -W * 0.32f;
  float z = -D * 0.5f - 0.06f;

  float baseY = 0.18f;
  float ridgeY = H + rise;
  float topY = ridgeY + 0.35f;

  QVector3D baseSz(BuildingProportions::chimneyWidth * 0.65f, 0.16f,
                   BuildingProportions::chimneyWidth * 0.55f);
  unitBox(out, unit, white, p.model, QVector3D(x, baseY + baseSz.y(), z),
          baseSz, C.stoneDark);

  int segments = 4;
  float segH = (topY - (baseY + baseSz.y() * 2.0f)) / float(segments);
  float w0 = BuildingProportions::chimneyWidth * 0.55f;
  float w1 = BuildingProportions::chimneyWidth * 0.34f;

  for (int i = 0; i < segments; ++i) {
    float t = float(i) / float(segments - 1);
    float wy = w0 * (1.0f - t) + w1 * t;
    float hz = wy * 0.85f;
    QVector3D col = (i % 2 == 0) ? C.stone : lerp(C.stone, C.stoneDark, 0.35f);
    float yMid = baseY + baseSz.y() * 2.0f + segH * (i + 0.5f);
    unitBox(out, unit, white, p.model, QVector3D(x, yMid, z),
            QVector3D(wy, segH * 0.5f, hz), col);
  }

  float corbelY = topY - 0.14f;
  unitBox(out, unit, white, p.model, QVector3D(x, corbelY, z),
          QVector3D(w1 * 1.22f, 0.025f, w1 * 1.22f), C.stoneDark);
  unitBox(out, unit, white, p.model, QVector3D(x, corbelY + 0.05f, z),
          QVector3D(w1 * 1.05f, 0.02f, w1 * 1.05f),
          lerp(C.stone, C.stoneDark, 0.2f));

  float potH = 0.10f;
  unitBox(out, unit, white, p.model, QVector3D(x, topY + potH * 0.5f, z),
          QVector3D(w1 * 0.45f, potH * 0.5f, w1 * 0.45f),
          lerp(C.stoneDark, QVector3D(0.08f, 0.08f, 0.08f), 0.35f));

  unitBox(out, unit, white, p.model, QVector3D(x, H + rise * 0.55f, z + 0.06f),
          QVector3D(w1 * 1.35f, 0.01f, 0.04f),
          lerp(C.stoneDark, QVector3D(0.05f, 0.05f, 0.05f), 0.3f));

  return ChimneyInfo{x, z, baseY, topY + potH, 0.28f};
}

static inline void drawRoofs(const DrawContext &p, ISubmitter &out, Mesh *,
                             Texture *white, const BarracksPalette &C,
                             const ChimneyInfo &ch) {
  constexpr float W = BuildingProportions::baseWidth;
  constexpr float D = BuildingProportions::baseDepth;
  constexpr float H = BuildingProportions::baseHeight;
  constexpr float rise = BuildingProportions::roofPitch;
  constexpr float over = BuildingProportions::roofOverhang;

  const float r = 0.085f;

  const float leftX = -W * 0.5f;
  const float rightX = W * 0.5f;
  const float backZ = -D * 0.5f;
  const float frontZ = D * 0.5f;

  const float plateY = H;
  const float ridgeY = H + rise;

  drawCylinder(out, p.model, QVector3D(leftX - over, plateY, frontZ + over),
               QVector3D(rightX + over, plateY, frontZ + over), r, C.woodDark,
               white);
  drawCylinder(out, p.model, QVector3D(leftX - over, plateY, backZ - over),
               QVector3D(rightX + over, plateY, backZ - over), r, C.woodDark,
               white);

  drawCylinder(out, p.model, QVector3D(leftX - over * 0.5f, ridgeY, 0.0f),
               QVector3D(rightX + over * 0.5f, ridgeY, 0.0f), r, C.timberLight,
               white);

  const int pairs = 7;
  for (int i = 0; i < pairs; ++i) {
    float t = (pairs == 1) ? 0.0f : (float(i) / float(pairs - 1));
    float x = (leftX - over * 0.5f) * (1.0f - t) + (rightX + over * 0.5f) * t;

    drawCylinder(out, p.model, QVector3D(x, plateY, backZ - over),
                 QVector3D(x, ridgeY, 0.0f), r * 0.85f, C.woodDark, white);

    drawCylinder(out, p.model, QVector3D(x, plateY, frontZ + over),
                 QVector3D(x, ridgeY, 0.0f), r * 0.85f, C.woodDark, white);
  }

  auto purlin = [&](float tz, bool front) {
    float z = front ? (frontZ + over - tz * (frontZ + over))
                    : (backZ - over - tz * (backZ - over));
    float y = plateY + tz * (ridgeY - plateY);
    drawCylinder(out, p.model, QVector3D(leftX - over * 0.4f, y, z),
                 QVector3D(rightX + over * 0.4f, y, z), r * 0.6f, C.timber,
                 white);
  };
  purlin(0.35f, true);
  purlin(0.70f, true);
  purlin(0.35f, false);
  purlin(0.70f, false);

  auto splitThatch = [&](float y, float z, float rad, const QVector3D &col) {
    float gapL = ch.x - ch.gapRadius;
    float gapR = ch.x + ch.gapRadius;
    drawCylinder(out, p.model, QVector3D(leftX - over * 0.35f, y, z),
                 QVector3D(gapL, y, z), rad, col, white);
    drawCylinder(out, p.model, QVector3D(gapR, y, z),
                 QVector3D(rightX + over * 0.35f, y, z), rad, col, white);
  };

  auto thatchRow = [&](float tz, bool front, float radScale, float tint) {
    float z = front ? (frontZ + over - tz * (frontZ + over))
                    : (backZ - over - tz * (backZ - over));
    float y = plateY + tz * (ridgeY - plateY);
    QVector3D col = lerp(C.thatchDark, C.thatch, clamp01(tint));
    splitThatch(y, z, r * radScale, col);
  };

  const int rows = 9;
  for (int i = 0; i < rows; ++i) {
    float tz = float(i) / float(rows - 1);
    float s = 1.30f - 0.6f * tz;
    float tint = 0.2f + 0.6f * (1.0f - tz);
    thatchRow(tz, true, s, tint);
    thatchRow(tz * 0.98f, false, s, tint * 0.95f);
  }

  float eaveY = plateY + 0.06f;
  splitThatch(eaveY, frontZ + over * 1.02f, r * 0.55f, C.thatchDark);
  splitThatch(eaveY, backZ - over * 1.02f, r * 0.55f, C.thatchDark);

  float flashY = plateY + (ridgeY - plateY) * 0.55f;
  float flashZBack = backZ - over * 0.20f;
  float ring = ch.gapRadius + 0.04f;
  unitBox(out, nullptr, white, p.model, QVector3D(ch.x, flashY, flashZBack),
          QVector3D(ring, 0.008f, 0.02f), C.stoneDark);
}

static inline void drawDoor(const DrawContext &p, ISubmitter &out, Mesh *unit,
                            Texture *white, const BarracksPalette &C) {
  constexpr float D = BuildingProportions::baseDepth;
  constexpr float dW = BuildingProportions::doorWidth;
  constexpr float dH = BuildingProportions::doorHeight;

  const float y0 = 0.09f;
  const float zf = D * 0.5f;

  QVector3D frameCol = C.woodDark;
  unitBox(out, unit, white, p.model,
          QVector3D(0.0f, y0 + dH * 0.5f, zf + 0.015f),
          QVector3D(dW * 0.5f, dH * 0.5f, 0.02f), C.door);

  float plankW = dW / 6.0f;
  for (int i = 0; i < 6; ++i) {
    float cx = -dW * 0.5f + plankW * (i + 0.5f);
    QVector3D plankCol = lerp(C.door, C.woodDark, 0.15f * (i % 2));
    unitBox(out, unit, white, p.model,
            QVector3D(cx, y0 + dH * 0.5f, zf + 0.022f),
            QVector3D(plankW * 0.48f, dH * 0.48f, 0.006f), plankCol);
  }

  drawCylinder(out, p.model,
               QVector3D(-dW * 0.45f, y0 + dH * 0.35f, zf + 0.03f),
               QVector3D(+dW * 0.45f, y0 + dH * 0.35f, zf + 0.03f), 0.02f,
               frameCol, white);

  drawCylinder(out, p.model, QVector3D(dW * 0.32f, y0 + dH * 0.45f, zf + 0.04f),
               QVector3D(dW * 0.42f, y0 + dH * 0.45f, zf + 0.04f), 0.012f,
               C.timberLight, white);

  unitBox(out, unit, white, p.model,
          QVector3D(0.0f, y0 + dH + 0.10f, zf + 0.02f),
          QVector3D(0.22f, 0.06f, 0.01f), C.woodDark);
  unitBox(out, unit, white, p.model,
          QVector3D(0.0f, y0 + dH + 0.10f, zf + 0.025f),
          QVector3D(0.18f, 0.05f, 0.008f), C.team);
  unitBox(out, unit, white, p.model,
          QVector3D(0.0f, y0 + dH + 0.10f, zf + 0.03f),
          QVector3D(0.08f, 0.02f, 0.007f), C.teamTrim);
}

static inline void drawWindows(const DrawContext &p, ISubmitter &out,
                               Mesh *unit, Texture *white,
                               const BarracksPalette &C) {
  constexpr float W = BuildingProportions::baseWidth;
  constexpr float D = BuildingProportions::baseDepth;
  constexpr float H = BuildingProportions::baseHeight;

  const float leftX = -W * 0.5f;
  const float rightX = W * 0.5f;
  const float backZ = -D * 0.5f;
  const float frontZ = D * 0.5f;

  float w = BuildingProportions::windowWidth * 0.55f;
  float h = BuildingProportions::windowHeight * 0.55f;
  float frameT = 0.03f;

  auto framedWindow = [&](QVector3D center, bool shutters) {
    unitBox(out, unit, white, p.model, center + QVector3D(0, 0, 0.012f),
            QVector3D(w * 0.5f, h * 0.5f, 0.008f), C.window);
    unitBox(out, unit, white, p.model, center + QVector3D(0, 0, 0.016f),
            QVector3D(w * 0.5f, frameT, 0.006f), C.timber);
    unitBox(out, unit, white, p.model, center + QVector3D(0, 0, 0.016f),
            QVector3D(frameT, h * 0.5f, 0.006f), C.timber);
    unitBox(out, unit, white, p.model, center + QVector3D(0, 0, 0.016f),
            QVector3D(w * 0.5f, frameT, 0.006f), C.timber);
    unitBox(out, unit, white, p.model, center + QVector3D(0, 0, 0.016f),
            QVector3D(frameT, h * 0.5f, 0.006f), C.timber);

    unitBox(out, unit, white, p.model, center + QVector3D(0, 0, 0.02f),
            QVector3D(w * 0.02f, h * 0.48f, 0.004f), C.timberLight);
    unitBox(out, unit, white, p.model, center + QVector3D(0, 0, 0.02f),
            QVector3D(w * 0.48f, h * 0.02f, 0.004f), C.timberLight);

    if (shutters) {
      unitBox(out, unit, white, p.model,
              center + QVector3D(-w * 0.65f, 0, 0.018f),
              QVector3D(w * 0.30f, h * 0.55f, 0.004f), C.woodDark);
      unitBox(out, unit, white, p.model,
              center + QVector3D(+w * 0.65f, 0, 0.018f),
              QVector3D(w * 0.30f, h * 0.55f, 0.004f), C.woodDark);
    }
  };

  framedWindow(QVector3D(-0.65f, 0.95f, frontZ + 0.01f), true);
  framedWindow(QVector3D(+0.65f, 0.95f, frontZ + 0.01f), true);
  framedWindow(QVector3D(leftX + 0.06f, 0.85f, 0.0f), false);
  framedWindow(QVector3D(rightX - 0.06f, 0.85f, 0.0f), false);
  framedWindow(QVector3D(0.0f, 1.00f, backZ - 0.01f), true);
}

static inline void drawAnnex(const DrawContext &p, ISubmitter &out, Mesh *unit,
                             Texture *white, const BarracksPalette &C) {
  constexpr float W = BuildingProportions::baseWidth;
  constexpr float D = BuildingProportions::baseDepth;
  constexpr float h = BuildingProportions::annexHeight;
  constexpr float w = BuildingProportions::annexWidth;
  constexpr float d = BuildingProportions::annexDepth;

  float x = W * 0.5f + w * 0.5f - 0.05f;
  float z = 0.05f;

  unitBox(out, unit, white, p.model, QVector3D(x, h * 0.5f, z),
          QVector3D(w * 0.5f, h * 0.5f, d * 0.5f), C.plasterShade);

  unitBox(out, unit, white, p.model, QVector3D(x, h + 0.02f, z),
          QVector3D(w * 0.55f, 0.02f, d * 0.55f), C.woodDark);

  float plateY = h;
  float frontZ = z + d * 0.5f;
  float backZ = z - d * 0.5f;
  drawCylinder(out, p.model, QVector3D(x - w * 0.52f, plateY, backZ - 0.12f),
               QVector3D(x + w * 0.52f, plateY, backZ - 0.12f), 0.05f,
               C.woodDark, white);

  float ridgeY = h + BuildingProportions::annexRoofHeight;
  drawCylinder(out, p.model, QVector3D(x - w * 0.50f, ridgeY, backZ - 0.02f),
               QVector3D(x + w * 0.50f, ridgeY, backZ - 0.02f), 0.05f,
               C.timberLight, white);

  int rows = 6;
  for (int i = 0; i < rows; ++i) {
    float t = float(i) / float(rows - 1);
    float y = plateY + t * (ridgeY - plateY);
    float zrow = backZ - 0.02f - 0.10f * (1.0f - t);
    QVector3D col = lerp(C.thatchDark, C.thatch, 0.5f + 0.4f * (1.0f - t));
    drawCylinder(out, p.model, QVector3D(x - w * 0.55f, y, zrow),
                 QVector3D(x + w * 0.55f, y, zrow), 0.06f * (1.15f - 0.6f * t),
                 col, white);
  }

  unitBox(out, unit, white, p.model,
          QVector3D(x + w * 0.01f, 0.55f, frontZ + 0.01f),
          QVector3D(0.20f, 0.18f, 0.01f), C.door);
}

static inline void drawProps(const DrawContext &p, ISubmitter &out, Mesh *unit,
                             Texture *white, const BarracksPalette &C) {
  unitBox(out, unit, white, p.model, QVector3D(0.85f, 0.10f, 0.90f),
          QVector3D(0.16f, 0.10f, 0.16f), C.crate);
  unitBox(out, unit, white, p.model, QVector3D(0.85f, 0.22f, 0.90f),
          QVector3D(0.12f, 0.02f, 0.12f), C.woodDark);

  unitBox(out, unit, white, p.model, QVector3D(-0.9f, 0.12f, -0.80f),
          QVector3D(0.12f, 0.10f, 0.12f), C.crate);
  unitBox(out, unit, white, p.model, QVector3D(-0.9f, 0.20f, -0.80f),
          QVector3D(0.13f, 0.02f, 0.13f), C.woodDark);
}

static inline void drawBannerAndPole(const DrawContext &p, ISubmitter &out,
                                     Mesh *unit, Texture *white,
                                     const BarracksPalette &C) {
  constexpr float baseWidth = BuildingProportions::baseWidth;
  constexpr float baseDepth = BuildingProportions::baseDepth;
  constexpr float bannerPoleHeight = BuildingProportions::bannerPoleHeight;
  constexpr float bannerPoleRadius = BuildingProportions::bannerPoleRadius;
  constexpr float bannerWidth = BuildingProportions::bannerWidth;
  constexpr float bannerHeight = BuildingProportions::bannerHeight;

  float poleX = -baseWidth / 2 - 0.65f;
  float poleZ = baseDepth / 2 - 0.2f;

  float poleHeight = bannerPoleHeight * 1.9f;
  float poleRadius = bannerPoleRadius * 1.3f;
  float bw = bannerWidth * 1.8f;
  float bh = bannerHeight * 1.8f;

  QVector3D poleCenter(poleX, poleHeight / 2.0f, poleZ);
  QVector3D poleSize(poleRadius * 1.6f, poleHeight / 2.0f, poleRadius * 1.6f);
  unitBox(out, unit, white, p.model, poleCenter, poleSize, C.woodDark);

  float targetWidth = bw * 1.25f;
  float targetHeight = bh * 0.75f;
  float panelDepth = 0.02f;

  float beamLength = targetWidth * 0.45f;
  float beamY = poleHeight - targetHeight * 0.25f;
  QVector3D beamStart(poleX + 0.02f, beamY, poleZ);
  QVector3D beamEnd(poleX + beamLength + 0.02f, beamY, poleZ);
  drawCylinder(out, p.model, beamStart, beamEnd, poleRadius * 0.35f, C.timber,
               white);

  QVector3D connectorTop(beamEnd.x(), beamEnd.y() - targetHeight * 0.35f,
                         beamEnd.z());
  drawCylinder(out, p.model, beamEnd, connectorTop, poleRadius * 0.18f,
               C.timberLight, white);

  float panelX = beamEnd.x() + (targetWidth * 0.5f - beamLength);
  unitBox(out, unit, white, p.model,
          QVector3D(panelX, poleHeight - targetHeight / 2.0f, poleZ + 0.01f),
          QVector3D(targetWidth / 2.0f, targetHeight / 2.0f, panelDepth),
          C.team);

  unitBox(out, unit, white, p.model,
          QVector3D(panelX, poleHeight - targetHeight + 0.04f, poleZ + 0.01f),
          QVector3D(targetWidth / 2.0f + 0.02f, 0.04f, 0.015f), C.teamTrim);
  unitBox(out, unit, white, p.model,
          QVector3D(panelX, poleHeight - 0.04f, poleZ + 0.01f),
          QVector3D(targetWidth / 2.0f + 0.02f, 0.04f, 0.015f), C.teamTrim);
}

static inline void drawRallyFlagIfAny(const DrawContext &p, ISubmitter &out,
                                      Texture *white,
                                      const BarracksPalette &C) {
  if (auto *prod =
          p.entity->getComponent<Engine::Core::ProductionComponent>()) {
    if (prod->rallySet && p.resources) {
      auto flag = Render::Geom::Flag::create(prod->rallyX, prod->rallyZ,
                                             QVector3D(1.0f, 0.9f, 0.2f),
                                             C.woodDark, 1.0f);
      Mesh *unit = p.resources->unit();
      out.mesh(unit, flag.pole, flag.poleColor, white, 1.0f);
      out.mesh(unit, flag.pennant, flag.pennantColor, white, 1.0f);
      out.mesh(unit, flag.finial, flag.pennantColor, white, 1.0f);
    }
  }
}

static inline void drawHealthBar(const DrawContext &p, ISubmitter &out,
                                 Mesh *unit, Texture *white) {
  if (!p.entity)
    return;
  auto *u = p.entity->getComponent<Engine::Core::UnitComponent>();
  if (!u)
    return;

  int mh = std::max(1, u->maxHealth);
  float ratio = std::clamp(u->health / float(mh), 0.0f, 1.0f);
  if (ratio <= 0.0f)
    return;

  constexpr float baseHeight = BuildingProportions::baseHeight;
  constexpr float roofPitch = BuildingProportions::roofPitch;
  float roofPeak = baseHeight + roofPitch;
  float barY = roofPeak + 0.12f;

  constexpr float barWidth = BuildingProportions::baseWidth * 0.9f;
  constexpr float barHeight = 0.08f;
  constexpr float barDepth = 0.12f;

  QVector3D bgColor(0.06f, 0.06f, 0.06f);
  unitBox(out, unit, white, p.model, QVector3D(0.0f, barY, 0.0f),
          QVector3D(barWidth / 2.0f, barHeight / 2.0f, barDepth / 2.0f),
          bgColor);

  float fillWidth = barWidth * ratio;
  float fillX = -(barWidth - fillWidth) * 0.5f;

  QVector3D red(0.85f, 0.15f, 0.15f);
  QVector3D green(0.22f, 0.78f, 0.22f);
  QVector3D fgColor = green * ratio + red * (1.0f - ratio);

  unitBox(out, unit, white, p.model, QVector3D(fillX, barY + 0.005f, 0.0f),
          QVector3D(fillWidth / 2.0f, (barHeight / 2.0f) * 0.9f,
                    (barDepth / 2.0f) * 0.95f),
          fgColor);
}

static inline void drawSelectionFX(const DrawContext &p, ISubmitter &out) {
  QMatrix4x4 M;
  QVector3D pos = p.model.column(3).toVector3D();
  M.translate(pos.x(), 0.1f, pos.z());
  M.scale(2.2f, 1.0f, 2.0f);
  if (p.selected)
    out.selectionRing(M, 0.6f, 0.25f, QVector3D(0.2f, 0.85f, 0.2f));
  else if (p.hovered)
    out.selectionRing(M, 0.35f, 0.15f, QVector3D(0.95f, 0.92f, 0.25f));
}

static void drawBarracks(const DrawContext &p, ISubmitter &out) {
  if (!p.resources || !p.entity)
    return;

  auto *t = p.entity->getComponent<Engine::Core::TransformComponent>();
  auto *r = p.entity->getComponent<Engine::Core::RenderableComponent>();
  if (!t || !r)
    return;

  Mesh *unit = p.resources->unit();
  Texture *white = p.resources->white();

  QVector3D team(r->color[0], r->color[1], r->color[2]);
  BarracksPalette C = makePalette(team);

  drawFoundation(p, out, unit, white, C);
  drawAnnex(p, out, unit, white, C);
  drawWalls(p, out, unit, white, C);
  ChimneyInfo ch = drawChimney(p, out, unit, white, C);
  drawRoofs(p, out, unit, white, C, ch);
  drawDoor(p, out, unit, white, C);
  drawWindows(p, out, unit, white, C);
  drawBannerAndPole(p, out, unit, white, C);
  drawProps(p, out, unit, white, C);

  drawRallyFlagIfAny(p, out, white, C);
  drawHealthBar(p, out, unit, white);
  drawSelectionFX(p, out);
}

} // namespace

void registerBarracksRenderer(Render::GL::EntityRendererRegistry &registry) {
  registry.registerRenderer("barracks", drawBarracks);
}

} // namespace Render::GL
