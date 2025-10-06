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

static inline void drawCylinder(ISubmitter &out, const QMatrix4x4 &model,
                                const QVector3D &a, const QVector3D &b,
                                float radius, const QVector3D &color,
                                Texture *white) {
  out.mesh(getUnitCylinder(), cylinderBetween(a, b, radius) * model, color,
           white, 1.0f);
}

static inline void drawSphere(ISubmitter &out, const QMatrix4x4 &model,
                              const QVector3D &pos, float radius,
                              const QVector3D &color, Texture *white) {
  out.mesh(getUnitSphere(), sphereAt(pos, radius) * model, color, white, 1.0f);
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
}

static inline void drawTimberFrame(const DrawContext &p, ISubmitter &out,
                                   Mesh *unit, Texture *white,
                                   const BarracksPalette &C) {
  constexpr float baseWidth = BuildingProportions::baseWidth;
  constexpr float baseDepth = BuildingProportions::baseDepth;
  constexpr float baseHeight = BuildingProportions::baseHeight;
  constexpr float cornerPostRadius = BuildingProportions::cornerPostRadius;
  constexpr float beamThickness = BuildingProportions::beamThickness;

  auto cornerPost = [&](float x, float z) {
    QVector3D bottom(x, 0.0f, z);
    QVector3D top(x, baseHeight, z);
    drawCylinder(out, p.model, bottom, top, cornerPostRadius, C.timber, white);
  };

  float hw = baseWidth / 2 - cornerPostRadius;
  float hd = baseDepth / 2 - cornerPostRadius;
  cornerPost(hw, hd);
  cornerPost(-hw, hd);
  cornerPost(hw, -hd);
  cornerPost(-hw, -hd);

  auto beam = [&](const QVector3D &a, const QVector3D &b) {
    drawCylinder(out, p.model, a, b, beamThickness * 0.6f, C.timber, white);
  };

  float y = baseHeight;
  beam(QVector3D(-hw, y, hd), QVector3D(hw, y, hd));
  beam(QVector3D(-hw, y, -hd), QVector3D(hw, y, -hd));
  beam(QVector3D(-hw, y, -hd), QVector3D(-hw, y, hd));
  beam(QVector3D(hw, y, -hd), QVector3D(hw, y, hd));

  y = baseHeight * 0.5f;
  beam(QVector3D(-hw, y, hd), QVector3D(hw, y, hd));
  beam(QVector3D(-hw, y, -hd), QVector3D(hw, y, -hd));
}

static inline void drawWalls(const DrawContext &p, ISubmitter &out, Mesh *unit,
                             Texture *white, const BarracksPalette &C) {
  constexpr float baseWidth = BuildingProportions::baseWidth;
  constexpr float baseDepth = BuildingProportions::baseDepth;
  constexpr float baseHeight = BuildingProportions::baseHeight;
  constexpr float wallThickness = BuildingProportions::wallThickness;
  constexpr float cornerPostRadius = BuildingProportions::cornerPostRadius;

  float wallY = baseHeight / 2;
  float wallH = baseHeight * 0.85f;

  unitBox(out, unit, white, p.model,
          QVector3D(-baseWidth / 2 + 0.35f, wallY,
                    baseDepth / 2 - wallThickness / 2),
          QVector3D(0.55f, wallH / 2, wallThickness / 2), C.plaster);
  unitBox(out, unit, white, p.model,
          QVector3D(baseWidth / 2 - 0.35f, wallY,
                    baseDepth / 2 - wallThickness / 2),
          QVector3D(0.55f, wallH / 2, wallThickness / 2), C.plaster);

  unitBox(
      out, unit, white, p.model,
      QVector3D(0.0f, wallY, -baseDepth / 2 + wallThickness / 2),
      QVector3D(baseWidth / 2 - cornerPostRadius, wallH / 2, wallThickness / 2),
      C.plaster);

  unitBox(
      out, unit, white, p.model,
      QVector3D(-baseWidth / 2 + wallThickness / 2, wallY, 0.0f),
      QVector3D(wallThickness / 2, wallH / 2, baseDepth / 2 - cornerPostRadius),
      C.plasterShade);

  unitBox(
      out, unit, white, p.model,
      QVector3D(baseWidth / 2 - wallThickness / 2, wallY, 0.0f),
      QVector3D(wallThickness / 2, wallH / 2, baseDepth / 2 - cornerPostRadius),
      C.plasterShade);
}

static inline void drawRoofs(const DrawContext &p, ISubmitter &out, Mesh *unit,
                             Texture *white, const BarracksPalette &C) {
  constexpr float baseWidth = BuildingProportions::baseWidth;
  constexpr float baseDepth = BuildingProportions::baseDepth;
  constexpr float baseHeight = BuildingProportions::baseHeight;
  constexpr float roofPitch = BuildingProportions::roofPitch;
  constexpr float roofOverhang = BuildingProportions::roofOverhang;
  constexpr float thatchLayerHeight = BuildingProportions::thatchLayerHeight;

  float roofBase = baseHeight;
  float roofPeak = roofBase + roofPitch;

  float layerWidth = baseWidth / 2 + roofOverhang;
  float layerDepth = baseDepth / 2 + roofOverhang;

  unitBox(out, unit, white, p.model,
          QVector3D(0.0f, roofBase + thatchLayerHeight * 0.5f, 0.0f),
          QVector3D(layerWidth, thatchLayerHeight / 2, layerDepth), C.thatch);

  unitBox(
      out, unit, white, p.model,
      QVector3D(0.0f, roofBase + thatchLayerHeight * 1.8f, 0.0f),
      QVector3D(layerWidth * 0.85f, thatchLayerHeight / 2, layerDepth * 0.85f),
      C.thatchDark);

  unitBox(
      out, unit, white, p.model,
      QVector3D(0.0f, roofBase + thatchLayerHeight * 3.0f, 0.0f),
      QVector3D(layerWidth * 0.7f, thatchLayerHeight / 2, layerDepth * 0.7f),
      C.thatch);
}

static inline void drawDoor(const DrawContext &p, ISubmitter &out, Mesh *unit,
                            Texture *white, const BarracksPalette &C) {
  constexpr float baseDepth = BuildingProportions::baseDepth;
  constexpr float doorWidth = BuildingProportions::doorWidth;
  constexpr float doorHeight = BuildingProportions::doorHeight;

  unitBox(out, unit, white, p.model,
          QVector3D(0.0f, doorHeight / 2, baseDepth / 2 + 0.01f),
          QVector3D(doorWidth / 2, doorHeight / 2, 0.05f), C.door);

  float trimW = 0.04f;
  unitBox(out, unit, white, p.model,
          QVector3D(0.0f, doorHeight + trimW, baseDepth / 2 + 0.01f),
          QVector3D(doorWidth / 2 + trimW, trimW, 0.02f), C.timber);
}

static inline void drawWindows(const DrawContext &p, ISubmitter &out,
                               Mesh *unit, Texture *white,
                               const BarracksPalette &C) {
  constexpr float baseWidth = BuildingProportions::baseWidth;
  constexpr float baseDepth = BuildingProportions::baseDepth;
  constexpr float baseHeight = BuildingProportions::baseHeight;
  constexpr float windowWidth = BuildingProportions::windowWidth;
  constexpr float windowHeight = BuildingProportions::windowHeight;

  unitBox(out, unit, white, p.model,
          QVector3D(0.0f, baseHeight * 0.6f, -baseDepth / 2 - 0.01f),
          QVector3D(windowWidth / 2, windowHeight / 2, 0.05f), C.window);

  unitBox(out, unit, white, p.model,
          QVector3D(-baseWidth / 2 - 0.01f, baseHeight * 0.6f, 0.2f),
          QVector3D(0.05f, windowHeight / 2 * 0.8f, windowWidth / 2 * 0.7f),
          C.window);
  unitBox(out, unit, white, p.model,
          QVector3D(baseWidth / 2 + 0.01f, baseHeight * 0.6f, -0.2f),
          QVector3D(0.05f, windowHeight / 2 * 0.8f, windowWidth / 2 * 0.7f),
          C.window);
}

static inline void drawAnnex(const DrawContext &p, ISubmitter &out, Mesh *unit,
                             Texture *white, const BarracksPalette &C) {
  constexpr float baseWidth = BuildingProportions::baseWidth;
  constexpr float annexWidth = BuildingProportions::annexWidth;
  constexpr float annexDepth = BuildingProportions::annexDepth;
  constexpr float annexHeight = BuildingProportions::annexHeight;
  constexpr float annexRoofHeight = BuildingProportions::annexRoofHeight;
  constexpr float cornerPostRadius = BuildingProportions::cornerPostRadius;

  float annexX = baseWidth / 2 + annexWidth / 2 - 0.2f;
  float annexY = annexHeight / 2;

  unitBox(out, unit, white, p.model, QVector3D(annexX, annexY, -0.3f),
          QVector3D(annexWidth / 2, annexHeight / 2, annexDepth / 2),
          C.plasterShade);

  auto annexPost = [&](float ox, float oz) {
    QVector3D base(annexX + ox, 0.0f, -0.3f + oz);
    QVector3D top(annexX + ox, annexHeight, -0.3f + oz);
    drawCylinder(out, p.model, base, top, cornerPostRadius * 0.7f, C.timber,
                 white);
  };

  float hw = annexWidth / 2 * 0.8f;
  float hd = annexDepth / 2 * 0.8f;
  annexPost(hw, hd);
  annexPost(-hw, hd);
  annexPost(hw, -hd);
  annexPost(-hw, -hd);

  unitBox(out, unit, white, p.model,
          QVector3D(annexX, annexHeight + annexRoofHeight / 2, -0.3f),
          QVector3D(annexWidth / 2 + 0.1f, annexRoofHeight / 2,
                    annexDepth / 2 + 0.1f),
          C.thatchDark);
}

static inline void drawChimney(const DrawContext &p, ISubmitter &out,
                               Mesh *unit, Texture *white,
                               const BarracksPalette &C) {
  constexpr float baseWidth = BuildingProportions::baseWidth;
  constexpr float baseDepth = BuildingProportions::baseDepth;
  constexpr float baseHeight = BuildingProportions::baseHeight;
  constexpr float roofPitch = BuildingProportions::roofPitch;
  constexpr float chimneyWidth = BuildingProportions::chimneyWidth;
  constexpr float chimneyHeight = BuildingProportions::chimneyHeight;
  constexpr float chimneyCapSize = BuildingProportions::chimneyCapSize;

  float chimneyX = -baseWidth / 2 + 0.4f;
  float chimneyZ = -baseDepth / 2 + 0.3f;
  float chimneyBase = baseHeight + roofPitch * 0.3f;

  unitBox(out, unit, white, p.model,
          QVector3D(chimneyX, chimneyBase + chimneyHeight / 2, chimneyZ),
          QVector3D(chimneyWidth / 2, chimneyHeight / 2, chimneyWidth / 2),
          C.stone);

  unitBox(out, unit, white, p.model,
          QVector3D(chimneyX, chimneyBase + chimneyHeight + 0.08f, chimneyZ),
          QVector3D(chimneyCapSize / 2, 0.08f, chimneyCapSize / 2),
          C.stoneDark);
}

static inline void drawPavers(const DrawContext &p, ISubmitter &out, Mesh *unit,
                              Texture *white, const BarracksPalette &C) {
  constexpr float baseDepth = BuildingProportions::baseDepth;
  constexpr float foundationHeight = BuildingProportions::foundationHeight;

  auto paver = [&](float ox, float oz, float sx, float sz) {
    unitBox(out, unit, white, p.model,
            QVector3D(0.0f, -foundationHeight + 0.02f, baseDepth / 2 + oz),
            QVector3D(sx, 0.02f, sz), C.path);
  };

  paver(0.0f, 0.3f, 0.8f, 0.25f);
  paver(0.0f, 0.6f, 0.6f, 0.2f);
  paver(0.0f, 0.85f, 0.7f, 0.15f);
}

static inline void drawCrates(const DrawContext &p, ISubmitter &out, Mesh *unit,
                              Texture *white, const BarracksPalette &C) {
  constexpr float baseWidth = BuildingProportions::baseWidth;
  constexpr float baseDepth = BuildingProportions::baseDepth;

  float crateX = baseWidth / 2 - 0.4f;
  float crateZ = baseDepth / 2 - 0.3f;

  unitBox(out, unit, white, p.model, QVector3D(crateX, 0.12f, crateZ),
          QVector3D(0.12f, 0.12f, 0.12f), C.crate);
  unitBox(out, unit, white, p.model,
          QVector3D(crateX + 0.28f, 0.10f, crateZ + 0.05f),
          QVector3D(0.10f, 0.10f, 0.10f), C.crate * 0.9f);
}

static inline void drawFencePosts(const DrawContext &p, ISubmitter &out,
                                  Mesh *unit, Texture *white,
                                  const BarracksPalette &C) {
  constexpr float baseWidth = BuildingProportions::baseWidth;
  constexpr float baseDepth = BuildingProportions::baseDepth;

  for (int i = 0; i < 4; ++i) {
    float x = -baseWidth / 2 - 0.25f;
    float z = -baseDepth / 2 + 0.3f + i * 0.35f;
    QVector3D bottom(x, 0.0f, z);
    QVector3D top(x, 0.6f, z);
    drawCylinder(out, p.model, bottom, top, 0.04f, C.timber, white);
  }

  for (int i = 0; i < 3; ++i) {
    float x = -baseWidth / 2 - 0.25f;
    float z1 = -baseDepth / 2 + 0.3f + i * 0.35f;
    float z2 = z1 + 0.35f;
    drawCylinder(out, p.model, QVector3D(x, 0.35f, z1), QVector3D(x, 0.35f, z2),
                 0.025f, C.timberLight, white);
  }
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

  float poleX = -baseWidth / 2 - 0.35f;
  float poleZ = baseDepth / 2 - 0.4f;

  drawCylinder(out, p.model, QVector3D(poleX, 0.0f, poleZ),
               QVector3D(poleX, bannerPoleHeight, poleZ), bannerPoleRadius,
               C.timber, white);

  drawSphere(out, p.model, QVector3D(poleX, bannerPoleHeight + 0.08f, poleZ),
             0.08f, C.teamTrim, white);

  unitBox(out, unit, white, p.model,
          QVector3D(poleX + bannerWidth / 2,
                    bannerPoleHeight - bannerHeight / 2, poleZ),
          QVector3D(bannerWidth / 2, bannerHeight / 2, 0.02f), C.team);

  unitBox(out, unit, white, p.model,
          QVector3D(poleX + bannerWidth / 2,
                    bannerPoleHeight - bannerHeight + 0.04f, poleZ),
          QVector3D(bannerWidth / 2 + 0.02f, 0.04f, 0.015f), C.teamTrim);
  unitBox(out, unit, white, p.model,
          QVector3D(poleX + bannerWidth / 2, bannerPoleHeight - 0.04f, poleZ),
          QVector3D(bannerWidth / 2 + 0.02f, 0.04f, 0.015f), C.teamTrim);
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

static inline void drawSelectionFX(const DrawContext &p, ISubmitter &out) {
  QMatrix4x4 M;
  QVector3D pos = p.model.column(3).toVector3D();
  M.translate(pos.x(), 0.0f, pos.z());
  M.scale(2.2f, 1.0f, 2.0f);
  if (p.selected)
    out.selectionSmoke(M, QVector3D(0.2f, 0.85f, 0.2f), 0.35f);
  else if (p.hovered)
    out.selectionSmoke(M, QVector3D(0.95f, 0.92f, 0.25f), 0.22f);
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
  drawTimberFrame(p, out, unit, white, C);
  drawWalls(p, out, unit, white, C);
  drawRoofs(p, out, unit, white, C);
  drawDoor(p, out, unit, white, C);
  drawWindows(p, out, unit, white, C);
  drawAnnex(p, out, unit, white, C);
  drawChimney(p, out, unit, white, C);
  drawPavers(p, out, unit, white, C);
  drawCrates(p, out, unit, white, C);
  drawFencePosts(p, out, unit, white, C);
  drawBannerAndPole(p, out, unit, white, C);

  drawRallyFlagIfAny(p, out, white, C);
  drawSelectionFX(p, out);
}

} // namespace

void registerBarracksRenderer(Render::GL::EntityRendererRegistry &registry) {
  registry.registerRenderer("barracks", drawBarracks);
}

} // namespace Render::GL